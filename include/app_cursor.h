#pragma once

#include "M5Cardputer.h"

void enterCursorApp();
void leaveCursorApp();
void drawCursorApp();
void updateCursorApp();
void pollCursorBtnA();
bool isCursorDisplayBlanked();
// 空闲满 5 分钟后可用 1s 主循环间隔
bool isCursorIdleSlowLoop();
void handleCursorApp(const Keyboard_Class::KeysState& status);
// 开机写入 /cursor.err：复位原因 + 堆，崩溃后仍可查
void cursorLogBootBreadcrumb();
