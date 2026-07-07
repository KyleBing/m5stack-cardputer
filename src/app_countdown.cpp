#include "app_countdown.h"
#include "app_colors.h"
#include "app_common.h"
#include "app_header.h"
#include <cstring>

enum class CountdownPhase {
    SETUP,
    RUNNING,
    PAUSED,
    FINISHED,
};

static CountdownPhase cdPhase = CountdownPhase::SETUP;
static int cdHours = 0;
static int cdMinutes = 5;
static int cdSeconds = 0;
static int cdField = 0;  // 0=h 1=m 2=s
static uint32_t cdEndMs = 0;
static uint32_t cdRemainMs = 0;
static bool cdScreenReady = false;

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

// 大号 HH:MM:SS，可高亮当前编辑字段
static void drawLargeHms(const int y, const int h, const int hours, const int minutes,
                         const int seconds, const int active_field, const bool highlight_field) {
    const int screen_w = M5Cardputer.Display.width();
    const int margin = APP_CONTENT_X;
    const int avail_w = screen_w - margin * 2;

    char hbuf[4];
    char mbuf[4];
    char sbuf[4];
    snprintf(hbuf, sizeof(hbuf), "%02d", hours);
    snprintf(mbuf, sizeof(mbuf), "%02d", minutes);
    snprintf(sbuf, sizeof(sbuf), "%02d", seconds);

    char full[16];
    snprintf(full, sizeof(full), "%s:%s:%s", hbuf, mbuf, sbuf);
    const int ts = calcTextSizeForWidth(full, avail_w);
    M5Cardputer.Display.setTextSize(ts);
    const int line_h = 8 * ts;
    const int cy = y + (h - line_h) / 2;
    const int cx0 = margin + (avail_w - M5Cardputer.Display.textWidth(full)) / 2;

    struct Part {
        const char* text;
        int field;
    };
    const Part parts[] = {
        {hbuf, 0}, {":", -1}, {mbuf, 1}, {":", -1}, {sbuf, 2},
    };

    int cx = cx0;
    for (const Part& part : parts) {
        uint16_t color = WHITE;
        if (highlight_field && part.field == active_field) {
            color = YELLOW;
        }
        M5Cardputer.Display.setTextColor(color, BLACK);
        M5Cardputer.Display.setCursor(cx, cy);
        M5Cardputer.Display.print(part.text);
        cx += M5Cardputer.Display.textWidth(part.text);
    }
}

static uint32_t cdSetupTotalMs() {
    const uint32_t total_sec =
        static_cast<uint32_t>(cdHours) * 3600u + static_cast<uint32_t>(cdMinutes) * 60u +
        static_cast<uint32_t>(cdSeconds);
    return total_sec * 1000u;
}

static void cdGetDisplayHms(int& hours, int& minutes, int& seconds) {
    if (cdPhase == CountdownPhase::SETUP) {
        hours = cdHours;
        minutes = cdMinutes;
        seconds = cdSeconds;
        return;
    }

    uint32_t remain_ms = 0;
    if (cdPhase == CountdownPhase::RUNNING) {
        const int32_t left = static_cast<int32_t>(cdEndMs - millis());
        remain_ms = left > 0 ? static_cast<uint32_t>(left) : 0;
    } else {
        remain_ms = cdRemainMs;
    }

    const uint32_t total_sec = remain_ms / 1000u;
    seconds = static_cast<int>(total_sec % 60u);
    minutes = static_cast<int>((total_sec / 60u) % 60u);
    hours = static_cast<int>((total_sec / 3600u) % 100u);
}

static void drawCountdownHints() {
    const int y = M5Cardputer.Display.height() - 12;
    M5Cardputer.Display.fillRect(APP_CONTENT_X, y, 236, 12, BLACK);
    if (cdPhase == CountdownPhase::SETUP) {
        drawHintText(APP_CONTENT_X, y, "[ ] field  , . adjust  o start");
    } else if (cdPhase == CountdownPhase::RUNNING) {
        drawHintText(APP_CONTENT_X, y, "f pause  r reset");
    } else if (cdPhase == CountdownPhase::PAUSED) {
        drawHintText(APP_CONTENT_X, y, "o resume  r reset");
    } else {
        drawHintText(APP_CONTENT_X, y, "o again  r setup");
    }
}

