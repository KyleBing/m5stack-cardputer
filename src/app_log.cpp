#include "app_log.h"
#include "app_colors.h"
#include "app_common.h"
#include "app_header.h"
#include "M5Cardputer.h"
#include <FS.h>
#include <LittleFS.h>
#include <cctype>
#include <cstdio>
#include <cstring>

static constexpr const char* LOG_PATH_ERR = "/cursor.err"; // 错误专用，重启后优先看这个
static constexpr const char* LOG_PATH_FULL = "/cursor.log";
static constexpr size_t LOG_BUF_MAX = 12288;
static constexpr int LOG_LINE_MAX = 400;   // 源日志行
static constexpr int LOG_DISP_MAX = 800;   // 换行后显示行
static constexpr int LOG_HINT_H = 12;
static constexpr int LOG_ROW_H = 10; // 1x 字体 8px + 2px 间距
static constexpr uint16_t LOG_COLOR_TIME = APP_COLOR_LABEL; // 时间戳青色
static constexpr uint16_t LOG_COLOR_BODY = APP_COLOR_VALUE;

static char g_buf[LOG_BUF_MAX + 1];
static uint16_t g_line_off[LOG_LINE_MAX];
static int g_line_count = 0;

// 换行后的显示行：指向源行某一段
struct LogDispRow {
    uint16_t src; // 源行下标
    uint16_t off; // 源行内字节偏移
    uint8_t len;  // 本段字符数
};

static LogDispRow g_disp[LOG_DISP_MAX];
static int g_disp_count = 0;
static int g_page = 0;
static bool g_help_visible = false;
static bool g_loaded = false;
static bool g_empty = true;
static bool g_view_err = true; // 默认 Err；f 切换完整 log

static int logContentHeight() {
    return M5Cardputer.Display.height() - APP_CONTENT_Y - LOG_HINT_H - 2;
}

static int logRowsPerPage() {
    const int per = logContentHeight() / LOG_ROW_H;
    return per > 0 ? per : 1;
}

static int logPageCount() {
    if (g_disp_count <= 0) {
        return 1;
    }
    const int per = logRowsPerPage();
    return (g_disp_count + per - 1) / per;
}

// 是否为 `YYYY-MM-DD HH:MM:SS` 格式
static bool logHasDateTime(const char* line) {
    return line != nullptr && line[0] >= '0' && line[0] <= '9' && strlen(line) >= 19 &&
           line[4] == '-' && line[7] == '-' && line[10] == ' ' && line[13] == ':' &&
           line[16] == ':';
}

// 解析行首时间戳长度：`YYYY-MM-DD HH:MM:SS ` 或 `ms=123 `
static int logTimestampLen(const char* line) {
    if (line == nullptr || line[0] == '\0') {
        return 0;
    }
    if (logHasDateTime(line)) {
        int len = 19;
        if (line[19] == ' ') {
            len = 20;
        }
        return len;
    }
    if (strncmp(line, "ms=", 3) == 0) {
        int i = 3;
        while (line[i] != '\0' && isdigit(static_cast<unsigned char>(line[i]))) {
            i++;
        }
        if (line[i] == ' ') {
            i++;
        }
        return i;
    }
    return 0;
}

// 在 max_w 宽度内能放下几个字符（至少推进 1，避免死循环）
static int logCharsFit(const char* s, const int max_w) {
    if (s == nullptr || s[0] == '\0' || max_w <= 0) {
        return 0;
    }
    int w = 0;
    int n = 0;
    char ch[2] = {0, 0};
    while (s[n] != '\0') {
        ch[0] = s[n];
        const int cw = M5Cardputer.Display.textWidth(ch);
        if (w + cw > max_w) {
            break;
        }
        w += cw;
        n++;
    }
    if (n == 0) {
        return 1;
    }
    return n;
}

static void emitDispSeg(const int src, const int off, const int len) {
    if (g_disp_count >= LOG_DISP_MAX || len < 0) {
        return;
    }
    g_disp[g_disp_count++] = {static_cast<uint16_t>(src), static_cast<uint16_t>(off),
                              static_cast<uint8_t>(len > 255 ? 255 : len)};
}

