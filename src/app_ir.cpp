#include "app_ir.h"
#include "app_colors.h"
#include "app_common.h"
#include "app_header.h"

#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRac.h>

#include <cstring>

// Cardputer / Adv 板载红外发射管
static constexpr uint16_t IR_TX_PIN = 44;

static IRsend g_irsend(IR_TX_PIN);
static IRac g_irac(IR_TX_PIN);
static bool g_ir_ready = false;

enum class IrCategory : uint8_t { TV = 0, AC = 1 };

enum class IrTvBrand : uint8_t {
    Samsung = 0,
    Sony,
    Lg,
    Panasonic,
    Nec,
    Count,
};

enum class IrAcBrand : uint8_t {
    Midea = 0,
    Gree,
    Haier,
    Aux,
    Hisense,
    Xiaomi,
    Count,
};

enum class IrTvAction : uint8_t {
    Power = 0,
    VolUp,
    VolDown,
    Mute,
    ChUp,
    ChDown,
    Input,
    Count,
};

// 空调可调字段
enum class IrAcField : uint8_t {
    Power = 0,
    Mode,
    Temp,
    Fan,
    Count,
};

static IrCategory g_category = IrCategory::TV;
static int g_tv_brand = 0;
static int g_ac_brand = 0;
static int g_tv_action = 0;
static int g_ac_field = 0;

static bool g_ac_power = true;
static stdAc::opmode_t g_ac_mode = stdAc::opmode_t::kCool;
static uint8_t g_ac_temp = 26;
static stdAc::fanspeed_t g_ac_fan = stdAc::fanspeed_t::kAuto;

static bool g_help_visible = false;
static int g_help_page = 0;
static constexpr int IR_HELP_PAGE_COUNT = 2;

static const char* g_tx_status = "";
static uint32_t g_tx_status_until_ms = 0;
static bool g_screen_ready = false;

static const char* tvBrandName(const int idx) {
    static const char* names[] = {"Samsung", "Sony", "LG", "Panasonic", "NEC"};
    if (idx < 0 || idx >= static_cast<int>(IrTvBrand::Count)) {
        return "?";
    }
    return names[idx];
}

static const char* acBrandName(const int idx) {
    static const char* names[] = {"Midea", "Gree", "Haier", "AUX", "Hisense", "Xiaomi"};
    if (idx < 0 || idx >= static_cast<int>(IrAcBrand::Count)) {
        return "?";
    }
    return names[idx];
}

static const char* tvActionName(const int idx) {
    static const char* names[] = {"Power", "Vol+", "Vol-", "Mute", "Ch+", "Ch-", "Input"};
    if (idx < 0 || idx >= static_cast<int>(IrTvAction::Count)) {
        return "?";
    }
    return names[idx];
}

static const char* acModeName(const stdAc::opmode_t mode) {
    switch (mode) {
        case stdAc::opmode_t::kCool:
            return "Cool";
        case stdAc::opmode_t::kHeat:
            return "Heat";
        case stdAc::opmode_t::kDry:
            return "Dry";
        case stdAc::opmode_t::kFan:
            return "Fan";
        case stdAc::opmode_t::kAuto:
            return "Auto";
        default:
            return "?";
    }
}

static const char* acFanName(const stdAc::fanspeed_t fan) {
    switch (fan) {
        case stdAc::fanspeed_t::kAuto:
            return "Auto";
        case stdAc::fanspeed_t::kMin:
            return "Min";
        case stdAc::fanspeed_t::kLow:
            return "Low";
        case stdAc::fanspeed_t::kMedium:
            return "Med";
        case stdAc::fanspeed_t::kHigh:
            return "High";
        case stdAc::fanspeed_t::kMax:
            return "Max";
        default:
            return "?";
    }
}

