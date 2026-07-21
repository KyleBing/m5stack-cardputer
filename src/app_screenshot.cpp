#include "app_screenshot.h"
#include "app_common.h"
#include "app_web.h"
#include "M5Cardputer.h"
#include <FS.h>
#include <LittleFS.h>
#include <SD.h>
#include <SPI.h>
#include <cstdio>
#include <cstring>

// Cardputer microSD SPI（与 app_mic 相同）
static constexpr int SHOT_SD_SCK = 40;
static constexpr int SHOT_SD_MISO = 39;
static constexpr int SHOT_SD_MOSI = 14;
static constexpr int SHOT_SD_CS = 12;

static constexpr const char* SHOT_LAST = "/shot/.last";
static constexpr const char* SHOT_BOOT_PENDING = "/shot/.boot_pending";
// 开机至少留这么多空闲；截图约 100KB，再留配置/日志余量
static constexpr size_t SHOT_MIN_FREE_BOOT = 96 * 1024;
static constexpr size_t SHOT_MIN_FREE_SAVE = 120 * 1024;
static constexpr size_t SHOT_MIN_FREE_SD = 256 * 1024;

static bool g_shot_sd_ready = false;

// 写 54 字节 BMP 头（24bit，bottom-up）
static void writeBmpHeader(File& f, const int w, const int h, const uint32_t image_size) {
    const uint32_t file_size = 54u + image_size;
    uint8_t hdr[54] = {};
    hdr[0] = 'B';
    hdr[1] = 'M';
    hdr[2] = static_cast<uint8_t>(file_size);
    hdr[3] = static_cast<uint8_t>(file_size >> 8);
    hdr[4] = static_cast<uint8_t>(file_size >> 16);
    hdr[5] = static_cast<uint8_t>(file_size >> 24);
    hdr[10] = 54;
    hdr[14] = 40;
    hdr[18] = static_cast<uint8_t>(w);
    hdr[19] = static_cast<uint8_t>(w >> 8);
    hdr[20] = static_cast<uint8_t>(w >> 16);
    hdr[21] = static_cast<uint8_t>(w >> 24);
    hdr[22] = static_cast<uint8_t>(h);
    hdr[23] = static_cast<uint8_t>(h >> 8);
    hdr[24] = static_cast<uint8_t>(h >> 16);
    hdr[25] = static_cast<uint8_t>(h >> 24);
    hdr[26] = 1;
    hdr[28] = 24;
    hdr[34] = static_cast<uint8_t>(image_size);
    hdr[35] = static_cast<uint8_t>(image_size >> 8);
    hdr[36] = static_cast<uint8_t>(image_size >> 16);
    hdr[37] = static_cast<uint8_t>(image_size >> 24);
    f.write(hdr, sizeof(hdr));
}

// 逐行读屏写 BMP
static bool captureDisplayToBmpFile(File& f, char* err, const size_t err_len) {
    auto& d = M5Cardputer.Display;
    if (!d.isReadable()) {
        snprintf(err, err_len, "panel not readable");
        return false;
    }

    const int w = d.width();
    const int h = d.height();
    if (w <= 0 || h <= 0 || w > 320 || h > 240) {
        snprintf(err, err_len, "bad size %dx%d", w, h);
        return false;
    }

    const int row_stride = ((w * 3 + 3) / 4) * 4;
    const uint32_t image_size = static_cast<uint32_t>(row_stride) * static_cast<uint32_t>(h);
    writeBmpHeader(f, w, h, image_size);

    uint8_t* rgb = static_cast<uint8_t*>(malloc(static_cast<size_t>(w) * 3u));
    uint8_t* bgr = static_cast<uint8_t*>(malloc(static_cast<size_t>(row_stride)));
    if (rgb == nullptr || bgr == nullptr) {
        free(rgb);
        free(bgr);
        snprintf(err, err_len, "oom heap=%u", ESP.getFreeHeap());
        return false;
    }
    memset(bgr, 0, static_cast<size_t>(row_stride));

    for (int y = h - 1; y >= 0; y--) {
        d.readRectRGB(0, y, w, 1, rgb);
        for (int x = 0; x < w; x++) {
            const int i = x * 3;
            bgr[i + 0] = rgb[i + 2];
            bgr[i + 1] = rgb[i + 1];
            bgr[i + 2] = rgb[i + 0];
        }
        if (f.write(bgr, static_cast<size_t>(row_stride)) != static_cast<size_t>(row_stride)) {
            free(rgb);
            free(bgr);
            snprintf(err, err_len, "row write fail");
            return false;
        }
    }

    free(rgb);
    free(bgr);
    return true;
}

