#include "app_mijia.h"
#include "app_common.h"
#include "app_config.h"
#include "app_header.h"
#include "app_colors.h"
#include "app_icons.h"
#include "app_mijia_ui.h"
#include "mijia_control.h"
#include <cstring>

static int mijiaDeviceIdx = 0;
static bool mijiaOverviewMode = false;
static MijiaUiState mijiaUi{};

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

// 查询当前设备状态
static void refreshMijiaDevice() {
    const MijiaDevice* dev = getCurrentMijiaDevice();
    if (dev == nullptr) {
        strncpy(mijiaUi.status, "no device", sizeof(mijiaUi.status));
        mijiaUi.power_known = false;
        return;
    }

    if (!ensureConfigWifi()) {
        strncpy(mijiaUi.status, "wifi fail", sizeof(mijiaUi.status));
        mijiaUi.power_known = false;
        return;
    }

    strncpy(mijiaUi.status, "query...", sizeof(mijiaUi.status));
    drawMijiaApp();
    mijiaRefreshDevice(dev, mijiaUi);
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

// 设备概览：总数量 + 每台 name / ip / model
static void drawMijiaOverview(int& y) {
    const AppConfig& cfg = getAppConfig();
    const int screen_bottom = M5Cardputer.Display.height() - INFO_LINE_H;

    if (!cfg.loaded || cfg.device_count == 0) {
        drawInfoLine(APP_CONTENT_X, y, "total", "0");
        drawInfoLine(APP_CONTENT_X, y, "hint", "press u web");
        drawHintText(APP_CONTENT_X, y, "i back");
        return;
    }

    char buf[40];
    snprintf(buf, sizeof(buf), "%d", cfg.device_count);
    drawInfoLine(APP_CONTENT_X, y, "total", buf);

    for (int i = 0; i < cfg.device_count; i++) {
        if (y > screen_bottom) {
            drawHintText(APP_CONTENT_X, y, "...");
            return;
        }
        const MijiaDevice& entry = cfg.devices[i];
        const MijiaDevKind kind = mijiaClassifyModel(entry.model);
        snprintf(buf, sizeof(buf), "#%d%s", i + 1, i == mijiaDeviceIdx ? "*" : "");
        drawMijiaDeviceIcon(kind, APP_CONTENT_X, y, APP_COLOR_HINT);
        drawInfoLine(APP_CONTENT_X + MIJIA_ICON_W + 4, y, buf, entry.name);
        if (y > screen_bottom) {
            return;
        }
        if (entry.model[0] != '\0') {
            snprintf(buf, sizeof(buf), "%s  %.16s", entry.ip, entry.model);
        } else {
            snprintf(buf, sizeof(buf), "%s", entry.ip);
        }
        drawInfoLine(APP_CONTENT_X, y, "ip", buf);
    }

    if (y <= screen_bottom) {
        drawHintText(APP_CONTENT_X, y, "i back");
    }
}

// 第二行提示：R 刷新 + 方向键切换设备
static void drawMijiaRefreshHint(const int x, const int y) {
    int cx = x + drawKeyBadge(x, y, 'r', 1);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);

    const char* prefix = "refresh ";
    M5Cardputer.Display.setCursor(cx, y);
    M5Cardputer.Display.print(prefix);
    cx += M5Cardputer.Display.textWidth(prefix);

    const int arrow_cy = y + 4;
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

// 按设备类型绘制操作提示
static void drawMijiaHints(const MijiaDevice* dev, int y) {
    const MijiaDevKind kind = mijiaClassifyModel(dev->model);

    drawHintText(APP_CONTENT_X, y, "o on f off t toggle i info");
    y += INFO_LINE_H;
    drawMijiaRefreshHint(APP_CONTENT_X, y);
    y += INFO_LINE_H;

    switch (kind) {
        case MijiaDevKind::LIGHT:
            drawHintText(APP_CONTENT_X, y, "-/+ bright");
            break;
        case MijiaDevKind::FAN_P5:
            drawHintText(APP_CONTENT_X, y, "9/0 spd w roll m mode");
            break;
        case MijiaDevKind::FAN_GENERIC:
            drawHintText(APP_CONTENT_X, y, "1-4 speed level");
            break;
        case MijiaDevKind::AIR_PURIFIER_F20:
            drawHintText(APP_CONTENT_X, y, "1-5 mode 9/0 fan lv");
            break;
        default:
            break;
    }
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
    drawMijiaPowerTags(APP_CONTENT_X, y, mijiaUi.power_known, mijiaUi.power_on);
    y += MIJIA_TAG_H + 6;
    y = drawMijiaDeviceControls(dev, kind, mijiaUi, APP_CONTENT_X, y);
    y += 4;
    drawInfoLine(APP_CONTENT_X, y, "status", mijiaUi.status);
    drawMijiaHints(dev, y);
}

void enterMijiaApp() {
    mijiaDeviceIdx = 0;
    mijiaOverviewMode = false;
    mijiaResetUiState(mijiaUi);
    strncpy(mijiaUi.status, "connecting", sizeof(mijiaUi.status));
    drawMijiaApp();
    refreshMijiaDevice();
    drawMijiaApp();
}

void handleMijiaApp(const String& key) {
    if (key == "i") {
        mijiaOverviewMode = !mijiaOverviewMode;
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

    if (!ensureConfigWifi()) {
        strncpy(mijiaUi.status, "wifi fail", sizeof(mijiaUi.status));
        drawMijiaApp();
        return;
    }

    const MijiaDevKind kind = mijiaClassifyModel(dev->model);
    bool handled = true;

    if (key == "o") {
        setMijiaPower(true);
    } else if (key == "f") {
        setMijiaPower(false);
    } else if (key == "t") {
        setMijiaPower(!mijiaUi.power_on);
    } else if (key == "r") {
        refreshMijiaDevice();
    } else if (key == "," || key == ";") {
        mijiaDeviceIdx = (mijiaDeviceIdx - 1 + cfg.device_count) % cfg.device_count;
        mijiaResetUiState(mijiaUi);
        refreshMijiaDevice();
    } else if (key == "." || key == "/") {
        mijiaDeviceIdx = (mijiaDeviceIdx + 1) % cfg.device_count;
        mijiaResetUiState(mijiaUi);
        refreshMijiaDevice();
    } else if (kind == MijiaDevKind::LIGHT &&
               (key == "-" || key == "_" || key == "+" || key == "=")) {
        const int delta = (key == "-" || key == "_") ? -10 : 10;
        mijiaAdjustBright(dev, mijiaUi, delta);
    } else if (kind == MijiaDevKind::FAN_P5) {
        if (key == "9") {
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
        mijiaSetFanSpeedLevel(dev, mijiaUi, key[0] - '0');
    } else if (kind == MijiaDevKind::AIR_PURIFIER_F20) {
        if (key.length() == 1 && key[0] >= '1' && key[0] <= '5') {
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
