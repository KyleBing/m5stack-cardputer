#include "app_icons.h"
#include "M5Cardputer.h"
#include <cmath>

// ===== Logo =====

struct AppLogoCircle {
    int center_x;
    int center_y;
    int radius;
    int border_width;
    uint32_t border_color;
    uint32_t fill_color;
};

static const AppLogoCircle APP_LOGO_CIRCLES[] = {
    {30, 22, 20, 2, 0x000000, 0xFCEE21},
    {46, 31, 16, 2, 0x000000, 0x8CC63F},
    {20, 38, 18, 2, 0x000000, 0x29ABE2},
    {37, 46, 11, 2, 0x000000, 0xFF7BAC},
};

static uint16_t colorFromRgb(const uint32_t rgb) {
    return M5Cardputer.Display.color565(
        static_cast<uint8_t>((rgb >> 16) & 0xFF),
        static_cast<uint8_t>((rgb >> 8) & 0xFF),
        static_cast<uint8_t>(rgb & 0xFF));
}

void drawAppLogo(const int dest_x, const int dest_y, const int size) {
    const int circle_count = sizeof(APP_LOGO_CIRCLES) / sizeof(APP_LOGO_CIRCLES[0]);
    for (int i = 0; i < circle_count; i++) {
        const AppLogoCircle& c = APP_LOGO_CIRCLES[i];
        const int cx = dest_x + c.center_x * size / APP_LOGO_DESIGN_SIZE;
        const int cy = dest_y + c.center_y * size / APP_LOGO_DESIGN_SIZE;
        const int r = c.radius * size / APP_LOGO_DESIGN_SIZE;
        const int border = (c.border_width * size + APP_LOGO_DESIGN_SIZE - 1) / APP_LOGO_DESIGN_SIZE;
        if (r <= 0) {
            continue;
        }
        M5Cardputer.Display.fillCircle(cx, cy, r, colorFromRgb(c.fill_color));
        for (int b = 0; b < border; b++) {
            M5Cardputer.Display.drawCircle(cx, cy, r - b, colorFromRgb(c.border_color));
        }
    }
}

// ===== 方向箭头 =====

void drawIconArrowLeft(const int x, const int cy, const uint16_t color) {
    M5Cardputer.Display.fillTriangle(x, cy - 3, x + 5, cy, x, cy + 3, color);
}

void drawIconArrowRight(const int x, const int cy, const uint16_t color) {
    M5Cardputer.Display.fillTriangle(x + 5, cy - 3, x, cy, x + 5, cy + 3, color);
}

void drawIconArrowUp(const int x, const int cy, const uint16_t color) {
    const int cx = x + ICON_ARROW_W / 2;
    M5Cardputer.Display.fillTriangle(cx, cy - 3, cx - 3, cy + 2, cx + 3, cy + 2, color);
}

void drawIconArrowDown(const int x, const int cy, const uint16_t color) {
    const int cx = x + ICON_ARROW_W / 2;
    M5Cardputer.Display.fillTriangle(cx, cy + 3, cx - 3, cy - 2, cx + 3, cy - 2, color);
}

// 左右箭头合成（x 为左缘，cy 为垂直中心）
void drawIconArrowLeftRight(const int x, const int cy, const uint16_t color) {
    drawIconArrowLeft(x, cy, color);
    drawIconArrowRight(x + ICON_ARROW_W + 2, cy, color);
}

// 上下箭头合成（x 为左缘，cy 为垂直中心）
void drawIconArrowUpDown(const int x, const int cy, const uint16_t color) {
    drawIconArrowUp(x, cy - 4, color);
    drawIconArrowDown(x, cy + 4, color);
}

// ===== WiFi 信号条 =====

int signalLevelFromRssi(const int rssi) {
    if (rssi >= -50) {
        return 4;
    }
    if (rssi >= -60) {
        return 3;
    }
    if (rssi >= -70) {
        return 2;
    }
    if (rssi >= -80) {
        return 1;
    }
    return 0;
}

