#include "mijia_control.h"
#include "miio_client.h"
#include <cstring>

static int clampInt(const int value, const int min_v, const int max_v) {
    if (value < min_v) {
        return min_v;
    }
    if (value > max_v) {
        return max_v;
    }
    return value;
}

static bool startsWith(const char* str, const char* prefix) {
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

MijiaDevKind mijiaClassifyModel(const char* model) {
    if (model == nullptr || model[0] == '\0') {
        return MijiaDevKind::GENERIC;
    }
    if (startsWith(model, "yeelink.light.")) {
        return MijiaDevKind::LIGHT;
    }
    if (strcmp(model, "dmaker.fan.p5") == 0) {
        return MijiaDevKind::FAN_P5;
    }
    if (strstr(model, ".fan.") != nullptr) {
        return MijiaDevKind::FAN_GENERIC;
    }
    if (strcmp(model, "dmaker.airpurifier.f20") == 0) {
        return MijiaDevKind::AIR_PURIFIER_F20;
    }
    if (strstr(model, "airfryer") != nullptr || strstr(model, ".fryer.") != nullptr) {
        return MijiaDevKind::AIR_FRYER;
    }
    if (strstr(model, ".plug.") != nullptr) {
        return MijiaDevKind::PLUG;
    }
    return MijiaDevKind::GENERIC;
}

// 按型号取色温范围（K）；无调节能力时 min == max
static void mijiaLightCtRange(const char* model, int& min_k, int& max_k) {
    min_k = 2700;
    max_k = 6500;
    if (model == nullptr || model[0] == '\0') {
        return;
    }
    if (strcmp(model, "yeelink.light.lamp2") == 0) {
        min_k = 2500;
        max_k = 4800;
        return;
    }
    if (strcmp(model, "yeelink.light.lamp1") == 0 || strcmp(model, "yeelink.light.lamp4") == 0) {
        min_k = 2700;
        max_k = 5000;
        return;
    }
    if (strstr(model, ".mono") != nullptr) {
        min_k = 2700;
        max_k = 2700;
        return;
    }
    if (!startsWith(model, "yeelink.light.")) {
        min_k = 0;
        max_k = 0;
    }
}

bool mijiaLightSupportsCt(const char* model) {
    int min_k = 0;
    int max_k = 0;
    mijiaLightCtRange(model, min_k, max_k);
    return max_k > min_k;
}

// 床头灯 2 / color8 / color2 等支持 HSV 色相（j/k 调节）
bool mijiaLightSupportsHue(const char* model) {
    if (model == nullptr) {
        return false;
    }
    return strstr(model, "bslamp2") != nullptr || strstr(model, "color8") != nullptr ||
           strstr(model, "color2") != nullptr;
}

void mijiaResetUiState(MijiaUiState& state) {
    state.power_known = false;
    state.power_on = false;
    state.extra_known = false;
    state.bright = 50;
    state.color_temp = 4000;
    state.ct_known = false;
    state.ct_min = 2700;
    state.ct_max = 6500;
    state.hue = 0;
    state.hue_known = false;
    state.sat = 100;
    state.speed = 0;
    state.roll = false;
    state.roll_angle = 90;
    state.mode = 0;
    state.fan_level = 0;
    state.aqi = 0;
    state.fryer_time = 15;
    strncpy(state.status, "ready", sizeof(state.status));
}

static void applyResult(MijiaUiState& state, const MiioResult& result) {
    strncpy(state.status, result.message, sizeof(state.status) - 1);
    state.status[sizeof(state.status) - 1] = '\0';
}

// 状态查询失败时统一显示 timeout
static void applyRefreshResult(MijiaUiState& state, const MiioResult& result) {
    if (!result.ok) {
        strncpy(state.status, "timeout", sizeof(state.status));
        return;
    }
    applyResult(state, result);
}

void mijiaRefreshDevice(const MijiaDevice* dev, MijiaUiState& state) {
    if (dev == nullptr) {
        strncpy(state.status, "no device", sizeof(state.status));
        state.power_known = false;
        state.extra_known = false;
        return;
    }

    strncpy(state.status, "query...", sizeof(state.status));
    state.power_known = false;
    state.extra_known = false;

    const MijiaDevKind kind = mijiaClassifyModel(dev->model);
    MiioResult result{};

    switch (kind) {
        case MijiaDevKind::LIGHT: {
            bool bright_known = false;
            bool ct_known = false;
            bool hue_known = false;
            mijiaLightCtRange(dev->model, state.ct_min, state.ct_max);
            result = miioGetLightStatus(dev->ip, dev->token, state.power_on, state.bright,
                                        bright_known, state.color_temp, ct_known,
                                        mijiaLightSupportsHue(dev->model), state.hue, hue_known,
                                        state.sat);
            if (result.ok) {
                state.power_known = true;
                state.ct_known = ct_known && mijiaLightSupportsCt(dev->model);
                state.hue_known = hue_known && mijiaLightSupportsHue(dev->model);
                state.extra_known = bright_known || state.ct_known || state.hue_known;
            }
            break;
        }
        case MijiaDevKind::FAN_P5:
            result = miioFanP5GetStatus(dev->ip, dev->token, state.power_on, state.speed,
                                        state.roll, state.mode, state.roll_angle);
            if (result.ok) {
                state.power_known = true;
                state.extra_known = true;
            }
            break;
        case MijiaDevKind::FAN_GENERIC:
            result = miioFanGetStatus(dev->ip, dev->token, state.power_on, state.speed);
            if (result.ok) {
                state.power_known = true;
                state.extra_known = true;
            }
            break;
        case MijiaDevKind::AIR_PURIFIER_F20:
            result = miioF20GetStatus(dev->ip, dev->token, dev->id, state.power_on, state.mode,
                                      state.fan_level, state.aqi);
            if (result.ok) {
                state.power_known = true;
                state.extra_known = true;
            }
            break;
        case MijiaDevKind::AIR_FRYER:
            // MIoT：不能用 get_prop power，否则会超时显示“离线”
            result = miioFryerGetStatus(dev->ip, dev->token, dev->id, state.power_on, state.mode,
                                        state.fan_level, state.fryer_time, state.aqi);
            if (result.ok) {
                state.power_known = true;
                state.extra_known = true;
            }
            break;
        default: {
            bool power = false;
            result = miioGetPower(dev->ip, dev->token, power);
            if (result.ok) {
                state.power_known = true;
                state.power_on = power;
            }
            break;
        }
    }

    applyRefreshResult(state, result);
}

void mijiaSetDevicePower(const MijiaDevice* dev, MijiaUiState& state, const bool on) {
    if (dev == nullptr) {
        strncpy(state.status, "no device", sizeof(state.status));
        return;
    }

    strncpy(state.status, on ? "turn on..." : "turn off...", sizeof(state.status));

    const MijiaDevKind kind = mijiaClassifyModel(dev->model);
    MiioResult result{};

    switch (kind) {
        case MijiaDevKind::FAN_P5:
            result = miioFanP5SetPower(dev->ip, dev->token, on);
            break;
        case MijiaDevKind::AIR_PURIFIER_F20:
            result = miioF20SetPower(dev->ip, dev->token, dev->id, on);
            break;
        case MijiaDevKind::AIR_FRYER: {
            const int temp = state.fan_level >= 40 ? state.fan_level : 180;
            const int mins = state.fryer_time > 0 ? state.fryer_time : 15;
            result = miioFryerSetPower(dev->ip, dev->token, dev->id, on, temp, mins);
            // 以设备回读为准，避免乐观 UI 显示已开但实际仍是待机
            if (result.ok) {
                MiioResult st =
                    miioFryerGetStatus(dev->ip, dev->token, dev->id, state.power_on, state.mode,
                                       state.fan_level, state.fryer_time, state.aqi);
                if (st.ok) {
                    state.power_known = true;
                    state.extra_known = true;
                    if (on && !state.power_on) {
                        // 多半还在关机/待机：需机身先开机，或锅未推到位
                        st.ok = false;
                        strncpy(st.message, "need wake?", sizeof(st.message));
                        st.message[sizeof(st.message) - 1] = '\0';
                    }
                    result = st;
                }
            }
            break;
        }
        default:
            result = miioSetPower(dev->ip, dev->token, on);
            break;
    }

    if (result.ok) {
        state.power_known = true;
        // 炸锅已用回读状态，勿用请求值覆盖
        if (kind != MijiaDevKind::AIR_FRYER) {
            state.power_on = on;
        }
    }
    applyResult(state, result);
}

void mijiaAdjustBright(const MijiaDevice* dev, MijiaUiState& state, const int delta) {
    if (dev == nullptr) {
        return;
    }

    int target = state.extra_known ? state.bright : 50;
    target = clampInt(target + delta, 1, 100);
    mijiaSetBrightPercent(dev, state, target);
}

void mijiaSetBrightPercent(const MijiaDevice* dev, MijiaUiState& state, const int percent) {
    if (dev == nullptr) {
        return;
    }

    const int target = clampInt(percent, 1, 100);
    strncpy(state.status, "bright...", sizeof(state.status));
    const MiioResult result = miioSetBright(dev->ip, dev->token, target);
    if (result.ok) {
        state.extra_known = true;
        state.bright = target;
        state.power_on = true;
        state.power_known = true;
    }
    applyResult(state, result);
}

void mijiaAdjustColorTemp(const MijiaDevice* dev, MijiaUiState& state, const int delta_k) {
    if (dev == nullptr || !mijiaLightSupportsCt(dev->model)) {
        return;
    }

    int target = state.ct_known ? state.color_temp : (state.ct_min + state.ct_max) / 2;
    target = clampInt(target + delta_k, state.ct_min, state.ct_max);
    mijiaSetColorTemp(dev, state, target);
}

void mijiaSetColorTemp(const MijiaDevice* dev, MijiaUiState& state, const int kelvin) {
    if (dev == nullptr || !mijiaLightSupportsCt(dev->model)) {
        return;
    }

    const int target = clampInt(kelvin, state.ct_min, state.ct_max);
    strncpy(state.status, "ct...", sizeof(state.status));
    const MiioResult result = miioSetColorTemp(dev->ip, dev->token, target);
    if (result.ok) {
        state.ct_known = true;
        state.color_temp = target;
        state.extra_known = true;
        state.power_on = true;
        state.power_known = true;
    }
    applyResult(state, result);
}

void mijiaAdjustHue(const MijiaDevice* dev, MijiaUiState& state, const int delta) {
    if (dev == nullptr || !mijiaLightSupportsHue(dev->model)) {
        return;
    }
    int target = state.hue_known ? state.hue : 0;
    target = ((target + delta) % 360 + 360) % 360;
    mijiaSetHue(dev, state, target);
}

void mijiaSetHue(const MijiaDevice* dev, MijiaUiState& state, const int hue) {
    if (dev == nullptr || !mijiaLightSupportsHue(dev->model)) {
        return;
    }
    const int target = ((hue % 360) + 360) % 360;
    const int sat = state.sat > 0 ? state.sat : 100;
    strncpy(state.status, "hue...", sizeof(state.status));
    const MiioResult result = miioSetHue(dev->ip, dev->token, target, sat);
    if (result.ok) {
        state.hue_known = true;
        state.hue = target;
        state.sat = sat;
        state.extra_known = true;
        state.power_on = true;
        state.power_known = true;
    }
    applyResult(state, result);
}

void mijiaAdjustFanP5Speed(const MijiaDevice* dev, MijiaUiState& state, const int delta) {
    if (dev == nullptr) {
        return;
    }

    int target = state.extra_known ? state.speed : 30;
    target = clampInt(target + delta, 0, 100);

    strncpy(state.status, "speed...", sizeof(state.status));
    const MiioResult result = miioFanP5SetSpeed(dev->ip, dev->token, target);
    if (result.ok) {
        state.extra_known = true;
        state.speed = target;
        state.power_known = true;
        state.power_on = target > 0;
    }
    applyResult(state, result);
}

void mijiaToggleFanP5Roll(const MijiaDevice* dev, MijiaUiState& state) {
    if (dev == nullptr) {
        return;
    }

    const bool next = !state.roll;
    strncpy(state.status, "roll...", sizeof(state.status));
    const MiioResult result = miioFanP5SetRoll(dev->ip, dev->token, next);
    if (result.ok) {
        state.roll = next;
        state.extra_known = true;
    }
    applyResult(state, result);
}

void mijiaToggleFanP5Mode(const MijiaDevice* dev, MijiaUiState& state) {
    if (dev == nullptr) {
        return;
    }

    // mode 字段：0=normal 1=nature
    const int next = state.mode == 1 ? 0 : 1;
    const char* mode = next == 1 ? "nature" : "normal";

    strncpy(state.status, "mode...", sizeof(state.status));
    const MiioResult result = miioFanP5SetMode(dev->ip, dev->token, mode);
    if (result.ok) {
        state.mode = next;
        state.extra_known = true;
    }
    applyResult(state, result);
}

void mijiaCycleFanP5Angle(const MijiaDevice* dev, MijiaUiState& state) {
    if (dev == nullptr) {
        return;
    }

    // P5 支持：30 / 60 / 90 / 120 / 140
    static const int kAngles[] = {30, 60, 90, 120, 140};
    constexpr int kCount = 5;
    int idx = 0;
    for (int i = 0; i < kCount; i++) {
        if (state.roll_angle == kAngles[i]) {
            idx = (i + 1) % kCount;
            break;
        }
        if (state.roll_angle < kAngles[i]) {
            idx = i;
            break;
        }
        idx = 0;
    }
    const int next = kAngles[idx];

    strncpy(state.status, "angle...", sizeof(state.status));
    const MiioResult result = miioFanP5SetAngle(dev->ip, dev->token, next);
    if (result.ok) {
        state.roll_angle = next;
        state.extra_known = true;
    }
    applyResult(state, result);
}

void mijiaSetFanSpeedLevel(const MijiaDevice* dev, MijiaUiState& state, const int level) {
    if (dev == nullptr) {
        return;
    }

    const int lv = clampInt(level, 1, 4);
    strncpy(state.status, "speed...", sizeof(state.status));
    const MiioResult result = miioFanSetSpeedLevel(dev->ip, dev->token, lv);
    if (result.ok) {
        state.speed = lv;
        state.extra_known = true;
        state.power_known = true;
        state.power_on = true;
    }
    applyResult(state, result);
}

void mijiaSetPurifierMode(const MijiaDevice* dev, MijiaUiState& state, const int mode) {
    if (dev == nullptr) {
        return;
    }

    const int m = clampInt(mode, 0, 5);
    strncpy(state.status, "mode...", sizeof(state.status));
    const MiioResult result = miioF20SetMode(dev->ip, dev->token, dev->id, m);
    if (result.ok) {
        state.mode = m;
        state.extra_known = true;
        state.power_known = true;
        state.power_on = true;
    }
    applyResult(state, result);
}

void mijiaAdjustPurifierFanLevel(const MijiaDevice* dev, MijiaUiState& state, const int delta) {
    if (dev == nullptr) {
        return;
    }

    int target = state.extra_known ? state.fan_level : 1;
    target = clampInt(target + delta, 0, 5);

    strncpy(state.status, "fan lv...", sizeof(state.status));
    const MiioResult result = miioF20SetFanLevel(dev->ip, dev->token, dev->id, target);
    if (result.ok) {
        state.fan_level = target;
        state.extra_known = true;
    }
    applyResult(state, result);
}

void mijiaAdjustFryerTemp(const MijiaDevice* dev, MijiaUiState& state, const int delta) {
    if (dev == nullptr) {
        return;
    }
    int temp = state.fan_level >= 40 ? state.fan_level : 180;
    temp = clampInt(temp + delta, 40, 200);
    const int mins = state.fryer_time > 0 ? state.fryer_time : 15;
    strncpy(state.status, "temp...", sizeof(state.status));
    const MiioResult result = miioFryerSetTempTime(dev->ip, dev->token, dev->id, temp, mins);
    if (result.ok) {
        state.fan_level = temp;
        state.fryer_time = mins;
        state.extra_known = true;
    }
    applyResult(state, result);
}

void mijiaAdjustFryerTime(const MijiaDevice* dev, MijiaUiState& state, const int delta) {
    if (dev == nullptr) {
        return;
    }
    int mins = state.fryer_time > 0 ? state.fryer_time : 15;
    mins = clampInt(mins + delta, 1, 120);
    const int temp = state.fan_level >= 40 ? state.fan_level : 180;
    strncpy(state.status, "time...", sizeof(state.status));
    const MiioResult result = miioFryerSetTempTime(dev->ip, dev->token, dev->id, temp, mins);
    if (result.ok) {
        state.fan_level = temp;
        state.fryer_time = mins;
        state.extra_known = true;
    }
    applyResult(state, result);
}
