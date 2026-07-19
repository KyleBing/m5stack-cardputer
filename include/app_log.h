#pragma once

#include "M5Cardputer.h"

// 错误/诊断日志查看（入口：主菜单 Fn+i）
void enterLogApp();
void handleLogApp(const Keyboard_Class::KeysState& status);