static decode_type_t acProtocol(const int brand) {
    switch (static_cast<IrAcBrand>(brand)) {
        case IrAcBrand::Midea:
            return decode_type_t::MIDEA;
        case IrAcBrand::Gree:
            return decode_type_t::GREE;
        case IrAcBrand::Haier:
            return decode_type_t::HAIER_AC176;
        case IrAcBrand::Aux:
            return decode_type_t::ELECTRA_AC;
        case IrAcBrand::Hisense:
            return decode_type_t::KELON;
        case IrAcBrand::Xiaomi:
            // 多数小米/酷批机用 Coolix；壁挂 OEM 可再试 Midea
            return decode_type_t::COOLIX;
        default:
            return decode_type_t::MIDEA;
    }
}

static void ensureIrReady() {
    if (g_ir_ready) {
        return;
    }
    g_irsend.begin();
    g_ir_ready = true;
}

static void setTxStatus(const char* text) {
    g_tx_status = text;
    g_tx_status_until_ms = millis() + 1500;
}

// 常用电视红外码（公开遥控码表，机型可能有差异）
static void sendTvAction() {
    ensureIrReady();
    const auto brand = static_cast<IrTvBrand>(g_tv_brand);
    const auto action = static_cast<IrTvAction>(g_tv_action);

    switch (brand) {
        case IrTvBrand::Samsung: {
            uint32_t code = 0;
            switch (action) {
                case IrTvAction::Power:
                    code = 0xE0E040BF;
                    break;
                case IrTvAction::VolUp:
                    code = 0xE0E0E01F;
                    break;
                case IrTvAction::VolDown:
                    code = 0xE0E0D02F;
                    break;
                case IrTvAction::Mute:
                    code = 0xE0E0F00F;
                    break;
                case IrTvAction::ChUp:
                    code = 0xE0E048B7;
                    break;
                case IrTvAction::ChDown:
                    code = 0xE0E008F7;
                    break;
                case IrTvAction::Input:
                    code = 0xE0E0807F;
                    break;
                default:
                    break;
            }
            g_irsend.sendSAMSUNG(code);
            break;
        }
        case IrTvBrand::Sony: {
            uint16_t code = 0;
            switch (action) {
                case IrTvAction::Power:
                    code = 0xA90;
                    break;
                case IrTvAction::VolUp:
                    code = 0x490;
                    break;
                case IrTvAction::VolDown:
                    code = 0xC90;
                    break;
                case IrTvAction::Mute:
                    code = 0x290;
                    break;
                case IrTvAction::ChUp:
                    code = 0x090;
                    break;
                case IrTvAction::ChDown:
                    code = 0x890;
                    break;
                case IrTvAction::Input:
                    code = 0xA50;
                    break;
                default:
                    break;
            }
            g_irsend.sendSony(code, 12, 2);
            break;
        }
        case IrTvBrand::Lg: {
            uint32_t code = 0;
            switch (action) {
                case IrTvAction::Power:
                    code = 0x20DF10EF;
                    break;
                case IrTvAction::VolUp:
                    code = 0x20DF40BF;
                    break;
                case IrTvAction::VolDown:
                    code = 0x20DFC03F;
                    break;
                case IrTvAction::Mute:
                    code = 0x20DF906F;
                    break;
                case IrTvAction::ChUp:
                    code = 0x20DF00FF;
                    break;
                case IrTvAction::ChDown:
                    code = 0x20DF807F;
                    break;
                case IrTvAction::Input:
                    code = 0x20DFD02F;
                    break;
                default:
                    break;
            }
            g_irsend.sendLG(code);
            break;
        }
        case IrTvBrand::Panasonic: {
            uint32_t data = 0;
            switch (action) {
                case IrTvAction::Power:
                    data = 0x100BCBD;
                    break;
                case IrTvAction::VolUp:
                    data = 0x1000405;
                    break;
                case IrTvAction::VolDown:
                    data = 0x1008485;
                    break;
                case IrTvAction::Mute:
                    data = 0x1004C4D;
                    break;
                case IrTvAction::ChUp:
                    data = 0x1002C2D;
                    break;
                case IrTvAction::ChDown:
                    data = 0x100ACAD;
                    break;
                case IrTvAction::Input:
                    data = 0x100A0A1;
                    break;
                default:
                    break;
            }
            g_irsend.sendPanasonic(0x4004, data);
            break;
        }
        case IrTvBrand::Nec: {
            uint64_t code = 0;
            switch (action) {
                case IrTvAction::Power:
                    code = 0x00FF02FD;
                    break;
                case IrTvAction::VolUp:
                    code = 0x00FFA857;
                    break;
                case IrTvAction::VolDown:
                    code = 0x00FFE01F;
                    break;
                case IrTvAction::Mute:
                    code = 0x00FF906F;
                    break;
                case IrTvAction::ChUp:
                    code = 0x00FFE21D;
                    break;
                case IrTvAction::ChDown:
                    code = 0x00FF629D;
                    break;
                case IrTvAction::Input:
                    code = 0x00FF22DD;
                    break;
                default:
                    break;
            }
            g_irsend.sendNEC(code);
            break;
        }
        default:
            setTxStatus("fail");
            return;
    }
    setTxStatus("sent");
}

