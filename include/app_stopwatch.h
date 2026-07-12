#pragma once

#include "M5Cardputer.h"

void enterStopwatchApp();
void updateStopwatchApp();
void redrawStopwatchApp();
void handleStopwatchApp(const Keyboard_Class::KeysState& status);
// 每帧调用：BtnA 开始/暂停（wasPressed 仅单帧有效）
void pollStopwatchBtnA();