// slug 仅保留 a-z0-9_
static void sanitizeSlug(const char* in, char* out, const size_t out_len) {
    if (out_len == 0) {
        return;
    }
    size_t j = 0;
    for (size_t i = 0; in != nullptr && in[i] != '\0' && j + 1 < out_len; i++) {
        const char c = in[i];
        const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
        if (ok) {
            out[j++] = c;
        }
    }
    if (j == 0) {
        strncpy(out, "unknown", out_len);
        out[out_len - 1] = '\0';
        return;
    }
    out[j] = '\0';
}

// 尝试挂载 TF；失败返回 false（可与 mic 共用已挂载的 SD）
static bool shotEnsureSd() {
    if (g_shot_sd_ready && SD.cardType() != CARD_NONE) {
        return true;
    }
    g_shot_sd_ready = false;

    SPI.begin(SHOT_SD_SCK, SHOT_SD_MISO, SHOT_SD_MOSI, SHOT_SD_CS);
    if (!SD.begin(SHOT_SD_CS, SPI, 25000000)) {
        // 可能已被 mic 挂载：再看卡类型
        if (SD.cardType() != CARD_NONE) {
            g_shot_sd_ready = true;
            return true;
        }
        return false;
    }
    if (SD.cardType() == CARD_NONE) {
        SD.end();
        return false;
    }
    g_shot_sd_ready = true;
    return true;
}

bool isScreenshotSdReady() {
    return shotEnsureSd();
}

static bool ensureShotDirOn(fs::FS& fs) {
    if (fs.exists(SHOT_DIR)) {
        return true;
    }
    return fs.mkdir(SHOT_DIR);
}

// 从路径或文件名取出 base 名
static const char* shotBaseName(const char* name) {
    const char* slash = strrchr(name, '/');
    return slash != nullptr ? slash + 1 : name;
}

static bool isShotBmpName(const char* base) {
    return base != nullptr && strncmp(base, "app_", 4) == 0 && strstr(base, ".bmp") != nullptr;
}

static size_t fsFreeBytes(fs::FS& fs, const bool is_sd) {
    if (is_sd) {
        const size_t total = SD.totalBytes();
        const size_t used = SD.usedBytes();
        return total > used ? total - used : 0;
    }
    const size_t total = LittleFS.totalBytes();
    const size_t used = LittleFS.usedBytes();
    return total > used ? total - used : 0;
}

// 扫描同前缀最大序号（app_<slug>_NNN.bmp）
static int findNextShotIndexOn(fs::FS& fs, const char* slug) {
    char prefix[40];
    snprintf(prefix, sizeof(prefix), "app_%s_", slug);
    const size_t prefix_len = strlen(prefix);

    int max_n = 0;
    File dir = fs.open(SHOT_DIR);
    if (!dir || !dir.isDirectory()) {
        return 1;
    }
    File f = dir.openNextFile();
    while (f) {
        const char* base = shotBaseName(f.name());
        if (strncmp(base, prefix, prefix_len) == 0) {
            int n = 0;
            if (sscanf(base + prefix_len, "%d", &n) == 1 && n > max_n) {
                max_n = n;
            }
        }
        f = dir.openNextFile();
    }
    dir.close();
    return max_n + 1;
}

// 记录最后一张文件名（写在对应 FS）
static void writeLastShotNameOn(fs::FS& fs, const char* filename) {
    File f = fs.open(SHOT_LAST, "w");
    if (!f) {
        return;
    }
    f.print(filename);
    f.close();
}

static bool readLastShotNameOn(fs::FS& fs, char* out, const size_t out_len) {
    if (out_len == 0) {
        return false;
    }
    out[0] = '\0';
    if (!fs.exists(SHOT_LAST)) {
        return false;
    }
    File f = fs.open(SHOT_LAST, "r");
    if (!f) {
        return false;
    }
    const size_t n = f.readBytes(out, out_len - 1);
    f.close();
    out[n] = '\0';
    for (size_t i = 0; i < n; i++) {
        if (out[i] == '\r' || out[i] == '\n') {
            out[i] = '\0';
            break;
        }
    }
    return out[0] != '\0' && isShotBmpName(out);
}

