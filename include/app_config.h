#pragma once

#include <Arduino.h>
// 米家设备条目（miIO 控制需 ip + token，其余为识别信息）
struct MijiaDevice {
    char name[32];
    char id[16];
    char mac[18];
    char ip[16];
    char token[33];
    char model[48];
};

static constexpr int MIJIA_DEVICE_MAX = 8;

struct AppConfig {
    char wifi_ssid[33];
    char wifi_password[65];
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

// 读取原始 config.json 文本（用于 Web 展示）
bool readAppConfigRaw(String& out);

const AppConfig& getAppConfig();
