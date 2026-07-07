#include "app_mijia.h"
#include "app_common.h"
#include "app_config.h"
#include "app_header.h"
#include "app_colors.h"
#include "app_icons.h"
#include "app_mijia_ui.h"
#include "mijia_control.h"
#include <cctype>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static int mijiaDeviceIdx = 0;
static bool mijiaOverviewMode = false;
static bool mijiaHelpHeld = false;
static int mijiaOverviewScrollIdx = 0;
static MijiaUiState mijiaUi{};
static int mijiaRefreshGen = 0;
static volatile bool mijiaRefreshBusy = false;
static volatile bool mijiaNeedRedraw = false;

struct MijiaRefreshJob {
    int gen;
    int device_idx;
    MijiaDevice device;
};

static void scheduleMijiaRefresh();

static const MijiaDevice* getCurrentMijiaDevice() {
    const AppConfig& cfg = getAppConfig();
    if (!cfg.loaded || cfg.device_count == 0) {
        return nullptr;
    }
    if (mijiaDeviceIdx < 0) {
        mijiaDeviceIdx = 0;
    }
    if (mijiaDeviceIdx >= cfg.device_count) {
        mijiaDeviceIdx = cfg.device_count - 1;
    }
    return &cfg.devices[mijiaDeviceIdx];
}

// 后台任务：查询设备状态，结果仅在与当前 gen/索引一致时写回
static void mijiaRefreshTaskFn(void* arg) {
    MijiaRefreshJob* job = static_cast<MijiaRefreshJob*>(arg);
    const int job_gen = job->gen;
    const int job_idx = job->device_idx;
    const MijiaDevice device = job->device;
    delete job;

    MijiaUiState temp{};
    mijiaResetUiState(temp);
    mijiaRefreshDevice(&device, temp);

    if (job_gen == mijiaRefreshGen && job_idx == mijiaDeviceIdx) {
        mijiaUi = temp;
        mijiaNeedRedraw = true;
    }

    mijiaRefreshBusy = false;
    if (job_gen != mijiaRefreshGen) {
        // 查询期间又切换了设备，继续拉最新一台
        scheduleMijiaRefresh();
    }
    vTaskDelete(nullptr);
}

// 启动一次异步状态查询（若已有任务在跑则等其结束后链式触发）
static void scheduleMijiaRefresh() {
    if (mijiaRefreshBusy) {
        return;
    }

    const MijiaDevice* dev = getCurrentMijiaDevice();
    if (dev == nullptr) {
        strncpy(mijiaUi.status, "no device", sizeof(mijiaUi.status));
        mijiaUi.power_known = false;
        mijiaNeedRedraw = true;
        return;
    }

    if (!ensureConfigWifi()) {
        strncpy(mijiaUi.status, "wifi fail", sizeof(mijiaUi.status));
        mijiaUi.power_known = false;
        mijiaNeedRedraw = true;
        return;
    }

    auto* job = new MijiaRefreshJob{};
    job->gen = mijiaRefreshGen;
    job->device_idx = mijiaDeviceIdx;
    job->device = *dev;

    mijiaRefreshBusy = true;
    xTaskCreate(mijiaRefreshTaskFn, "mijia_ref", 8192, job, 1, nullptr);
}

// 请求刷新当前设备（不阻塞按键处理）
static void requestMijiaRefresh() {
    mijiaRefreshGen++;
    strncpy(mijiaUi.status, "query...", sizeof(mijiaUi.status));
    mijiaUi.power_known = false;
    mijiaUi.extra_known = false;
    scheduleMijiaRefresh();
}

// 立即切换设备并异步拉状态
static void switchMijiaDevice(const int delta, const int device_count) {
    mijiaDeviceIdx = (mijiaDeviceIdx + delta + device_count) % device_count;
    mijiaResetUiState(mijiaUi);
    strncpy(mijiaUi.status, "query...", sizeof(mijiaUi.status));
    drawMijiaApp();
    requestMijiaRefresh();
}

