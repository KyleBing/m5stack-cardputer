#include "M5Cardputer.h"
#include "app_logo.h"
#include "app_header.h"
#include <BLEDevice.h>
#include <WiFi.h>
#include <esp_chip_info.h>
#include <esp_system.h>



// ===== COMMON =====

struct VersionInfo {
    const String version;
    const String update_time;
    const String author;
    const String email;
    const String website;
};

// 应用状态
enum class AppState {
    MENU,
    VERSION,
    KEYBOARD,
    BMI,
    INFO,
    MIC,
    SETTINGS,
    BTNA,
    POWER,
    SPEAKER,
    RTC,
    IN_I2C,
    EX_I2C,
    WIFI,
    BLE,
    DISP,
    CIRCLE,
    SLEEP,
};

struct MenuItem {
    char key;
    const char* name;
    AppState state;
};


// Cardputer 技能 → 字母入口
static const MenuItem MENU_ITEMS[] = {
    {'v', "Ver", AppState::VERSION},
    {'k', "Key", AppState::KEYBOARD},
    {'g', "BMI", AppState::BMI},
    {'i', "Info", AppState::INFO},
    {'m', "Mic", AppState::MIC},
    {'o', "Set", AppState::SETTINGS},
    {'a', "BtnA", AppState::BTNA},
    {'p', "Pwr", AppState::POWER},
    {'l', "Spk", AppState::SPEAKER},
    {'s', "Slp", AppState::SLEEP},
    {'t', "Time", AppState::RTC},
    {'n', "InI2", AppState::IN_I2C},
    {'e', "ExI2", AppState::EX_I2C},
    {'w', "WiFi", AppState::WIFI},
    {'b', "BLE", AppState::BLE},
    {'d', "Disp", AppState::DISP},
    {'c', "Circ", AppState::CIRCLE},
};

static const int MENU_ITEM_COUNT = sizeof(MENU_ITEMS) / sizeof(MENU_ITEMS[0]);

AppState currentState = AppState::MENU;
static uint32_t btnTestCount = 0;
static bool micHeaderReady = false;

void enterApp(const AppState state);

// 获取当前按下的可打印字符
String getPressedKey() {
    const Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
    String key;
    for (const char c : status.word) {
        key += c;
    }
    return key;
}

// 根据字母查找 app（支持大小写）
bool enterAppByKey(const char key) {
    const char keyLower = (key >= 'A' && key <= 'Z') ? static_cast<char>(key - 'A' + 'a') : key;
    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        if (MENU_ITEMS[i].key == keyLower) {
            enterApp(MENU_ITEMS[i].state);
            return true;
        }
    }
    return false;
}

// ===== MENU =====

static constexpr const char* APP_NAME = "Cardputer";
static constexpr int MENU_COLS = 3;
static constexpr int MENU_ROWS_PER_PAGE = 4;
static constexpr int MENU_ITEMS_PER_PAGE = MENU_COLS * MENU_ROWS_PER_PAGE;
static constexpr int MENU_LINE_H = 16;

static int menuPage = 0;

// 计算菜单总页数
int getMenuPageCount() {
    return (MENU_ITEM_COUNT + MENU_ITEMS_PER_PAGE - 1) / MENU_ITEMS_PER_PAGE;
}

// 检测翻页键：-1 上一页，0 无，1 下一页（直接按 ; , . /，无需 Fn）
int getMenuNavDelta(const Keyboard_Class::KeysState& status) {
    for (const uint8_t hid : status.hid_keys) {
        if (hid == 0x52 || hid == 0x50 || hid == 0x33 || hid == 0x36) {
            return -1;  // Up / Left / ; ,
        }
        if (hid == 0x51 || hid == 0x4F || hid == 0x37 || hid == 0x38) {
            return 1;   // Down / Right / . /
        }
    }
    for (const char c : status.word) {
        if (c == ';' || c == ',') {
            return -1;
        }
        if (c == '.' || c == '/') {
            return 1;
        }
    }
    return 0;
}

// 绘制单个菜单项：触发字母与菜单名各用一种固定颜色
void drawMenuItem(const MenuItem& item) {
    M5Cardputer.Display.setTextColor(YELLOW, BLACK);
    M5Cardputer.Display.printf("%c.", toupper(item.key));
    M5Cardputer.Display.setTextColor(WHITE, BLACK);
    M5Cardputer.Display.printf("%s", item.name);
}

