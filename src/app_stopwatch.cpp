#include "app_stopwatch.h"
#include "app_colors.h"
#include "app_common.h"
#include "app_header.h"
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
    };
    drawTimeBottomHints(items, 2);
}

static void drawStopwatchChrome() {
    drawTimeModeTag("SW");
    drawStopwatchActionHints();
}

static void drawStopwatchApp(const bool full_init) {
    int area_y = 0;
    int area_h = 0;
    getTimeDisplayArea(area_y, area_h);
    const uint32_t elapsed = swElapsedMs();

    int hours = 0;
    int minutes = 0;
    int seconds = 0;
    int frac = 0;
    splitTimeMs(elapsed, hours, minutes, seconds, frac);

    if (full_init || !swScreenReady) {
        beginAppScreen("Time");
        swScreenReady = true;
        swTimeState = BigTimeState{};
        drawStopwatchChrome();
        drawBigTimeDisplay(swTimeState, area_y, area_h, hours, minutes, seconds, frac, true, true);
        return;
    }

    drawBigTimeDisplay(swTimeState, area_y, area_h, hours, minutes, seconds, frac, true, false);
}

static void swReset() {
    swRunning = false;
    swAccumMs = 0;
    swStartMs = 0;
    swTimeState = BigTimeState{};
    drawStopwatchChrome();
    int area_y = 0;
    int area_h = 0;
    getTimeDisplayArea(area_y, area_h);
    drawBigTimeDisplay(swTimeState, area_y, area_h, 0, 0, 0, 0, true, true);
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
