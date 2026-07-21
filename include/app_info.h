#pragma once

#include "M5Cardputer.h"

// 系统信息 / 内存占用（入口：字母 i；原 Options → info）
void enterInfoApp();
void updateInfoApp();
void handleInfoApp(const Keyboard_Class::KeysState& status);
