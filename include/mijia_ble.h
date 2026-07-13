#pragma once

#include "app_config.h"
#include <cstdint>

// MiBeacon / 青萍广播解析结果
struct MijiaBleReading {
    bool ok;
    bool has_temp;
    bool has_humidity;
    bool has_battery;
    bool has_motion;
    bool has_button;
    float temperature; // °C
    float humidity;    // %
    int battery;       // %
    bool motion;
    bool button;
    char message[48];
};

// 非阻塞扫描：start 后每帧 poll，禁止在调用线程里死等
bool mijiaBleScanStart(const MijiaDevice& dev, uint32_t scan_seconds = 2);
// 返回 true 表示本轮结束（成功或失败都写进 out）
bool mijiaBleScanPoll(MijiaBleReading& out);
bool mijiaBleScanIsRunning();
void mijiaBleScanAbort();
