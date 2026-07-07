#pragma once

#include "app_config.h"
#include "mijia_control.h"
#include <cstdint>

static constexpr int MIJIA_ICON_W = 16;
static constexpr int MIJIA_ICON_H = 16;
static constexpr int MIJIA_HEADER_ICON_H = 32; // 控制页设备名 2x 时配套图标边长
static constexpr int MIJIA_TAG_H = 12;
static constexpr int MIJIA_TAG_H_2X = 20;      // 2x 字号 tag 高度
static constexpr int MIJIA_PANEL_TEXT_SIZE = 1;        // 控制页右栏控制项字号
static constexpr int MIJIA_PANEL_NAME_TEXT_SIZE = 2;   // 控制页设备名字号
static constexpr int MIJIA_PANEL_POWER_SIZE = 22; // 控制页电源符号边长
static constexpr int MIJIA_PANEL_ICON_SHRINK = 8; // 控制页设备图标额外缩减边长
// 列表项高度：名称 2x + IP 1x + 型号 1x（与 app_common INFO_LINE_H 对齐）
static constexpr int MIJIA_LIST_ITEM_H = 38;
static constexpr int MIJIA_LIST_ITEM_GAP = 6;

// 按设备类型绘制简笔图标（size 为边长，默认 16）
void drawMijiaDeviceIcon(MijiaDevKind kind, int x, int y, uint16_t color, int size = MIJIA_ICON_H);

// 圆角 tag，active 时高亮；返回占用宽度（含间距）
int drawMijiaStatusTag(int x, int y, const char* text, bool active, uint16_t active_bg,
                       int text_size = 1);

// 百分比进度条（0-100）
void drawMijiaPercentBar(int x, int y, int w, int h, int percent, uint16_t fill_color);

// 带刻度线的百分比进度条（tick_count 为刻度数量，含首尾）
void drawMijiaScaledPercentBar(int x, int y, int w, int h, int percent, uint16_t fill_color,
                               int tick_count = 11);

// 分段档位条（level 1..max_level，0 表示全灭）
void drawMijiaLevelSegments(int x, int y, int w, int h, int level, int max_level,
                            uint16_t fill_color);

// 控制页主面板：左大图标 + 开关状态，右设备信息与控制；返回下一行 y
int drawMijiaDevicePanel(const MijiaDevice* dev, MijiaDevKind kind, int device_idx,
                         int device_count, const MijiaUiState& ui, int x, int y);

// ON/OFF 双 tag；inline_status 为 false 时不绘制行尾状态字
void drawMijiaPowerTags(int x, int y, bool known, bool on, const char* status,
                        bool inline_status = true, int text_size = 1);

// 按设备类型绘制控制区（右栏）；返回下一行 y
int drawMijiaDeviceControls(const MijiaDevice* dev, MijiaDevKind kind, const MijiaUiState& ui,
                            int x, int y, int w);