static void sendAcState() {
    ensureIrReady();
    stdAc::state_t s = {};
    s.protocol = acProtocol(g_ac_brand);
    s.model = -1;
    s.power = g_ac_power;
    s.mode = g_ac_mode;
    s.degrees = g_ac_temp;
    s.celsius = true;
    s.fanspeed = g_ac_fan;
    s.swingv = stdAc::swingv_t::kOff;
    s.swingh = stdAc::swingh_t::kOff;
    s.quiet = false;
    s.turbo = false;
    s.econo = false;
    s.light = true;
    s.filter = false;
    s.clean = false;
    s.beep = false;
    s.sleep = -1;
    s.clock = -1;
    g_irac.sendAc(s, nullptr);
    setTxStatus("sent");
}

static void sendCurrent() {
    if (g_category == IrCategory::TV) {
        sendTvAction();
    } else {
        sendAcState();
    }
}

static int getHorizontalDelta(const Keyboard_Class::KeysState& status) {
    int delta = 0;
    for (const uint8_t hid : status.hid_keys) {
        if (hid == 0x50 || hid == 0x36) {
            delta = -1;
        } else if (hid == 0x4F || hid == 0x38) {
            delta = 1;
        }
    }
    for (const char c : status.word) {
        if (c == ',') {
            delta = -1;
        } else if (c == '/') {
            delta = 1;
        }
    }
    return delta;
}

static int getVerticalDelta(const Keyboard_Class::KeysState& status) {
    int delta = 0;
    for (const uint8_t hid : status.hid_keys) {
        if (hid == 0x52 || hid == 0x33) {
            delta = -1;
        } else if (hid == 0x51 || hid == 0x37) {
            delta = 1;
        }
    }
    for (const char c : status.word) {
        if (c == ';') {
            delta = -1;
        } else if (c == '.') {
            delta = 1;
        }
    }
    return delta;
}

static void cycleAcMode(const int delta) {
    static const stdAc::opmode_t modes[] = {
        stdAc::opmode_t::kCool, stdAc::opmode_t::kHeat, stdAc::opmode_t::kDry,
        stdAc::opmode_t::kFan,  stdAc::opmode_t::kAuto,
    };
    constexpr int n = 5;
    int idx = 0;
    for (int i = 0; i < n; i++) {
        if (modes[i] == g_ac_mode) {
            idx = i;
            break;
        }
    }
    idx = (idx + delta + n) % n;
    g_ac_mode = modes[idx];
}

