#include "app_header.h"
#include "app_icons.h"
#include "app_common.h"
#include "app_connectivity.h"
#include "M5Cardputer.h"

static constexpr int MENU_LOGO_SIZE = 24;
static constexpr int HEADER_STATUS_GAP = 4;
static constexpr int APP_GO_BTN_W = 36;

static int headerStatusIconY(const int icon_h) {
    return (APP_HEADER_H - icon_h) / 2;
}

static int getMenuStatusRightX(const int screen_w, const int page_count) {
    int right = screen_w - 4;
    if (page_count > 1) {
        constexpr int dot_r = 2;
        constexpr int dot_gap = 6;
        const int dots_w = page_count * dot_r * 2 + (page_count - 1) * dot_gap;
        right -= dots_w + 6;
    }
    return right;
}

static int getHeaderStatusWidth(const bool include_battery, const bool wifi, const bool ble,
                                const bool charging) {
    int w = 0;
    if (include_battery) {
        w += getIconBatteryDisplayWidth(charging);
    }
    if (wifi) {
        w += (w > 0 ? HEADER_STATUS_GAP : 0) + ICON_WIFI_W;
    }
    if (ble) {
        w += (w > 0 ? HEADER_STATUS_GAP : 0) + ICON_BLE_W;
    }
    return w;
}

// 从右向左绘制连接状态图标，在 header 内垂直居中
static int drawHeaderStatusIcons(const int right_x, const bool include_battery) {
    const bool wifi = isWifiStaConnected();
    const bool ble = isBleConnected();
    const bool charging = isBatteryCharging();
    const int body_h = getIconBatteryBodyHeight();

    int x = right_x;
    if (include_battery) {
        x -= getIconBatteryDisplayWidth(charging);
        drawIconBattery(x, headerStatusIconY(body_h), M5Cardputer.Power.getBatteryLevel(),
                        charging);
    }
    if (wifi) {
        x -= HEADER_STATUS_GAP + ICON_WIFI_W;
        drawIconWifi(x, headerStatusIconY(ICON_WIFI_H), getWifiStaRssi(), WHITE);
    }
    if (ble) {
        x -= HEADER_STATUS_GAP + ICON_BLE_W;
        drawIconBle(x, headerStatusIconY(ICON_BLE_H), WHITE);
    }
    return x;
}

// 仅清除分割线以上区域，避免盖住底边线
static void clearHeaderStatusArea(const int left_x, const int right_x) {
    if (right_x <= left_x) {
        return;
    }
    M5Cardputer.Display.fillRect(left_x, 0, right_x - left_x, APP_HEADER_H - 1, BLACK);
}

// 绘制右侧 BtnA(GO) 返回按钮样式
static void drawBackButton(const int screen_w) {
    constexpr int btn_w = APP_GO_BTN_W;
    constexpr int btn_h = 18;
    constexpr int btn_r = 4;
    const int btn_x = screen_w - btn_w - 2;
    const int btn_y = (APP_HEADER_H - btn_h) / 2;

    M5Cardputer.Display.fillRoundRect(btn_x, btn_y, btn_w, btn_h, btn_r, DARKGREY);
    M5Cardputer.Display.drawRoundRect(btn_x, btn_y, btn_w, btn_h, btn_r, WHITE);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(WHITE, DARKGREY);
    M5Cardputer.Display.drawCenterString("GO", btn_x + btn_w / 2, btn_y + 5);
}

static void drawHeaderDivider(const int screen_w) {
    M5Cardputer.Display.drawFastHLine(0, APP_HEADER_H - 1, screen_w, DARKGREY);
}

void drawAppScreenHeader(const char* title) {
    const int screen_w = M5Cardputer.Display.width();
    M5Cardputer.Display.fillRect(0, 0, screen_w, APP_HEADER_H, BLACK);

    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setTextColor(WHITE, BLACK);
    M5Cardputer.Display.setCursor(4, (APP_HEADER_H - 16) / 2);
    M5Cardputer.Display.print(title);

    const int status_right = screen_w - 2 - APP_GO_BTN_W - 4;
    drawHeaderStatusIcons(status_right, false);
    drawBackButton(screen_w);
    drawHeaderDivider(screen_w);
    M5Cardputer.Display.setTextColor(WHITE, BLACK);
}

void drawMenuScreenHeader(const char* app_name, const int page, const int page_count) {
    const int screen_w = M5Cardputer.Display.width();
    M5Cardputer.Display.fillRect(0, 0, screen_w, APP_HEADER_H, BLACK);

    const int logo_y = (APP_HEADER_H - MENU_LOGO_SIZE) / 2;
    drawAppLogo(2, logo_y, MENU_LOGO_SIZE);

    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setTextColor(WHITE, BLACK);
    M5Cardputer.Display.setCursor(2 + MENU_LOGO_SIZE + 4, logo_y + 4);
    M5Cardputer.Display.print(app_name);

    const int status_right = getMenuStatusRightX(screen_w, page_count);
    drawHeaderStatusIcons(status_right, true);

    if (page_count > 1) {
        constexpr int dot_r = 2;
        constexpr int dot_gap = 6;
        const int dots_w = page_count * dot_r * 2 + (page_count - 1) * dot_gap;
        const int dot_x = screen_w - dots_w - 4;
        drawIconPageDots(dot_x, APP_HEADER_H / 2, page, page_count);
    }

    drawHeaderDivider(screen_w);
}

void updateMenuHeaderStatus(const int page_count) {
    const int screen_w = M5Cardputer.Display.width();
    const int status_right = getMenuStatusRightX(screen_w, page_count);
    const bool wifi = isWifiStaConnected();
    const bool ble = isBleConnected();
    const bool charging = isBatteryCharging();
    const int width = getHeaderStatusWidth(true, wifi, ble, charging);
    const int left_x = status_right - width;
    clearHeaderStatusArea(left_x, status_right);
    drawHeaderStatusIcons(status_right, true);
    drawHeaderDivider(screen_w);
}

void updateAppHeaderStatus() {
    const int screen_w = M5Cardputer.Display.width();
    const int status_right = screen_w - 2 - APP_GO_BTN_W - 4;
    const bool wifi = isWifiStaConnected();
    const bool ble = isBleConnected();
    const int width = getHeaderStatusWidth(false, wifi, ble, false);
    const int left_x = status_right - width;
    clearHeaderStatusArea(left_x, status_right);
    drawHeaderStatusIcons(status_right, false);
    drawHeaderDivider(screen_w);
}

void updateMenuScreenBattery(const int page_count) {
    updateMenuHeaderStatus(page_count);
}

void beginAppScreen(const char* title) {
    M5Cardputer.Display.clear();
    drawAppScreenHeader(title);
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setTextColor(WHITE, BLACK);
}