// 按当前字宽把源行拆成显示行；日期（YYYY-MM-DD）独占一行
static void rebuildDispRows() {
    g_disp_count = 0;
    M5Cardputer.Display.setFont(nullptr);
    M5Cardputer.Display.setTextSize(1);
    const int max_w = M5Cardputer.Display.width() - APP_CONTENT_X * 2;
    char last_date[11] = ""; // 连续相同日期只显示一次

    for (int i = 0; i < g_line_count && g_disp_count < LOG_DISP_MAX; i++) {
        const char* line = g_buf + g_line_off[i];
        const int total = static_cast<int>(strlen(line));
        if (total == 0) {
            emitDispSeg(i, 0, 0);
            continue;
        }

        int off = 0;
        if (logHasDateTime(line)) {
            // 日期独占一行；同日连续条目不重复画日期
            char date[11];
            memcpy(date, line, 10);
            date[10] = '\0';
            if (strcmp(date, last_date) != 0) {
                emitDispSeg(i, 0, 10);
                memcpy(last_date, date, 11);
            }
            // 从时刻开始换行展示（跳过日期后的空格）
            off = 11;
            if (off >= total) {
                continue;
            }
        } else {
            last_date[0] = '\0';
        }

        while (off < total && g_disp_count < LOG_DISP_MAX) {
            const int fit = logCharsFit(line + off, max_w);
            emitDispSeg(i, off, fit);
            off += fit;
        }
    }
}

static const char* currentLogPath() {
    return g_view_err ? LOG_PATH_ERR : LOG_PATH_FULL;
}

// 从 LittleFS 加载日志并拆行
static void loadLogFile() {
    g_line_count = 0;
    g_disp_count = 0;
    g_page = 0;
    g_buf[0] = '\0';
    g_loaded = true;
    g_empty = true;

    const char* path = currentLogPath();
    if (!LittleFS.begin(false) || !LittleFS.exists(path)) {
        return;
    }

    File f = LittleFS.open(path, "r");
    if (!f) {
        return;
    }
    size_t n = f.readBytes(g_buf, LOG_BUF_MAX);
    f.close();
    if (n == 0) {
        g_buf[0] = '\0';
        return;
    }
    if (n > LOG_BUF_MAX) {
        n = LOG_BUF_MAX;
    }
    g_buf[n] = '\0';
    g_empty = false;

    g_line_off[0] = 0;
    g_line_count = 1;
    for (size_t i = 0; i < n; i++) {
        if (g_buf[i] != '\n') {
            continue;
        }
        g_buf[i] = '\0';
        if (i + 1 < n && g_line_count < LOG_LINE_MAX) {
            g_line_off[g_line_count++] = static_cast<uint16_t>(i + 1);
        }
    }
    while (g_line_count > 0) {
        const char* line = g_buf + g_line_off[g_line_count - 1];
        if (line[0] != '\0') {
            break;
        }
        g_line_count--;
    }
    if (g_line_count == 0) {
        g_empty = true;
        return;
    }
    rebuildDispRows();
}

static int drawLogHelpColHeader(const int x, const int y, const int w, const char* title) {
    M5Cardputer.Display.fillRect(x, y, w, 11, APP_COLOR_LABEL);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(BLACK, APP_COLOR_LABEL);
    M5Cardputer.Display.setCursor(x + 2, y + 1);
    M5Cardputer.Display.print(title);
    return y + 13;
}

static int drawLogHelpKey(const int x, const int y, const char key, const char* text) {
    const int cx = x + drawKeyBadge(x, y, key, 1);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
    M5Cardputer.Display.setCursor(cx, y);
    M5Cardputer.Display.print(text);
    return y + 11;
}

static int drawLogHelpBadge(const int x, const int y, const char* badge, const char* text) {
    const int cx = x + drawTextBadge(x, y, badge, 1);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
    M5Cardputer.Display.setCursor(cx, y);
    M5Cardputer.Display.print(text);
    return y + 11;
}

