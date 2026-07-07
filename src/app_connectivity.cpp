#include "app_connectivity.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <WiFi.h>

static bool g_ble_ready = false;
static bool g_ble_advertising = false;
static bool g_ble_initialized = false;
static BLEServer* g_ble_server = nullptr;

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

    if (!g_ble_initialized) {
        BLEDevice::init("Cardputer");
        g_ble_server = BLEDevice::createServer();
        g_ble_server->setCallbacks(new AppBleServerCallbacks());

        BLEService* service = g_ble_server->createService("0000ffe0-0000-1000-8000-00805f9b34fb");
        service->createCharacteristic("0000ffe1-0000-1000-8000-00805f9b34fb",
                                      BLECharacteristic::PROPERTY_READ);
        service->start();

        BLEAdvertising* advertising = BLEDevice::getAdvertising();
        advertising->addServiceUUID(service->getUUID());
        g_ble_initialized = true;
    }

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
