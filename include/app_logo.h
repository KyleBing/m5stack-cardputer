#pragma once

#include <cstdint>

// 应用 Logo 设计坐标系边长
static constexpr int APP_LOGO_DESIGN_SIZE = 64;

// 在 (dest_x, dest_y) 绘制可缩放的应用 Logo
void drawAppLogo(int dest_x, int dest_y, int size = APP_LOGO_DESIGN_SIZE);
