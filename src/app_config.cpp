#include "app_config.h"
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include <cstring>

static constexpr const char* CONFIG_PATH = "/config.json";

static AppConfig g_config{};

// 安全拷贝字符串到定长缓冲区
static void copyField(char* dest, const size_t dest_size, const char* src) {
    if (src == nullptr || dest_size == 0) {
        return;
    }
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

// 粗判私网局域网 IP（用于区分云端假 IP）
static bool isPrivateLanIp(const char* ip) {
    if (ip == nullptr || ip[0] == '\0') {
        return false;
    }
    unsigned a = 0, b = 0, c = 0, d = 0;
    if (sscanf(ip, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
        return false;
    }
    if (a > 255 || b > 255 || c > 255 || d > 255) {
        return false;
    }
    if (a == 10) {
        return true;
    }
    if (a == 192 && b == 168) {
        return true;
    }
    if (a == 172 && b >= 16 && b <= 31) {
        return true;
    }
    return false;
}

bool initAppConfigFs() {
    return LittleFS.begin(false);
}

bool loadAppConfig() {
    g_config = {};
    g_config.loaded = false;
    g_config.brightness = 30;
    g_config.time_key_sound = true; // 默认开

    if (!LittleFS.exists(CONFIG_PATH)) {
        return false;
    }

    File file = LittleFS.open(CONFIG_PATH, "r");
    if (!file) {
        return false;
    }

    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, file);
    file.close();
    if (err) {
        return false;
    }

    JsonObject wifi = doc["wifi"];
    if (!wifi.isNull()) {
        copyField(g_config.wifi_ssid, sizeof(g_config.wifi_ssid), wifi["ssid"]);
        copyField(g_config.wifi_password, sizeof(g_config.wifi_password), wifi["password"]);
    }

    JsonObject cursor = doc["cursor"];
    if (!cursor.isNull()) {
        const char* token = cursor["token"];
        if (token == nullptr || token[0] == '\0') {
            token = cursor["api_key"]; // 兼容旧字段名
        }
        copyField(g_config.cursor_token, sizeof(g_config.cursor_token), token);
    }

    g_config.brightness = static_cast<uint8_t>(doc["brightness"] | 30);
    // 默认开；缺字段时保持开启
    g_config.time_key_sound = true;
    JsonObject sound = doc["sound"];
    if (!sound.isNull()) {
        g_config.time_key_sound = sound["time_key"] | true;
    }

    JsonArray devices = doc["devices"].as<JsonArray>();
    if (!devices.isNull()) {
        for (JsonObject device : devices) {
            if (g_config.device_count >= MIJIA_DEVICE_MAX) {
                break;
            }
            MijiaDevice& entry = g_config.devices[g_config.device_count];
            copyField(entry.name, sizeof(entry.name), device["name"]);
            // name_zh 优先；兼容旧字段 name_cn
            const char* name_zh = device["name_zh"];
            if (name_zh == nullptr || name_zh[0] == '\0') {
                name_zh = device["name_cn"];
            }
            copyField(entry.name_zh, sizeof(entry.name_zh), name_zh);
            copyField(entry.id, sizeof(entry.id), device["id"]);
            copyField(entry.mac, sizeof(entry.mac), device["mac"]);
            copyField(entry.ip, sizeof(entry.ip), device["ip"]);
            copyField(entry.token, sizeof(entry.token), device["token"]);
            copyField(entry.model, sizeof(entry.model), device["model"]);
            // ble.key 优先；兼容顶层 ble_key
            const char* ble_key = nullptr;
            JsonObject ble = device["ble"].as<JsonObject>();
            if (!ble.isNull()) {
                ble_key = ble["key"];
            }
            if (ble_key == nullptr || ble_key[0] == '\0') {
                ble_key = device["ble_key"];
            }
            copyField(entry.ble_key, sizeof(entry.ble_key), ble_key);
            g_config.device_count++;
        }
    }

    g_config.loaded = true;
    return true;
}

const AppConfig& getAppConfig() {
    return g_config;
}

const char* mijiaDeviceDisplayName(const MijiaDevice& dev) {
    // 界面标题只用英文 name，不切换中文字体
    if (dev.name[0] != '\0') {
        return dev.name;
    }
    return "device";
}

bool mijiaDeviceUsesBle(const MijiaDevice& dev) {
    if (dev.ble_key[0] == '\0') {
        return false;
    }
    // 有可用局域网 miIO 时优先走 WiFi
    if (isPrivateLanIp(dev.ip) && strlen(dev.token) >= 32) {
        return false;
    }
    return true;
}

bool saveAppConfigJson(const char* json) {
    if (json == nullptr) {
        return false;
    }

    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, json);
    if (err) {
        return false;
    }

    File file = LittleFS.open(CONFIG_PATH, "w");
    if (!file) {
        return false;
    }
    serializeJsonPretty(doc, file);
    file.close();
    return loadAppConfig();
}

bool saveAppConfigWifi(const char* ssid, const char* password) {
    if (ssid == nullptr || ssid[0] == '\0') {
        return false;
    }

    JsonDocument doc;
    if (LittleFS.exists(CONFIG_PATH)) {
        File in = LittleFS.open(CONFIG_PATH, "r");
        if (in) {
            const DeserializationError err = deserializeJson(doc, in);
            in.close();
            if (err) {
                doc.clear();
            }
        }
    }

    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["ssid"] = ssid;
    wifi["password"] = password == nullptr ? "" : password;

    if (doc["devices"].isNull()) {
        doc["devices"].to<JsonArray>();
    }

    File out = LittleFS.open(CONFIG_PATH, "w");
    if (!out) {
        return false;
    }
    serializeJsonPretty(doc, out);
    out.close();
    return loadAppConfig();
}

bool saveAppConfigBrightness(const uint8_t brightness) {
    JsonDocument doc;
    if (LittleFS.exists(CONFIG_PATH)) {
        File in = LittleFS.open(CONFIG_PATH, "r");
        if (in) {
            const DeserializationError err = deserializeJson(doc, in);
            in.close();
            if (err) {
                doc.clear();
            }
        }
    }

    doc["brightness"] = brightness;

    if (doc["devices"].isNull()) {
        doc["devices"].to<JsonArray>();
    }

    File out = LittleFS.open(CONFIG_PATH, "w");
    if (!out) {
        return false;
    }
    serializeJsonPretty(doc, out);
    out.close();
    return loadAppConfig();
}

bool saveAppConfigTimeKeySound(const bool enabled) {
    JsonDocument doc;
    if (LittleFS.exists(CONFIG_PATH)) {
        File in = LittleFS.open(CONFIG_PATH, "r");
        if (in) {
            const DeserializationError err = deserializeJson(doc, in);
            in.close();
            if (err) {
                doc.clear();
            }
        }
    }

    JsonObject sound = doc["sound"].to<JsonObject>();
    sound["time_key"] = enabled;

    if (doc["devices"].isNull()) {
        doc["devices"].to<JsonArray>();
    }

    File out = LittleFS.open(CONFIG_PATH, "w");
    if (!out) {
        return false;
    }
    serializeJsonPretty(doc, out);
    out.close();
    return loadAppConfig();
}

bool readAppConfigRaw(String& out) {
    out = "";
    if (!LittleFS.exists(CONFIG_PATH)) {
        return false;
    }

    File file = LittleFS.open(CONFIG_PATH, "r");
    if (!file) {
        return false;
    }
    out = file.readString();
    file.close();
    return true;
}