void drawSignalBars(const int x, const int y, const int rssi, const uint16_t color) {
    constexpr int BAR_COUNT = 4;
    constexpr int BAR_W = 2;
    constexpr int BAR_GAP = 1;
    constexpr int HEIGHTS[BAR_COUNT] = {2, 4, 6, 8};

    const int level = signalLevelFromRssi(rssi);
    const int base_y = y + ICON_SIGNAL_H;

    for (int i = 0; i < BAR_COUNT; i++) {
        const int bx = x + i * (BAR_W + BAR_GAP);
        const int h = HEIGHTS[i];
        const int by = base_y - h;
        if (i < level) {
            M5Cardputer.Display.fillRect(bx, by, BAR_W, h, color);
        } else {
            M5Cardputer.Display.drawRect(bx, by, BAR_W, h, DARKGREY);
        }
    }
}

// ===== WiFi 扇形圆图标（header） =====

// 左下锚点为圆心，绘制右上象限 1/4 圆弧
static void drawWifiQuarterArc(const int cx, const int cy, const int radius,
                               const uint16_t color) {
    if (radius <= 0) {
        return;
    }
    for (int deg = 0; deg <= 90; deg++) {
        const float rad = deg * 3.14159265f / 180.0f;
        const int px = cx + static_cast<int>(radius * cosf(rad) + 0.5f);
        const int py = cy - static_cast<int>(radius * sinf(rad) + 0.5f);
        M5Cardputer.Display.drawPixel(px, py, color);
    }
}

void drawIconWifi(const int x, const int y, const int rssi, const uint16_t color) {
    const int level = signalLevelFromRssi(rssi);
    const int cx = x;
    const int cy = y + ICON_WIFI_H - 1;

    M5Cardputer.Display.fillRect(cx, cy - WIFI_INNER_SIDE + 1, WIFI_INNER_SIDE, WIFI_INNER_SIDE,
                                 color);

    for (int i = 0; i < WIFI_RING_COUNT; i++) {
        const int radius = WIFI_INNER_SIDE + 3 * (i + 1) - 1;
        const bool lit = level >= i + 2;
        drawWifiQuarterArc(cx, cy, radius, lit ? color : DARKGREY);
    }
}

// ===== 蓝牙 =====

void drawIconBle(const int x, const int y, const uint16_t color) {
    const int cx = x + ICON_BLE_W / 2;
    const int cy = y + ICON_BLE_H / 2;
    M5Cardputer.Display.drawLine(cx - 3, cy - 4, cx - 3, cy + 4, color);
    M5Cardputer.Display.drawLine(cx + 3, cy - 4, cx + 3, cy + 4, color);
    M5Cardputer.Display.fillTriangle(cx - 3, cy - 4, cx + 3, cy - 1, cx - 3, cy - 1, color);
    M5Cardputer.Display.fillTriangle(cx - 3, cy + 1, cx + 3, cy + 1, cx - 3, cy + 4, color);
}

// ===== 充电闪电 =====

void drawIconChargingBolt(const int zone_x, const int y, const int body_h) {
    constexpr int zone_w = 6;
    const int cx = zone_x + zone_w / 2;
    const int cy = y + body_h / 2;
    const int s = body_h <= 12 ? 1 : 0;
    M5Cardputer.Display.fillTriangle(cx + 1 - s, cy - 4 + s, cx - 2, cy + 1, cx, cy + 1, YELLOW);
    M5Cardputer.Display.fillTriangle(cx - 1 + s, cy + 4 - s, cx + 2, cy - 1, cx, cy - 1, YELLOW);
}

// ===== 电池图标 =====

static constexpr int BATTERY_SEGMENTS = 5;
static constexpr int BATTERY_SEG_W = 3;
static constexpr int BATTERY_SEG_H = 8;
static constexpr int BATTERY_SEG_GAP = 1;
static constexpr int BATTERY_BORDER = 1;
static constexpr int BATTERY_INNER_GAP = 1;
static constexpr int BATTERY_HEAD_W = 2;
static constexpr int BATTERY_HEAD_H = 4;
static constexpr int BOLT_ZONE_W = 6;
static constexpr int BOLT_BATTERY_GAP = 1;

static int getBatteryInset() {
    return BATTERY_BORDER + BATTERY_INNER_GAP;
}

static int getBatteryInnerWidth() {
    return BATTERY_SEGMENTS * BATTERY_SEG_W + (BATTERY_SEGMENTS - 1) * BATTERY_SEG_GAP;
}

static int getBatteryBodyWidth() {
    return getBatteryInnerWidth() + getBatteryInset() * 2;
}

int getIconBatteryBodyHeight() {
    return BATTERY_SEG_H + getBatteryInset() * 2;
}

