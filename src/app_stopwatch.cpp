#include "app_stopwatch.h"
#include "app_colors.h"
#include "app_common.h"
#include "app_header.h"
#include <cstring>

static bool swRunning = false;
static uint32_t swAccumMs = 0;
static uint32_t swStartMs = 0;
static bool swScreenReady = false;
static char swLastMain[16] = "";
static char swLastMs[8] = "";

// 按宽度选取最大字号
static int calcTextSizeForWidth(const char* text, const int max_w) {
    for (int ts = 6; ts >= 1; ts--) {
        M5Cardputer.Display.setTextSize(ts);
        if (M5Cardputer.Display.textWidth(text) <= max_w) {
            return ts;
        }
    }
    return 1;
}

static uint32_t swElapsedMs() {
    if (swRunning) {
        return swAccumMs + (millis() - swStartMs);
    }
    return swAccumMs;
}

// 大号时分秒 + 小字毫秒，尽量占满内容区
static void drawStopwatchTime(const int y, const int h, const uint32_t elapsed_ms) {
    const uint32_t ms = elapsed_ms % 1000u;
    const uint32_t total_sec = elapsed_ms / 1000u;
    const int seconds = static_cast<int>(total_sec % 60u);
    const int minutes = static_cast<int>((total_sec / 60u) % 60u);
    const int hours = static_cast<int>((total_sec / 3600u) % 100u);

    const int screen_w = M5Cardputer.Display.width();
    const int margin = APP_CONTENT_X;
    const int avail_w = screen_w - margin * 2;

    char main_buf[16];
    char ms_buf[8];
    snprintf(main_buf, sizeof(main_buf), "%02d:%02d:%02d", hours, minutes, seconds);
    snprintf(ms_buf, sizeof(ms_buf), ".%03d", static_cast<int>(ms));

    if (strcmp(main_buf, swLastMain) == 0 && strcmp(ms_buf, swLastMs) == 0) {
        return;
    }
    strncpy(swLastMain, main_buf, sizeof(swLastMain) - 1);
    swLastMain[sizeof(swLastMain) - 1] = '\0';
    strncpy(swLastMs, ms_buf, sizeof(swLastMs) - 1);
    swLastMs[sizeof(swLastMs) - 1] = '\0';

    const int ts = calcTextSizeForWidth(main_buf, avail_w);
    M5Cardputer.Display.setTextSize(ts);
    const int main_w = M5Cardputer.Display.textWidth(main_buf);
    const int main_h = 8 * ts;

    M5Cardputer.Display.setTextSize(1);
    const int ms_w = M5Cardputer.Display.textWidth(ms_buf);
    const int ms_h = 8;

    const int total_h = main_h + 2 + ms_h;
    const int start_y = y + (h - total_h) / 2;
    const int main_x = margin + (avail_w - main_w) / 2;

    M5Cardputer.Display.fillRect(margin, y, avail_w, h, BLACK);

    M5Cardputer.Display.setTextSize(ts);
    M5Cardputer.Display.setTextColor(WHITE, BLACK);
    M5Cardputer.Display.setCursor(main_x, start_y);
    M5Cardputer.Display.print(main_buf);

    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
    M5Cardputer.Display.setCursor(main_x + main_w - ms_w, start_y + main_h + 2);
    M5Cardputer.Display.print(ms_buf);
}

static void drawStopwatchHints() {
    const int hint_y = M5Cardputer.Display.height() - 12;
    M5Cardputer.Display.fillRect(APP_CONTENT_X, hint_y, 236, 12, BLACK);
    if (swRunning) {
        drawHintText(APP_CONTENT_X, hint_y, "f pause  r reset");
    } else if (swAccumMs > 0) {
        drawHintText(APP_CONTENT_X, hint_y, "o resume  r reset");
    } else {
        drawHintText(APP_CONTENT_X, hint_y, "o start  r reset");
    }
}

static void drawStopwatchStatusTag() {
    M5Cardputer.Display.fillRect(APP_CONTENT_X, APP_CONTENT_Y, 56, 10, BLACK);
    if (swRunning) {
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setTextColor(APP_COLOR_OK, BLACK);
        M5Cardputer.Display.setCursor(APP_CONTENT_X, APP_CONTENT_Y + 2);
        M5Cardputer.Display.print("RUN");
    } else if (swAccumMs > 0) {
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
        M5Cardputer.Display.setCursor(APP_CONTENT_X, APP_CONTENT_Y + 2);
        M5Cardputer.Display.print("PAUSED");
    }
}

static void drawStopwatchApp(const bool full_init) {
    if (full_init || !swScreenReady) {
        beginAppScreen("Stopwatch");
        swScreenReady = true;
        swLastMain[0] = '\0';
        swLastMs[0] = '\0';
    }

    const int screen_h = M5Cardputer.Display.height();
    const int hint_h = 12;
    const int area_y = APP_CONTENT_Y;
    const int area_h = screen_h - area_y - hint_h;

    drawStopwatchTime(area_y, area_h, swElapsedMs());
    drawStopwatchStatusTag();
    drawStopwatchHints();
}

static void swReset() {
    swRunning = false;
    swAccumMs = 0;
    swStartMs = 0;
    swLastMain[0] = '\0';
    swLastMs[0] = '\0';
    drawStopwatchApp(true);
}

void enterStopwatchApp() {
    swRunning = false;
    swAccumMs = 0;
    swStartMs = 0;
    swScreenReady = false;
    swLastMain[0] = '\0';
    swLastMs[0] = '\0';
    drawStopwatchApp(true);
}

void updateStopwatchApp() {
    if (!swRunning) {
        return;
    }

    static uint32_t last_draw_ms = 0;
    if (millis() - last_draw_ms >= 30) {
        last_draw_ms = millis();
        drawStopwatchApp(false);
    }
}

void handleStopwatchApp(const String& key) {
    if (key == "o") {
        if (swRunning) {
            return;
        }
        swStartMs = millis();
        swRunning = true;
        drawStopwatchApp(true);
    } else if (key == "f") {
        if (!swRunning) {
            return;
        }
        swAccumMs += millis() - swStartMs;
        swRunning = false;
        drawStopwatchApp(true);
    } else if (key == "r") {
        swReset();
    }
}
