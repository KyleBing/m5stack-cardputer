#pragma once

#include "app_config.h"
#include "mijia_control.h"
#include <cstdint>

static constexpr int MIJIA_ICON_W = 16;
static constexpr int MIJIA_ICON_H = 16;
static constexpr int MIJIA_TAG_H = 12;

// 按设备类型绘制 16x16 简笔图标
void drawMijiaDeviceIcon(MijiaDevKind kind, int x, int y, uint16_t color);

// 圆角 tag，active 时高亮；返回占用宽度（含间距）
int drawMijiaStatusTag(int x, int y, const char* text, bool active, uint16_t active_bg);

// 百分比进度条（0-100）
void drawMijiaPercentBar(int x, int y, int w, int h, int percent, uint16_t fill_color);

// 分段档位条（level 1..max_level，0 表示全灭）
void drawMijiaLevelSegments(int x, int y, int w, int h, int level, int max_level,
                            uint16_t fill_color);

// 设备名 + 分页；返回下一行 y
int drawMijiaDeviceHeader(const MijiaDevice* dev, MijiaDevKind kind, int device_idx,
                          int device_count, int x, int y);

// ON/OFF 双 tag
void drawMijiaPowerTags(int x, int y, bool known, bool on);

// 按设备类型绘制控制区；返回下一行 y
int drawMijiaDeviceControls(const MijiaDevice* dev, MijiaDevKind kind, const MijiaUiState& ui,
                            int x, int y);
