#include "app_ble.h"
#include "app_connectivity.h"
#include "app_header.h"
#include "app_common.h"
#include "app_colors.h"
#include <BLEDevice.h>
#include <cstring>

static constexpr const char* BLE_DEVICE_NAME = "Cardputer";
static constexpr int BLE_SCAN_SECONDS = 4;
static constexpr int BLE_SCAN_MAX_ITEMS = 24;
static constexpr int BLE_SCAN_PAGE_SIZE = 4;
static constexpr int BLE_TAG_H = 18;
static constexpr int BLE_LIST_LINE_START_X = APP_CONTENT_X + 36;
static constexpr int BLE_LIST_LINE_END_X = APP_CONTENT_X + 236;

static bool bleScreenReady = false;
static bool bleScanning = false;
static int bleScanCount = 0;
static int bleScanPage = 0;
static bool bleInfoMode = false;
static char bleLastState[8] = "";
static char bleLastName[20] = "";
static char bleLastAddr[20] = "";
static char bleLastAdv[8] = "";
static char bleLastClient[8] = "";
static bool bleListDirty = true;
static bool bleLastInfoMode = false;
static bool bleLastScanning = false;
static int bleLastScanCount = -1;
static int bleLastScanPage = -1;

struct BleScanItem {
    char name[24];
    char addr[20];
    int rssi;
    char category[12];
};

static BleScanItem bleScanItems[BLE_SCAN_MAX_ITEMS];

// 仅重绘变化的 kv 行
static void updateBleLine(const int y, const char* label, const char* value, char* cache,
                          const size_t cache_size) {
    if (strncmp(cache, value, cache_size) == 0 && cache[0] != '\0') {
        return;
    }

    M5Cardputer.Display.fillRect(APP_CONTENT_X, y, 220, INFO_LINE_H, BLACK);
    drawInfoLineAt(APP_CONTENT_X, y, label, value);
    strncpy(cache, value, cache_size - 1);
    cache[cache_size - 1] = '\0';
}

static void resetBleLineCache() {
    bleLastState[0] = '\0';
    bleLastName[0] = '\0';
    bleLastAddr[0] = '\0';
    bleLastAdv[0] = '\0';
    bleLastClient[0] = '\0';
}

static void resetBleListCache() {
    bleListDirty = true;
    bleLastInfoMode = false;
    bleLastScanning = false;
    bleLastScanCount = -1;
    bleLastScanPage = -1;
}

// 列表模式下仅在状态变化时重绘，减少频繁闪烁
static bool shouldRedrawBleList() {
    if (bleListDirty) {
        return true;
    }
    if (bleLastInfoMode != bleInfoMode) {
        return true;
    }
    if (bleLastScanning != bleScanning) {
        return true;
    }
    if (bleLastScanCount != bleScanCount) {
        return true;
    }
    if (bleLastScanPage != bleScanPage) {
        return true;
    }
    return false;
}

static void markBleListDrawn() {
    bleListDirty = false;
    bleLastInfoMode = bleInfoMode;
    bleLastScanning = bleScanning;
    bleLastScanCount = bleScanCount;
    bleLastScanPage = bleScanPage;
}

// 使用按键徽章绘制 BLE 操作说明（仅按键字母包徽章）
static void drawBleActionHints(const int x, const int y, const bool info_mode) {
    if (info_mode) {
        static const KeyHintItem items[] = {
            {'s', "scan"},
            {'i', "list"},
            {'o', "on"},
            {'f', "off"},
            {'t', "toggle"},
        };
        drawKeyHintsRow(x, y, items, sizeof(items) / sizeof(items[0]), 1, APP_COLOR_HINT);
        return;
    }

    static const KeyHintItem items[] = {
        {'s', "scan"},
        {'i', "info"},
    };
    drawKeyHintsRow(x, y, items, sizeof(items) / sizeof(items[0]), 1, APP_COLOR_HINT);
}

// 大号状态 tag：ON 蓝底，OFF 黄底
static void drawBleStateTag(const int x, const int y, const bool on) {
    const uint16_t bg = on ? BLUE : YELLOW;
    const char* text = on ? "ON" : "OFF";
    M5Cardputer.Display.setTextSize(2);
    const int tw = M5Cardputer.Display.textWidth(text);
    const int w = tw + 18;
    M5Cardputer.Display.fillRoundRect(x, y, w, BLE_TAG_H, 4, bg);
    M5Cardputer.Display.drawRoundRect(x, y, w, BLE_TAG_H, 4, bg);
    M5Cardputer.Display.setTextColor(on ? WHITE : BLACK, bg);
    M5Cardputer.Display.setCursor(x + (w - tw) / 2, y + 2);
    M5Cardputer.Display.print(text);
}

