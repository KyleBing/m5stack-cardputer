#include "app_stopwatch.h"
#include "app_colors.h"
#include "app_common.h"
#include "app_header.h"
#include "app_rtc.h"
#include "app_time_ui.h"
#include <cstdio>

static bool swRunning = false;
static uint32_t swAccumMs = 0;
static uint32_t swStartMs = 0;
static bool swScreenReady = false;
static BigTimeState swTimeState{};

static uint32_t swElapsedMs() {
    if (swRunning) {
        return swAccumMs + (millis() - swStartMs);
    }
    return swAccumMs;
}

static void drawStopwatchActionHints() {
    const char* g_text = "start";
    if (swRunning) {
        g_text = "pause";
    } else if (swAccumMs > 0) {
        g_text = "resume";
    }
    const KeyHintItem items[] = {
        {'g', g_text},
        {'r', "reset"},
        {'p', "pure"},
    };
    drawTimeBottomHints(items, 3);
}

static void drawStopwatchChrome() {
    if (isTimePureMode()) {
        return;
    }
    drawTimeModeTag("SW");
    drawStopwatchActionHints();
}

static void drawStopwatchTimeArea(const int area_y, const int area_h, const bool force) {
    const uint32_t elapsed = swElapsedMs();

    int hours = 0;
    int minutes = 0;
    int seconds = 0;
    int frac = 0;
    splitTimeMs(elapsed, hours, minutes, seconds, frac);
    drawBigTimeDisplay(swTimeState, area_y, area_h, hours, minutes, seconds, frac, true, force);
}

static void drawStopwatchApp(const bool full_init) {
    int area_y = 0;
    int area_h = 0;
    if (isTimePureMode()) {
        getTimePureDisplayArea(area_y, area_h);
    } else {
        getTimeDisplayArea(area_y, area_h);
    }

    if (full_init || !swScreenReady) {
        if (isTimePureMode()) {
            if (full_init) {
                M5Cardputer.Display.fillScreen(BLACK);
            }
            swScreenReady = true;
            swTimeState = BigTimeState{};
            drawStopwatchChrome();
            drawStopwatchTimeArea(area_y, area_h, true);
            return;
        }
        beginAppScreen("Time");
        swScreenReady = true;
        swTimeState = BigTimeState{};
        drawStopwatchChrome();
        drawStopwatchTimeArea(area_y, area_h, true);
        return;
    }

    drawStopwatchTimeArea(area_y, area_h, false);
}

static void swReset() {
    swRunning = false;
    swAccumMs = 0;
    swStartMs = 0;
    swTimeState = BigTimeState{};
    drawStopwatchChrome();
    int area_y = 0;
    int area_h = 0;
    if (isTimePureMode()) {
        getTimePureDisplayArea(area_y, area_h);
    } else {
        getTimeDisplayArea(area_y, area_h);
    }
    drawStopwatchTimeArea(area_y, area_h, true);
}

void redrawStopwatchApp() {
    drawStopwatchApp(true);
}

void enterStopwatchApp() {
    swRunning = false;
    swAccumMs = 0;
    swStartMs = 0;
    swScreenReady = false;
    swTimeState = BigTimeState{};
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

static void swToggleRun() {
    if (swRunning) {
        swAccumMs += millis() - swStartMs;
        swRunning = false;
    } else {
        swStartMs = millis();
        swRunning = true;
    }
    drawStopwatchChrome();
}

void handleStopwatchApp(const Keyboard_Class::KeysState& status) {
    if (status.space || status.enter) {
        swToggleRun();
        return;
    }
    const String key = getPressedKey();
    if (key == "g") {
        swToggleRun();
    } else if (key == "r") {
        swReset();
    }
}