// 找时间最新的一张（无 .last 时兜底）
static bool findNewestShotNameOn(fs::FS& fs, char* out, const size_t out_len) {
    if (out_len == 0) {
        return false;
    }
    out[0] = '\0';
    if (!fs.exists(SHOT_DIR)) {
        return false;
    }
    File dir = fs.open(SHOT_DIR);
    if (!dir || !dir.isDirectory()) {
        return false;
    }
    time_t best_t = 0;
    char best[48] = "";
    bool any = false;
    File f = dir.openNextFile();
    while (f) {
        const char* base = shotBaseName(f.name());
        if (isShotBmpName(base)) {
            const time_t t = f.getLastWrite();
            if (!any || t >= best_t) {
                best_t = t;
                strncpy(best, base, sizeof(best) - 1);
                best[sizeof(best) - 1] = '\0';
                any = true;
            }
        }
        f = dir.openNextFile();
    }
    dir.close();
    if (!any) {
        return false;
    }
    strncpy(out, best, out_len - 1);
    out[out_len - 1] = '\0';
    return true;
}

static bool deleteLastOn(fs::FS& fs, const bool is_sd) {
    char name[48];
    if (!readLastShotNameOn(fs, name, sizeof(name))) {
        if (!findNewestShotNameOn(fs, name, sizeof(name))) {
            return false;
        }
    }
    char path[64];
    snprintf(path, sizeof(path), "%s/%s", SHOT_DIR, name);
    if (!fs.exists(path)) {
        if (!findNewestShotNameOn(fs, name, sizeof(name))) {
            fs.remove(SHOT_LAST);
            return false;
        }
        snprintf(path, sizeof(path), "%s/%s", SHOT_DIR, name);
    }
    const bool ok = fs.remove(path);
    fs.remove(SHOT_LAST);
    if (ok) {
        Serial.printf("[shot] deleted last %s on %s free=%u\n", name, is_sd ? "TF" : "Flash",
                      static_cast<unsigned>(fsFreeBytes(fs, is_sd)));
    }
    return ok;
}

bool deleteLastScreenshot() {
    if (!LittleFS.begin(false)) {
        return false;
    }
    return deleteLastOn(LittleFS, false);
}

static int countOn(fs::FS& fs) {
    if (!fs.exists(SHOT_DIR)) {
        return 0;
    }
    File dir = fs.open(SHOT_DIR);
    if (!dir || !dir.isDirectory()) {
        return 0;
    }
    int n = 0;
    File f = dir.openNextFile();
    while (f) {
        if (isShotBmpName(shotBaseName(f.name()))) {
            n++;
        }
        f = dir.openNextFile();
    }
    dir.close();
    return n;
}

static size_t usedBytesOn(fs::FS& fs) {
    if (!fs.exists(SHOT_DIR)) {
        return 0;
    }
    File dir = fs.open(SHOT_DIR);
    if (!dir || !dir.isDirectory()) {
        return 0;
    }
    size_t sum = 0;
    File f = dir.openNextFile();
    while (f) {
        if (isShotBmpName(shotBaseName(f.name()))) {
            sum += static_cast<size_t>(f.size());
        }
        f = dir.openNextFile();
    }
    dir.close();
    return sum;
}

static int clearAllOn(fs::FS& fs) {
    if (!fs.exists(SHOT_DIR)) {
        return 0;
    }
    int n = 0;
    for (;;) {
        File dir = fs.open(SHOT_DIR);
        if (!dir || !dir.isDirectory()) {
            break;
        }
        char to_del[48] = "";
        File f = dir.openNextFile();
        while (f) {
            const char* base = shotBaseName(f.name());
            if (isShotBmpName(base)) {
                strncpy(to_del, base, sizeof(to_del) - 1);
                to_del[sizeof(to_del) - 1] = '\0';
                f.close(); // 打开着 remove 会失败
                break;
            }
            f = dir.openNextFile();
        }
        dir.close();
        if (to_del[0] == '\0') {
            break;
        }
        char path[64];
        snprintf(path, sizeof(path), "%s/%s", SHOT_DIR, to_del);
        if (fs.remove(path)) {
            n++;
            Serial.printf("[shot] cleared %s\n", to_del);
        } else {
            Serial.printf("[shot] clear remove fail %s\n", path);
            break;
        }
    }
    fs.remove(SHOT_LAST);
    return n;
}

static void enumOn(fs::FS& fs, const char* storage, ShotEnumCallback cb, void* user) {
    if (cb == nullptr || !fs.exists(SHOT_DIR)) {
        return;
    }
    File dir = fs.open(SHOT_DIR);
    if (!dir || !dir.isDirectory()) {
        return;
    }
    File f = dir.openNextFile();
    while (f) {
        const char* base = shotBaseName(f.name());
        if (isShotBmpName(base)) {
            cb(storage, base, static_cast<size_t>(f.size()), user);
        }
        f = dir.openNextFile();
    }
    dir.close();
}

