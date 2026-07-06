#pragma once

// 米家设备条目（miIO：UDP ip + token）
struct MijiaDevice {
    char name[32];
    char ip[16];
    char token[33];
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

const AppConfig& getAppConfig();
