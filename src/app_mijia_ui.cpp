#include "app_mijia_ui.h"
#include "app_colors.h"
#include "app_common.h"
#include "app_header.h"
#include "M5Cardputer.h"
#include <cstring>

// 16x16 基准坐标缩放到任意边长
static int mijiaIconS(const int v, const int size) {
    return (v * size + 8) / 16;
}

// 绘制灯泡图标
static void drawMijiaIconLight(const int x, const int y, const int size, const uint16_t color) {
    const int cx = x + mijiaIconS(8, size);
    M5Cardputer.Display.drawCircle(cx, y + mijiaIconS(6, size), mijiaIconS(5, size), color);
    M5Cardputer.Display.drawFastHLine(x + mijiaIconS(6, size), y + mijiaIconS(11, size),
                                      mijiaIconS(5, size), color);
    M5Cardputer.Display.fillRect(x + mijiaIconS(5, size), y + mijiaIconS(12, size),
                                 mijiaIconS(7, size), mijiaIconS(2, size), color);
    M5Cardputer.Display.drawFastHLine(x + mijiaIconS(6, size), y + mijiaIconS(15, size),
                                      mijiaIconS(5, size), color);
    M5Cardputer.Display.drawPixel(cx, y + mijiaIconS(4, size), color);
}

// 绘制风扇图标
static void drawMijiaIconFan(const int x, const int y, const int size, const uint16_t color) {
    const int cx = x + mijiaIconS(8, size);
    const int cy = y + mijiaIconS(8, size);
    const int r = mijiaIconS(7, size);
    M5Cardputer.Display.drawCircle(cx, cy, r, color);
    M5Cardputer.Display.fillTriangle(cx, cy - mijiaIconS(1, size), cx - mijiaIconS(2, size),
                                     y + mijiaIconS(2, size), cx + mijiaIconS(2, size),
                                     y + mijiaIconS(3, size), color);
    M5Cardputer.Display.fillTriangle(cx + mijiaIconS(1, size), cy, x + mijiaIconS(13, size),
                                     cy - mijiaIconS(2, size), x + mijiaIconS(14, size),
                                     cy + mijiaIconS(2, size), color);
    M5Cardputer.Display.fillTriangle(cx, cy + mijiaIconS(1, size), cx + mijiaIconS(2, size),
                                     y + mijiaIconS(14, size), cx - mijiaIconS(2, size),
                                     y + mijiaIconS(13, size), color);
    M5Cardputer.Display.fillTriangle(cx - mijiaIconS(1, size), cy, x + mijiaIconS(2, size),
                                     cy + mijiaIconS(2, size), x + mijiaIconS(3, size),
                                     cy - mijiaIconS(2, size), color);
    const int hub = mijiaIconS(2, size);
    M5Cardputer.Display.fillCircle(cx, cy, hub, BLACK);
    M5Cardputer.Display.drawCircle(cx, cy, hub, color);
}

// 绘制净化器图标
static void drawMijiaIconPurifier(const int x, const int y, const int size, const uint16_t color) {
    M5Cardputer.Display.drawRoundRect(x + mijiaIconS(3, size), y + mijiaIconS(1, size),
                                      mijiaIconS(10, size), mijiaIconS(14, size), mijiaIconS(2, size),
                                      color);
    for (int i = 0; i < 3; i++) {
        const int ly = y + mijiaIconS(4, size) + i * mijiaIconS(4, size);
        M5Cardputer.Display.drawFastHLine(x + mijiaIconS(5, size), ly, mijiaIconS(6, size), color);
    }
    M5Cardputer.Display.fillRect(x + mijiaIconS(6, size), y + mijiaIconS(13, size),
                                 mijiaIconS(4, size), mijiaIconS(2, size), color);
}

// 绘制空气炸锅图标
static void drawMijiaIconAirFryer(const int x, const int y, const int size, const uint16_t color) {
    M5Cardputer.Display.drawRoundRect(x + mijiaIconS(3, size), y + mijiaIconS(4, size),
                                      mijiaIconS(10, size), mijiaIconS(10, size),
                                      mijiaIconS(2, size), color);
    M5Cardputer.Display.fillRect(x + mijiaIconS(5, size), y + mijiaIconS(2, size),
                                 mijiaIconS(6, size), mijiaIconS(3, size), color);
    M5Cardputer.Display.drawFastHLine(x + mijiaIconS(5, size), y + mijiaIconS(7, size),
                                      mijiaIconS(6, size), color);
    M5Cardputer.Display.fillRect(x + mijiaIconS(6, size), y + mijiaIconS(10, size),
                                 mijiaIconS(4, size), mijiaIconS(2, size), color);
    M5Cardputer.Display.fillRect(x + mijiaIconS(13, size), y + mijiaIconS(7, size),
                                 mijiaIconS(2, size), mijiaIconS(4, size), color);
}