static int getBlePageCount() {
    if (bleScanCount <= 0) {
        return 1;
    }
    return (bleScanCount + BLE_SCAN_PAGE_SIZE - 1) / BLE_SCAN_PAGE_SIZE;
}

// 蓝牙设备类型：普通 BLE / Beacon / BLE 服务设备
static const char* classifyBleCategory(BLEAdvertisedDevice& dev) {
    if (dev.haveManufacturerData() && !dev.haveServiceUUID()) {
        return "beacon";
    }
    if (dev.haveServiceUUID()) {
        return "ble-svc";
    }
    return "normal";
}

static void copySafe(char* dest, const size_t size, const char* src) {
    if (size == 0) {
        return;
    }
    if (src == nullptr) {
        dest[0] = '\0';
        return;
    }
    strncpy(dest, src, size - 1);
    dest[size - 1] = '\0';
}

static void scanNearbyBleDevices() {
    if (!isBleStackReady()) {
        startBleStack();
    }

    bleScanning = true;
    bleListDirty = true;
    drawBleApp(false);

    BLEScan* scan = BLEDevice::getScan();
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(80);
    BLEScanResults results = scan->start(BLE_SCAN_SECONDS, false);

    const int count = results.getCount();
    bleScanCount = count > BLE_SCAN_MAX_ITEMS ? BLE_SCAN_MAX_ITEMS : count;
    for (int i = 0; i < bleScanCount; i++) {
        BLEAdvertisedDevice dev = results.getDevice(i);
        if (dev.haveName()) {
            copySafe(bleScanItems[i].name, sizeof(bleScanItems[i].name), dev.getName().c_str());
        } else {
            copySafe(bleScanItems[i].name, sizeof(bleScanItems[i].name), "(no name)");
        }
        copySafe(bleScanItems[i].addr, sizeof(bleScanItems[i].addr), dev.getAddress().toString().c_str());
        bleScanItems[i].rssi = dev.getRSSI();
        copySafe(bleScanItems[i].category, sizeof(bleScanItems[i].category), classifyBleCategory(dev));
    }
    bleScanPage = 0;
    scan->clearResults();
    bleScanning = false;
    bleListDirty = true;
}

// 扫描列表全屏（内容区），支持上下翻页
static void drawBleScanListFull() {
    const int y_start = APP_CONTENT_Y;
    const int content_h = M5Cardputer.Display.height() - y_start;
    M5Cardputer.Display.fillRect(APP_CONTENT_X, y_start, 236, content_h, BLACK);
    int y = y_start;

    if (bleScanning) {
        drawInfoLineAt(APP_CONTENT_X, y, "scan", "scanning...", 2);
        return;
    }

    char buf[40];
    const int page_count = getBlePageCount();
    if (bleScanPage >= page_count) {
        bleScanPage = page_count - 1;
    }
    if (bleScanPage < 0) {
        bleScanPage = 0;
    }
    if (bleScanCount == 0) {
        // 未扫描前使用 2 倍提示，不展示分页信息
        drawInfoLineAt(APP_CONTENT_X, y, "scan", "press s", 2);
        y += INFO_LINE_H_2X;
        drawInfoLineAt(APP_CONTENT_X, y, "list", "empty", 2);
        return;
    }

    // 仅在有列表内容时展示 1 倍提示和分页信息
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
    drawBleActionHints(APP_CONTENT_X, y, false);
    snprintf(buf, sizeof(buf), "%d/%d", bleScanPage + 1, page_count);
    M5Cardputer.Display.setCursor(APP_CONTENT_X + 160, y);
    M5Cardputer.Display.print("page:");
    M5Cardputer.Display.print(buf);
    y += INFO_LINE_H;

    const int start = bleScanPage * BLE_SCAN_PAGE_SIZE;
    const int end = (start + BLE_SCAN_PAGE_SIZE < bleScanCount) ? start + BLE_SCAN_PAGE_SIZE : bleScanCount;
    for (int i = start; i < end; i++) {
        const BleScanItem& item = bleScanItems[i];
        snprintf(buf, sizeof(buf), "%02d %s", i + 1, item.name);
        drawInfoLineAt(APP_CONTENT_X, y, "dev", buf);
        y += INFO_LINE_H;
        snprintf(buf, sizeof(buf), "%s %ddBm %s", item.addr, item.rssi, item.category);
        drawInfoLineAt(APP_CONTENT_X, y, "sig", buf);
        y += INFO_LINE_H;
        // 分隔线从 label 区域右侧一直画到最右边
        M5Cardputer.Display.drawFastHLine(BLE_LIST_LINE_START_X, y,
                                          BLE_LIST_LINE_END_X - BLE_LIST_LINE_START_X,
                                          APP_COLOR_MUTED);
        y += 2;
    }
}