// 设置当前设备开关
static void setMijiaPower(const bool on) {
    const MijiaDevice* dev = getCurrentMijiaDevice();
    if (dev == nullptr) {
        strncpy(mijiaUi.status, "no device", sizeof(mijiaUi.status));
        return;
    }

    if (!ensureConfigWifi()) {
        strncpy(mijiaUi.status, "wifi fail", sizeof(mijiaUi.status));
        return;
    }

    drawMijiaApp();
    mijiaSetDevicePower(dev, mijiaUi, on);
}

// 概览列表每屏可见项数
static int getMijiaOverviewVisibleCount() {
    const int hint_h = 12;
    const int avail_h = M5Cardputer.Display.height() - APP_CONTENT_Y - hint_h;
    return avail_h / (MIJIA_LIST_ITEM_H + MIJIA_LIST_ITEM_GAP);
}

// 绘制单项：左图标（与三行文字同高）+ 右名称/IP/型号
static void drawMijiaOverviewItem(const MijiaDevice& entry, const int x, const int y,
                                  const bool selected) {
    const MijiaDevKind kind = mijiaClassifyModel(entry.model);
    const uint16_t name_color = selected ? APP_COLOR_OK : APP_COLOR_VALUE;
    drawMijiaDeviceIcon(kind, x, y, selected ? APP_COLOR_OK : APP_COLOR_HINT, MIJIA_LIST_ITEM_H);

    const int text_x = x + MIJIA_LIST_ITEM_H + 6;

    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setTextColor(name_color, BLACK);
    M5Cardputer.Display.setCursor(text_x, y);
    if (entry.name[0] != '\0') {
        M5Cardputer.Display.print(entry.name);
    } else {
        M5Cardputer.Display.print("device");
    }

    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(APP_COLOR_VALUE, BLACK);
    M5Cardputer.Display.setCursor(text_x, y + INFO_LINE_H_2X);
    M5Cardputer.Display.print(entry.ip);

    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
    M5Cardputer.Display.setCursor(text_x, y + INFO_LINE_H_2X + INFO_LINE_H);
    if (entry.model[0] != '\0') {
        M5Cardputer.Display.print(entry.model);
    } else {
        M5Cardputer.Display.print("-");
    }
}

static void drawMijiaOverview(int& y) {
    const AppConfig& cfg = getAppConfig();

    if (!cfg.loaded || cfg.device_count == 0) {
        drawInfoLine(APP_CONTENT_X, y, "total", "0");
        drawInfoLine(APP_CONTENT_X, y, "hint", "press u web");
        drawHintText(APP_CONTENT_X, M5Cardputer.Display.height() - 12, "i back");
        return;
    }

    const int visible = getMijiaOverviewVisibleCount();
    const int max_scroll = cfg.device_count > visible ? cfg.device_count - visible : 0;
    if (mijiaOverviewScrollIdx > max_scroll) {
        mijiaOverviewScrollIdx = max_scroll;
    }
    if (mijiaOverviewScrollIdx < 0) {
        mijiaOverviewScrollIdx = 0;
    }

    int item_y = APP_CONTENT_Y;
    for (int i = 0; i < visible; i++) {
        const int idx = mijiaOverviewScrollIdx + i;
        if (idx >= cfg.device_count) {
            break;
        }
        drawMijiaOverviewItem(cfg.devices[idx], APP_CONTENT_X, item_y, idx == mijiaDeviceIdx);
        item_y += MIJIA_LIST_ITEM_H + MIJIA_LIST_ITEM_GAP;
    }
    y = item_y;

    if (cfg.device_count > visible) {
        char hint[32];
        snprintf(hint, sizeof(hint), "%d/%d  , . [ ] scroll  i back", mijiaOverviewScrollIdx + 1,
                 max_scroll + 1);
        drawHintText(APP_CONTENT_X, M5Cardputer.Display.height() - 12, hint);
    } else {
        drawHintText(APP_CONTENT_X, M5Cardputer.Display.height() - 12, "i back");
    }
}

