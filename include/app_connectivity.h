#pragma once

// 全局 WiFi STA 是否已连接
bool isWifiStaConnected();

// 已连接时返回 RSSI，否则 0
int getWifiStaRssi();

// BLE 栈是否已启动
bool isBleStackReady();

// BLE 是否有客户端连接
bool isBleConnected();

// 启动 BLE 广播（BLE 应用调用）
void ensureBleStack();
