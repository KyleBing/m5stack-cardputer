#pragma once

#include "mijia_control.h"

// LittleFS 中设备 PNG 图标目录（由构建脚本从 assets/img 复制）
static constexpr const char* DEVICE_ICON_DIR = "/img";

// Mijia 设备类型 → PNG 路径；无匹配返回 nullptr
const char* deviceIconPathForKind(MijiaDevKind kind);

// 从 LittleFS 绘制 PNG，缩放到 size×size；成功返回 true
bool drawDevicePngIcon(MijiaDevKind kind, int x, int y, int size);

// 按路径绘制（Icons 展示页用）
bool drawDevicePngFile(const char* path, int x, int y, int size);

// 设备 PNG 资源是否已在 LittleFS 中
bool deviceIconsAvailable();