// 绘制插座图标
static void drawMijiaIconPlug(const int x, const int y, const int size, const uint16_t color) {
    M5Cardputer.Display.fillRoundRect(x + mijiaIconS(2, size), y + mijiaIconS(6, size),
                                      mijiaIconS(12, size), mijiaIconS(9, size),
                                      mijiaIconS(2, size), color);
    M5Cardputer.Display.fillRect(x + mijiaIconS(5, size), y + mijiaIconS(2, size),
                                 mijiaIconS(2, size), mijiaIconS(5, size), color);
    M5Cardputer.Display.fillRect(x + mijiaIconS(9, size), y + mijiaIconS(2, size),
                                 mijiaIconS(2, size), mijiaIconS(5, size), color);
}

// 绘制通用设备图标
static void drawMijiaIconGeneric(const int x, const int y, const int size, const uint16_t color) {
    M5Cardputer.Display.drawRoundRect(x + mijiaIconS(2, size), y + mijiaIconS(2, size),
                                      mijiaIconS(12, size), mijiaIconS(12, size),
                                      mijiaIconS(2, size), color);
    M5Cardputer.Display.fillCircle(x + mijiaIconS(8, size), y + mijiaIconS(8, size),
                                   mijiaIconS(2, size), color);
}

void drawMijiaDeviceIcon(const MijiaDevKind kind, const int x, const int y, const uint16_t color,
                         const int size) {
    switch (kind) {
        case MijiaDevKind::LIGHT:
            drawMijiaIconLight(x, y, size, color);
            break;
        case MijiaDevKind::FAN_P5:
        case MijiaDevKind::FAN_GENERIC:
            drawMijiaIconFan(x, y, size, color);
            break;
        case MijiaDevKind::AIR_PURIFIER_F20:
            drawMijiaIconPurifier(x, y, size, color);
            break;
        case MijiaDevKind::AIR_FRYER:
            drawMijiaIconAirFryer(x, y, size, color);
            break;
        case MijiaDevKind::PLUG:
            drawMijiaIconPlug(x, y, size, color);
            break;
        default:
            drawMijiaIconGeneric(x, y, size, color);
            break;
    }
}

int drawMijiaStatusTag(const int x, const int y, const char* text, const bool active,
                       const uint16_t active_bg) {
    M5Cardputer.Display.setTextSize(1);
    const int tw = M5Cardputer.Display.textWidth(text);
    constexpr int pad_x = 4;
    constexpr int pad_y = 1;
    const int w = tw + pad_x * 2;
    const int h = MIJIA_TAG_H;
    const uint16_t bg = active ? active_bg : BLACK;
    const uint16_t fg = active ? BLACK : APP_COLOR_HINT;
    const uint16_t border = active ? active_bg : APP_COLOR_MUTED;

    M5Cardputer.Display.fillRoundRect(x, y, w, h, 3, bg);
    M5Cardputer.Display.drawRoundRect(x, y, w, h, 3, border);
    M5Cardputer.Display.setTextColor(fg, bg);
    M5Cardputer.Display.setCursor(x + pad_x, y + pad_y + 1);
    M5Cardputer.Display.print(text);
    return w + 4;
}

void drawMijiaPercentBar(const int x, const int y, const int w, const int h, const int percent,
                         const uint16_t fill_color) {
    const int clamped = constrain(percent, 0, 100);
    const int inner_w = w - 2;

    M5Cardputer.Display.drawRoundRect(x, y, w, h, 2, APP_COLOR_MUTED);
    const int fill_w = inner_w * clamped / 100;
    if (fill_w > 0) {
        M5Cardputer.Display.fillRoundRect(x + 1, y + 1, fill_w, h - 2, 1, fill_color);
    }
}

void drawMijiaLevelSegments(const int x, const int y, const int w, const int h, const int level,
                            const int max_level, const uint16_t fill_color) {
    if (max_level <= 0) {
        return;
    }
    constexpr int gap = 2;
    const int seg_w = (w - gap * (max_level - 1)) / max_level;
    int sx = x;
    for (int i = 1; i <= max_level; i++) {
        if (i <= level) {
            M5Cardputer.Display.fillRoundRect(sx, y, seg_w, h, 2, fill_color);
        } else {
            M5Cardputer.Display.drawRoundRect(sx, y, seg_w, h, 2, APP_COLOR_MUTED);
        }
        sx += seg_w + gap;
    }
}

