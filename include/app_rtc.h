#pragma once

#include "M5Cardputer.h"

void enterRtcApp();
void updateRtcApp();
void handleTimeApp(const Keyboard_Class::KeysState& status);
bool isTimePureMode();
