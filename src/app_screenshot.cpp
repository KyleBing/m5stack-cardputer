#include "app_screenshot.h"
#include "app_common.h"
#include "app_web.h"
#include "M5Cardputer.h"
#include <FS.h>
#include <LittleFS.h>
#include <cstdio>
#include <cstring>

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

// 确保 /shot 目录存在
static bool ensureShotDir() {
    if (LittleFS.exists(SHOT_DIR)) {
        return true;
    }
    return LittleFS.mkdir(SHOT_DIR);
}

// 扫描同前缀最大序号（app_<slug>_NNN.bmp）
static int findNextShotIndex(const char* slug) {
    char prefix[40];
    snprintf(prefix, sizeof(prefix), "app_%s_", slug);
    const size_t prefix_len = strlen(prefix);

    int max_n = 0;
    File dir = LittleFS.open(SHOT_DIR);
    if (!dir || !dir.isDirectory()) {
        return 1;
    }
    File f = dir.openNextFile();
    while (f) {
        const char* name = f.name();
        // LittleFS 可能返回带路径或仅文件名
        const char* base = name;
        const char* slash = strrchr(name, '/');
        if (slash != nullptr) {
            base = slash + 1;
        }
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

static constexpr const char* SHOT_LAST = "/shot/.last";
static constexpr const char* SHOT_BOOT_PENDING = "/shot/.boot_pending";
// 开机至少留这么多空闲；截图约 100KB，再留配置/日志余量
static constexpr size_t SHOT_MIN_FREE_BOOT = 96 * 1024;
static constexpr size_t SHOT_MIN_FREE_SAVE = 120 * 1024;

static size_t shotFreeBytes() {
    const size_t total = LittleFS.totalBytes();
    const size_t used = LittleFS.usedBytes();
    return total > used ? total - used : 0;
}

// 从路径或文件名取出 base 名
static const char* shotBaseName(const char* name) {
    const char* slash = strrchr(name, '/');
    return slash != nullptr ? slash + 1 : name;
}

static bool isShotBmpName(const char* base) {
    return base != nullptr && strncmp(base, "app_", 4) == 0 && strstr(base, ".bmp") != nullptr;
}

// 记录最后一张文件名
static void writeLastShotName(const char* filename) {
    File f = LittleFS.open(SHOT_LAST, "w");
    if (!f) {
        return;
    }
    f.print(filename);
    f.close();
}

static bool readLastShotName(char* out, const size_t out_len) {
    if (out_len == 0) {
        return false;
    }
    out[0] = '\0';
    if (!LittleFS.exists(SHOT_LAST)) {
        return false;
    }
    File f = LittleFS.open(SHOT_LAST, "r");
    if (!f) {
        return false;
    }
    const size_t n = f.readBytes(out, out_len - 1);
    f.close();
    out[n] = '\0';
    // 去掉换行
    for (size_t i = 0; i < n; i++) {
        if (out[i] == '\r' || out[i] == '\n') {
            out[i] = '\0';
            break;
        }
    }
    return out[0] != '\0' && isShotBmpName(out);
}

// 找时间最新的一张（无 .last 时兜底）
static bool findNewestShotName(char* out, const size_t out_len) {
    if (out_len == 0) {
        return false;
    }
    out[0] = '\0';
    if (!LittleFS.exists(SHOT_DIR)) {
        return false;
    }
    File dir = LittleFS.open(SHOT_DIR);
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

bool deleteLastScreenshot() {
    if (!LittleFS.begin(false)) {
        return false;
    }
    char name[48];
    if (!readLastShotName(name, sizeof(name))) {
        if (!findNewestShotName(name, sizeof(name))) {
            return false;
        }
    }
    char path[64];
    snprintf(path, sizeof(path), "%s/%s", SHOT_DIR, name);
    if (!LittleFS.exists(path)) {
        // .last 过期：改删时间最新的
        if (!findNewestShotName(name, sizeof(name))) {
            LittleFS.remove(SHOT_LAST);
            return false;
        }
        snprintf(path, sizeof(path), "%s/%s", SHOT_DIR, name);
    }
    const bool ok = LittleFS.remove(path);
    LittleFS.remove(SHOT_LAST);
    if (ok) {
        Serial.printf("[shot] deleted last %s free=%u\n", name, static_cast<unsigned>(shotFreeBytes()));
    }
    return ok;
}

void recoverScreenshotsOnBoot() {
    if (!LittleFS.begin(false)) {
        return;
    }
    ensureShotDir();

    // 上次 setup 没跑完（崩溃/看门狗）→ 删最后一张截图
    if (LittleFS.exists(SHOT_BOOT_PENDING)) {
        Serial.println("[shot] boot pending: remove last screenshot");
        deleteLastScreenshot();
        LittleFS.remove(SHOT_BOOT_PENDING);
    }

    // 空间过紧：继续删最后一张直到够用或没有截图
    int guard = 0;
    while (shotFreeBytes() < SHOT_MIN_FREE_BOOT && guard < 32) {
        if (!deleteLastScreenshot()) {
            break;
        }
        guard++;
    }
    Serial.printf("[shot] boot free=%u used=%u/%u\n", static_cast<unsigned>(shotFreeBytes()),
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

bool saveScreenshotToFlash(const char* app_slug, char* out_name, const size_t out_name_len, char* err,
                           const size_t err_len) {
    if (err != nullptr && err_len > 0) {
        err[0] = '\0';
    }
    if (out_name != nullptr && out_name_len > 0) {
        out_name[0] = '\0';
    }

    if (!LittleFS.begin(false)) {
        snprintf(err, err_len, "fs mount fail");
        return false;
    }
    if (!ensureShotDir()) {
        snprintf(err, err_len, "mkdir /shot fail");
        return false;
    }

    // 空间不够先删最后一张腾地方（最多删几张）
    for (int i = 0; i < 4 && shotFreeBytes() < SHOT_MIN_FREE_SAVE; i++) {
        if (!deleteLastScreenshot()) {
            break;
        }
    }
    if (shotFreeBytes() < SHOT_MIN_FREE_SAVE) {
        snprintf(err, err_len, "fs full free=%u", static_cast<unsigned>(shotFreeBytes()));
        return false;
    }

    char slug[24];
    sanitizeSlug(app_slug, slug, sizeof(slug));

    const int idx = findNextShotIndex(slug);
    if (idx > 999) {
        snprintf(err, err_len, "too many %s shots", slug);
        return false;
    }

    char filename[48];
    snprintf(filename, sizeof(filename), "app_%s_%03d.bmp", slug, idx);
    char path[64];
    snprintf(path, sizeof(path), "%s/%s", SHOT_DIR, filename);

    File out = LittleFS.open(path, "w");
    if (!out) {
        snprintf(err, err_len, "open fail (fs full?)");
        return false;
    }
    if (!captureDisplayToBmpFile(out, err, err_len)) {
        out.close();
        LittleFS.remove(path);
        return false;
    }
    out.close();
    writeLastShotName(filename);

    if (out_name != nullptr && out_name_len > 0) {
        strncpy(out_name, filename, out_name_len);
        out_name[out_name_len - 1] = '\0';
    }
    Serial.printf("[shot] saved %s free=%u\n", path, static_cast<unsigned>(shotFreeBytes()));
    return true;
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

    // 由 main / app_web 提供当前界面短名
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

int countScreenshots() {
    if (!LittleFS.begin(false) || !LittleFS.exists(SHOT_DIR)) {
        return 0;
    }
    File dir = LittleFS.open(SHOT_DIR);
    if (!dir || !dir.isDirectory()) {
        return 0;
    }
    int n = 0;
    File f = dir.openNextFile();
    while (f) {
        const char* base = shotBaseName(f.name());
        if (isShotBmpName(base)) {
            n++;
        }
        f = dir.openNextFile();
    }
    dir.close();
    return n;
}

size_t screenshotsUsedBytes() {
    if (!LittleFS.begin(false) || !LittleFS.exists(SHOT_DIR)) {
        return 0;
    }
    File dir = LittleFS.open(SHOT_DIR);
    if (!dir || !dir.isDirectory()) {
        return 0;
    }
    size_t sum = 0;
    File f = dir.openNextFile();
    while (f) {
        const char* base = shotBaseName(f.name());
        if (isShotBmpName(base)) {
            sum += static_cast<size_t>(f.size());
        }
        f = dir.openNextFile();
    }
    dir.close();
    return sum;
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

int clearAllScreenshots() {
    if (!LittleFS.begin(false) || !LittleFS.exists(SHOT_DIR)) {
        return 0;
    }
    int n = 0;
    // 每次只删一个，避免遍历中删文件；删前必须关掉文件句柄
    for (;;) {
        File dir = LittleFS.open(SHOT_DIR);
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
        if (LittleFS.remove(path)) {
            n++;
            Serial.printf("[shot] cleared %s\n", to_del);
        } else {
            Serial.printf("[shot] clear remove fail %s\n", path);
            break;
        }
    }
    LittleFS.remove(SHOT_LAST);
    LittleFS.remove(SHOT_BOOT_PENDING);
    return n;
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