// 绘制主菜单当前页
void drawMenuPage() {
    const int startIdx = menuPage * MENU_ITEMS_PER_PAGE;
    const int endIdx = (startIdx + MENU_ITEMS_PER_PAGE < MENU_ITEM_COUNT)
                           ? startIdx + MENU_ITEMS_PER_PAGE
                           : MENU_ITEM_COUNT;

    M5Cardputer.Display.setTextSize(2);
    int row = 0;
    for (int i = startIdx; i < endIdx; i += MENU_COLS) {
        const int y = APP_CONTENT_Y + row * MENU_LINE_H;
        M5Cardputer.Display.setCursor(APP_CONTENT_X, y);
        drawMenuItem(MENU_ITEMS[i]);
        if (i + 1 < endIdx) {
            M5Cardputer.Display.print(" ");
            drawMenuItem(MENU_ITEMS[i + 1]);
        }
        if (i + 2 < endIdx) {
            M5Cardputer.Display.print(" ");
            drawMenuItem(MENU_ITEMS[i + 2]);
        }
        row++;
    }
}

// 绘制主菜单（header + 可翻页菜单区）
void showMenu() {
    currentState = AppState::MENU;
    const int pageCount = getMenuPageCount();
    if (menuPage >= pageCount) {
        menuPage = 0;
    }

    M5Cardputer.Display.clear();
    drawMenuScreenHeader(APP_NAME, menuPage, getMenuPageCount());
    drawMenuPage();
}

// 方向键翻页，返回 true 表示已处理
bool handleMenuPageNav(const Keyboard_Class::KeysState& status) {
    const int delta = getMenuNavDelta(status);
    if (delta == 0) {
        return false;
    }

    const int pageCount = getMenuPageCount();
    menuPage = (menuPage + delta + pageCount) % pageCount;
    showMenu();
    return true;
}

// 菜单按键
void handleMenuKey(const String& key) {
    if (key.length() != 1) {
        return;
    }
    const char c = key[0];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) {
        return;
    }

    if (!enterAppByKey(c)) {
        beginAppScreen("Menu");
        M5Cardputer.Display.setCursor(APP_CONTENT_X, APP_CONTENT_Y);
        M5Cardputer.Display.setTextSize(2);
        M5Cardputer.Display.setTextColor(RED, BLACK);
        M5Cardputer.Display.printf("No app: %c\n", toupper(c));
    }
}

// ===== VERSION =====

// 返回固件版本信息
VersionInfo getVersionInfo() {
    return VersionInfo{
        "0.0.1",
        "2026-07-06",
        "KyleBing",
        "kylebing@163.com",
        "kylebing.cn"
    };
}

// 绘制 Version 页面
void drawVersionApp() {
    const VersionInfo info = getVersionInfo();
    beginAppScreen("Ver");

    constexpr int logoSize = APP_LOGO_DESIGN_SIZE;
    const int logoX = (M5Cardputer.Display.width() - logoSize) / 2;
    const int logoY = APP_CONTENT_Y + 4;
    drawAppLogo(logoX, logoY, logoSize);

    const int textY = logoY + logoSize + 10;
    const int centerX = M5Cardputer.Display.width() / 2;
    constexpr int lineH = 12;

    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(CYAN, BLACK);
    M5Cardputer.Display.drawCenterString(
        ("ver: " + info.version).c_str(), centerX, textY);
    M5Cardputer.Display.setTextColor(LIGHTGREY, BLACK);
    M5Cardputer.Display.drawCenterString(
        ("date: " + info.update_time).c_str(), centerX, textY + lineH);
    M5Cardputer.Display.setTextColor(WHITE, BLACK);
    M5Cardputer.Display.drawCenterString(
        ("auth: " + info.author).c_str(), centerX, textY + lineH * 2);
}

// ===== KEYBOARD =====

String getKeyLabel(const Keyboard_Class::KeysState& status) {
    String key;
    for (const char c : status.word) {
        key += c;
    }
    if (key.length() > 0) {
        return key;
    }
    if (status.del) {
        return "DEL";
    }
    if (status.enter) {
        return "ENT";
    }
    if (status.space) {
        return "SPC";
    }
    if (status.tab) {
        return "TAB";
    }
    if (status.fn || status.shift || status.ctrl || status.opt || status.alt) {
        return "MOD";
    }
    return "-";
}