static int getBatteryIndicatorWidth() {
    return BATTERY_HEAD_W + getBatteryBodyWidth();
}

int getIconBatteryDisplayWidth(const bool charging) {
    return getBatteryIndicatorWidth() + (charging ? BOLT_ZONE_W + BOLT_BATTERY_GAP : 0);
}

void drawIconBattery(const int x, const int y, const int level, const bool charging) {
    const int body_w = getBatteryBodyWidth();
    const int body_h = getIconBatteryBodyHeight();
    const int bolt_offset = charging ? BOLT_ZONE_W + BOLT_BATTERY_GAP : 0;
    const int battery_x = x + bolt_offset;
    const int body_x = battery_x + BATTERY_HEAD_W;
    const int total_w = getBatteryIndicatorWidth();
    const uint16_t accent = charging ? GREEN : WHITE;

    M5Cardputer.Display.fillRect(x, y, bolt_offset + total_w, body_h, BLACK);

    if (charging) {
        drawIconChargingBolt(x, y, body_h);
    }

    const int head_y = y + (body_h - BATTERY_HEAD_H) / 2;
    M5Cardputer.Display.fillRect(battery_x, head_y, BATTERY_HEAD_W, BATTERY_HEAD_H, accent);
    M5Cardputer.Display.drawRect(body_x, y, body_w, body_h, accent);

    const int inset = getBatteryInset();
    const int seg_x0 = body_x + inset;
    const int seg_y = y + inset;
    const int filled = constrain((level + 19) / 20, 0, BATTERY_SEGMENTS);
    const int fill_start = BATTERY_SEGMENTS - filled;
    for (int i = 0; i < BATTERY_SEGMENTS; i++) {
        const int sx = seg_x0 + i * (BATTERY_SEG_W + BATTERY_SEG_GAP);
        if (i >= fill_start) {
            M5Cardputer.Display.fillRect(sx, seg_y, BATTERY_SEG_W, BATTERY_SEG_H, accent);
        } else {
            M5Cardputer.Display.drawRect(sx, seg_y, BATTERY_SEG_W, BATTERY_SEG_H, DARKGREY);
        }
    }
}

// ===== 分页圆点 =====

void drawIconPageDots(const int x, const int cy, const int page, const int page_count) {
    if (page_count <= 1) {
        return;
    }

    constexpr int dot_r = 2;
    constexpr int dot_gap = 6;
    for (int i = 0; i < page_count; i++) {
        const int cx = x + dot_r + i * (dot_r * 2 + dot_gap);
        if (i == page) {
            M5Cardputer.Display.fillCircle(cx, cy, dot_r, WHITE);
        } else {
            M5Cardputer.Display.drawCircle(cx, cy, dot_r, DARKGREY);
        }
    }
}

// ===== Info 列表图标 =====

void drawIconInfoChip(const int x, const int y, const uint16_t color) {
    M5Cardputer.Display.drawRoundRect(x + 4, y + 4, 16, 16, 2, color);
    for (int i = 0; i < 4; i++) {
        const int px = x + 6 + (i % 2) * 10;
        const int py = y + 2 + (i / 2) * 18;
        M5Cardputer.Display.fillRect(px, py, 2, 3, color);
    }
    M5Cardputer.Display.drawFastHLine(x + 8, y + 11, 8, color);
    M5Cardputer.Display.drawFastHLine(x + 8, y + 14, 8, color);
}

void drawIconInfoStorage(const int x, const int y, const uint16_t color) {
    M5Cardputer.Display.drawRoundRect(x + 5, y + 3, 14, 18, 2, color);
    M5Cardputer.Display.fillRect(x + 8, y + 6, 8, 3, color);
    for (int i = 0; i < 3; i++) {
        M5Cardputer.Display.drawFastHLine(x + 8, y + 11 + i * 3, 8, color);
    }
}

void drawIconInfoBattery(const int x, const int y, const uint16_t color) {
    const int body_x = x + 4;
    const int body_y = y + 6;
    const int body_w = 16;
    const int body_h = 12;
    M5Cardputer.Display.fillRect(x + 9, y + 4, 4, 3, color);
    M5Cardputer.Display.drawRoundRect(body_x, body_y, body_w, body_h, 2, color);
    M5Cardputer.Display.fillRect(body_x + 3, body_y + 3, 7, 6, color);
}