// 概览列表翻页：-1 上一页，1 下一页
static bool handleMijiaOverviewNav(const int delta) {
    const AppConfig& cfg = getAppConfig();
    if (!cfg.loaded || cfg.device_count == 0) {
        return false;
    }

    const int visible = getMijiaOverviewVisibleCount();
    if (cfg.device_count <= visible) {
        return false;
    }

    const int max_scroll = cfg.device_count - visible;
    const int next = mijiaOverviewScrollIdx + delta;
    if (next < 0 || next > max_scroll) {
        return false;
    }

    mijiaOverviewScrollIdx = next;
    drawMijiaApp();
    return true;
}

bool handleMijiaOverviewPageNav(const Keyboard_Class::KeysState& status) {
    if (!mijiaOverviewMode) {
        return false;
    }
    const int delta = getMenuNavDelta(status);
    if (delta == 0) {
        return false;
    }
    return handleMijiaOverviewNav(delta);
}

// 控制页切换设备
bool handleMijiaDeviceNav(const Keyboard_Class::KeysState& status) {
    if (mijiaOverviewMode || mijiaHelpHeld) {
        return false;
    }

    const AppConfig& cfg = getAppConfig();
    if (!cfg.loaded || cfg.device_count <= 1) {
        return false;
    }

    const int delta = getMenuNavDelta(status);
    if (delta == 0) {
        return false;
    }

    switchMijiaDevice(delta, cfg.device_count);
    return true;
}

// 第二行提示：R 刷新 + 方向键切换设备
static void drawMijiaRefreshHint(const int x, const int y, const int text_size = 1) {
    int cx = x + drawKeyBadge(x, y, 'r', text_size);
    M5Cardputer.Display.setTextSize(text_size);
    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);

    const char* prefix = "refresh ";
    M5Cardputer.Display.setCursor(cx, y);
    M5Cardputer.Display.print(prefix);
    cx += M5Cardputer.Display.textWidth(prefix);

    const int arrow_cy = y + 4 * text_size;
    drawIconArrowLeft(cx, arrow_cy, APP_COLOR_HINT);
    cx += ICON_ARROW_W + 2;
    M5Cardputer.Display.setCursor(cx, y);
    M5Cardputer.Display.print(" ");
    cx += M5Cardputer.Display.textWidth(" ");
    drawIconArrowRight(cx, arrow_cy, APP_COLOR_HINT);
    cx += ICON_ARROW_W + 2;
    M5Cardputer.Display.setCursor(cx, y);
    M5Cardputer.Display.print("switch");
}

// 第一行提示：o/f/t/i 使用按键徽章样式
static void drawMijiaActionHints(const int x, const int y, const int text_size = 1) {
    static const KeyHintItem items[] = {
        {'o', "on"},
        {'f', "off"},
        {'t', "toggle"},
        {'i', "info"},
    };
    drawKeyHintsRow(x, y, items, sizeof(items) / sizeof(items[0]), text_size, APP_COLOR_HINT);
}

// 灯：[/] 调亮度，1-9/0 设百分比
static void drawMijiaLightHints(const int x, int y, const int text_size = 1) {
    int cx = x + drawKeyBadge(x, y, '[', text_size);
    M5Cardputer.Display.setTextSize(text_size);
    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
    M5Cardputer.Display.setCursor(cx, y);
    M5Cardputer.Display.print("/");
    cx += M5Cardputer.Display.textWidth("/");
    cx += drawKeyBadge(cx, y, ']', text_size);
    M5Cardputer.Display.setCursor(cx, y);
    M5Cardputer.Display.print(" bright  1-9,0%");
}

static int mijiaHintLineStep(const int text_size) {
    return text_size == 2 ? INFO_LINE_H_2X : INFO_LINE_H;
}

