#pragma once

#include <cstdint>

// RSSI(dBm) → 信号格数 0-4
int signalLevelFromRssi(int rssi);

// 绘制信号强度条（4 格天线图标，左低右高）
void drawSignalBars(int x, int y, int rssi, uint16_t color = 0xFFFF);