void recoverScreenshotsOnBoot() {
    if (!LittleFS.begin(false)) {
        return;
    }
    ensureShotDirOn(LittleFS);

    // 上次 setup 没跑完（崩溃/看门狗）→ 删最后一张 Flash 截图
    if (LittleFS.exists(SHOT_BOOT_PENDING)) {
        Serial.println("[shot] boot pending: remove last screenshot");
        deleteLastScreenshot();
        LittleFS.remove(SHOT_BOOT_PENDING);
    }

    // 空间过紧：继续删最后一张直到够用或没有截图
    int guard = 0;
    while (fsFreeBytes(LittleFS, false) < SHOT_MIN_FREE_BOOT && guard < 32) {
        if (!deleteLastScreenshot()) {
            break;
        }
        guard++;
    }
    Serial.printf("[shot] boot free=%u used=%u/%u\n",
                  static_cast<unsigned>(fsFreeBytes(LittleFS, false)),
                  static_cast<unsigned>(LittleFS.usedBytes()),
                  static_cast<unsigned>(LittleFS.totalBytes()));

    // 标记启动中；正常结束由 markScreenshotBootOk 清除
    File pend = LittleFS.open(SHOT_BOOT_PENDING, "w");
    if (pend) {
        pend.print("1");
        pend.close();
    }
}

void markScreenshotBootOk() {
    if (!LittleFS.begin(false)) {
        return;
    }
    LittleFS.remove(SHOT_BOOT_PENDING);
}

// 写到指定 FS；is_sd 决定空闲阈值与日志标签
static bool saveToFs(fs::FS& fs, const bool is_sd, const char* app_slug, char* out_name,
                     const size_t out_name_len, char* err, const size_t err_len) {
    if (!ensureShotDirOn(fs)) {
        snprintf(err, err_len, "mkdir /shot fail");
        return false;
    }

    const size_t min_free = is_sd ? SHOT_MIN_FREE_SD : SHOT_MIN_FREE_SAVE;
    for (int i = 0; i < 4 && fsFreeBytes(fs, is_sd) < min_free; i++) {
        if (!deleteLastOn(fs, is_sd)) {
            break;
        }
    }
    if (fsFreeBytes(fs, is_sd) < min_free) {
        snprintf(err, err_len, "%s full free=%u", is_sd ? "TF" : "fs",
                 static_cast<unsigned>(fsFreeBytes(fs, is_sd)));
        return false;
    }

    char slug[24];
    sanitizeSlug(app_slug, slug, sizeof(slug));

    const int idx = findNextShotIndexOn(fs, slug);
    if (idx > 999) {
        snprintf(err, err_len, "too many %s shots", slug);
        return false;
    }

    char filename[48];
    snprintf(filename, sizeof(filename), "app_%s_%03d.bmp", slug, idx);
    char path[64];
    snprintf(path, sizeof(path), "%s/%s", SHOT_DIR, filename);

    File out = fs.open(path, "w");
    if (!out) {
        snprintf(err, err_len, "open fail (%s full?)", is_sd ? "TF" : "fs");
        return false;
    }
    if (!captureDisplayToBmpFile(out, err, err_len)) {
        out.close();
        fs.remove(path);
        return false;
    }
    out.close();
    writeLastShotNameOn(fs, filename);

    if (out_name != nullptr && out_name_len > 0) {
        strncpy(out_name, filename, out_name_len);
        out_name[out_name_len - 1] = '\0';
    }
    Serial.printf("[shot] saved %s on %s free=%u\n", path, is_sd ? "TF" : "Flash",
                  static_cast<unsigned>(fsFreeBytes(fs, is_sd)));
    return true;
}

bool saveScreenshotToFlash(const char* app_slug, char* out_name, const size_t out_name_len, char* err,
                           const size_t err_len) {
    if (err != nullptr && err_len > 0) {
        err[0] = '\0';
    }
    if (out_name != nullptr && out_name_len > 0) {
        out_name[0] = '\0';
    }

    // 有 TF 优先写卡
    if (shotEnsureSd()) {
        if (saveToFs(SD, true, app_slug, out_name, out_name_len, err, err_len)) {
            return true;
        }
        Serial.printf("[shot] TF save fail (%s), fallback Flash\n", err != nullptr ? err : "?");
    }

    if (!LittleFS.begin(false)) {
        snprintf(err, err_len, "fs mount fail");
        return false;
    }
    return saveToFs(LittleFS, false, app_slug, out_name, out_name_len, err, err_len);
}