int drawMijiaDeviceHeader(const MijiaDevice* dev, const MijiaDevKind kind, const int device_idx,
                          const int device_count, const int x, const int y) {
    drawMijiaDeviceIcon(kind, x, y, APP_COLOR_VALUE);

    const int text_x = x + MIJIA_ICON_W + 4;
    const int screen_w = M5Cardputer.Display.width();
    char pager[12];
    snprintf(pager, sizeof(pager), "%d/%d", device_idx + 1, device_count);

    M5Cardputer.Display.setTextSize(1);
    const int pager_w = M5Cardputer.Display.textWidth(pager);
    const int pager_x = screen_w - APP_CONTENT_X - pager_w;
    const int name_max_w = pager_x - text_x - 6;

    M5Cardputer.Display.setTextColor(APP_COLOR_LABEL, BLACK);
    M5Cardputer.Display.setCursor(text_x, y + 4);
    if (dev != nullptr && dev->name[0] != '\0') {
        char name[32];
        strncpy(name, dev->name, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        while (name[0] != '\0' && M5Cardputer.Display.textWidth(name) > name_max_w) {
            name[strlen(name) - 1] = '\0';
        }
        M5Cardputer.Display.print(name);
    } else {
        M5Cardputer.Display.print("device");
    }

    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
    M5Cardputer.Display.setCursor(pager_x, y + 4);
    M5Cardputer.Display.print(pager);
    return y + MIJIA_ICON_H + 4;
}

// 仅设备无法读取状态时，在 ON/OFF 或 ? 后显示连接/查询状态
static bool mijiaShouldShowInlineStatus(const char* status, const bool power_known) {
    if (power_known) {
        return false;
    }
    return status != nullptr && status[0] != '\0';
}

// 流程状态小字（跟在 tag 后面）
static void drawMijiaInlineStatus(const int x, const int y, const char* status) {
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
    M5Cardputer.Display.setCursor(x, y + 1);
    M5Cardputer.Display.print(status);
}

void drawMijiaPowerTags(const int x, const int y, const bool known, const bool on,
                        const char* status) {
    int cx = x;
    if (!known) {
        cx += drawMijiaStatusTag(cx, y, "?", true, APP_COLOR_MUTED);
    } else {
        cx += drawMijiaStatusTag(cx, y, "ON", on, APP_COLOR_OK);
        cx += drawMijiaStatusTag(cx, y, "OFF", !on, APP_COLOR_LABEL);
    }
    if (mijiaShouldShowInlineStatus(status, known)) {
        drawMijiaInlineStatus(cx, y, status);
    }
}

// 绘制带标签的百分比条（标签 + 数值 + 条）
static int drawMijiaLabeledBar(int x, int y, const char* label, const int percent,
                               const uint16_t fill_color) {
    const int screen_w = M5Cardputer.Display.width();
    const int bar_w = screen_w - x - APP_CONTENT_X;
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", percent);

    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(APP_COLOR_LABEL, BLACK);
    M5Cardputer.Display.setCursor(x, y);
    M5Cardputer.Display.print(label);
    M5Cardputer.Display.print(": ");
    M5Cardputer.Display.setTextColor(APP_COLOR_VALUE, BLACK);
    M5Cardputer.Display.println(buf);

    y += INFO_LINE_H;
    drawMijiaPercentBar(x, y, bar_w, 10, percent, fill_color);
    return y + 14;
}

int drawMijiaDeviceControls(const MijiaDevice* dev, const MijiaDevKind kind,
                            const MijiaUiState& ui, const int x, int y) {
    const int screen_w = M5Cardputer.Display.width();
    const int bar_w = screen_w - x - APP_CONTENT_X;
    char buf[24];

    if (!ui.extra_known) {
        return y;
    }

    switch (kind) {
        case MijiaDevKind::LIGHT:
            return drawMijiaLabeledBar(x, y, "bright", ui.bright, YELLOW);

        case MijiaDevKind::FAN_P5: {
            y = drawMijiaLabeledBar(x, y, "speed", ui.speed, CYAN);
            int cx = x;
            cx += drawMijiaStatusTag(cx, y, ui.roll ? "roll ON" : "roll OFF", ui.roll, CYAN);
            const char* mode_text = ui.mode == 1 ? "nature" : "normal";
            drawMijiaStatusTag(cx, y, mode_text, true, APP_COLOR_MUTED);
            return y + MIJIA_TAG_H + 4;
        }

        case MijiaDevKind::FAN_GENERIC:
            M5Cardputer.Display.setTextSize(1);
            M5Cardputer.Display.setTextColor(APP_COLOR_LABEL, BLACK);
            M5Cardputer.Display.setCursor(x, y);
            M5Cardputer.Display.print("speed:");
            y += INFO_LINE_H;
            drawMijiaLevelSegments(x, y, bar_w, 10, ui.speed, 4, CYAN);
            return y + 14;

        case MijiaDevKind::AIR_PURIFIER_F20: {
            static const char* MODE_NAMES[] = {"auto", "sleep", "low", "med", "high", "fav"};
            const int mi = constrain(ui.mode, 0, 5);
            int cx = x;
            cx += drawMijiaStatusTag(cx, y, MODE_NAMES[mi], true, GREEN);
            snprintf(buf, sizeof(buf), "fan %d", ui.fan_level);
            cx += drawMijiaStatusTag(cx, y, buf, true, APP_COLOR_MUTED);
            snprintf(buf, sizeof(buf), "aqi %d", ui.aqi);
            drawMijiaStatusTag(cx, y, buf, true, APP_COLOR_MUTED);
            y += MIJIA_TAG_H + 4;
            drawMijiaLevelSegments(x, y, bar_w, 8, ui.fan_level, 5, GREEN);
            return y + 12;
        }

        case MijiaDevKind::PLUG:
        case MijiaDevKind::AIR_FRYER:
        default:
            return y;
    }
}
