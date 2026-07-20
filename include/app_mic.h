#pragma once

#include "M5Cardputer.h"
#include <WString.h>

void enterMicApp();
void leaveMicApp();
void updateMicApp();
void handleMicApp(const Keyboard_Class::KeysState& status);
// 每帧轮询：BtnA 开始/停止录音（wasPressed 仅单帧有效）
void pollMicBtnA();
