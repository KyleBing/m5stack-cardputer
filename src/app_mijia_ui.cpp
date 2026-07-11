#include "app_mijia_ui.h"
#include "app_colors.h"
#include "app_common.h"
#include "app_header.h"
#include "app_device_icons.h"
#include "M5Cardputer.h"
#include <cstring>

// 16x16 基准坐标按倍数缩放
static int mijiaIconS(const int v, const int scale) {
    return (v * MIJIA_ICON_BASE * scale + MIJIA_ICON_BASE / 2) / MIJIA_ICON_BASE;
}

// 绘制灯泡图标
static void drawMijiaIconLight(const int x, const int y, const int scale, const uint16_t color) {
    const int cx = x + mijiaIconS(8, scale);
    const int cy = y + mijiaIconS(6, scale);
    const int r = mijiaIconS(5, scale);
    M5Cardputer.Display.drawCircle(cx, cy, r, color);
    M5Cardputer.Display.drawFastHLine(x + mijiaIconS(6, scale), y + mijiaIconS(11, scale),
                                      mijiaIconS(5, scale), color);
    M5Cardputer.Display.fillRect(x + mijiaIconS(5, scale), y + mijiaIconS(12, scale),
                                 mijiaIconS(7, scale), mijiaIconS(2, scale), color);
    M5Cardputer.Display.drawFastHLine(x + mijiaIconS(6, scale), y + mijiaIconS(15, scale),
                                      mijiaIconS(5, scale), color);

    // 与灯泡圆同心的左侧 1/6 圆弧（半径为原圆 2/3）
    const int arc_r = r * 2 / 3;
    if (arc_r > 0) {
        constexpr float k_left_arc_span = 360.0f / 6.0f;
        M5Cardputer.Display.drawArc(cx, cy, arc_r - 1, arc_r, 180.0f - k_left_arc_span / 2.0f,
                                    180.0f + k_left_arc_span / 2.0f, color);
    }
}

// 绘制风扇图标
static void drawMijiaIconFan(const int x, const int y, const int scale, const uint16_t color) {
    const int cx = x + mijiaIconS(8, scale);
    const int cy = y + mijiaIconS(8, scale);
    const int r = mijiaIconS(7, scale);
    M5Cardputer.Display.drawCircle(cx, cy, r, color);
    M5Cardputer.Display.fillTriangle(cx, cy - mijiaIconS(1, scale), cx - mijiaIconS(2, scale),
                                     y + mijiaIconS(2, scale), cx + mijiaIconS(2, scale),
                                     y + mijiaIconS(3, scale), color);
    M5Cardputer.Display.fillTriangle(cx + mijiaIconS(1, scale), cy, x + mijiaIconS(13, scale),
                                     cy - mijiaIconS(2, scale), x + mijiaIconS(14, scale),
                                     cy + mijiaIconS(2, scale), color);
    M5Cardputer.Display.fillTriangle(cx, cy + mijiaIconS(1, scale), cx + mijiaIconS(2, scale),
                                     y + mijiaIconS(14, scale), cx - mijiaIconS(2, scale),
                                     y + mijiaIconS(13, scale), color);
    M5Cardputer.Display.fillTriangle(cx - mijiaIconS(1, scale), cy, x + mijiaIconS(2, scale),
                                     cy + mijiaIconS(2, scale), x + mijiaIconS(3, scale),
                                     cy - mijiaIconS(2, scale), color);
    const int hub = mijiaIconS(2, scale);
    M5Cardputer.Display.fillCircle(cx, cy, hub, BLACK);
    M5Cardputer.Display.drawCircle(cx, cy, hub, color);
}

