#pragma once

// 全局 WiFi STA 是否已连接
bool isWifiStaConnected();

// 已连接时返回 RSSI，否则 0
int getWifiStaRssi();

// BLE 栈是否已启动
bool isBleStackReady();

// BLE 是否有客户端连接
bool isBleConnected();

// 已连接客户端数量（未启动时返回 0）
int getBleConnectedCount();

// BLE 是否在广播
bool isBleAdvertising();

// 启动 BLE 广播
void startBleStack();

// 关闭 BLE 并释放栈
void stopBleStack();

// 兼容旧调用
void ensureBleStack();
