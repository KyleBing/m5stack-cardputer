#include "app_connectivity.h"
#include "app_config.h"
#include "app_header.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <WiFi.h>

static bool g_ble_ready = false;
static bool g_ble_advertising = false;
static bool g_ble_initialized = false;
static bool g_ble_scan_busy = false;
static bool g_ble_adv_before_scan = false;
static bool g_ble_parked = false;  // 已 init 但 App 释放，Header 不当成在用
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

void claimStaWifi() {
    // 无延迟关射频后无需取消计时；保留空实现兼容调用点
}

bool ensureStaWifi(const uint32_t timeout_ms) {
    const AppConfig& cfg = getAppConfig();
    if (!cfg.loaded || cfg.wifi_ssid[0] == '\0') {
        return false;
    }

    if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == cfg.wifi_ssid) {
        return true;
    }

    // 已连其它 SSID 时才断开，避免无谓 WIFI_OFF 硬重启
    if (WiFi.status() == WL_CONNECTED) {
        WiFi.disconnect(true);
        delay(50);
    }

    WiFi.mode(WIFI_STA);
    applyWifiRadioSleepPolicy();
    WiFi.begin(cfg.wifi_ssid, cfg.wifi_password);

    const uint32_t deadline = millis() + timeout_ms;
    while (WiFi.status() != WL_CONNECTED && static_cast<int32_t>(millis() - deadline) < 0) {
        delay(200);
    }
    return WiFi.status() == WL_CONNECTED;
}

void releaseStaWifi() {
    forceShutdownStaWifi();
}

void forceShutdownStaWifi() {
    if (WiFi.getMode() != WIFI_OFF || WiFi.status() == WL_CONNECTED) {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
    }
    updateAppHeaderStatus();
}

void updateStaWifiIdle() {
    // 已无延迟关射频
}

void applyWifiRadioSleepPolicy() {
    // BT 一旦 init，ESP-IDF 4.x 要求 WiFi modem sleep 保持开启
    WiFi.setSleep(g_ble_initialized);
}

bool isBleStackReady() {
    if (g_ble_parked) {
        return false;
    }
    // HID Keyboard 等会独自 init BLE，不走本模块的 g_ble_ready
    return g_ble_ready || BLEDevice::getInitialized();
}

void markBleStackParked() {
    g_ble_parked = true;
    g_ble_ready = false;
    g_ble_advertising = false;
}

void clearBleStackParked() {
    g_ble_parked = false;
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

// 完全释放 BLE，之后其它模块可重新 init
void resetBleStackFully() {
    if (!g_ble_initialized && !BLEDevice::getInitialized()) {
        g_ble_parked = false;
        return;
    }
    if (BLEDevice::getInitialized()) {
        BLEDevice::stopAdvertising();
        BLEDevice::deinit(false);
    }
    g_ble_initialized = false;
    g_ble_ready = false;
    g_ble_advertising = false;
    g_ble_server = nullptr;
    g_ble_scan_busy = false;
    g_ble_parked = false;
    applyWifiRadioSleepPolicy();
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
