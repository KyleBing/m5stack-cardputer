#pragma once

#include "M5Cardputer.h"

void enterCursorApp();
void leaveCursorApp();
void drawCursorApp();
void updateCursorApp();
void pollCursorBtnA();
bool isCursorDisplayBlanked();
void handleCursorApp(const Keyboard_Class::KeysState& status);