bool tryHandleScreenshotHotkey() {
    if (!M5Cardputer.Keyboard.isPressed()) {
        return false;
    }
    const Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
    if (!status.fn) {
        return false;
    }
    bool hit = false;
    for (const char c : status.word) {
        if (c == 's' || c == 'S') {
            hit = true;
            break;
        }
    }
    if (!hit) {
        return false;
    }

    const char* slug = getCurrentAppShotSlug();

    char name[48];
    char err[96];
    if (saveScreenshotToFlash(slug, name, sizeof(name), err, sizeof(err))) {
        playUiTone(1200.0f, 40);
        Serial.printf("[shot] Fn+s ok %s\n", name);
    } else {
        playUiTone(300.0f, 80);
        Serial.printf("[shot] Fn+s fail: %s\n", err);
    }
    return true;
}

int countTfScreenshots() {
    return shotEnsureSd() ? countOn(SD) : 0;
}

int countFlashScreenshots() {
    return LittleFS.begin(false) ? countOn(LittleFS) : 0;
}

int countScreenshots() {
    return countTfScreenshots() + countFlashScreenshots();
}

size_t screenshotsUsedBytesTf() {
    return shotEnsureSd() ? usedBytesOn(SD) : 0;
}

size_t screenshotsUsedBytesFlash() {
    return LittleFS.begin(false) ? usedBytesOn(LittleFS) : 0;
}

size_t screenshotsUsedBytes() {
    return screenshotsUsedBytesTf() + screenshotsUsedBytesFlash();
}

void getFlashDataSpace(size_t* total, size_t* used, size_t* free_bytes) {
    if (total != nullptr) {
        *total = 0;
    }
    if (used != nullptr) {
        *used = 0;
    }
    if (free_bytes != nullptr) {
        *free_bytes = 0;
    }
    if (!LittleFS.begin(false)) {
        return;
    }
    const size_t t = LittleFS.totalBytes();
    const size_t u = LittleFS.usedBytes();
    const size_t free_n = t > u ? t - u : 0;
    if (total != nullptr) {
        *total = t;
    }
    if (used != nullptr) {
        *used = u;
    }
    if (free_bytes != nullptr) {
        *free_bytes = free_n;
    }
}

void getSdDataSpace(size_t* total, size_t* used, size_t* free_bytes) {
    if (total != nullptr) {
        *total = 0;
    }
    if (used != nullptr) {
        *used = 0;
    }
    if (free_bytes != nullptr) {
        *free_bytes = 0;
    }
    if (!shotEnsureSd()) {
        return;
    }
    const size_t t = SD.totalBytes();
    const size_t u = SD.usedBytes();
    const size_t free_n = t > u ? t - u : 0;
    if (total != nullptr) {
        *total = t;
    }
    if (used != nullptr) {
        *used = u;
    }
    if (free_bytes != nullptr) {
        *free_bytes = free_n;
    }
}

void enumTfScreenshots(ShotEnumCallback cb, void* user) {
    if (cb == nullptr || !shotEnsureSd()) {
        return;
    }
    enumOn(SD, "TF", cb, user);
}

void enumFlashScreenshots(ShotEnumCallback cb, void* user) {
    if (cb == nullptr || !LittleFS.begin(false)) {
        return;
    }
    enumOn(LittleFS, "Flash", cb, user);
}

void enumScreenshots(ShotEnumCallback cb, void* user) {
    enumTfScreenshots(cb, user);
    enumFlashScreenshots(cb, user);
}

bool openScreenshotFile(const String& uri, File& out) {
    if (!isSafeShotPath(uri)) {
        return false;
    }
    if (shotEnsureSd()) {
        out = SD.open(uri, "r");
        if (out) {
            return true;
        }
    }
    if (LittleFS.begin(false)) {
        out = LittleFS.open(uri, "r");
        if (out) {
            return true;
        }
    }
    return false;
}

int clearTfScreenshots() {
    if (!shotEnsureSd()) {
        return 0;
    }
    return clearAllOn(SD);
}

int clearFlashScreenshots() {
    if (!LittleFS.begin(false)) {
        return 0;
    }
    const int n = clearAllOn(LittleFS);
    LittleFS.remove(SHOT_BOOT_PENDING);
    return n;
}

int clearAllScreenshots() {
    return clearTfScreenshots() + clearFlashScreenshots();
}

bool isSafeShotPath(const String& uri) {
    // 允许 /shot/app_xxx_001.bmp
    if (!uri.startsWith("/shot/app_") || !uri.endsWith(".bmp")) {
        return false;
    }
    if (uri.indexOf("..") >= 0 || uri.length() > 64) {
        return false;
    }
    for (size_t i = 0; i < uri.length(); i++) {
        const char c = uri[i];
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                        c == '_' || c == '-' || c == '.' || c == '/';
        if (!ok) {
            return false;
        }
    }
    return true;
}
