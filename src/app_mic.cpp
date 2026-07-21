#include "app_mic.h"
#include "app_colors.h"
#include "app_common.h"
#include "app_config.h"
#include "app_connectivity.h"
#include "app_header.h"
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <strings.h>
#include <esp_heap_caps.h>
#include <time.h>

// Cardputer microSD SPI
static constexpr int MIC_SD_SCK = 40;
static constexpr int MIC_SD_MISO = 39;
static constexpr int MIC_SD_MOSI = 14;
static constexpr int MIC_SD_CS = 12;
static constexpr const char* MIC_SD_DIR = "/audioRecord";
static constexpr uint32_t MIC_SAMPLE_RATE = 16000;
// 256 样点 ≈ 16ms；多缓冲 + 落后 2 块再取用（对齐 M5 Mic 双槽）
static constexpr size_t MIC_CAPTURE_N = 256;
static constexpr size_t MIC_BUF_COUNT = 8;
static constexpr int MIC_PLOT_N = 48;
static constexpr uint32_t MIC_WIFI_TIMEOUT_MS = 10000;
static constexpr uint32_t MIC_NTP_TIMEOUT_MS = 8000;
static constexpr uint32_t MIC_MSG_HOLD_MS = 2500;
// 环形缓冲约 1s@32KB/s；主循环批量落盘
static constexpr size_t MIC_RING_SIZE = 32768;
static constexpr size_t MIC_RING_MASK = MIC_RING_SIZE - 1;
static constexpr size_t MIC_WRITE_CHUNK = 4096;
static constexpr int MIC_DRAIN_CHUNKS_PER_FRAME = 12;
static constexpr int MIC_LIST_MAX = 48;
static constexpr int MIC_LIST_VISIBLE = 5;
static constexpr int MIC_LIST_LINE_H = 14;
static constexpr int MIC_LIST_HINT_H = 12;
static constexpr size_t MIC_PLAY_SAMPLES = 1024;

enum class MicTimeSync : uint8_t {
    Idle = 0,
    BeginWifi,
    WaitWifi,
    BeginNtp,
    WaitNtp,
    Done,
};

enum class MicUiMode : uint8_t {
    Scope = 0,
    Help,
    List,
};

struct MicListEntry {
    char name[28];
    size_t size;
};

static M5Canvas micSpr(&M5Cardputer.Display);
static bool micSprOk = false;
static bool micHeaderReady = false;
static MicUiMode micUiMode = MicUiMode::Scope;
static int micUserGain = 1; // 1/2/4/8/16

static MicListEntry micList[MIC_LIST_MAX];
static int micListCount = 0;
static int micListSel = 0;
static int micListScroll = 0;
static bool micListDirty = true;

static bool micPlaying = false;
static File micPlayFile;
static uint32_t micPlayRate = MIC_SAMPLE_RATE;
static uint32_t micPlayDataBytes = 0;
static uint32_t micPlayDoneBytes = 0;
static int16_t micPlayBuf[2][MIC_PLAY_SAMPLES];
static uint8_t micPlayBufIdx = 0;
static bool micPlayEof = false;
static uint32_t micPlayUiMs = 0;

static MicTimeSync micSyncState = MicTimeSync::Idle;
static uint32_t micSyncDeadlineMs = 0;
static uint32_t micHeaderStatusMs = 0;

static bool micSdReady = false;
static bool micSdChecked = false;
static bool micRecording = false;
static File micRecFile;
static uint32_t micRecStartMs = 0;
static uint32_t micRecDataBytes = 0;
static char micStatusMsg[20] = "";
static uint32_t micStatusMsgUntilMs = 0;

// 录音环形缓冲：采集先入环，主循环攒块后写 TF（与显示串行，不抢 SPI）
static uint8_t* micRing = nullptr;
static size_t micRingW = 0;
static size_t micRingR = 0;
static uint8_t micWriteChunk[MIC_WRITE_CHUNK];
static bool micWriteFail = false;

// 多缓冲采集：record() 只入队，落后 2 块的缓冲才已填满可写盘/绘图
static int16_t micCapBuf[MIC_BUF_COUNT][MIC_CAPTURE_N];
static size_t micRecIdx = 2;
static size_t micDrawIdx = 0;
static uint8_t micSkipReady = 2;
static const int16_t* micViewSamples = nullptr;
static uint32_t micAmpHold = 3000;
static int micPeakHoldPx = 0;
static uint8_t micLiveBlink = 0;
static uint8_t micUiSkip = 0;
static bool micSpkWasEnded = false;
// 本会话列表里用过喇叭：回示波器勿再 Mic.begin（PDM 灌 LRCLK 会嗡），退出时保持拉低
static bool micHadSpeakerPlay = false;

// 开麦前先解除 hold，避免抢不到 G43/G46
static void micEnsureCaptureOn() {
    if (M5Cardputer.Mic.isRunning()) {
        return;
    }
    releaseAudioPinHolds();
    M5Cardputer.Mic.begin();
}

// ---- WAV 头（PCM 16bit mono）----
#pragma pack(push, 1)
struct MicWavHeader {
    char riff[4];
    uint32_t file_size;
    char wave[4];
    char fmt[4];
    uint32_t fmt_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data[4];
    uint32_t data_size;
};
#pragma pack(pop)

static void micFillWavHeader(MicWavHeader& h, const uint32_t data_bytes) {
    memcpy(h.riff, "RIFF", 4);
    h.file_size = 36 + data_bytes;
    memcpy(h.wave, "WAVE", 4);
    memcpy(h.fmt, "fmt ", 4);
    h.fmt_size = 16;
    h.audio_format = 1;
    h.num_channels = 1;
    h.sample_rate = MIC_SAMPLE_RATE;
    h.byte_rate = MIC_SAMPLE_RATE * 2;
    h.block_align = 2;
    h.bits_per_sample = 16;
    memcpy(h.data, "data", 4);
    h.data_size = data_bytes;
}

static bool micFinalizeRecording();
static bool micSyncBusy();

static void micSetStatusMsg(const char* msg) {
    strncpy(micStatusMsg, msg, sizeof(micStatusMsg) - 1);
    micStatusMsg[sizeof(micStatusMsg) - 1] = '\0';
    micStatusMsgUntilMs = millis() + MIC_MSG_HOLD_MS;
}

static bool micStatusMsgActive() {
    return micStatusMsg[0] != '\0' && static_cast<int32_t>(millis() - micStatusMsgUntilMs) < 0;
}

static size_t micRingUsed() {
    return micRingW - micRingR;
}

static size_t micRingFree() {
    return MIC_RING_SIZE - micRingUsed();
}

// 推入 PCM；满则丢本块
static bool micRingPush(const uint8_t* data, const size_t n) {
    if (micRing == nullptr || n == 0) {
        return false;
    }
    if (n > micRingFree()) {
        return false;
    }
    const size_t w = micRingW;
    const size_t first = min(n, MIC_RING_SIZE - (w & MIC_RING_MASK));
    memcpy(micRing + (w & MIC_RING_MASK), data, first);
    if (first < n) {
        memcpy(micRing, data + first, n - first);
    }
    micRingW = w + n;
    return true;
}