// 按设备类型绘制操作帮助（text_size=2 用于帮助页）
static void drawMijiaHelpContent(const MijiaDevice* dev, int y, const int text_size) {
    const MijiaDevKind kind =
        dev != nullptr ? mijiaClassifyModel(dev->model) : MijiaDevKind::GENERIC;

    drawMijiaActionHints(APP_CONTENT_X, y, text_size);
    y += mijiaHintLineStep(text_size);
    drawMijiaRefreshHint(APP_CONTENT_X, y, text_size);
    y += mijiaHintLineStep(text_size);

    switch (kind) {
        case MijiaDevKind::LIGHT:
            drawMijiaLightHints(APP_CONTENT_X, y, text_size);
            break;
        case MijiaDevKind::FAN_P5:
            drawHintText(APP_CONTENT_X, y, "9/0 spd w roll m mode", text_size);
            break;
        case MijiaDevKind::FAN_GENERIC:
            drawHintText(APP_CONTENT_X, y, "1-4 speed level", text_size);
            break;
        case MijiaDevKind::AIR_PURIFIER_F20:
            drawHintText(APP_CONTENT_X, y, "1-5 mode 9/0 fan lv", text_size);
            break;
        default:
            break;
    }
}

// 按住 H 时显示的帮助页
static void drawMijiaHelpPage() {
    beginAppScreen("Help");
    const MijiaDevice* dev = getCurrentMijiaDevice();
    drawMijiaHelpContent(dev, APP_CONTENT_Y, 2);
}

void drawMijiaApp() {
    beginAppScreen("Mijia");
    M5Cardputer.Display.setTextSize(1);

    int y = APP_CONTENT_Y;
    const AppConfig& cfg = getAppConfig();
    const MijiaDevice* dev = getCurrentMijiaDevice();

    if (mijiaOverviewMode) {
        drawMijiaOverview(y);
        return;
    }

    if (!cfg.loaded || dev == nullptr) {
        drawInfoLine(APP_CONTENT_X, y, "cfg", "none");
        drawInfoLine(APP_CONTENT_X, y, "hint", "press u web");
        return;
    }

    const MijiaDevKind kind = mijiaClassifyModel(dev->model);
    y = drawMijiaDeviceHeader(dev, kind, mijiaDeviceIdx, cfg.device_count, APP_CONTENT_X, y);
    drawMijiaPowerTags(APP_CONTENT_X, y, mijiaUi.power_known, mijiaUi.power_on, mijiaUi.status);
    y += MIJIA_TAG_H + 6;
    drawMijiaDeviceControls(dev, kind, mijiaUi, APP_CONTENT_X, y);
}

void enterMijiaApp() {
    mijiaDeviceIdx = 0;
    mijiaOverviewMode = false;
    mijiaHelpHeld = false;
    mijiaOverviewScrollIdx = 0;
    mijiaResetUiState(mijiaUi);
    strncpy(mijiaUi.status, "connecting", sizeof(mijiaUi.status));
    drawMijiaApp();
    requestMijiaRefresh();
}

void updateMijiaApp() {
    if (mijiaNeedRedraw) {
        mijiaNeedRedraw = false;
        if (mijiaHelpHeld) {
            drawMijiaHelpPage();
        } else {
            drawMijiaApp();
        }
    }
}

// 检测按键状态中是否包含指定字符（不区分大小写）
static bool mijiaKeysContain(const Keyboard_Class::KeysState& status, const char ch) {
    for (const char c : status.word) {
        if (tolower(static_cast<unsigned char>(c)) == tolower(static_cast<unsigned char>(ch))) {
            return true;
        }
    }
    return false;
}

bool handleMijiaHelpKey(const Keyboard_Class::KeysState& status) {
    if (mijiaOverviewMode) {
        return false;
    }

    const bool held = mijiaKeysContain(status, 'h');
    if (held != mijiaHelpHeld) {
        mijiaHelpHeld = held;
        if (held) {
            drawMijiaHelpPage();
        } else {
            drawMijiaApp();
        }
    }
    return held;
}

