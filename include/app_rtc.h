#pragma once

#include "M5Cardputer.h"

void enterRtcApp();
void updateRtcApp();
// 每帧轮询 BtnA（Countdown / Stopwatch 开始暂停）
void pollTimeAppBtnA();
void handleTimeApp(const Keyboard_Class::KeysState& status);
bool isTimePureMode();