static void cycleAcFan(const int delta) {
    static const stdAc::fanspeed_t fans[] = {
        stdAc::fanspeed_t::kAuto, stdAc::fanspeed_t::kMin, stdAc::fanspeed_t::kLow,
        stdAc::fanspeed_t::kMedium, stdAc::fanspeed_t::kHigh, stdAc::fanspeed_t::kMax,
    };
    constexpr int n = 6;
    int idx = 0;
    for (int i = 0; i < n; i++) {
        if (fans[i] == g_ac_fan) {
            idx = i;
            break;
        }
    }
    idx = (idx + delta + n) % n;
    g_ac_fan = fans[idx];
}

static void adjustAcField(const int delta) {
    switch (static_cast<IrAcField>(g_ac_field)) {
        case IrAcField::Power:
            g_ac_power = !g_ac_power;
            break;
        case IrAcField::Mode:
            cycleAcMode(delta == 0 ? 1 : delta);
            break;
        case IrAcField::Temp: {
            int t = static_cast<int>(g_ac_temp) + (delta == 0 ? 1 : delta);
            if (t < 16) {
                t = 16;
            }
            if (t > 30) {
                t = 30;
            }
            g_ac_temp = static_cast<uint8_t>(t);
            break;
        }
        case IrAcField::Fan:
            cycleAcFan(delta == 0 ? 1 : delta);
            break;
        default:
            break;
    }
}

static int drawHelpKey(const int y, const char key, const char* text) {
    int cx = APP_CONTENT_X + drawKeyBadge(APP_CONTENT_X, y, key, 1);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
    M5Cardputer.Display.setCursor(cx, y + 1);
    M5Cardputer.Display.print(text);
    return y + 12;
}

static int drawHelpArrows(const int y, const char* text) {
    int cx = APP_CONTENT_X + drawArrowBadge(APP_CONTENT_X, y, 1);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
    M5Cardputer.Display.setCursor(cx, y + 1);
    M5Cardputer.Display.print(text);
    return y + 12;
}

static int drawHelpTitle(const int y, const char* title) {
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setTextColor(APP_COLOR_LABEL, BLACK);
    M5Cardputer.Display.setCursor(APP_CONTENT_X, y);
    M5Cardputer.Display.print(title);
    return y + INFO_LINE_H_2X;
}

static int drawHelpText(const int y, const char* text) {
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
    M5Cardputer.Display.setCursor(APP_CONTENT_X, y);
    M5Cardputer.Display.print(text);
    return y + 12;
}

static void drawIrHelpPage() {
    beginAppScreen("Help");
    int y = APP_CONTENT_Y;
    if (g_help_page == 0) {
        y = drawHelpTitle(y, "Keys");
        y = drawHelpKey(y, 't', "TV / AC");
        y = drawHelpArrows(y, "brand / field");
        y = drawHelpKey(y, '-', "value-  = value+");
        y = drawHelpKey(y, ' ', "ent/spc send");
        y = drawHelpKey(y, 'h', "help / close");
    } else {
        y = drawHelpTitle(y, "Notes");
        y = drawHelpText(y, "TX only GPIO44");
        y = drawHelpText(y, "AC: state frame");
        y = drawHelpText(y, "Xiaomi: Coolix");
        y = drawHelpText(y, "OEM? try Midea");
        y = drawHelpText(y, "aim IR window");
    }

    const int hint_y = M5Cardputer.Display.height() - 12;
    M5Cardputer.Display.fillRect(APP_CONTENT_X, hint_y, 236, 12, BLACK);
    int cx = APP_CONTENT_X;
    cx += drawArrowBadge(cx, hint_y, 1);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
    M5Cardputer.Display.setCursor(cx, hint_y);
    M5Cardputer.Display.print("page ");
    cx += M5Cardputer.Display.textWidth("page ");
    char buf[8];
    snprintf(buf, sizeof(buf), "%d/%d", g_help_page + 1, IR_HELP_PAGE_COUNT);
    M5Cardputer.Display.setCursor(cx, hint_y);
    M5Cardputer.Display.print(buf);
    drawHelpHintRight("close");
    updateAppHeaderStatus();
}