static int drawLogHelpArrows(const int x, const int y, const char* text) {
    const int cx = x + drawArrowBadge(x, y, 1);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
    M5Cardputer.Display.setCursor(cx, y);
    M5Cardputer.Display.print(text);
    return y + 11;
}

static int drawLogHelpText(const int x, const int y, const char* text) {
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
    M5Cardputer.Display.setCursor(x, y);
    M5Cardputer.Display.print(text);
    return y + 11;
}

static void drawLogHelpPage() {
    beginAppScreen("Help");
    constexpr int col_gap = 4;
    const int screen_w = M5Cardputer.Display.width();
    const int col_w = (screen_w - col_gap) / 2;
    const int manual_x = col_w + col_gap;
    const int col_y = APP_CONTENT_Y_NO_TAP_TO_HEADER;
    M5Cardputer.Display.drawFastVLine(col_w + col_gap / 2, col_y,
                                     M5Cardputer.Display.height() - col_y, DARKGREY);

    int y = drawLogHelpColHeader(0, col_y, col_w, "keymap");
    y = drawLogHelpBadge(2, y, "[ ]", "page");
    y = drawLogHelpArrows(2, y, "page");
    y = drawLogHelpKey(2, y, 'r', "reload");
    y = drawLogHelpKey(2, y, 'f', "err/full");
    y = drawLogHelpKey(2, y, 'h', "help");

    y = drawLogHelpColHeader(manual_x, col_y, screen_w - manual_x, "manual");
    y = drawLogHelpText(manual_x + 2, y, "survives reboot");
    y = drawLogHelpText(manual_x + 2, y, "Err: /cursor.err");
    y = drawLogHelpText(manual_x + 2, y, "Full: /cursor.log");
    y = drawLogHelpText(manual_x + 2, y, "web: /cursor-err");
    y = drawLogHelpText(manual_x + 2, y, "entry: Fn+i");

    drawHelpHintRight("close");
    updateAppHeaderStatus();
}

static void drawLogHints() {
    const int hint_y = M5Cardputer.Display.height() - LOG_HINT_H;
    M5Cardputer.Display.fillRect(APP_CONTENT_X, hint_y,
                                 M5Cardputer.Display.width() - APP_CONTENT_X * 2, LOG_HINT_H, BLACK);

    int cx = APP_CONTENT_X;
    cx += drawTextBadge(cx, hint_y, "[ ]", 1);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
    M5Cardputer.Display.setCursor(cx, hint_y);
    M5Cardputer.Display.print("page ");
    cx = M5Cardputer.Display.getCursorX();
    cx += drawKeyBadge(cx, hint_y, 'f', 1);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
    M5Cardputer.Display.setCursor(cx, hint_y);
    M5Cardputer.Display.print(g_view_err ? "full " : "err ");
    cx = M5Cardputer.Display.getCursorX();
    cx += drawKeyBadge(cx, hint_y, 'r', 1);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
    M5Cardputer.Display.setCursor(cx, hint_y);
    M5Cardputer.Display.print("reload");

    drawHelpHintRight("help");
}

