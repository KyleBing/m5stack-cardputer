#include "app_connectivity.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <WiFi.h>

static bool g_ble_ready = false;
static bool g_ble_advertising = false;
static bool g_ble_initialized = false;
static bool g_ble_scan_busy = false;
static bool g_ble_adv_before_scan = false;
static BLEServer* g_ble_server = nullptr;

void initBleStackOnly(); // 前向声明：startBleStack 会调用

class AppBleServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* server) override {
        (void)server;
    }

    void onDisconnect(BLEServer* server) override {
        (void)server;
        // 仅在 BLE 功能处于开启态时恢复广播，避免关闭态被回调重新拉起
        if (g_ble_ready && g_ble_advertising) {
            BLEDevice::startAdvertising();
        }
    }
};

bool isWifiStaConnected() {
    return WiFi.getMode() == WIFI_STA && WiFi.status() == WL_CONNECTED;
}

int getWifiStaRssi() {
    if (!isWifiStaConnected()) {
        return 0;
    }
    return WiFi.RSSI();
}

void applyWifiRadioSleepPolicy() {
    // BT 一旦 init，ESP-IDF 4.x 要求 WiFi modem sleep 保持开启
    WiFi.setSleep(g_ble_initialized);
}

bool isBleStackReady() {
    return g_ble_ready;
}

bool isBleConnected() {
    if (!g_ble_ready || g_ble_server == nullptr) {
        return false;
    }
    return g_ble_server->getConnectedCount() > 0;
}

int getBleConnectedCount() {
    if (!g_ble_ready || g_ble_server == nullptr) {
        return 0;
    }
    return g_ble_server->getConnectedCount();
}

bool isBleAdvertising() {
    return g_ble_ready && g_ble_advertising;
}

// 启动 BLE 并广播
void startBleStack() {
    if (g_ble_ready) {
        return;
    }
    initBleStackOnly();
    BLEDevice::startAdvertising();
    g_ble_ready = true;
    g_ble_advertising = true;
}

// 关闭 BLE（停止广播，不反复 deinit，避免 toggle 卡死）
void stopBleStack() {
    if (!g_ble_initialized || !g_ble_ready) {
        return;
    }

    BLEDevice::stopAdvertising();
    g_ble_ready = false;
    g_ble_advertising = false;
}

void ensureBleStack() {
    startBleStack();
}

void initBleStackOnly() {
    if (!g_ble_initialized) {
        // ESP-IDF 4.x 共存层要求 WiFi modem sleep 开启，否则启用 BT 会直接 abort
        WiFi.setSleep(true);
        BLEDevice::init("Cardputer");
        g_ble_initialized = true;
    }
    if (g_ble_server != nullptr) {
        return;
    }
    g_ble_server = BLEDevice::createServer();
    g_ble_server->setCallbacks(new AppBleServerCallbacks());

    BLEService* service = g_ble_server->createService("0000ffe0-0000-1000-8000-00805f9b34fb");
    service->createCharacteristic("0000ffe1-0000-1000-8000-00805f9b34fb",
                                  BLECharacteristic::PROPERTY_READ);
    service->start();

    BLEAdvertising* advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(service->getUUID());
}

// 扫描专用：只 init，不建 Server；调用前应已尽量释放经典蓝牙 / 暂停 WiFi
void initBleCentralOnly() {
    if (g_ble_initialized) {
        return;
    }
    // ESP-IDF 4.x 共存层要求 WiFi modem sleep 开启，否则启用 BT 会直接 abort
    WiFi.setSleep(true);
    // 名称留空，减少广播侧开销
    BLEDevice::init("");
    g_ble_initialized = true;
}

bool beginBleScanSession() {
    if (g_ble_scan_busy) {
        return false;
    }
    // 尚未 init 时只用 Central（不建 GATT Server），降低内存占用
    if (!g_ble_initialized) {
        initBleCentralOnly();
    }
    g_ble_adv_before_scan = g_ble_advertising;
    if (g_ble_advertising) {
        BLEDevice::stopAdvertising();
        g_ble_advertising = false;
    }
    g_ble_scan_busy = true;
    return true;
}

void endBleScanSession(const bool restore_adv) {
    g_ble_scan_busy = false;
    if (restore_adv && g_ble_adv_before_scan) {
        BLEDevice::startAdvertising();
        g_ble_advertising = true;
        g_ble_ready = true;
    }
}

bool isBleScanBusy() {
    return g_ble_scan_busy;
}