void drawKeyboardApp(const Keyboard_Class::KeysState& status) {
    beginAppScreen("Key");
    M5Cardputer.Display.setCursor(APP_CONTENT_X, APP_CONTENT_Y);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.printf("Fn:%s Sh:%s Ct:%s\n",
                               status.fn ? "ON" : "--",
                               status.shift ? "ON" : "--",
                               status.ctrl ? "ON" : "--");
    M5Cardputer.Display.printf("Op:%s Al:%s Tb:%s\n",
                               status.opt ? "ON" : "--",
                               status.alt ? "ON" : "--",
                               status.tab ? "ON" : "--");
    M5Cardputer.Display.printf("Dl:%s En:%s Sp:%s\n",
                               status.del ? "ON" : "--",
                               status.enter ? "ON" : "--",
                               status.space ? "ON" : "--");

    const String label = getKeyLabel(status);
    M5Cardputer.Display.setTextSize(4);
    M5Cardputer.Display.printf("%s\n", label.c_str());
    Serial.println(label);
}

// ===== BMI =====

const char* getImuTypeName(const m5::imu_t type) {
    switch (type) {
        case m5::imu_bmi270:
            return "BMI270";
        case m5::imu_mpu6886:
            return "MPU6886";
        case m5::imu_mpu6050:
            return "MPU6050";
        case m5::imu_mpu9250:
            return "MPU9250";
        case m5::imu_sh200q:
            return "SH200Q";
        case m5::imu_unknown:
            return "Unknown";
        default:
            return "N/A";
    }
}

// BMI（IMU）页面
void drawBmiApp() {
    // 保持屏幕与 CPU 活跃，避免休眠影响 IMU 刷新
    M5Cardputer.Display.wakeup();
    M5Cardputer.Display.powerSaveOff();

    M5.Imu.update();

    beginAppScreen("BMI");
    M5Cardputer.Display.setCursor(APP_CONTENT_X, APP_CONTENT_Y);
    M5Cardputer.Display.setTextSize(1);

    if (!M5.Imu.isEnabled()) {
        M5Cardputer.Display.println("IMU not found");
        return;
    }

    M5Cardputer.Display.printf("%s\n", getImuTypeName(M5.Imu.getType()));

    float ax = 0;
    float ay = 0;
    float az = 0;
    float gx = 0;
    float gy = 0;
    float gz = 0;
    float mx = 0;
    float my = 0;
    float mz = 0;
    float temp = 0;

    M5.Imu.getAccel(&ax, &ay, &az);
    M5.Imu.getGyro(&gx, &gy, &gz);

    M5Cardputer.Display.printf("X→ %+.2f\n", ax);
    M5Cardputer.Display.printf("Y→ %+.2f\n", ay);
    M5Cardputer.Display.printf("Z→ %+.2f\n", az);
    M5Cardputer.Display.printf("Gx→ %+.0f\n", gx);
    M5Cardputer.Display.printf("Gy→ %+.0f\n", gy);
    M5Cardputer.Display.printf("Gz→ %+.0f\n", gz);

    if (M5.Imu.getMag(&mx, &my, &mz)) {
        M5Cardputer.Display.printf("Mx→ %+.0f\n", mx);
        M5Cardputer.Display.printf("My→ %+.0f\n", my);
        M5Cardputer.Display.printf("Mz→ %+.0f\n", mz);
    }
    if (M5.Imu.getTemp(&temp)) {
        M5Cardputer.Display.printf("T→ %+.1fC\n", temp);
    }
}

// ===== INFO =====

// 绘制板级 Info 页面
void drawInfoApp() {
    const esp_chip_info_t chipInfo = []() {
        esp_chip_info_t info{};
        esp_chip_info(&info);
        return info;
    }();

    beginAppScreen("Info");
    M5Cardputer.Display.setCursor(APP_CONTENT_X, APP_CONTENT_Y);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.printf("model: %s\n", ESP.getChipModel());
    M5Cardputer.Display.printf("cores: %d\n", chipInfo.cores);
    M5Cardputer.Display.printf("freq: %d MHz\n", ESP.getCpuFreqMHz());
    M5Cardputer.Display.printf("flash: %d MB\n", ESP.getFlashChipSize() / (1024 * 1024));
    M5Cardputer.Display.printf("heap: %d KB\n", ESP.getFreeHeap() / 1024);
    M5Cardputer.Display.printf("sdk: %s\n", ESP.getSdkVersion());
}

