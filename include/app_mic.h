#pragma once

#include "M5Cardputer.h"
#include <WString.h>

void enterMicApp();
void leaveMicApp();
void updateMicApp();
void handleMicApp(const Keyboard_Class::KeysState& status);
