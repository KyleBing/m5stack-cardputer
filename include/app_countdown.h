#pragma once

#include "M5Cardputer.h"

void enterCountdownApp();
void updateCountdownApp();
void redrawCountdownApp();
void handleCountdownApp(const Keyboard_Class::KeysState& status);
// 每帧调用：BtnA 开始/暂停（wasPressed 仅单帧有效）
void pollCountdownBtnA();