static size_t micRingPop(uint8_t* dst, const size_t max_n) {
    if (micRing == nullptr || max_n == 0) {
        return 0;
    }
    const size_t avail = micRingUsed();
    const size_t n = avail < max_n ? avail : max_n;
    if (n == 0) {
        return 0;
    }
    const size_t r = micRingR;
    const size_t first = min(n, MIC_RING_SIZE - (r & MIC_RING_MASK));
    memcpy(dst, micRing + (r & MIC_RING_MASK), first);
    if (first < n) {
        memcpy(dst + first, micRing, n - first);
    }
    micRingR = r + n;
    return n;
}

static bool micEnsureRing() {
    if (micRing != nullptr) {
        return true;
    }
    // 优先 PSRAM（有则用）；否则内部堆
    if (ESP.getPsramSize() > 0) {
        micRing = static_cast<uint8_t*>(
            heap_caps_malloc(MIC_RING_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (micRing != nullptr) {
            return true;
        }
    }
    micRing = static_cast<uint8_t*>(malloc(MIC_RING_SIZE));
    if (micRing != nullptr) {
        return true;
    }
    // 示波器 sprite 占 ~48KB 连续堆，先释放再分配录音环
    if (micSprOk) {
        micSpr.deleteSprite();
        micSprOk = false;
        micRing = static_cast<uint8_t*>(malloc(MIC_RING_SIZE));
    }
    return micRing != nullptr;
}

// 主循环落盘：优先整块；环过半时也写残余，避免丢样
static void micDrainToSd(const bool flush_all) {
    if (!micRecFile || micWriteFail) {
        return;
    }
    int wrote_chunks = 0;
    while (true) {
        const size_t avail = micRingUsed();
        if (avail == 0) {
            break;
        }
        if (!flush_all) {
            if (wrote_chunks >= MIC_DRAIN_CHUNKS_PER_FRAME) {
                break;
            }
            if (avail < MIC_WRITE_CHUNK && micRingUsed() < (MIC_RING_SIZE / 2)) {
                break;
            }
        }
        const size_t want = min(avail, MIC_WRITE_CHUNK);
        const size_t n = micRingPop(micWriteChunk, want);
        if (n == 0) {
            break;
        }
        if (micRecFile.write(micWriteChunk, n) != n) {
            micWriteFail = true;
            micRingR = micRingW;
            return;
        }
        micRecDataBytes += static_cast<uint32_t>(n);
        wrote_chunks++;
    }
}

static void micResetCapturePipeline() {
    micRecIdx = 2;
    micDrawIdx = 0;
    micSkipReady = 2;
    micViewSamples = nullptr;
    micUiSkip = 0;
    memset(micCapBuf, 0, sizeof(micCapBuf));
}

// 处理一块已完成的 PCM：入环并尽量落盘
static void micConsumeReady(const int16_t* ready) {
    micViewSamples = ready;
    if (!micRecording) {
        return;
    }
    if (micWriteFail) {
        micFinalizeRecording();
        micSetStatusMsg("no SD");
        return;
    }
    const size_t nbytes = MIC_CAPTURE_N * sizeof(int16_t);
    if (!micRingPush(reinterpret_cast<const uint8_t*>(ready), nbytes)) {
        micSetStatusMsg("buf full");
    }
    micDrainToSd(false);
    if (micWriteFail) {
        micFinalizeRecording();
        micSetStatusMsg("no SD");
    }
}

// 入队下一块；成功时消费落后 2 块的就绪缓冲（Mic.record 为异步）
static bool micPollCapture() {
    // 勿用 isEnabled（仅表示脚位已配置）；record() 内部会 begin，播完静音后会被再次拉起
    if (!M5Cardputer.Mic.isRunning()) {
        return false;
    }
    int16_t* dest = micCapBuf[micRecIdx];
    if (!M5Cardputer.Mic.record(dest, MIC_CAPTURE_N, MIC_SAMPLE_RATE, false)) {
        return false;
    }
    if (micSkipReady > 0) {
        micSkipReady--;
    } else {
        micConsumeReady(micCapBuf[micDrawIdx]);
    }
    micDrawIdx = (micDrawIdx + 1) % MIC_BUF_COUNT;
    micRecIdx = (micRecIdx + 1) % MIC_BUF_COUNT;
    return true;
}

// 尝试挂载 SD；失败返回 false
static bool micEnsureSd() {
    if (micSdReady) {
        return true;
    }
    SPI.begin(MIC_SD_SCK, MIC_SD_MISO, MIC_SD_MOSI, MIC_SD_CS);
    if (!SD.begin(MIC_SD_CS, SPI, 25000000)) {
        micSdReady = false;
        micSdChecked = true;
        return false;
    }
    if (SD.cardType() == CARD_NONE) {
        SD.end();
        micSdReady = false;
        micSdChecked = true;
        return false;
    }
    if (!SD.exists(MIC_SD_DIR)) {
        SD.mkdir(MIC_SD_DIR);
    }
    micSdReady = true;
    micSdChecked = true;
    return true;
}

static void micReleaseSd() {
    if (micSdReady) {
        SD.end();
        micSdReady = false;
    }
    micSdChecked = false;
}

static bool micHasValidTime(struct tm& out) {
    applyLocalTimezone();
    const time_t now = time(nullptr);
    if (now > 1600000000 && localtime_r(&now, &out) != nullptr) {
        return true;
    }
    if (M5.Rtc.isEnabled()) {
        const m5::rtc_datetime_t dt = M5.Rtc.getDateTime();
        if (dt.date.year >= 2020) {
            struct tm utc{};
            utc.tm_year = dt.date.year - 1900;
            utc.tm_mon = dt.date.month - 1;
            utc.tm_mday = dt.date.date;
            utc.tm_hour = dt.time.hours;
            utc.tm_min = dt.time.minutes;
            utc.tm_sec = dt.time.seconds;
            utc.tm_isdst = 0;
            setenv("TZ", "GMT0", 1);
            tzset();
            const time_t epoch = mktime(&utc);
            applyLocalTimezone();
            if (epoch > 1600000000 && localtime_r(&epoch, &out) != nullptr) {
                return true;
            }
        }
    }
    return false;
}

static void micBuildRecPath(char* path, const size_t path_len) {
    struct tm ti{};
    if (micHasValidTime(ti)) {
        snprintf(path, path_len, "%s/%04d%02d%02d_%02d%02d%02d.wav", MIC_SD_DIR, ti.tm_year + 1900,
                 ti.tm_mon + 1, ti.tm_mday, ti.tm_hour, ti.tm_min, ti.tm_sec);
    } else {
        snprintf(path, path_len, "%s/rec_%lu.wav", MIC_SD_DIR,
                 static_cast<unsigned long>(millis()));
    }
}

static bool micFinalizeRecording() {
    if (!micRecording) {
        if (micRecFile) {
            micRecFile.close();
        }
        return false;
    }
    micRecording = false;
    // 排空环形缓冲再写 WAV 头
    micDrainToSd(true);

    const bool had_file = static_cast<bool>(micRecFile);
    const bool write_fail = micWriteFail;
    const uint32_t data_bytes = micRecDataBytes;

    if (had_file) {
        MicWavHeader hdr{};
        micFillWavHeader(hdr, data_bytes);
        micRecFile.seek(0);
        micRecFile.write(reinterpret_cast<const uint8_t*>(&hdr), sizeof(hdr));
        micRecFile.flush();
        micRecFile.close();
    }
    micRecDataBytes = 0;
    micWriteFail = false;
    return had_file && !write_fail;
}

static bool micStartRecording() {
    if (micRecording || micUiMode != MicUiMode::Scope || micPlaying) {
        return false;
    }
    micEnsureCaptureOn();
    if (!micEnsureSd()) {
        micSetStatusMsg("no SD");
        return false;
    }
    if (!micEnsureRing()) {
        micSetStatusMsg("no mem");
        return false;
    }

    // 录音时停 WiFi/NTP，减少丢块与破音
    if (micSyncBusy()) {
        releaseConfigWifi();
        micSyncState = MicTimeSync::Done;
    }

    char path[48];
    micBuildRecPath(path, sizeof(path));
    // 同名已存在则改用时间戳文件名
    if (SD.exists(path)) {
        snprintf(path, sizeof(path), "%s/rec_%lu.wav", MIC_SD_DIR,
                 static_cast<unsigned long>(millis()));
    }

    micRecFile = SD.open(path, FILE_WRITE);
    if (!micRecFile) {
        micSetStatusMsg("no SD");
        return false;
    }

    MicWavHeader hdr{};
    micFillWavHeader(hdr, 0);
    if (micRecFile.write(reinterpret_cast<const uint8_t*>(&hdr), sizeof(hdr)) != sizeof(hdr)) {
        micRecFile.close();
        micSetStatusMsg("no SD");
        return false;
    }

    micRingW = 0;
    micRingR = 0;
    micRecDataBytes = 0;
    micWriteFail = false;
    micRecStartMs = millis();
    micRecording = true;
    micStatusMsg[0] = '\0';
    return true;
}

static void micToggleRecording() {
    if (micUiMode != MicUiMode::Scope || micPlaying) {
        return;
    }
    if (micRecording) {
        micFinalizeRecording();
    } else {
        micStartRecording();
    }
}

static const char* micBaseName(const char* name) {
    const char* slash = strrchr(name, '/');
    return slash != nullptr ? slash + 1 : name;
}

static bool micIsWavName(const char* name) {
    if (name == nullptr) {
        return false;
    }
    const size_t n = strlen(name);
    if (n < 5) {
        return false;
    }
    const char* ext = name + n - 4;
    return strcasecmp(ext, ".wav") == 0;
}

static int micListCmpDesc(const void* a, const void* b) {
    const MicListEntry* ea = static_cast<const MicListEntry*>(a);
    const MicListEntry* eb = static_cast<const MicListEntry*>(b);
    return strcasecmp(eb->name, ea->name); // 新文件名（时间戳）靠前
}

static void micFormatSizeShort(char* out, const size_t out_len, const size_t n) {
    if (n >= 1024u * 1024u) {
        snprintf(out, out_len, "%.1fM", static_cast<double>(n) / (1024.0 * 1024.0));
    } else if (n >= 1024u) {
        snprintf(out, out_len, "%.0fK", static_cast<double>(n) / 1024.0);
    } else {
        snprintf(out, out_len, "%uB", static_cast<unsigned>(n));
    }
}

static void micFormatDuration(char* out, const size_t out_len, const uint32_t data_bytes,
                              const uint32_t rate) {
    const uint32_t bytes_per_sec = rate > 0 ? rate * 2u : MIC_SAMPLE_RATE * 2u;
    const uint32_t sec = data_bytes / bytes_per_sec;
    snprintf(out, out_len, "%lu:%02lu", static_cast<unsigned long>(sec / 60u),
             static_cast<unsigned long>(sec % 60u));
}

// 扫描 /audioRecord 下 wav
static void micScanList() {
    micListCount = 0;
    micListSel = 0;
    micListScroll = 0;
    if (!micEnsureSd()) {
        return;
    }
    File dir = SD.open(MIC_SD_DIR);
    if (!dir || !dir.isDirectory()) {
        if (dir) {
            dir.close();
        }
        return;
    }
    File f = dir.openNextFile();
    while (f) {
        const char* base = micBaseName(f.name());
        if (!f.isDirectory() && micIsWavName(base) && micListCount < MIC_LIST_MAX) {
            MicListEntry& e = micList[micListCount];
            strncpy(e.name, base, sizeof(e.name) - 1);
            e.name[sizeof(e.name) - 1] = '\0';
            e.size = static_cast<size_t>(f.size());
            micListCount++;
        }
        f = dir.openNextFile();
    }
    dir.close();
    if (micListCount > 1) {
        qsort(micList, static_cast<size_t>(micListCount), sizeof(MicListEntry), micListCmpDesc);
    }
}

static void micClampListSel() {
    if (micListCount <= 0) {
        micListSel = 0;
        micListScroll = 0;
        return;
    }
    if (micListSel < 0) {
        micListSel = 0;
    }
    if (micListSel >= micListCount) {
        micListSel = micListCount - 1;
    }
    if (micListSel < micListScroll) {
        micListScroll = micListSel;
    }
    if (micListSel >= micListScroll + MIC_LIST_VISIBLE) {
        micListScroll = micListSel - MIC_LIST_VISIBLE + 1;
    }
}

static void micPlayStop(const bool restore_mic) {
    if (micPlayFile) {
        micPlayFile.close();
    }
    micPlaying = false;
    micPlayEof = false;
    micPlayDoneBytes = 0;
    micPlayDataBytes = 0;
    // 列表模式不要 Mic.begin：PDM 时钟灌进功放 LRCLK 会嗡嗡；拉低 I2S 脚即可
    releaseSpeakerQuiet();
    if (restore_mic) {
        micEnsureCaptureOn();
        micResetCapturePipeline();
    }
    micSpkWasEnded = true;
}

// 结束麦克占用，准备播 WAV
static void micPauseCaptureForPlay() {
    if (micRecording) {
        micFinalizeRecording();
    }
    if (M5Cardputer.Mic.isRunning()) {
        uint32_t wait_ms = millis() + 80;
        while (M5Cardputer.Mic.isRecording() && static_cast<int32_t>(millis() - wait_ms) < 0) {
            delay(1);
        }
        M5Cardputer.Mic.end();
        delay(10);
    }
    if (micSprOk) {
        micSpr.deleteSprite();
        micSprOk = false;
    }
}

static bool micPlayStartSelected() {
    if (micListCount <= 0 || micListSel < 0 || micListSel >= micListCount) {
        return false;
    }
    if (!micEnsureSd()) {
        micSetStatusMsg("no SD");
        return false;
    }

    micPauseCaptureForPlay();
    cancelSpeakerQuietRelease(); // 避免主循环 poll 在播中途拆掉 Speaker
    releaseAudioPinHolds();
    micHadSpeakerPlay = true;
    if (!M5Cardputer.Speaker.isRunning()) {
        M5Cardputer.Speaker.begin();
    }
    applyAppSpeakerVolume();

    char path[64];
    snprintf(path, sizeof(path), "%s/%s", MIC_SD_DIR, micList[micListSel].name);
    micPlayFile = SD.open(path, FILE_READ);
    if (!micPlayFile) {
        micPlayStop(false);
        micSetStatusMsg("open fail");
        return false;
    }

    MicWavHeader hdr{};
    if (micPlayFile.read(reinterpret_cast<uint8_t*>(&hdr), sizeof(hdr)) != sizeof(hdr) ||
        memcmp(hdr.riff, "RIFF", 4) != 0 || memcmp(hdr.wave, "WAVE", 4) != 0 ||
        hdr.bits_per_sample != 16 || hdr.num_channels != 1) {
        micPlayStop(false);
        micSetStatusMsg("bad wav");
        return false;
    }

    micPlayRate = hdr.sample_rate > 0 ? hdr.sample_rate : MIC_SAMPLE_RATE;
    micPlayDataBytes = hdr.data_size;
    micPlayDoneBytes = 0;
    micPlayBufIdx = 0;
    micPlayEof = false;
    micPlaying = true;
    micPlayUiMs = 0;
    micStatusMsg[0] = '\0';

    // 预填两块缓冲启动播放
    for (int i = 0; i < 2 && !micPlayEof; i++) {
        const size_t n = micPlayFile.read(reinterpret_cast<uint8_t*>(micPlayBuf[i]),
                                          MIC_PLAY_SAMPLES * sizeof(int16_t));
        const size_t samples = n / sizeof(int16_t);
        if (samples == 0) {
            micPlayEof = true;
            break;
        }
        micPlayDoneBytes += static_cast<uint32_t>(n);
        M5Cardputer.Speaker.playRaw(micPlayBuf[i], samples, micPlayRate, false, 1, 0, i == 0);
    }
    return true;
}

// 每帧向 Speaker 队列补块
static void micPlayFeed() {
    if (!micPlaying) {
        return;
    }
    while (!micPlayEof && M5Cardputer.Speaker.isPlaying(0) < 2) {
        const size_t n = micPlayFile.read(reinterpret_cast<uint8_t*>(micPlayBuf[micPlayBufIdx]),
                                          MIC_PLAY_SAMPLES * sizeof(int16_t));
        const size_t samples = n / sizeof(int16_t);
        if (samples == 0) {
            micPlayEof = true;
            break;
        }
        micPlayDoneBytes += static_cast<uint32_t>(n);
        M5Cardputer.Speaker.playRaw(micPlayBuf[micPlayBufIdx], samples, micPlayRate, false, 1, 0,
                                    false);
        micPlayBufIdx = static_cast<uint8_t>(micPlayBufIdx ^ 1u);
    }
    if (micPlayEof && !M5Cardputer.Speaker.isPlaying()) {
        micPlayStop(false); // 列表内保持喇叭脚拉低，不启 Mic
        flushSpeakerVolumeSave();
        micListDirty = true;
        micSetStatusMsg("done");
    }
}

static bool micDeleteSelected() {
    if (micPlaying || micListCount <= 0 || micListSel < 0 || micListSel >= micListCount) {
        return false;
    }
    if (!micEnsureSd()) {
        micSetStatusMsg("no SD");
        return false;
    }
    char path[64];
    snprintf(path, sizeof(path), "%s/%s", MIC_SD_DIR, micList[micListSel].name);
    if (!SD.remove(path)) {
        micSetStatusMsg("del fail");
        return false;
    }
    // 从内存列表移除该项
    for (int i = micListSel; i + 1 < micListCount; i++) {
        micList[i] = micList[i + 1];
    }
    micListCount--;
    micClampListSel();
    micListDirty = true;
    micSetStatusMsg("deleted");
    return true;
}

static void micEnterListMode() {
    if (micRecording) {
        micFinalizeRecording();
    }
    if (micPlaying) {
        micPlayStop(false);
    }
    // 列表不采麦：关 Mic 并拉低喇叭脚，避免 PDM 时钟灌进功放
    if (M5Cardputer.Mic.isRunning()) {
        uint32_t wait_ms = millis() + 80;
        while (M5Cardputer.Mic.isRecording() && static_cast<int32_t>(millis() - wait_ms) < 0) {
            delay(1);
        }
        M5Cardputer.Mic.end();
        delay(10);
    }
    releaseSpeakerQuiet();
    if (micSprOk) {
        micSpr.deleteSprite();
        micSprOk = false;
    }
    micUiMode = MicUiMode::List;
    micScanList();
    micListDirty = true;
}

static void micLeaveListMode() {
    if (micPlaying) {
        micPlayStop(false);
    }
    flushSpeakerVolumeSave();
    micUiMode = MicUiMode::Scope;
    if (micSprOk) {
        micSpr.deleteSprite();
        micSprOk = false;
    }
    // 播过音后再开麦会把 PDM 时钟灌进功放；保持喇叭脚拉低，录音时再 micEnsureCaptureOn
    if (micHadSpeakerPlay) {
        releaseSpeakerQuiet();
    } else {
        micEnsureCaptureOn();
    }
    micResetCapturePipeline();
    // 整页重绘 Record，避免只刷新底栏/小字、列表残影残留
    beginAppScreen("Mic");
    micHeaderReady = true;
    M5Cardputer.Display.fillRect(0, APP_CONTENT_Y, M5Cardputer.Display.width(),
                                 M5Cardputer.Display.height() - APP_CONTENT_Y, BLACK);
}

static void drawMicListHints() {
    const int hint_y = M5Cardputer.Display.height() - MIC_LIST_HINT_H;
    M5Cardputer.Display.fillRect(0, hint_y, M5Cardputer.Display.width(), MIC_LIST_HINT_H, BLACK);
    int cx = APP_CONTENT_X;
    if (micPlaying) {
        cx += drawTextBadge(cx, hint_y, "Ent", 1);
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
        M5Cardputer.Display.setCursor(cx, hint_y + 1);
        M5Cardputer.Display.print("stop ");
        cx = M5Cardputer.Display.getCursorX();
        cx += drawTextBadge(cx, hint_y, "-=", 1);
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
        M5Cardputer.Display.setCursor(cx, hint_y + 1);
        M5Cardputer.Display.print("vol ");
        cx = M5Cardputer.Display.getCursorX();
    } else {
        cx += drawKeyBadge(cx, hint_y, 'p', 1);
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
        M5Cardputer.Display.setCursor(cx, hint_y + 1);
        M5Cardputer.Display.print("play ");
        cx = M5Cardputer.Display.getCursorX();
        cx += drawTextBadge(cx, hint_y, "Bksp", 1);
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
        M5Cardputer.Display.setCursor(cx, hint_y + 1);
        M5Cardputer.Display.print("del ");
        cx = M5Cardputer.Display.getCursorX();
    }
    cx += drawKeyBadge(cx, hint_y, 'l', 1);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
    M5Cardputer.Display.setCursor(cx, hint_y + 1);
    M5Cardputer.Display.print("back");
}

// 仅刷新列表顶部进度/状态行（播放中用，避免整页闪烁）
static void drawMicListProgress() {
    // 与 header 留出 APP_CONTENT_Y 间距，避免贴顶
    const int y = APP_CONTENT_Y;
    const int w = M5Cardputer.Display.width() - APP_CONTENT_X;
    M5Cardputer.Display.fillRect(APP_CONTENT_X, y, w, 10, BLACK);
    M5Cardputer.Display.setTextSize(1);

    char head[36];
    if (micPlaying) {
        char tcur[8];
        char ttot[8];
        micFormatDuration(tcur, sizeof(tcur), micPlayDoneBytes, micPlayRate);
        micFormatDuration(ttot, sizeof(ttot), micPlayDataBytes, micPlayRate);
        snprintf(head, sizeof(head), "PLAY %s/%s v%d", tcur, ttot,
                 getAppSpeakerVolumePercent());
        M5Cardputer.Display.setTextColor(APP_COLOR_OK, BLACK);
    } else if (micStatusMsgActive()) {
        snprintf(head, sizeof(head), "%s  %d files", micStatusMsg, micListCount);
        M5Cardputer.Display.setTextColor(APP_COLOR_WARN, BLACK);
    } else {
        snprintf(head, sizeof(head), "%d files", micListCount);
        M5Cardputer.Display.setTextColor(CYAN, BLACK);
    }
    M5Cardputer.Display.setCursor(APP_CONTENT_X, y);
    M5Cardputer.Display.print(head);
}

static void drawMicListScreen() {
    beginAppScreen("Mic Recs");
    micHeaderReady = false;
    M5Cardputer.Display.setTextSize(1);

    int y = APP_CONTENT_Y;
    if (!micSdReady && micSdChecked) {
        M5Cardputer.Display.setTextColor(APP_COLOR_ERROR, BLACK);
        M5Cardputer.Display.setCursor(APP_CONTENT_X, y);
        M5Cardputer.Display.print("no SD");
        drawMicListHints();
        updateAppHeaderStatus();
        micListDirty = false;
        return;
    }

    // 状态行与 drawMicListProgress 同一位置
    drawMicListProgress();
    y += 12;

    if (micListCount <= 0) {
        M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
        M5Cardputer.Display.setCursor(APP_CONTENT_X, y);
        M5Cardputer.Display.print("empty /audioRecord");
        drawMicListHints();
        updateAppHeaderStatus();
        micListDirty = false;
        return;
    }

    const int screen_w = M5Cardputer.Display.width();
    const int end = min(micListScroll + MIC_LIST_VISIBLE, micListCount);
    for (int i = micListScroll; i < end; i++) {
        const MicListEntry& e = micList[i];
        const bool sel = (i == micListSel);
        const int row_y = y + (i - micListScroll) * MIC_LIST_LINE_H;
        if (sel) {
            // 选中条相对文字上移 3px，与字形基线对齐
            M5Cardputer.Display.fillRect(0, row_y - 3, screen_w, MIC_LIST_LINE_H, 0x2104);
        }

        char size_buf[10];
        micFormatSizeShort(size_buf, sizeof(size_buf), e.size);
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setTextColor(DARKGREY, sel ? 0x2104 : BLACK);
        const int size_w = M5Cardputer.Display.textWidth(size_buf);
        const int size_x = screen_w - APP_CONTENT_X - size_w;
        M5Cardputer.Display.setCursor(size_x, row_y);
        M5Cardputer.Display.print(size_buf);

        M5Cardputer.Display.setTextColor(sel ? YELLOW : WHITE, sel ? 0x2104 : BLACK);
        M5Cardputer.Display.setCursor(APP_CONTENT_X, row_y);
        const char mark = (micPlaying && sel) ? '>' : (sel ? '>' : ' ');
        M5Cardputer.Display.print(mark);
        M5Cardputer.Display.print(' ');

        // 截断文件名适配右侧大小列
        const int name_x = M5Cardputer.Display.getCursorX();
        const int name_max_w = size_x - 4 - name_x;
        char shown[28];
        strncpy(shown, e.name, sizeof(shown) - 1);
        shown[sizeof(shown) - 1] = '\0';
        while (shown[0] != '\0' && M5Cardputer.Display.textWidth(shown) > name_max_w) {
            const size_t len = strlen(shown);
            if (len <= 1) {
                break;
            }
            shown[len - 1] = '\0';
        }
        M5Cardputer.Display.setCursor(name_x, row_y);
        M5Cardputer.Display.print(shown);
    }

    drawMicListHints();
    updateAppHeaderStatus();
    micListDirty = false;
}

// 后台 WiFi + NTP（不阻塞主循环）
static void micUpdateTimeSync() {
    const AppConfig& cfg = getAppConfig();
    switch (micSyncState) {
        case MicTimeSync::Idle:
        case MicTimeSync::Done:
            return;
        case MicTimeSync::BeginWifi: {
            if (!cfg.loaded || cfg.wifi_ssid[0] == '\0') {
                micSyncState = MicTimeSync::Done;
                return;
            }
            struct tm ti{};
            // 已有可用时间则跳过联网
            if (micHasValidTime(ti)) {
                micSyncState = MicTimeSync::Done;
                return;
            }
            if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == cfg.wifi_ssid) {
                claimStaWifi();
                micSyncState = MicTimeSync::BeginNtp;
                return;
            }
            claimStaWifi();
            WiFi.mode(WIFI_STA);
            applyWifiRadioSleepPolicy();
            WiFi.begin(cfg.wifi_ssid, cfg.wifi_password);
            micSyncDeadlineMs = millis() + MIC_WIFI_TIMEOUT_MS;
            micSyncState = MicTimeSync::WaitWifi;
            break;
        }
        case MicTimeSync::WaitWifi:
            if (WiFi.status() == WL_CONNECTED) {
                micSyncState = MicTimeSync::BeginNtp;
            } else if (static_cast<int32_t>(millis() - micSyncDeadlineMs) >= 0) {
                releaseConfigWifi();
                micSyncState = MicTimeSync::Done;
            }
            break;
        case MicTimeSync::BeginNtp:
            configTzTime(getAppTimezone(), "ntp.aliyun.com", "pool.ntp.org", "time.windows.com");
            micSyncDeadlineMs = millis() + MIC_NTP_TIMEOUT_MS;
            micSyncState = MicTimeSync::WaitNtp;
            break;
        case MicTimeSync::WaitNtp: {
            struct tm timeinfo{};
            if (getLocalTime(&timeinfo, 0)) {
                if (M5.Rtc.isEnabled()) {
                    const time_t now = time(nullptr);
                    struct tm utc{};
                    gmtime_r(&now, &utc);
                    M5.Rtc.setDateTime(&utc);
                    M5.Rtc.setSystemTimeFromRtc();
                    applyLocalTimezone();
                }
                saveAppConfigTimezone(getAppTimezone());
                releaseConfigWifi();
                micSyncState = MicTimeSync::Done;
            } else if (static_cast<int32_t>(millis() - micSyncDeadlineMs) >= 0) {
                releaseConfigWifi();
                micSyncState = MicTimeSync::Done;
            }
            break;
        }
    }

    // 联网期间刷新 header WiFi 图标
    if (micSyncState == MicTimeSync::WaitWifi || micSyncState == MicTimeSync::WaitNtp) {
        if (millis() - micHeaderStatusMs >= 500) {
            micHeaderStatusMs = millis();
            updateAppHeaderStatus();
        }
    }
}

static bool micSyncBusy() {
    return micSyncState == MicTimeSync::WaitWifi || micSyncState == MicTimeSync::WaitNtp ||
           micSyncState == MicTimeSync::BeginWifi || micSyncState == MicTimeSync::BeginNtp;
}

static int micSampleToY(const int16_t sample, const int centerY, const int halfH,
                        const uint32_t ampHold, const int userGain) {
    const int64_t scaled = static_cast<int64_t>(sample) * userGain;
    int y = centerY - static_cast<int>(scaled * (halfH - 2) / static_cast<int>(ampHold));
    return constrain(y, centerY - halfH + 1, centerY + halfH - 1);
}

static int drawMicHelpColHeader(const int x, const int y, const int w, const char* title) {
    M5Cardputer.Display.fillRect(x, y, w, 11, APP_COLOR_LABEL);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(BLACK, APP_COLOR_LABEL);
    M5Cardputer.Display.setCursor(x + 2, y + 1);
    M5Cardputer.Display.print(title);
    return y + 13;
}

static int drawMicHelpKeyAt(const int x, int y, const char key, const char* text) {
    int cx = x;
    cx += drawKeyBadge(cx, y, key, 1);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
    M5Cardputer.Display.setCursor(cx, y);
    M5Cardputer.Display.print(text);
    return y + 11;
}

static int drawMicHelpBadgeAt(const int x, int y, const char* badge, const char* text) {
    int cx = x;
    cx += drawTextBadge(cx, y, badge, 1);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
    M5Cardputer.Display.setCursor(cx, y);
    M5Cardputer.Display.print(text);
    return y + 11;
}

static int drawMicHelpTextAt(const int x, int y, const char* text) {
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
    M5Cardputer.Display.setCursor(x, y);
    M5Cardputer.Display.print(text);
    return y + 10;
}

static void drawMicHelpPage() {
    beginAppScreen("Mic");
    const int screen_w = M5Cardputer.Display.width();
    constexpr int col_gap = 4;
    const int col_w = (screen_w - col_gap) / 2;
    const int keys_x = 0;
    const int notes_x = col_w + col_gap;
    const int col_y = APP_CONTENT_Y_NO_TAP_TO_HEADER;
    const int content_h = M5Cardputer.Display.height() - col_y;
    M5Cardputer.Display.drawFastVLine(col_w + col_gap / 2, col_y, content_h, DARKGREY);

    int y = drawMicHelpColHeader(keys_x, col_y, col_w, "keymap");
    const int kx = keys_x + 2;
    y = drawMicHelpKeyAt(kx, y, 'r', "rec start/stop");
    y = drawMicHelpBadgeAt(kx, y, "BtnA", "rec start/stop");
    y = drawMicHelpKeyAt(kx, y, 'l', "rec list");
    y = drawMicHelpKeyAt(kx, y, 'p', "play (list)");
    y = drawMicHelpBadgeAt(kx, y, "Bksp", "delete");
    y = drawMicHelpBadgeAt(kx, y, "-=", "vol (play)");
    y = drawMicHelpKeyAt(kx, y, 'h', "help / close");

    y = drawMicHelpColHeader(notes_x, col_y, screen_w - notes_x, "manual");
    const int nx = notes_x + 2;
    y = drawMicHelpTextAt(nx, y, "need microSD");
    y = drawMicHelpTextAt(nx, y, "save /audioRecord");
    y = drawMicHelpTextAt(nx, y, "WAV 16k mono");
    y = drawMicHelpTextAt(nx, y, "vol in Options");
    y = drawMicHelpTextAt(nx, y, "list: play/del");
    y = drawMicHelpTextAt(nx, y, "WiFi syncs time");
    y = drawMicHelpTextAt(nx, y, "for file name");

    drawHelpHintRight("close");
    updateAppHeaderStatus();
}

static void drawMicScope() {
    constexpr int kMeterW = 16;
    constexpr int kStatusH = 11;
    constexpr int kPad = 2;
    constexpr uint16_t kGridColor = 0x2104;
    constexpr uint16_t kWaveColor = CYAN;
    constexpr uint16_t kWaveDim = 0x0478;

    const int screenW = M5Cardputer.Display.width();
    const int screenH = M5Cardputer.Display.height();
    const int statusY = APP_CONTENT_Y;
    const int contentH = screenH - statusY;
    const int waveTop = kStatusH;
    const int waveH = contentH - waveTop - kPad;
    const int waveW = screenW - kMeterW - kPad - 1;
    const int halfH = waveH / 2;
    const int centerY = waveTop + halfH;
    const int meterX = waveW + 1;
    const int meterInnerX = meterX + 3;
    const int meterInnerW = kMeterW - 6;
    const int barBottom = waveTop + waveH;

    if (!M5Cardputer.Mic.isEnabled()) {
        beginAppScreen("Mic");
        micHeaderReady = false;
        M5Cardputer.Display.setCursor(APP_CONTENT_X, APP_CONTENT_Y);
        M5Cardputer.Display.println("not found");
        return;
    }

    // 列表播过后故意关麦静音：示波器暂停，按 R 录音时再开
    if (!M5Cardputer.Mic.isRunning()) {
        if (!micHeaderReady) {
            beginAppScreen("Mic");
            micHeaderReady = true;
            M5Cardputer.Display.fillRect(0, APP_CONTENT_Y, M5Cardputer.Display.width(),
                                         M5Cardputer.Display.height() - APP_CONTENT_Y, BLACK);
        }
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
        M5Cardputer.Display.setCursor(APP_CONTENT_X, APP_CONTENT_Y + 8);
        M5Cardputer.Display.print("mic paused (R to rec)");
        return;
    }

    if (!micHeaderReady) {
        beginAppScreen("Mic");
        micHeaderReady = true;
        micSprOk = false;
        M5Cardputer.Display.fillRect(0, APP_CONTENT_Y, M5Cardputer.Display.width(),
                                     M5Cardputer.Display.height() - APP_CONTENT_Y, BLACK);
    }

    if (!micSprOk) {
        micSpr.setColorDepth(16);
        if (!micSpr.createSprite(screenW, contentH)) {
            return;
        }
        micSprOk = true;
    }

    // 采集：尽量填满 Mic 双槽；record 异步，仅消费已填满的落后缓冲
    micPollCapture();
    if (M5Cardputer.Mic.isRecording() < 2) {
        micPollCapture();
    }
    if (micViewSamples == nullptr) {
        // 首帧未到：保持已清屏，勿提前 return 留下列表残影
        return;
    }
    const int16_t* micSamples = micViewSamples;

    // 录音时降低 UI 刷新，把时间让给采集/落盘
    if (micRecording) {
        micUiSkip = static_cast<uint8_t>(micUiSkip + 1);
        if ((micUiSkip & 0x01) != 0) {
            return;
        }
    }

    int32_t framePeak = 0;
    int64_t sumSq = 0;
    for (size_t i = 0; i < MIC_CAPTURE_N; i++) {
        const int32_t v = micSamples[i];
        const int32_t a = abs(v);
        if (a > framePeak) {
            framePeak = a;
        }
        sumSq += static_cast<int64_t>(v) * v;
    }
    const int32_t frameRms =
        static_cast<int32_t>(sqrtf(static_cast<float>(sumSq / static_cast<int64_t>(MIC_CAPTURE_N))));

    if (framePeak > static_cast<int32_t>(micAmpHold)) {
        micAmpHold = static_cast<uint32_t>(framePeak);
    } else {
        micAmpHold = micAmpHold * 15 / 16 + static_cast<uint32_t>(framePeak) / 16;
    }
    if (micAmpHold < 800) {
        micAmpHold = 800;
    }

    size_t sync = 0;
    for (size_t i = 1; i + 64 < MIC_CAPTURE_N; i++) {
        if (micSamples[i - 1] < 0 && micSamples[i] >= 0) {
            sync = i;
            break;
        }
    }
    const size_t viewN = min(static_cast<size_t>(192), MIC_CAPTURE_N - sync);
    const size_t win = max(static_cast<size_t>(1), viewN / static_cast<size_t>(MIC_PLOT_N));

    micSpr.fillSprite(BLACK);

    micLiveBlink = static_cast<uint8_t>(micLiveBlink + 1);
    const bool clip = framePeak > 30000;
    const float db =
        framePeak > 0 ? 20.0f * log10f(static_cast<float>(framePeak) / 32768.0f) : -60.0f;
    const float dbShow = db < -60.0f ? -60.0f : db;

    // ---- 状态行：LIVE/REC + 时长/WiFi/no SD + dB + 增益 ----
    micSpr.setTextSize(1);
    const bool show_msg = micStatusMsgActive();
    int cx = 2;
    if (micRecording) {
        if ((micLiveBlink & 0x02) == 0) {
            micSpr.fillCircle(cx + 3, 4, 3, RED);
        } else {
            micSpr.drawCircle(cx + 3, 4, 3, DARKGREY);
        }
        cx += 10;
        micSpr.setTextColor(APP_COLOR_ERROR, BLACK);
        micSpr.setCursor(cx, 0);
        micSpr.print("REC");
        cx += 22;
        const uint32_t elapsed = (millis() - micRecStartMs) / 1000;
        const uint32_t mm = elapsed / 60;
        const uint32_t ss = elapsed % 60;
        micSpr.setTextColor(YELLOW, BLACK);
        micSpr.setCursor(cx, 0);
        micSpr.printf("%lu:%02lu", static_cast<unsigned long>(mm), static_cast<unsigned long>(ss));
        cx += 36;
    } else {
        if ((micLiveBlink & 0x04) == 0) {
            micSpr.fillCircle(cx + 3, 4, 3, RED);
        } else {
            micSpr.drawCircle(cx + 3, 4, 3, DARKGREY);
        }
        cx += 10;
        micSpr.setTextColor(APP_COLOR_HINT, BLACK);
        micSpr.setCursor(cx, 0);
        micSpr.print("LIVE");
        cx += 28;
        if (micSyncBusy()) {
            micSpr.setTextColor(CYAN, BLACK);
            micSpr.setCursor(cx, 0);
            micSpr.print("WiFi");
            cx += 28;
        }
    }

    if (show_msg) {
        micSpr.setTextColor(APP_COLOR_WARN, BLACK);
        micSpr.setCursor(cx, 0);
        micSpr.print(micStatusMsg);
    } else if (clip) {
        micSpr.setTextColor(APP_COLOR_ERROR, BLACK);
        micSpr.setCursor(cx, 0);
        micSpr.print("CLIP");
    } else {
        micSpr.setTextColor(YELLOW, BLACK);
        micSpr.setCursor(cx, 0);
        micSpr.printf("%4.0fdB", static_cast<double>(dbShow));
    }

    micSpr.setTextColor(CYAN, BLACK);
    micSpr.setCursor(waveW - 28, 0);
    micSpr.printf("x%d", micUserGain);

    for (int g = 1; g < 4; g++) {
        const int gy = waveTop + (waveH * g) / 4;
        micSpr.drawFastHLine(0, gy, waveW, kGridColor);
    }
    for (int g = 1; g < 4; g++) {
        const int gx = (waveW * g) / 4;
        micSpr.drawFastVLine(gx, waveTop, waveH, kGridColor);
    }
    micSpr.drawFastHLine(0, centerY, waveW, DARKGREY);
    micSpr.drawRect(0, waveTop, waveW, waveH, DARKGREY);

    int prevX = 1;
    int prevY = micSampleToY(micSamples[sync], centerY, halfH, micAmpHold, micUserGain);
    for (int i = 0; i < MIC_PLOT_N; i++) {
        const size_t base = sync + static_cast<size_t>(i) * win;
        int32_t acc = 0;
        size_t n = 0;
        for (size_t j = 0; j < win && base + j < MIC_CAPTURE_N; j++) {
            acc += micSamples[base + j];
            n++;
        }
        const int16_t avg = n > 0 ? static_cast<int16_t>(acc / static_cast<int32_t>(n)) : 0;
        const int x = 1 + (i * (waveW - 3)) / (MIC_PLOT_N - 1);
        const int y = micSampleToY(avg, centerY, halfH, micAmpHold, micUserGain);
        const int fillTop = min(centerY, y);
        const int fillH = abs(y - centerY);
        if (fillH > 1 && (i & 1) == 0) {
            micSpr.drawFastVLine(x, fillTop, fillH, kWaveDim);
        }
        micSpr.drawLine(prevX, prevY, x, y, kWaveColor);
        prevX = x;
        prevY = y;
    }

    constexpr int kSegH = 3;
    constexpr int kSegGap = 1;
    const int segPitch = kSegH + kSegGap;
    const int segCount = max(1, waveH / segPitch);
    // 常用 dBFS 对数表头：-60 dB 到 0 dB 映射到底部到顶部
    const float rmsDb =
        frameRms > 0 ? 20.0f * log10f(static_cast<float>(frameRms) / 32768.0f) : -60.0f;
    const float peakDb =
        framePeak > 0 ? 20.0f * log10f(static_cast<float>(framePeak) / 32768.0f) : -60.0f;
    const int rmsPx =
        constrain(static_cast<int>((rmsDb + 60.0f) * waveH / 60.0f + 0.5f), 0, waveH);
    const int peakPx =
        constrain(static_cast<int>((peakDb + 60.0f) * waveH / 60.0f + 0.5f), 0, waveH);
    if (peakPx > micPeakHoldPx) {
        micPeakHoldPx = peakPx;
    } else if (micPeakHoldPx > 0) {
        micPeakHoldPx -= 1;
    }
    const int litSegs = constrain((rmsPx * segCount + waveH - 1) / waveH, 0, segCount);

    micSpr.drawRect(meterX, waveTop, kMeterW, waveH, DARKGREY);
    for (int s = 0; s < litSegs; s++) {
        const float pct = static_cast<float>(s + 1) / static_cast<float>(segCount);
        uint16_t c = APP_COLOR_OK;
        if (pct > 0.85f) {
            c = APP_COLOR_ERROR;
        } else if (pct > 0.62f) {
            c = YELLOW;
        }
        const int sy = barBottom - (s + 1) * segPitch + kSegGap;
        micSpr.fillRect(meterInnerX, sy, meterInnerW, kSegH, c);
    }
    if (micPeakHoldPx > 0) {
        const int holdY = constrain(barBottom - micPeakHoldPx, waveTop, barBottom - 1);
        micSpr.drawFastHLine(meterX + 1, holdY, kMeterW - 2, YELLOW);
    }

    micSpr.pushSprite(0, statusY);
}

void enterMicApp() {
    leaveMicApp();
    micUiMode = MicUiMode::Scope;
    micUserGain = micUserGain < 1 ? 1 : micUserGain;
    micStatusMsg[0] = '\0';
    micPlaying = false;
    micListCount = 0;
    micResetCapturePipeline();
    // 麦克与喇叭共用 I2S，进 Mic 先关喇叭并拉低脚
    releaseSpeakerQuiet();
    micSpkWasEnded = true;
    micHadSpeakerPlay = false;
    micEnsureCaptureOn();
    // 先占录音环（在 sprite / WiFi 之前），降低“no mem”
    if (!micEnsureRing()) {
        micSetStatusMsg("no mem");
    }
    micSyncState = MicTimeSync::BeginWifi;
    micHeaderReady = false;
    micSprOk = false;
    beginAppScreen("Mic");
    micHeaderReady = true;
    updateMicApp();
}

void leaveMicApp() {
    if (micPlaying) {
        micPlayStop(false);
    }
    if (micRecording) {
        micFinalizeRecording();
    }
    // 等 Mic 双槽排空再关；播过音后须卸掉 PDM/I2S 再拉低喇叭脚
    if (M5Cardputer.Mic.isRunning()) {
        uint32_t wait_ms = millis() + 80;
        while (M5Cardputer.Mic.isRecording() && static_cast<int32_t>(millis() - wait_ms) < 0) {
            delay(1);
        }
        M5Cardputer.Mic.end();
        delay(20); // 等 I2S 矩阵松开，否则后面 gpio 拉低无效
    }
    // 关喇叭并 reset+hold 拉低 I2S 脚，避免退出后悬空嗡嗡
    releaseSpeakerQuiet();
    flushSpeakerVolumeSave();
    micSpkWasEnded = false;
    micHadSpeakerPlay = false;
    if (micSprOk) {
        micSpr.deleteSprite();
        micSprOk = false;
    }
    if (micSyncBusy() || micSyncState == MicTimeSync::BeginWifi ||
        micSyncState == MicTimeSync::BeginNtp) {
        releaseConfigWifi();
    }
    micSyncState = MicTimeSync::Done;
    micReleaseSd();
    micResetCapturePipeline();
    // 环形缓冲下次进 Mic 可复用，离场不释放
    micHeaderReady = false;
    micUiMode = MicUiMode::Scope;
}

void updateMicApp() {
    micUpdateTimeSync();
    if (micUiMode == MicUiMode::Help) {
        return;
    }
    if (micUiMode == MicUiMode::List) {
        if (micPlaying) {
            micPlayFeed();
            // 播放中只刷进度行；列表结构变化时再整页重绘
            if (micListDirty) {
                drawMicListScreen();
                micPlayUiMs = millis();
            } else if (millis() - micPlayUiMs >= 250) {
                micPlayUiMs = millis();
                drawMicListProgress();
            }
        } else if (micListDirty) {
            drawMicListScreen();
        } else if (micStatusMsgActive() && millis() - micPlayUiMs >= 200) {
            micPlayUiMs = millis();
            drawMicListProgress();
        }
        return;
    }
    drawMicScope();
}

void handleMicApp(const Keyboard_Class::KeysState& status) {
    // Backspace：列表里删除选中录音
    if (status.del && micUiMode == MicUiMode::List && !micPlaying) {
        micDeleteSelected();
        drawMicListScreen();
        return;
    }

    // Enter：列表播放 / 播放中停止
    if (status.enter && micUiMode == MicUiMode::List) {
        if (micPlaying) {
            micPlayStop(false);
            micListDirty = true;
            drawMicListScreen();
        } else {
            if (micPlayStartSelected()) {
                drawMicListScreen();
            } else {
                drawMicListScreen();
            }
        }
        return;
    }

    // 列表翻页/移动选中
    if (micUiMode == MicUiMode::List && !micPlaying) {
        const int nav = getMenuNavDelta(status);
        if (nav != 0 && micListCount > 0) {
            micListSel += nav;
            if (micListSel < 0) {
                micListSel = micListCount - 1;
            } else if (micListSel >= micListCount) {
                micListSel = 0;
            }
            micClampListSel();
            micListDirty = true;
            drawMicListScreen();
            return;
        }
    }

    for (const char c : status.word) {
        if (c == 'h' || c == 'H') {
            if (micPlaying) {
                micPlayStop(false);
            }
            if (micUiMode == MicUiMode::Help) {
                micUiMode = MicUiMode::Scope;
                micHeaderReady = false;
                if (micHadSpeakerPlay) {
                    releaseSpeakerQuiet();
                } else {
                    micEnsureCaptureOn();
                }
                micResetCapturePipeline();
                updateMicApp();
            } else {
                if (micRecording) {
                    micFinalizeRecording();
                }
                // Help 页不采麦，避免功放被 PDM 时钟干扰
                if (M5Cardputer.Mic.isRunning()) {
                    M5Cardputer.Mic.end();
                    delay(10);
                }
                releaseSpeakerQuiet();
                if (micSprOk) {
                    micSpr.deleteSprite();
                    micSprOk = false;
                }
                micUiMode = MicUiMode::Help;
                drawMicHelpPage();
            }
            return;
        }
        if (micUiMode == MicUiMode::Help) {
            continue;
        }
        if (c == 'l' || c == 'L') {
            if (micUiMode == MicUiMode::List) {
                micLeaveListMode();
                updateMicApp();
            } else {
                micEnterListMode();
                drawMicListScreen();
            }
            return;
        }
        if (micUiMode == MicUiMode::List) {
            if (micPlaying && (c == '-' || c == '=' || c == '+')) {
                adjustAppSpeakerVolume(c == '-' ? -5 : 5);
                drawMicListProgress();
                return;
            }
            if (c == 'p' || c == 'P' || c == ' ') {
                if (micPlaying) {
                    micPlayStop(false);
                    micListDirty = true;
                } else {
                    micPlayStartSelected();
                }
                drawMicListScreen();
                return;
            }
            continue;
        }
        if (c == 'r' || c == 'R') {
            micToggleRecording();
        } else if (c == '-') {
            if (micUserGain > 1) {
                micUserGain >>= 1;
            }
        } else if (c == '=' || c == '+') {
            if (micUserGain < 16) {
                micUserGain <<= 1;
            }
        } else if (c == ',') {
            if (micUserGain > 1) {
                micUserGain >>= 1;
            }
        } else if (c == '.') {
            if (micUserGain < 16) {
                micUserGain <<= 1;
            }
        }
    }
}

void pollMicBtnA() {
    if (!M5Cardputer.BtnA.wasPressed()) {
        return;
    }
    if (micUiMode == MicUiMode::Help) {
        return;
    }
    if (micUiMode == MicUiMode::List) {
        if (micPlaying) {
            micPlayStop(false);
            micListDirty = true;
            drawMicListScreen();
        } else {
            micPlayStartSelected();
            drawMicListScreen();
        }
        return;
    }
    micToggleRecording();
}