// 画一段显示行：日期整行青色；时刻行时刻青色+正文白；其它正文白
static void drawDispRow(const int x, const int y, const LogDispRow& row) {
    const char* line = g_buf + g_line_off[row.src];
    char seg[128];
    const int n = row.len < sizeof(seg) - 1 ? row.len : static_cast<int>(sizeof(seg) - 1);
    memcpy(seg, line + row.off, n);
    seg[n] = '\0';

    M5Cardputer.Display.setFont(nullptr);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setCursor(x, y);

    // 日期独占行：YYYY-MM-DD
    if (row.off == 0 && row.len == 10 && logHasDateTime(line)) {
        M5Cardputer.Display.setTextColor(LOG_COLOR_TIME, BLACK);
        M5Cardputer.Display.print(seg);
        return;
    }

    // 日期后的第一段：HH:MM:SS + 正文
    if (row.off == 11 && logHasDateTime(line)) {
        const int time_len = (n >= 8) ? 8 : n; // HH:MM:SS
        char time_buf[12];
        memcpy(time_buf, seg, time_len);
        time_buf[time_len] = '\0';
        M5Cardputer.Display.setTextColor(LOG_COLOR_TIME, BLACK);
        M5Cardputer.Display.print(time_buf);
        if (n > time_len) {
            M5Cardputer.Display.setTextColor(LOG_COLOR_BODY, BLACK);
            M5Cardputer.Display.print(seg + time_len);
        }
        return;
    }

    // ms= 行：时间戳青色
    const int ts = logTimestampLen(line);
    if (row.off == 0 && ts > 0) {
        const int ts_draw = ts <= n ? ts : n;
        char time_buf[32];
        memcpy(time_buf, seg, ts_draw);
        time_buf[ts_draw] = '\0';
        M5Cardputer.Display.setTextColor(LOG_COLOR_TIME, BLACK);
        M5Cardputer.Display.print(time_buf);
        if (n > ts_draw) {
            M5Cardputer.Display.setTextColor(LOG_COLOR_BODY, BLACK);
            M5Cardputer.Display.print(seg + ts_draw);
        }
        return;
    }

    M5Cardputer.Display.setTextColor(LOG_COLOR_BODY, BLACK);
    M5Cardputer.Display.print(seg);
}

static void drawLogContent() {
    char accent[16];
    const int pages = logPageCount();
    if (g_page >= pages) {
        g_page = pages - 1;
    }
    if (g_page < 0) {
        g_page = 0;
    }
    snprintf(accent, sizeof(accent), "%d/%d", g_page + 1, pages);
    // Err = 崩溃后优先看的错误轨；Full = 完整诊断
    beginAppScreenAccent(g_view_err ? "Err " : "Log ", accent, APP_COLOR_LABEL);

    int y = APP_CONTENT_Y;
    M5Cardputer.Display.setFont(nullptr);
    M5Cardputer.Display.setTextSize(1);

    if (!g_loaded || g_empty) {
        M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
        M5Cardputer.Display.setCursor(APP_CONTENT_X, y);
        if (g_view_err) {
            M5Cardputer.Display.print("(empty err) press f for full log");
        } else {
            M5Cardputer.Display.print("(empty) open Cursor App to generate");
        }
        drawLogHints();
        updateAppHeaderStatus();
        return;
    }

    const int per = logRowsPerPage();
    const int start = g_page * per;
    const int end = start + per < g_disp_count ? start + per : g_disp_count;
    const int y_max = APP_CONTENT_Y + logContentHeight();

    for (int i = start; i < end && y + LOG_ROW_H <= y_max + 2; i++) {
        drawDispRow(APP_CONTENT_X, y, g_disp[i]);
        y += LOG_ROW_H;
    }

    drawLogHints();
    updateAppHeaderStatus();
}

static void logPageNav(const int delta) {
    if (delta == 0) {
        return;
    }
    const int pages = logPageCount();
    g_page = (g_page + delta + pages) % pages;
    drawLogContent();
}

void enterLogApp() {
    g_help_visible = false;
    // 有错误轨则默认 Err，否则退回完整 log
    g_view_err = true;
    if (LittleFS.begin(false) && !LittleFS.exists(LOG_PATH_ERR)) {
        g_view_err = false;
    }
    loadLogFile();
    drawLogContent();
}

void handleLogApp(const Keyboard_Class::KeysState& status) {
    const String key = getPressedKey();

    if (g_help_visible) {
        if (key == "h") {
            g_help_visible = false;
            drawLogContent();
        }
        return;
    }

    if (key == "h") {
        g_help_visible = true;
        drawLogHelpPage();
        return;
    }

    if (key == "f") {
        // 切换 Err / 完整 log（崩溃后不用开 Config）
        g_view_err = !g_view_err;
        loadLogFile();
        drawLogContent();
        return;
    }

    if (key == "r") {
        loadLogFile();
        drawLogContent();
        return;
    }

    if (key == "[") {
        logPageNav(-1);
        return;
    }
    if (key == "]") {
        logPageNav(1);
        return;
    }

    const int delta = getMenuNavDelta(status);
    if (delta != 0) {
        logPageNav(delta);
    }
}
