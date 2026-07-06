#include "app_header.h"
#include "app_logo.h"
#include "M5Cardputer.h"

static constexpr int MENU_LOGO_SIZE = 24;

// 绘制右侧 BtnA(GO) 返回按钮样式
static void drawBackButton(const int screen_w) {
    constexpr int btn_w = 36;
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

// 绘制底部分隔线
static void drawHeaderDivider(const int screen_w) {
    M5Cardputer.Display.drawFastHLine(0, APP_HEADER_H - 1, screen_w, DARKGREY);
}

void drawAppScreenHeader(const char* title) {
    const int screen_w = M5Cardputer.Display.width();
    M5Cardputer.Display.fillRect(0, 0, screen_w, APP_HEADER_H, BLACK);

    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(WHITE, BLACK);
    M5Cardputer.Display.setCursor(4, (APP_HEADER_H - 8) / 2);
    M5Cardputer.Display.print(title);

    drawBackButton(screen_w);
    drawHeaderDivider(screen_w);
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

    if (page_count > 1) {
        constexpr int dot_r = 2;
        constexpr int dot_gap = 6;
        const int dots_w = page_count * dot_r * 2 + (page_count - 1) * dot_gap;
        int dot_x = screen_w - dots_w - 4;
        const int dot_cy = APP_HEADER_H / 2;
        for (int i = 0; i < page_count; i++) {
            const int cx = dot_x + dot_r + i * (dot_r * 2 + dot_gap);
            if (i == page) {
                M5Cardputer.Display.fillCircle(cx, dot_cy, dot_r, WHITE);
            } else {
                M5Cardputer.Display.drawCircle(cx, dot_cy, dot_r, DARKGREY);
            }
        }
    }

    drawHeaderDivider(screen_w);
}

void beginAppScreen(const char* title) {
    M5Cardputer.Display.clear();
    drawAppScreenHeader(title);
}