// ===== MIC =====

// Mic 实时波形
void drawMicApp() {
    const int waveTop = APP_CONTENT_Y;
    const int waveW = M5Cardputer.Display.width();
    const int waveH = M5Cardputer.Display.height() - waveTop - 2;
    const int centerY = waveTop + waveH / 2;

    static int16_t samples[240];

    if (!M5Cardputer.Mic.isEnabled()) {
        beginAppScreen("Mic");
        micHeaderReady = false;
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setCursor(APP_CONTENT_X, APP_CONTENT_Y);
        M5Cardputer.Display.println("not found");
        return;
    }

    if (!micHeaderReady) {
        beginAppScreen("Mic");
        micHeaderReady = true;
    }

    const size_t sampleCount = waveW < 240 ? waveW : 240;
    if (!M5Cardputer.Mic.record(samples, sampleCount)) {
        return;
    }

    M5Cardputer.Display.fillRect(0, waveTop, waveW, waveH, BLACK);
    M5Cardputer.Display.drawFastHLine(0, centerY, waveW, DARKGREY);

    constexpr int micGain = 2;
    int prevY = centerY;
    for (int x = 0; x < static_cast<int>(sampleCount); x++) {
        int y = centerY - static_cast<int>(samples[x] * micGain * (waveH / 2 - 2) / 32768);
        y = constrain(y, waveTop, waveTop + waveH - 1);
        if (x > 0) {
            M5Cardputer.Display.drawLine(x - 1, prevY, x, y, GREEN);
        }
        prevY = y;
    }
}

// ===== SETTINGS =====

void drawSettingsApp() {
    beginAppScreen("Set");
    M5Cardputer.Display.setCursor(APP_CONTENT_X, APP_CONTENT_Y);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.println("0-9  brightness");
    M5Cardputer.Display.println("b    show level");
    M5Cardputer.Display.println("r    invert screen");
    M5Cardputer.Display.printf("\ninvert: %s\n",
                               M5Cardputer.Display.getInvert() ? "ON" : "OFF");
    M5Cardputer.Display.printf("level:  %d\n", M5Cardputer.Display.getBrightness());
}

void handleSettingsApp(const String& key) {
    beginAppScreen("Set");
    M5Cardputer.Display.setCursor(APP_CONTENT_X, APP_CONTENT_Y);
    M5Cardputer.Display.setTextSize(1);

    if (key == "b") {
        M5Cardputer.Display.printf("brightness: %d\n", M5Cardputer.Display.getBrightness());
    } else if (key.length() == 1 && key[0] >= '0' && key[0] <= '9') {
        const int level = key[0] - '0';
        const uint8_t brightness = level * 255 / 9;
        M5Cardputer.Display.setBrightness(brightness);
        M5Cardputer.Display.printf("level %d -> %d\n", level, brightness);
    } else if (key == "r") {
        const bool inverted = M5Cardputer.Display.getInvert();
        M5Cardputer.Display.invertDisplay(!inverted);
        M5Cardputer.Display.printf("invert: %s\n", !inverted ? "ON" : "OFF");
    }
}

// ===== BTNA =====

// BtnA 侧键测试（短按计数，长按返回菜单）
void drawBtnAApp() {
    beginAppScreen("BtnA");
    M5Cardputer.Display.setCursor(APP_CONTENT_X, APP_CONTENT_Y);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.printf("count: %lu\n", btnTestCount);
    M5Cardputer.Display.printf("press: %s\n", M5Cardputer.BtnA.isPressed() ? "ON" : "--");
    M5Cardputer.Display.printf("hold:  %s\n", M5Cardputer.BtnA.isHolding() ? "ON" : "--");
    M5Cardputer.Display.println("tap=count");
    M5Cardputer.Display.println("hold=back");
}

// ===== POWER =====

