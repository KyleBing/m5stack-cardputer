#include "app_signal.h"
#include "M5Cardputer.h"

// RSSI(dBm) → 信号格数 0-4
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

// 绘制信号强度条（4 格天线图标，左低右高）
void drawSignalBars(const int x, const int y, const int rssi, const uint16_t color) {
    constexpr int BAR_COUNT = 4;
    constexpr int BAR_W = 2;
    constexpr int BAR_GAP = 1;
    constexpr int HEIGHTS[BAR_COUNT] = {2, 4, 6, 8};
    constexpr int ICON_H = 8;

    const int level = signalLevelFromRssi(rssi);
    const int base_y = y + ICON_H;

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
