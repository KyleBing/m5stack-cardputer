#pragma once

#include <stdint.h>

// 全局 WiFi STA 是否已连接
bool isWifiStaConnected();

// 已连接时返回 RSSI，否则 0
int getWifiStaRssi();

// 使用 config 连接 WiFi；已连同 SSID 则复用
bool ensureStaWifi(uint32_t timeout_ms = 12000);

// 声明正在使用 WiFi（自行 WiFi.begin 前调用；兼容旧调用）
void claimStaWifi();

// 用完后立刻 disconnect + WIFI_OFF
void releaseStaWifi();

// 立刻关闭射频（休眠 / AP 配网 / 必须独占射频时）；与 releaseStaWifi 同效
void forceShutdownStaWifi();

// 兼容旧调用（已无延迟关射频逻辑）
void updateStaWifiIdle();

// 按 BLE 初始化状态配置 WiFi 休眠，避免 ESP-IDF coexist 层 abort
void applyWifiRadioSleepPolicy();

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

// 关闭 BLE（停止广播，不 deinit）
void stopBleStack();

// 完全释放 BLE 栈（deinit），供 HID 键盘等需独占协议栈的场景
void resetBleStackFully();

// 兼容旧调用
void ensureBleStack();

// 仅初始化 BLE 栈（不强制广播，供 Central 扫描）
void initBleStackOnly();

// 仅 init 协议栈（不建 GATT Server），供米家被动扫描；更省内存、更稳
void initBleCentralOnly();

// 开始扫描会话：互斥 + 暂停广播；失败返回 false
bool beginBleScanSession();

// 结束扫描会话；restore_adv 为 true 时恢复进入前的广播状态
void endBleScanSession(bool restore_adv);

// 是否有扫描会话进行中
bool isBleScanBusy();