// 电源信息
void drawPowerApp() {
    beginAppScreen("Pwr");
    M5Cardputer.Display.setCursor(APP_CONTENT_X, APP_CONTENT_Y);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.printf("bat: %d%%\n", M5Cardputer.Power.getBatteryLevel());
    M5Cardputer.Display.printf("volt: %dmV\n", M5Cardputer.Power.getBatteryVoltage());
    M5Cardputer.Display.printf("curr: %dmA\n", M5Cardputer.Power.getBatteryCurrent());
    M5Cardputer.Display.printf("chg: %s\n", M5Cardputer.Power.isCharging() ? "ON" : "OFF");
    M5Cardputer.Display.printf("vbus: %dmV\n", M5Cardputer.Power.getVBUSVoltage());
}

// ===== SPEAKER =====

void drawSpeakerApp(const String& key) {
    beginAppScreen("Spk");
    M5Cardputer.Display.setCursor(APP_CONTENT_X, APP_CONTENT_Y);
    M5Cardputer.Display.setTextSize(1);
    if (key.length() == 1 && key[0] >= '1' && key[0] <= '9') {
        M5Cardputer.Display.printf("tone: %d Hz\n", 440 + (key[0] - '1') * 110);
    } else {
        M5Cardputer.Display.println("1-9 tone");
        M5Cardputer.Display.println("0 stop");
    }
}

void handleSpeakerApp(const String& key) {
    if (key.length() == 1 && key[0] >= '1' && key[0] <= '9') {
        const int n = key[0] - '1';
        const int freq = 440 + n * 110;
        M5Cardputer.Speaker.tone(freq, 300);
    } else if (key == "0") {
        M5Cardputer.Speaker.stop();
    }
    drawSpeakerApp(key);
}

// ===== RTC =====

// RTC 时钟
void drawRtcApp() {
    beginAppScreen("Time");
    M5Cardputer.Display.setCursor(APP_CONTENT_X, APP_CONTENT_Y);
    M5Cardputer.Display.setTextSize(1);

    if (!M5.Rtc.isEnabled()) {
        M5Cardputer.Display.println("RTC N/A");
        return;
    }

    const m5::rtc_datetime_t dt = M5.Rtc.getDateTime();
    M5Cardputer.Display.printf("%04d-%02d-%02d\n", dt.date.year, dt.date.month, dt.date.date);
    M5Cardputer.Display.printf("%02d:%02d:%02d\n", dt.time.hours, dt.time.minutes, dt.time.seconds);
}

// ===== IN I2C =====

// 绘制 I2C 扫描结果（IN I2C / EX I2C 共用）
void drawI2cScanApp(m5::I2C_Class& bus, const char* title) {
    bool found[120]{};
    if (bus.isEnabled()) {
        bus.scanID(found);
    }

    M5Cardputer.Display.clear();
    drawAppScreenHeader(title);
    M5Cardputer.Display.setCursor(APP_CONTENT_X, APP_CONTENT_Y);
    M5Cardputer.Display.setTextSize(1);

    if (!bus.isEnabled()) {
        M5Cardputer.Display.println("bus disabled");
        return;
    }

    M5Cardputer.Display.printf("SDA:%d SCL:%d\n", bus.getSDA(), bus.getSCL());

    int count = 0;
    for (int addr = 1; addr < 120; addr++) {
        if (!found[addr]) {
            continue;
        }
        M5Cardputer.Display.printf("0x%02X ", addr);
        count++;
        if (count % 5 == 0) {
            M5Cardputer.Display.println();
        }
    }
    if (count == 0) {
        M5Cardputer.Display.println("no device");
    }
}

// ===== EX I2C =====
// 使用 drawI2cScanApp(M5Cardputer.Ex_I2C, "EX I2C")

// ===== WIFI =====

// WiFi 状态
void drawWifiApp() {
    static bool wifiReady = false;
    if (!wifiReady) {
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        wifiReady = true;
    }

    M5Cardputer.Display.clear();
    drawAppScreenHeader("WiFi");
    M5Cardputer.Display.setCursor(APP_CONTENT_X, APP_CONTENT_Y);
    M5Cardputer.Display.setTextSize(1);

    if (WiFi.status() == WL_CONNECTED) {
        M5Cardputer.Display.printf("SSID: %s\n", WiFi.SSID().c_str());
        M5Cardputer.Display.printf("IP: %s\n", WiFi.localIP().toString().c_str());
        M5Cardputer.Display.printf("RSSI: %d dBm\n", WiFi.RSSI());
    } else {
        M5Cardputer.Display.println("not connected");
        M5Cardputer.Display.println("s = scan");
    }
}