static void drawSelectedLine(const int x, const int y, const char* label, const char* value,
                             const bool selected) {
    const uint16_t label_c = selected ? APP_COLOR_MENU_KEY : APP_COLOR_LABEL;
    const uint16_t value_c = selected ? WHITE : APP_COLOR_VALUE;
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(label_c, BLACK);
    M5Cardputer.Display.setCursor(x, y);
    M5Cardputer.Display.print(label);
    M5Cardputer.Display.setTextColor(value_c, BLACK);
    M5Cardputer.Display.setCursor(x + 48, y);
    M5Cardputer.Display.print(value);
}

static void drawIrMain() {
    if (!g_screen_ready) {
        beginAppScreen("Infrared");
        g_screen_ready = true;
    } else {
        clearAppContentArea();
    }

    int y = APP_CONTENT_Y;
    char buf[24];

    // type 用 2x
    const char* type_val = g_category == IrCategory::TV ? "TV" : "AC";
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setTextColor(APP_COLOR_LABEL, BLACK);
    M5Cardputer.Display.setCursor(APP_CONTENT_X, y);
    M5Cardputer.Display.print("type");
    M5Cardputer.Display.setTextColor(APP_COLOR_VALUE, BLACK);
    M5Cardputer.Display.setCursor(APP_CONTENT_X + 56, y);
    M5Cardputer.Display.print(type_val);
    y += INFO_LINE_H_2X + 2;

    if (g_category == IrCategory::TV) {
        drawSelectedLine(APP_CONTENT_X, y, "brand", tvBrandName(g_tv_brand), false);
        y += 12;
        drawSelectedLine(APP_CONTENT_X, y, "key", tvActionName(g_tv_action), true);
        y += 14;
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setTextColor(APP_COLOR_MUTED, BLACK);
        M5Cardputer.Display.setCursor(APP_CONTENT_X, y);
        M5Cardputer.Display.print("1pwr 2v+ 3v- 4mut");
    } else {
        drawSelectedLine(APP_CONTENT_X, y, "brand", acBrandName(g_ac_brand), false);
        y += 12;
        snprintf(buf, sizeof(buf), "%s", g_ac_power ? "ON" : "OFF");
        drawSelectedLine(APP_CONTENT_X, y, "power", buf,
                         g_ac_field == static_cast<int>(IrAcField::Power));
        y += 12;
        drawSelectedLine(APP_CONTENT_X, y, "mode", acModeName(g_ac_mode),
                         g_ac_field == static_cast<int>(IrAcField::Mode));
        y += 12;
        snprintf(buf, sizeof(buf), "%uC", static_cast<unsigned>(g_ac_temp));
        drawSelectedLine(APP_CONTENT_X, y, "temp", buf,
                         g_ac_field == static_cast<int>(IrAcField::Temp));
        y += 12;
        drawSelectedLine(APP_CONTENT_X, y, "fan", acFanName(g_ac_fan),
                         g_ac_field == static_cast<int>(IrAcField::Fan));
    }

    if (g_tx_status[0] != '\0' && static_cast<int32_t>(millis() - g_tx_status_until_ms) < 0) {
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setTextColor(APP_COLOR_OK, BLACK);
        M5Cardputer.Display.setCursor(APP_CONTENT_X + 160, APP_CONTENT_Y);
        M5Cardputer.Display.print(g_tx_status);
    }

    const int hint_y = M5Cardputer.Display.height() - 12;
    M5Cardputer.Display.fillRect(APP_CONTENT_X, hint_y, 236, 12, BLACK);
    int cx = APP_CONTENT_X;
    cx += drawArrowBadge(cx, hint_y, 1);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
    M5Cardputer.Display.setCursor(cx, hint_y);
    M5Cardputer.Display.print("nav ");
    cx += M5Cardputer.Display.textWidth("nav ");
    cx += drawKeyBadge(cx, hint_y, 't', 1);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
    M5Cardputer.Display.setCursor(cx, hint_y);
    M5Cardputer.Display.print("type");
    drawHelpHintRight("help");
}