// 绘制净化器图标
static void drawMijiaIconPurifier(const int x, const int y, const int scale, const uint16_t color) {
    M5Cardputer.Display.drawRoundRect(x + mijiaIconS(3, scale), y + mijiaIconS(1, scale),
                                      mijiaIconS(10, scale), mijiaIconS(14, scale), mijiaIconS(2, scale),
                                      color);
    for (int i = 0; i < 3; i++) {
        const int ly = y + mijiaIconS(4, scale) + i * mijiaIconS(4, scale);
        M5Cardputer.Display.drawFastHLine(x + mijiaIconS(5, scale), ly, mijiaIconS(6, scale), color);
    }
    M5Cardputer.Display.fillRect(x + mijiaIconS(6, scale), y + mijiaIconS(13, scale),
                                 mijiaIconS(4, scale), mijiaIconS(2, scale), color);
}

// 绘制空气炸锅图标
static void drawMijiaIconAirFryer(const int x, const int y, const int scale, const uint16_t color) {
    M5Cardputer.Display.drawRoundRect(x + mijiaIconS(3, scale), y + mijiaIconS(4, scale),
                                      mijiaIconS(10, scale), mijiaIconS(10, scale),
                                      mijiaIconS(2, scale), color);
    M5Cardputer.Display.fillRect(x + mijiaIconS(5, scale), y + mijiaIconS(2, scale),
                                 mijiaIconS(6, scale), mijiaIconS(3, scale), color);
    M5Cardputer.Display.drawFastHLine(x + mijiaIconS(5, scale), y + mijiaIconS(7, scale),
                                      mijiaIconS(6, scale), color);
    M5Cardputer.Display.fillRect(x + mijiaIconS(6, scale), y + mijiaIconS(10, scale),
                                 mijiaIconS(4, scale), mijiaIconS(2, scale), color);
    M5Cardputer.Display.fillRect(x + mijiaIconS(13, scale), y + mijiaIconS(7, scale),
                                 mijiaIconS(2, scale), mijiaIconS(4, scale), color);
}

// 绘制插座图标
static void drawMijiaIconPlug(const int x, const int y, const int scale, const uint16_t color) {
    M5Cardputer.Display.fillRoundRect(x + mijiaIconS(2, scale), y + mijiaIconS(6, scale),
                                      mijiaIconS(12, scale), mijiaIconS(9, scale),
                                      mijiaIconS(2, scale), color);
    M5Cardputer.Display.fillRect(x + mijiaIconS(5, scale), y + mijiaIconS(2, scale),
                                 mijiaIconS(2, scale), mijiaIconS(5, scale), color);
    M5Cardputer.Display.fillRect(x + mijiaIconS(9, scale), y + mijiaIconS(2, scale),
                                 mijiaIconS(2, scale), mijiaIconS(5, scale), color);
}

// 绘制通用设备图标
static void drawMijiaIconGeneric(const int x, const int y, const int scale, const uint16_t color) {
    M5Cardputer.Display.drawRoundRect(x + mijiaIconS(2, scale), y + mijiaIconS(2, scale),
                                      mijiaIconS(12, scale), mijiaIconS(12, scale),
                                      mijiaIconS(2, scale), color);
    M5Cardputer.Display.fillCircle(x + mijiaIconS(8, scale), y + mijiaIconS(8, scale),
                                   mijiaIconS(2, scale), color);
}

// 矢量简笔图标（PNG 不可用时的兜底）
static void drawMijiaDeviceIconVector(const MijiaDevKind kind, const int x, const int y,
                                      const uint16_t color, const int scale) {
    switch (kind) {
        case MijiaDevKind::LIGHT:
            drawMijiaIconLight(x, y, scale, color);
            break;
        case MijiaDevKind::FAN_P5:
        case MijiaDevKind::FAN_GENERIC:
            drawMijiaIconFan(x, y, scale, color);
            break;
        case MijiaDevKind::AIR_PURIFIER_F20:
            drawMijiaIconPurifier(x, y, scale, color);
            break;
        case MijiaDevKind::AIR_FRYER:
            drawMijiaIconAirFryer(x, y, scale, color);
            break;
        case MijiaDevKind::PLUG:
            drawMijiaIconPlug(x, y, scale, color);
            break;
        default:
            drawMijiaIconGeneric(x, y, scale, color);
            break;
    }
}

