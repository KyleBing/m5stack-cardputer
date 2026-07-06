#include "app_logo.h"
#include "M5Cardputer.h"

// 单圆定义（设计坐标系 64x64）
struct AppLogoCircle {
    int center_x;
    int center_y;
    int radius;
    int border_width;
    uint32_t border_color;
    uint32_t fill_color;  // 0xRRGGBB
};

// 自下而上叠加：先画的在底层
static const AppLogoCircle APP_LOGO_CIRCLES[] = {
    {30, 22, 20, 2, 0x000000, 0xFCEE21},
    {46, 31, 16, 2, 0x000000, 0x8CC63F},
    {20, 38, 18, 2, 0x000000, 0x29ABE2},
    {37, 46, 11, 2, 0x000000, 0xFF7BAC},
};

// 将 0xRRGGBB 转为屏幕色
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