void handleMijiaApp(const String& key) {
    if (key == "i") {
        mijiaOverviewMode = !mijiaOverviewMode;
        if (mijiaOverviewMode) {
            mijiaOverviewScrollIdx = 0;
        }
        drawMijiaApp();
        return;
    }
    if (mijiaOverviewMode) {
        return;
    }

    const AppConfig& cfg = getAppConfig();
    if (!cfg.loaded || cfg.device_count == 0) {
        return;
    }

    const MijiaDevice* dev = getCurrentMijiaDevice();
    if (dev == nullptr) {
        return;
    }

    const MijiaDevKind kind = mijiaClassifyModel(dev->model);
    bool handled = true;

    if (key == "o") {
        if (!ensureConfigWifi()) {
            strncpy(mijiaUi.status, "wifi fail", sizeof(mijiaUi.status));
        } else {
            setMijiaPower(true);
        }
    } else if (key == "f") {
        if (!ensureConfigWifi()) {
            strncpy(mijiaUi.status, "wifi fail", sizeof(mijiaUi.status));
        } else {
            setMijiaPower(false);
        }
    } else if (key == "t") {
        if (!ensureConfigWifi()) {
            strncpy(mijiaUi.status, "wifi fail", sizeof(mijiaUi.status));
        } else {
            setMijiaPower(!mijiaUi.power_on);
        }
    } else if (key == "r") {
        drawMijiaApp();
        requestMijiaRefresh();
    } else if (key == "," || key == ";") {
        switchMijiaDevice(-1, cfg.device_count);
        return;
    } else if (key == "." || key == "/") {
        switchMijiaDevice(1, cfg.device_count);
        return;
    } else if (kind == MijiaDevKind::LIGHT && key == "[") {
        if (!ensureConfigWifi()) {
            strncpy(mijiaUi.status, "wifi fail", sizeof(mijiaUi.status));
        } else {
            mijiaAdjustBright(dev, mijiaUi, -10);
        }
    } else if (kind == MijiaDevKind::LIGHT && key == "]") {
        if (!ensureConfigWifi()) {
            strncpy(mijiaUi.status, "wifi fail", sizeof(mijiaUi.status));
        } else {
            mijiaAdjustBright(dev, mijiaUi, 10);
        }
    } else if (kind == MijiaDevKind::LIGHT && key.length() == 1 && key[0] >= '0' && key[0] <= '9') {
        if (!ensureConfigWifi()) {
            strncpy(mijiaUi.status, "wifi fail", sizeof(mijiaUi.status));
        } else {
            const int percent = key[0] == '0' ? 100 : (key[0] - '0') * 10;
            mijiaSetBrightPercent(dev, mijiaUi, percent);
        }
    } else if (kind == MijiaDevKind::FAN_P5) {
        if (!ensureConfigWifi()) {
            strncpy(mijiaUi.status, "wifi fail", sizeof(mijiaUi.status));
        } else if (key == "9") {
            mijiaAdjustFanP5Speed(dev, mijiaUi, -10);
        } else if (key == "0") {
            mijiaAdjustFanP5Speed(dev, mijiaUi, 10);
        } else if (key == "w") {
            mijiaToggleFanP5Roll(dev, mijiaUi);
        } else if (key == "m") {
            mijiaToggleFanP5Mode(dev, mijiaUi);
        } else {
            handled = false;
        }
    } else if (kind == MijiaDevKind::FAN_GENERIC && key.length() == 1 && key[0] >= '1' &&
               key[0] <= '4') {
        if (!ensureConfigWifi()) {
            strncpy(mijiaUi.status, "wifi fail", sizeof(mijiaUi.status));
        } else {
            mijiaSetFanSpeedLevel(dev, mijiaUi, key[0] - '0');
        }
    } else if (kind == MijiaDevKind::AIR_PURIFIER_F20) {
        if (!ensureConfigWifi()) {
            strncpy(mijiaUi.status, "wifi fail", sizeof(mijiaUi.status));
        } else if (key.length() == 1 && key[0] >= '1' && key[0] <= '5') {
            mijiaSetPurifierMode(dev, mijiaUi, key[0] - '1');
        } else if (key == "9") {
            mijiaAdjustPurifierFanLevel(dev, mijiaUi, -1);
        } else if (key == "0") {
            mijiaAdjustPurifierFanLevel(dev, mijiaUi, 1);
        } else {
            handled = false;
        }
    } else {
        handled = false;
    }

    if (handled) {
        drawMijiaApp();
    }
}