static void redrawIr() {
    if (g_help_visible) {
        drawIrHelpPage();
    } else {
        drawIrMain();
    }
}

void enterIrApp() {
    g_screen_ready = false;
    g_help_visible = false;
    g_help_page = 0;
    g_tx_status = "";
    ensureIrReady();
    redrawIr();
}

void updateIrApp() {
    if (g_help_visible) {
        return;
    }
    if (g_tx_status[0] != '\0' && static_cast<int32_t>(millis() - g_tx_status_until_ms) >= 0) {
        g_tx_status = "";
        drawIrMain();
    }
}

void handleIrApp(const Keyboard_Class::KeysState& status) {
    if (!status.word.empty() || !status.hid_keys.empty() || status.enter || status.space ||
        status.del) {
        // continue
    } else {
        return;
    }

    for (const char c : status.word) {
        if (c == 'h' || c == 'H') {
            g_help_visible = !g_help_visible;
            if (g_help_visible) {
                g_help_page = 0;
            }
            g_screen_ready = false;
            redrawIr();
            return;
        }
    }

    if (g_help_visible) {
        const int hdelta = getHorizontalDelta(status);
        if (hdelta != 0) {
            g_help_page = (g_help_page + hdelta + IR_HELP_PAGE_COUNT) % IR_HELP_PAGE_COUNT;
            drawIrHelpPage();
        }
        return;
    }

    for (const char c : status.word) {
        if (c == 't' || c == 'T') {
            g_category = (g_category == IrCategory::TV) ? IrCategory::AC : IrCategory::TV;
            drawIrMain();
            return;
        }
    }

    // TV 快捷数字键
    if (g_category == IrCategory::TV) {
        for (const char c : status.word) {
            int action = -1;
            if (c == '1') {
                action = static_cast<int>(IrTvAction::Power);
            } else if (c == '2') {
                action = static_cast<int>(IrTvAction::VolUp);
            } else if (c == '3') {
                action = static_cast<int>(IrTvAction::VolDown);
            } else if (c == '4') {
                action = static_cast<int>(IrTvAction::Mute);
            } else if (c == '5') {
                action = static_cast<int>(IrTvAction::ChUp);
            } else if (c == '6') {
                action = static_cast<int>(IrTvAction::ChDown);
            } else if (c == '7') {
                action = static_cast<int>(IrTvAction::Input);
            }
            if (action >= 0) {
                g_tv_action = action;
                sendCurrent();
                drawIrMain();
                return;
            }
        }
    }

    const int hdelta = getHorizontalDelta(status);
    if (hdelta != 0) {
        if (g_category == IrCategory::TV) {
            const int n = static_cast<int>(IrTvBrand::Count);
            g_tv_brand = (g_tv_brand + hdelta + n) % n;
        } else {
            const int n = static_cast<int>(IrAcBrand::Count);
            g_ac_brand = (g_ac_brand + hdelta + n) % n;
        }
        drawIrMain();
        return;
    }

    const int vdelta = getVerticalDelta(status);
    if (vdelta != 0) {
        if (g_category == IrCategory::TV) {
            const int n = static_cast<int>(IrTvAction::Count);
            g_tv_action = (g_tv_action + vdelta + n) % n;
        } else {
            const int n = static_cast<int>(IrAcField::Count);
            g_ac_field = (g_ac_field + vdelta + n) % n;
        }
        drawIrMain();
        return;
    }

    for (const char c : status.word) {
        if (c == '-' || c == '_') {
            if (g_category == IrCategory::AC) {
                adjustAcField(-1);
                drawIrMain();
            }
            return;
        }
        if (c == '=' || c == '+') {
            if (g_category == IrCategory::AC) {
                adjustAcField(1);
                drawIrMain();
            }
            return;
        }
    }

    if (status.enter || status.space) {
        sendCurrent();
        drawIrMain();
        return;
    }
}