// 蓝牙信息页：大状态 tag + kv 信息
static void drawBleInfoPanel(const char* name, const char* addr, const char* adv, const char* client) {
    const bool on = isBleStackReady();
    M5Cardputer.Display.fillRect(APP_CONTENT_X, APP_CONTENT_Y, 236,
                                 M5Cardputer.Display.height() - APP_CONTENT_Y, BLACK);

    int y = APP_CONTENT_Y;
    drawBleStateTag(APP_CONTENT_X, y, on);
    y += BLE_TAG_H + 6;
    drawInfoLineAt(APP_CONTENT_X, y, "name", name);
    y += INFO_LINE_H;
    drawInfoLineAt(APP_CONTENT_X, y, "addr", addr);
    y += INFO_LINE_H;
    drawInfoLineAt(APP_CONTENT_X, y, "adv", adv);
    y += INFO_LINE_H;
    drawInfoLineAt(APP_CONTENT_X, y, "client", client);
    y += INFO_LINE_H + 4;
    drawBleActionHints(APP_CONTENT_X, y, true);
}

void drawBleApp(const bool full_init) {
    char state[8];
    char name[20];
    char addr[20];
    char adv[8];
    char client[8];

    if (isBleStackReady()) {
        strncpy(state, "ON", sizeof(state));
        strncpy(name, BLE_DEVICE_NAME, sizeof(name));
        strncpy(addr, BLEDevice::getAddress().toString().c_str(), sizeof(addr));
        strncpy(adv, isBleAdvertising() ? "ON" : "OFF", sizeof(adv));
        snprintf(client, sizeof(client), "%d", getBleConnectedCount());
    } else {
        strncpy(state, "OFF", sizeof(state));
        strncpy(name, BLE_DEVICE_NAME, sizeof(name));
        strncpy(addr, "N/A", sizeof(addr));
        strncpy(adv, "OFF", sizeof(adv));
        strncpy(client, "0", sizeof(client));
    }

    if (full_init || !bleScreenReady) {
        beginAppScreen("BLE");
        bleScreenReady = true;
        resetBleLineCache();
        resetBleListCache();
    }

    if (bleInfoMode) {
        drawBleInfoPanel(name, addr, adv, client);
        // 从信息页返回列表时强制重绘一次，避免显示旧内容
        bleListDirty = true;
    } else {
        if (!full_init && !shouldRedrawBleList()) {
            return;
        }
        drawBleScanListFull();
        markBleListDrawn();
    }
}

void enterBleApp() {
    bleScreenReady = false;
    bleInfoMode = false;
    drawBleApp(true);
}

void updateBleApp() {
    if (!bleScreenReady) {
        return;
    }
    drawBleApp(false);
}

void handleBleApp(const String& key) {
    if (key == "o") {
        startBleStack();
    } else if (key == "f") {
        stopBleStack();
    } else if (key == "t") {
        if (isBleStackReady()) {
            stopBleStack();
        } else {
            startBleStack();
        }
    } else if (key == "r") {
        // 仅刷新
    } else if (key == "s") {
        bleInfoMode = false;
        scanNearbyBleDevices();
    } else if (key == "i") {
        bleInfoMode = !bleInfoMode;
    } else {
        return;
    }
    drawBleApp(false);
    updateAppHeaderStatus();
}

bool handleBlePageNav(const Keyboard_Class::KeysState& status) {
    if (bleInfoMode || bleScanning) {
        return false;
    }
    const int delta = getMenuNavDelta(status);
    if (delta == 0) {
        return false;
    }
    const int page_count = getBlePageCount();
    if (page_count <= 1) {
        return false;
    }
    bleScanPage += delta;
    if (bleScanPage < 0) {
        bleScanPage = 0;
    }
    if (bleScanPage >= page_count) {
        bleScanPage = page_count - 1;
    }
    drawBleApp(false);
    return true;
}