void drawMijiaDeviceIcon(const MijiaDevKind kind, const int x, const int y, const uint16_t color,
                         const int scale) {
    drawMijiaDeviceIconVector(kind, x, y, color, scale);
}

void drawMijiaDeviceIconFor(const MijiaDevice* dev, const MijiaDevKind kind, const int x,
                            const int y, const uint16_t color, const bool active,
                            const int scale, const float png_scale) {
    if (drawDeviceIconForScaled(dev, x, y, active, png_scale)) {
        return;
    }
    drawMijiaDeviceIconVector(kind, x, y, color, scale);
}

// 概览列表：优先 _25w.png（1:1），失败回退矢量图标
void drawMijiaDeviceIconForList(const MijiaDevice* dev, const MijiaDevKind kind, const int x,
                                const int y, const uint16_t color, const bool active,
                                const int scale, const float png_scale) {
    if (drawDeviceIconForList(dev, x, y, active, png_scale)) {
        return;
    }
    drawMijiaDeviceIconVector(kind, x, y, color, scale);
}

int drawMijiaStatusTag(const int x, const int y, const char* text, const bool active,
                       const uint16_t active_bg, const int text_size) {
    M5Cardputer.Display.setTextSize(text_size);
    const int tw = M5Cardputer.Display.textWidth(text);
    const int pad_x = text_size == 2 ? 6 : 4;
    const int pad_y = text_size == 2 ? 2 : 1;
    const int w = tw + pad_x * 2;
    const int h = text_size == 2 ? MIJIA_TAG_H_2X : MIJIA_TAG_H;
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

// 宫格状态 tag：将 MijiaUiState 映射为短标签（ON/OFF/TMO/...）
void mijiaFormatGridStatusTag(const MijiaUiState& ui, MijiaGridStatusTag& tag) {
    tag.active = false;
    tag.bg = APP_COLOR_MUTED;
    tag.text[0] = '\0';

    const char* s = ui.status;
    if (strcmp(s, "turn on...") == 0) {
        strncpy(tag.text, "ON..", sizeof(tag.text));
        tag.active = true;
        tag.bg = APP_COLOR_OK;
        return;
    }
    if (strcmp(s, "turn off...") == 0) {
        strncpy(tag.text, "OFF..", sizeof(tag.text));
        tag.active = true;
        tag.bg = APP_COLOR_LABEL;
        return;
    }

    if (ui.power_known) {
        if (ui.power_on) {
            strncpy(tag.text, "ON", sizeof(tag.text));
            tag.active = true;
            tag.bg = APP_COLOR_OK;
        } else {
            strncpy(tag.text, "OFF", sizeof(tag.text));
            tag.active = true;
            tag.bg = APP_COLOR_LABEL;
        }
        return;
    }

    if (strcmp(s, "query...") == 0) {
        strncpy(tag.text, "...", sizeof(tag.text));
        return;
    }
    if (strcmp(s, "timeout") == 0) {
        strncpy(tag.text, "TMO", sizeof(tag.text));
        tag.bg = APP_COLOR_WARN;
        return;
    }
    if (strcmp(s, "task fail") == 0) {
        strncpy(tag.text, "FAIL", sizeof(tag.text));
        tag.bg = APP_COLOR_ERROR;
        return;
    }
    if (strcmp(s, "no device") == 0) {
        strncpy(tag.text, "N/A", sizeof(tag.text));
        return;
    }
    if (strcmp(s, "wifi fail") == 0) {
        strncpy(tag.text, "WIFI", sizeof(tag.text));
        tag.bg = APP_COLOR_WARN;
        return;
    }
    if (strcmp(s, "ready") == 0 || s[0] == '\0') {
        strncpy(tag.text, "?", sizeof(tag.text));
        return;
    }
    if (strcmp(s, "bad token") == 0) {
        strncpy(tag.text, "TOKEN", sizeof(tag.text));
        tag.bg = APP_COLOR_ERROR;
        return;
    }
    if (strcmp(s, "bad ip") == 0) {
        strncpy(tag.text, "IP", sizeof(tag.text));
        tag.bg = APP_COLOR_ERROR;
        return;
    }
    if (strcmp(s, "handshake") == 0) {
        strncpy(tag.text, "HS", sizeof(tag.text));
        tag.bg = APP_COLOR_WARN;
        return;
    }
    if (strcmp(s, "bad json") == 0) {
        strncpy(tag.text, "JSON", sizeof(tag.text));
        tag.bg = APP_COLOR_ERROR;
        return;
    }
    if (strcmp(s, "no power") == 0) {
        strncpy(tag.text, "NPWR", sizeof(tag.text));
        tag.bg = APP_COLOR_WARN;
        return;
    }
    if (strcmp(s, "no reply") == 0) {
        strncpy(tag.text, "NRPLY", sizeof(tag.text));
        tag.bg = APP_COLOR_WARN;
        return;
    }
    if (strcmp(s, "bad result") == 0) {
        strncpy(tag.text, "DATA", sizeof(tag.text));
        tag.bg = APP_COLOR_WARN;
        return;
    }
    if (strcmp(s, "tx begin") == 0 || strcmp(s, "tx end") == 0 || strcmp(s, "udp init") == 0 ||
        strcmp(s, "oom") == 0) {
        strncpy(tag.text, "NET", sizeof(tag.text));
        tag.bg = APP_COLOR_ERROR;
        return;
    }

    // 设备返回的 error.message 等未知文案，截短显示
    strncpy(tag.text, s, sizeof(tag.text) - 1);
    tag.text[sizeof(tag.text) - 1] = '\0';
    tag.bg = APP_COLOR_WARN;
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

// 进度条内置等距刻度线
void drawMijiaScaledPercentBar(const int x, const int y, const int w, const int h,
                               const int percent, const uint16_t fill_color,
                               const int tick_count) {
    drawMijiaPercentBar(x, y, w, h, percent, fill_color);
    if (tick_count < 2 || h < 6) {
        return;
    }

    const int inner_w = w - 2;
    const int tick_y0 = y + 2;
    const int tick_h = h - 4;
    for (int i = 0; i < tick_count; i++) {
        const int tx = x + 1 + inner_w * i / (tick_count - 1);
        M5Cardputer.Display.drawFastVLine(tx, tick_y0, tick_h, APP_COLOR_MUTED);
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

// 控制页左栏：原生 PNG 图标占满内容区高度并纵向居中
static void mijiaCalcPanelLayout(const int content_y, int& icon_px, int& left_w, int& content_h) {
    icon_px = DEVICE_ICON_NATIVE_PX;
    left_w = icon_px + 8;
    content_h = M5Cardputer.Display.height() - content_y;
}

// 仅设备无法读取状态时显示连接/查询状态
bool mijiaPanelShowsInlineStatus(const char* status, const bool power_known) {
    if (power_known) {
        return false;
    }
    return status != nullptr && status[0] != '\0';
}

static bool mijiaPanelStatusIsTimeout(const char* status) {
    return status != nullptr && strcmp(status, "timeout") == 0;
}

// 控制页右栏流程状态（超时时 2x + 警告色）
static void drawMijiaPanelInlineStatus(const int x, const int y, const char* status) {
    const bool timeout = mijiaPanelStatusIsTimeout(status);
    const int text_size = timeout ? 2 : MIJIA_PANEL_TEXT_SIZE;
    const uint16_t color = timeout ? APP_COLOR_WARN : APP_COLOR_HINT;
    M5Cardputer.Display.setTextSize(text_size);
    M5Cardputer.Display.setTextColor(color, BLACK);
    M5Cardputer.Display.setCursor(x, y);
    M5Cardputer.Display.print(status);
}

static int mijiaPanelInlineStatusLineH(const char* status) {
    return mijiaPanelStatusIsTimeout(status) ? INFO_LINE_H_2X : INFO_LINE_H;
}

MijiaPanelLayout calcMijiaPanelLayout(const int panel_y, const int x) {
    MijiaPanelLayout layout{};
    layout.layout_y = panel_y + MIJIA_DEVICE_NAME_TOP_MARGIN;
    mijiaCalcPanelLayout(layout.layout_y, layout.icon_px, layout.left_w, layout.content_h);
    layout.icon_x = x + (layout.left_w - layout.icon_px) / 2;
    layout.icon_y =
        layout.layout_y + (layout.content_h - layout.icon_px) / 2 - MIJIA_PANEL_ICON_UP_OFFSET;
    layout.info_x = x + layout.left_w + 6;
    const int screen_w = M5Cardputer.Display.width();
    layout.info_w = screen_w - layout.info_x - MIJIA_PANEL_RIGHT_PAD;
    layout.right_top_y = layout.layout_y + INFO_LINE_H_2X + 2;
    return layout;
}

void drawMijiaPanelIcon(const MijiaDevice* dev, const MijiaDevKind kind,
                        const MijiaPanelLayout& layout, const MijiaUiState& ui) {
    const bool icon_active = ui.power_known && ui.power_on;
    drawMijiaDeviceIconFor(dev, kind, layout.icon_x, layout.icon_y, APP_COLOR_VALUE, icon_active,
                           MIJIA_ICON_SCALE_DEFAULT);
}

void drawMijiaPanelHeader(const MijiaDevice* dev, const int device_idx, const int device_count,
                          const MijiaPanelLayout& layout) {
    char pager[12];
    snprintf(pager, sizeof(pager), "%d/%d", device_idx + 1, device_count);

    M5Cardputer.Display.setTextSize(1);
    const int pager_w = M5Cardputer.Display.textWidth(pager);
    const int content_right = M5Cardputer.Display.width() - MIJIA_PANEL_RIGHT_PAD;
    const int pager_x = content_right - pager_w;
    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
    M5Cardputer.Display.setCursor(pager_x, layout.layout_y);
    M5Cardputer.Display.print(pager);

    M5Cardputer.Display.setTextSize(MIJIA_PANEL_NAME_TEXT_SIZE);
    const int name_max_w = max(0, pager_x - layout.info_x - 6);
    M5Cardputer.Display.setTextColor(APP_COLOR_VALUE, BLACK);
    M5Cardputer.Display.setCursor(layout.info_x, layout.layout_y);
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
}

void drawMijiaPanelRightColumn(const MijiaDevice* dev, const MijiaDevKind kind,
                               const MijiaPanelLayout& layout, const MijiaUiState& ui,
                               const char* net_status) {
    constexpr int text_size = MIJIA_PANEL_TEXT_SIZE;
    int info_y = layout.right_top_y;

    if (net_status != nullptr && net_status[0] != '\0') {
        M5Cardputer.Display.setTextSize(MIJIA_PANEL_TEXT_SIZE);
        M5Cardputer.Display.setTextColor(APP_COLOR_LABEL, BLACK);
        M5Cardputer.Display.setCursor(layout.info_x, info_y);
        M5Cardputer.Display.print("网络: ");
        M5Cardputer.Display.setTextColor(APP_COLOR_VALUE, BLACK);
        M5Cardputer.Display.print(net_status);
        info_y += INFO_LINE_H;
    }
    if (mijiaPanelShowsInlineStatus(ui.status, ui.power_known)) {
        drawMijiaPanelInlineStatus(layout.info_x, info_y, ui.status);
        info_y += mijiaPanelInlineStatusLineH(ui.status);
    }
    drawMijiaDeviceControls(dev, kind, ui, layout.info_x, info_y, layout.info_w);
}

int drawMijiaDevicePanel(const MijiaDevice* dev, const MijiaDevKind kind, const int device_idx,
                         const int device_count, const MijiaUiState& ui, const int x, const int y,
                         const char* net_status) {
    const MijiaPanelLayout layout = calcMijiaPanelLayout(y, x);
    drawMijiaPanelIcon(dev, kind, layout, ui);
    drawMijiaPanelHeader(dev, device_idx, device_count, layout);
    drawMijiaPanelRightColumn(dev, kind, layout, ui, net_status);
    return max(layout.icon_y + layout.icon_px, layout.right_top_y) + 4;
}

// 流程状态小字（跟在 tag 后面）
static void drawMijiaInlineStatus(const int x, const int y, const char* status) {
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
    M5Cardputer.Display.setCursor(x, y + 1);
    M5Cardputer.Display.print(status);
}

void drawMijiaPowerTags(const int x, const int y, const bool known, const bool on,
                        const char* status, const bool inline_status, const int text_size) {
    int cx = x;
    if (!known) {
        cx += drawMijiaStatusTag(cx, y, "?", true, APP_COLOR_MUTED, text_size);
    } else {
        cx += drawMijiaStatusTag(cx, y, "ON", on, APP_COLOR_OK, text_size);
        cx += drawMijiaStatusTag(cx, y, "OFF", !on, APP_COLOR_LABEL, text_size);
    }
    if (inline_status && mijiaPanelShowsInlineStatus(status, known)) {
        drawMijiaInlineStatus(cx, y, status);
    }
}

// 说明在左、数值右对齐（上行）；下行进度条
int drawMijiaBarRow(const int x, const int y, const char* label, const char* value,
                    const int percent, const int total_w, const uint16_t fill_color) {
    constexpr int bar_h = 12;

    M5Cardputer.Display.setTextSize(MIJIA_PANEL_BAR_TEXT_SIZE);
    M5Cardputer.Display.setTextColor(APP_COLOR_LABEL, BLACK);
    M5Cardputer.Display.setCursor(x, y);
    M5Cardputer.Display.print(label);

    M5Cardputer.Display.setTextColor(INFO_VALUE_COLOR, BLACK);
    M5Cardputer.Display.drawRightString(value, x + total_w, y);

    const int bar_y = y + INFO_LINE_H_2X;
    drawMijiaPercentBar(x, bar_y, total_w, bar_h, percent, fill_color);
    return bar_y + bar_h + 5;
}

// 绘制带标签的百分比条
static int drawMijiaLabeledBar(const int x, const int y, const char* label, const int percent,
                               const uint16_t fill_color, const int bar_w) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", percent);
    return drawMijiaBarRow(x, y, label, buf, percent, bar_w, fill_color);
}

// Kelvin 映射为暖橙→冷蓝白 RGB565
static uint16_t mijiaKelvinToColor565(const int kelvin, const int min_k, const int max_k) {
    const int span = max_k - min_k;
    const int t = span > 0 ? constrain((kelvin - min_k) * 255 / span, 0, 255) : 128;
    // 暖色 2700K (255,166,87) → 冷色 6500K (214,228,255)
    const uint8_t r = static_cast<uint8_t>(255 - t * 41 / 255);
    const uint8_t g = static_cast<uint8_t>(166 + t * 62 / 255);
    const uint8_t b = static_cast<uint8_t>(87 + t * 168 / 255);
    return M5Cardputer.Display.color565(r, g, b);
}

// 色温进度条：背景随当前冷暖度变色，白色填充标示档位
static void drawMijiaColorTempPercentBar(const int x, const int y, const int w, const int h,
                                         const int percent, const int kelvin, const int min_k,
                                         const int max_k) {
    const int clamped = constrain(percent, 0, 100);
    const int inner_w = w - 2;
    const uint16_t bg = mijiaKelvinToColor565(kelvin, min_k, max_k);

    M5Cardputer.Display.drawRoundRect(x, y, w, h, 2, APP_COLOR_MUTED);
    M5Cardputer.Display.fillRoundRect(x + 1, y + 1, inner_w, h - 2, 1, bg);

    const int fill_w = max(2, inner_w * clamped / 100);
    if (clamped > 0) {
        M5Cardputer.Display.fillRoundRect(x + 1, y + 1, fill_w, h - 2, 1, WHITE);
    }
}

// 色温条：显示 Kelvin 数值
static int drawMijiaColorTempBar(const int x, const int y, const int kelvin, const int min_k,
                                 const int max_k, const int bar_w) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%dK", kelvin);
    const int pct =
        max_k > min_k ? constrain((kelvin - min_k) * 100 / (max_k - min_k), 0, 100) : 0;

    M5Cardputer.Display.setTextSize(MIJIA_PANEL_BAR_TEXT_SIZE);
    M5Cardputer.Display.setTextColor(APP_COLOR_LABEL, BLACK);
    M5Cardputer.Display.setCursor(x, y);
    M5Cardputer.Display.print("ct");

    M5Cardputer.Display.setTextColor(INFO_VALUE_COLOR, BLACK);
    M5Cardputer.Display.drawRightString(buf, x + bar_w, y);

    constexpr int bar_h = 12;
    const int bar_y = y + INFO_LINE_H_2X;
    drawMijiaColorTempPercentBar(x, bar_y, bar_w, bar_h, pct, kelvin, min_k, max_k);
    return bar_y + bar_h + 5;
}

int drawMijiaDeviceControls(const MijiaDevice* dev, const MijiaDevKind kind,
                            const MijiaUiState& ui, const int x, const int y, const int w) {
    char buf[24];
    constexpr int text_size = MIJIA_PANEL_TEXT_SIZE;

    if (!ui.extra_known) {
        return y;
    }

    switch (kind) {
        case MijiaDevKind::LIGHT: {
            int cy = y;
            if (ui.extra_known || ui.bright > 0) {
                cy = drawMijiaLabeledBar(x, cy, "bright", ui.bright, YELLOW, w);
            }
            if (dev != nullptr && mijiaLightSupportsCt(dev->model)) {
                const int ct =
                    ui.ct_known ? ui.color_temp : (ui.ct_min + ui.ct_max) / 2;
                cy = drawMijiaColorTempBar(x, cy, ct, ui.ct_min, ui.ct_max, w);
            }
            return cy;
        }

        case MijiaDevKind::FAN_P5: {
            const int cy = drawMijiaLabeledBar(x, y, "speed", ui.speed, CYAN, w);
            int cx = x;
            cx += drawMijiaStatusTag(cx, cy, ui.roll ? "roll ON" : "roll OFF", ui.roll, CYAN,
                                     text_size);
            const char* mode_text = ui.mode == 1 ? "nature" : "normal";
            drawMijiaStatusTag(cx, cy, mode_text, true, APP_COLOR_MUTED, text_size);
            return cy + MIJIA_TAG_H + 4;
        }

        case MijiaDevKind::FAN_GENERIC:
            M5Cardputer.Display.setTextSize(MIJIA_PANEL_BAR_TEXT_SIZE);
            M5Cardputer.Display.setTextColor(APP_COLOR_LABEL, BLACK);
            M5Cardputer.Display.setCursor(x, y);
            M5Cardputer.Display.println("speed:");
            drawMijiaLevelSegments(x, y + INFO_LINE_H_2X, w, 10, ui.speed, 4, CYAN);
            return y + INFO_LINE_H_2X + 14;

        case MijiaDevKind::AIR_PURIFIER_F20: {
            static const char* MODE_NAMES[] = {"auto", "sleep", "low", "med", "high", "fav"};
            const int mi = constrain(ui.mode, 0, 5);
            int cx = x;
            cx += drawMijiaStatusTag(cx, y, MODE_NAMES[mi], true, GREEN, text_size);
            snprintf(buf, sizeof(buf), "fan %d", ui.fan_level);
            cx += drawMijiaStatusTag(cx, y, buf, true, APP_COLOR_MUTED, text_size);
            snprintf(buf, sizeof(buf), "aqi %d", ui.aqi);
            drawMijiaStatusTag(cx, y, buf, true, APP_COLOR_MUTED, text_size);
            const int seg_y = y + MIJIA_TAG_H + 4;
            drawMijiaLevelSegments(x, seg_y, w, 8, ui.fan_level, 5, GREEN);
            return seg_y + 12;
        }

        case MijiaDevKind::PLUG:
        case MijiaDevKind::AIR_FRYER:
        default:
            return y;
    }
}