void handleWifiApp(const String& key) {
    if (key != "s") {
        return;
    }

    beginAppScreen("WiFi Scan");
    M5Cardputer.Display.setCursor(APP_CONTENT_X, APP_CONTENT_Y);
    M5Cardputer.Display.setTextSize(1);

    const int count = WiFi.scanNetworks();
    M5Cardputer.Display.printf("found: %d\n", count);
    const int show = count < 4 ? count : 4;
    for (int i = 0; i < show; i++) {
        M5Cardputer.Display.printf("%s %d\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i));
    }
}

// ===== BLE =====

// BLE 状态
void drawBleApp() {
    static bool bleReady = false;
    if (!bleReady) {
        BLEDevice::init("Cardputer");
        bleReady = true;
    }

    beginAppScreen("BLE");
    M5Cardputer.Display.setCursor(APP_CONTENT_X, APP_CONTENT_Y);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.printf("addr:\n%s\n", BLEDevice::getAddress().toString().c_str());
    M5Cardputer.Display.println("name: Cardputer");
}

// ===== DISP =====

// 屏幕色彩测试
void drawDisplayApp(const int colorIndex) {
    static const uint16_t colors[] = {RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, WHITE};
    static const char* names[] = {"RED", "GREEN", "BLUE", "YEL", "CYAN", "MAG", "WHT"};
    static const int colorCount = 7;

    const int idx = colorIndex % colorCount;
    M5Cardputer.Display.fillScreen(colors[idx]);
    drawAppScreenHeader("Disp");
    M5Cardputer.Display.setTextColor(BLACK, colors[idx]);
    M5Cardputer.Display.setCursor(APP_CONTENT_X, APP_CONTENT_Y);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.printf("color: %s\n", names[idx]);
    M5Cardputer.Display.println("1-7 switch");
    M5Cardputer.Display.setTextColor(WHITE, BLACK);
}

void handleDisplayApp(const String& key) {
    if (key.length() != 1 || key[0] < '1' || key[0] > '7') {
        return;
    }
    drawDisplayApp(key[0] - '1');
}

// ===== CIRCLE =====

// 像素比例测试：正圆 + 十字线，圆不变形则屏幕像素为 1:1
void drawCircleTestApp() {
    M5Cardputer.Display.clear();

    const int cx = M5Cardputer.Display.width() / 2;
    const int cy = (M5Cardputer.Display.height() + APP_HEADER_H) / 2;
    constexpr int radius = 30;

    M5Cardputer.Display.drawCircle(cx, cy, radius, WHITE);
    M5Cardputer.Display.drawFastHLine(cx - radius, cy, radius * 2 + 1, DARKGREY);
    M5Cardputer.Display.drawFastVLine(cx, cy - radius, radius * 2 + 1, DARKGREY);

    drawAppScreenHeader("Circ");
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(WHITE, BLACK);
    M5Cardputer.Display.setCursor(APP_CONTENT_X, APP_CONTENT_Y);
    M5Cardputer.Display.printf("r=%d\n", radius);
    M5Cardputer.Display.println("round=1:1 ok");
}

// ===== SLEEP =====

static bool displayAsleep = false;

// s 入口：直接关屏，loop 内等 BtnA 唤醒
void enterSleep() {
    displayAsleep = true;
    M5Cardputer.Display.sleep();
}

// ===== MAIN =====

void enterApp(const AppState state) {
    currentState = state;

    // Sleep 直接关屏，不刷新界面
    if (state == AppState::SLEEP) {
        enterSleep();
        return;
    }

    M5Cardputer.Display.clear();

    switch (state) {
        case AppState::VERSION:
            drawVersionApp();
            break;
        case AppState::KEYBOARD: {
            Keyboard_Class::KeysState status{};
            drawKeyboardApp(status);
            break;
        }
        case AppState::BMI:
            drawBmiApp();
            break;
        case AppState::INFO:
            drawInfoApp();
            break;
        case AppState::MIC:
            micHeaderReady = false;
            drawMicApp();
            break;
        case AppState::BTNA:
            drawBtnAApp();
            break;
        case AppState::POWER:
            drawPowerApp();
            break;
        case AppState::SPEAKER:
            drawSpeakerApp("");
            break;
        case AppState::RTC:
            drawRtcApp();
            break;
        case AppState::IN_I2C:
            drawI2cScanApp(M5Cardputer.In_I2C, "InI2");
            break;
        case AppState::EX_I2C:
            drawI2cScanApp(M5Cardputer.Ex_I2C, "ExI2");
            break;
        case AppState::WIFI:
            drawWifiApp();
            break;
        case AppState::BLE:
            drawBleApp();
            break;
        case AppState::DISP:
            drawDisplayApp(0);
            break;
        case AppState::CIRCLE:
            drawCircleTestApp();
            break;
        case AppState::SETTINGS:
            drawSettingsApp();
            break;
        default:
            break;
    }
}

void setup() {
    const auto cfg = M5.config();
    M5Cardputer.begin(cfg);
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setBrightness(30);
    showMenu();
}

void loop() {
    M5Cardputer.update();

    // 休眠中只处理 BtnA 唤醒
    if (displayAsleep) {
        if (M5Cardputer.BtnA.wasPressed()) {
            M5Cardputer.Display.wakeup();
            displayAsleep = false;
            showMenu();
        }
        return;
    }

    // BtnA：BtnA app 内短按计数，长按返回；其它 app 短按返回菜单
    if (M5Cardputer.BtnA.wasPressed()) {
        if (currentState == AppState::BTNA) {
            btnTestCount++;
            drawBtnAApp();
        } else if (currentState != AppState::MENU) {
            showMenu();
        }
    }
    if (currentState == AppState::BTNA && M5Cardputer.BtnA.wasHold()) {
        showMenu();
    }

    const uint32_t now = millis();

    // BMI 实时刷新，不使用 delay 以免触发 idle sleep
    if (currentState == AppState::BMI) {
        static uint32_t lastBmiUpdateMs = 0;
        if (now - lastBmiUpdateMs >= 100) {
            lastBmiUpdateMs = now;
            drawBmiApp();
        }
    } else if (currentState == AppState::MIC) {
        static uint32_t lastMicUpdateMs = 0;
        if (now - lastMicUpdateMs >= 40) {
            lastMicUpdateMs = now;
            drawMicApp();
        }
    }

    if (currentState == AppState::BTNA) {
        static uint32_t lastBtnUpdateMs = 0;
        if (now - lastBtnUpdateMs >= 80) {
            lastBtnUpdateMs = now;
            drawBtnAApp();
        }
    }

    if (currentState == AppState::POWER) {
        static uint32_t lastPowerUpdateMs = 0;
        if (now - lastPowerUpdateMs >= 500) {
            lastPowerUpdateMs = now;
            drawPowerApp();
        }
    }

    if (currentState == AppState::RTC) {
        static uint32_t lastRtcUpdateMs = 0;
        if (now - lastRtcUpdateMs >= 1000) {
            lastRtcUpdateMs = now;
            drawRtcApp();
        }
    }

    if (M5Cardputer.Keyboard.isChange()) {
        switch (currentState) {
            case AppState::MENU:
                if (M5Cardputer.Keyboard.isPressed()) {
                    const Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
                    if (!handleMenuPageNav(status)) {
                        handleMenuKey(getPressedKey());
                    }
                }
                break;
            case AppState::KEYBOARD:
                drawKeyboardApp(M5Cardputer.Keyboard.keysState());
                break;
            case AppState::SETTINGS:
                if (M5Cardputer.Keyboard.isPressed()) {
                    handleSettingsApp(getPressedKey());
                }
                break;
            case AppState::SPEAKER:
                if (M5Cardputer.Keyboard.isPressed()) {
                    handleSpeakerApp(getPressedKey());
                }
                break;
            case AppState::WIFI:
                if (M5Cardputer.Keyboard.isPressed()) {
                    handleWifiApp(getPressedKey());
                }
                break;
            case AppState::DISP:
                if (M5Cardputer.Keyboard.isPressed()) {
                    handleDisplayApp(getPressedKey());
                }
                break;
            default:
                break;
        }
    }

    // 实时 app 不休眠；其它状态 yield 10ms
    if (currentState != AppState::BMI && currentState != AppState::MIC) {
        delay(10);
    }
}
