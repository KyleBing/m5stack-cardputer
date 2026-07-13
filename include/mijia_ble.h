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

// 单设备聚焦扫描：解析成功即停；脏包（配对广播等）丢弃后继续听
bool mijiaBleScanStart(const MijiaDevice& dev, uint32_t scan_seconds = 30, int device_idx = -1);

// 后台多设备监听：扫满时长，命中任一 BLE 设备则上报读数（不提前停）
bool mijiaBleWatchStart(const MijiaDevice* devices, int device_count, uint32_t scan_seconds = 30);

// false=仍在扫；若 out.ok 表示本帧有新成功读数（后台可持续多次）
// true=本轮结束（超时 / 聚焦成功后收尾）
bool mijiaBleScanPoll(MijiaBleReading& out, int* device_idx = nullptr);

bool mijiaBleScanIsRunning();
void mijiaBleScanAbort();