static void drawCountdownApp(const bool full_init) {
    if (full_init || !cdScreenReady) {
        beginAppScreen("Countdown");
        cdScreenReady = true;
    }

    const int screen_h = M5Cardputer.Display.height();
    const int hint_h = 12;
    const int area_y = APP_CONTENT_Y;
    const int area_h = screen_h - area_y - hint_h;

    M5Cardputer.Display.fillRect(APP_CONTENT_X, area_y, M5Cardputer.Display.width() - APP_CONTENT_X * 2,
                                 area_h, BLACK);

    int hours = 0;
    int minutes = 0;
    int seconds = 0;
    cdGetDisplayHms(hours, minutes, seconds);

    const bool highlight = cdPhase == CountdownPhase::SETUP;
    drawLargeHms(area_y, area_h, hours, minutes, seconds, cdField, highlight);

    if (cdPhase == CountdownPhase::FINISHED) {
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setTextColor(APP_COLOR_OK, BLACK);
        M5Cardputer.Display.setCursor(APP_CONTENT_X, area_y + area_h - 10);
        M5Cardputer.Display.print("Time's up!");
    } else if (cdPhase == CountdownPhase::PAUSED) {
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
        M5Cardputer.Display.setCursor(APP_CONTENT_X, area_y + 2);
        M5Cardputer.Display.print("PAUSED");
    }

    drawCountdownHints();
}

static void cdAdjustField(const int delta) {
    if (cdPhase != CountdownPhase::SETUP) {
        return;
    }
    switch (cdField) {
        case 0:
            cdHours = constrain(cdHours + delta, 0, 99);
            break;
        case 1:
            cdMinutes = constrain(cdMinutes + delta, 0, 59);
            break;
        default:
            cdSeconds = constrain(cdSeconds + delta, 0, 59);
            break;
    }
}

static void cdStart() {
    const uint32_t total = cdSetupTotalMs();
    if (total == 0) {
        return;
    }
    cdRemainMs = total;
    cdEndMs = millis() + total;
    cdPhase = CountdownPhase::RUNNING;
    drawCountdownApp(true);
}

static void cdPause() {
    if (cdPhase != CountdownPhase::RUNNING) {
        return;
    }
    const int32_t left = static_cast<int32_t>(cdEndMs - millis());
    cdRemainMs = left > 0 ? static_cast<uint32_t>(left) : 0;
    cdPhase = CountdownPhase::PAUSED;
    drawCountdownApp(true);
}

static void cdResume() {
    if (cdPhase != CountdownPhase::PAUSED || cdRemainMs == 0) {
        return;
    }
    cdEndMs = millis() + cdRemainMs;
    cdPhase = CountdownPhase::RUNNING;
    drawCountdownApp(true);
}

static void cdResetToSetup() {
    cdPhase = CountdownPhase::SETUP;
    cdField = 0;
    cdRemainMs = 0;
    drawCountdownApp(true);
}

void enterCountdownApp() {
    cdPhase = CountdownPhase::SETUP;
    cdHours = 0;
    cdMinutes = 5;
    cdSeconds = 0;
    cdField = 0;
    cdRemainMs = 0;
    cdScreenReady = false;
    drawCountdownApp(true);
}

void updateCountdownApp() {
    if (cdPhase == CountdownPhase::RUNNING) {
        const int32_t left = static_cast<int32_t>(cdEndMs - millis());
        if (left <= 0) {
            cdRemainMs = 0;
            cdPhase = CountdownPhase::FINISHED;
            M5Cardputer.Speaker.tone(880, 400);
            drawCountdownApp(true);
            return;
        }

        static uint32_t last_tick_ms = 0;
        if (millis() - last_tick_ms >= 200) {
            last_tick_ms = millis();
            drawCountdownApp(false);
        }
    }
}

void handleCountdownApp(const String& key) {
    if (key == "[") {
        if (cdPhase == CountdownPhase::SETUP) {
            cdField = (cdField + 2) % 3;
            drawCountdownApp(false);
        }
    } else if (key == "]") {
        if (cdPhase == CountdownPhase::SETUP) {
            cdField = (cdField + 1) % 3;
            drawCountdownApp(false);
        }
    } else if (key == "," || key == ";") {
        cdAdjustField(-1);
        drawCountdownApp(false);
    } else if (key == "." || key == "/") {
        cdAdjustField(1);
        drawCountdownApp(false);
    } else if (key == "o") {
        if (cdPhase == CountdownPhase::SETUP || cdPhase == CountdownPhase::FINISHED) {
            if (cdPhase == CountdownPhase::FINISHED) {
                cdHours = 0;
                cdMinutes = 5;
                cdSeconds = 0;
                cdPhase = CountdownPhase::SETUP;
            }
            cdStart();
        } else if (cdPhase == CountdownPhase::PAUSED) {
            cdResume();
        }
    } else if (key == "f") {
        cdPause();
    } else if (key == "r") {
        cdResetToSetup();
    }
}
