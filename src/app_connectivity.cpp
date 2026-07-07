#include "app_connectivity.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <WiFi.h>

static bool g_ble_ready = false;
static BLEServer* g_ble_server = nullptr;

class AppBleServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* server) override {
        (void)server;
    }

    void onDisconnect(BLEServer* server) override {
        (void)server;
        BLEDevice::startAdvertising();
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

// 启动 BLE 并广播，供 header 显示连接状态
void ensureBleStack() {
    if (g_ble_ready) {
        return;
    }

    BLEDevice::init("Cardputer");
    g_ble_server = BLEDevice::createServer();
    g_ble_server->setCallbacks(new AppBleServerCallbacks());

    BLEService* service = g_ble_server->createService("0000ffe0-0000-1000-8000-00805f9b34fb");
    service->createCharacteristic("0000ffe1-0000-1000-8000-00805f9b34fb",
                                  BLECharacteristic::PROPERTY_READ);
    service->start();

    BLEAdvertising* advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(service->getUUID());
    advertising->start();
    g_ble_ready = true;
}
