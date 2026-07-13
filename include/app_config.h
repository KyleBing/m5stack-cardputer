#pragma once

#include <Arduino.h>

// 米家设备条目：WiFi miIO 用 ip+token；BLE 传感器用 mac+ble_key
struct MijiaDevice {
    char name[32];
    char name_zh[48]; // 中文显示名（优先于 name）
    char id[48];      // blt. 设备 id 较长
    char mac[18];
    char ip[16];
    char token[33];
    char model[48];
    char ble_key[33]; // 32 hex bindkey；空表示非 BLE
};

static constexpr int MIJIA_DEVICE_MAX = 50;

static constexpr int CURSOR_TOKEN_MAX = 1024;

struct AppConfig {
    char wifi_ssid[33];
    char wifi_password[65];
    char cursor_token[CURSOR_TOKEN_MAX];
    uint8_t brightness;
    bool time_key_sound; // Time 内按键声（countdown 到点闹钟不受影响）
    MijiaDevice devices[MIJIA_DEVICE_MAX];
    int device_count;
    bool loaded;
};

// 挂载 LittleFS（不自动格式化）
bool initAppConfigFs();

// 从 /config.json 加载；文件不存在或解析失败返回 false
bool loadAppConfig();

// 保存 JSON 到 /config.json 并重新加载
bool saveAppConfigJson(const char* json);

// 更新 WiFi 字段并写回（保留 devices 等其它配置）
bool saveAppConfigWifi(const char* ssid, const char* password);

// 更新屏幕亮度并写回
bool saveAppConfigBrightness(uint8_t brightness);

// 更新 Time 按键声开关并写回
bool saveAppConfigTimeKeySound(bool enabled);

// 读取原始 config.json 文本（用于 Web 展示）
bool readAppConfigRaw(String& out);

const AppConfig& getAppConfig();

// 显示名：优先 name_zh，否则 name
const char* mijiaDeviceDisplayName(const MijiaDevice& dev);

// 是否走 BLE 被动读取（有 ble_key 且无可用局域网 miIO）
bool mijiaDeviceUsesBle(const MijiaDevice& dev);
