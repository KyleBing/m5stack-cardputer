#pragma once

#include "M5Cardputer.h"
#include <WString.h>

void enterMijiaApp();
void drawMijiaApp();
void handleMijiaApp(const String& key);
// 概览列表翻页（方向键 / ; , . / / [ ]）
bool handleMijiaOverviewPageNav(const Keyboard_Class::KeysState& status);
// 控制页切换设备（方向键 / ; , . / / [ ]）
bool handleMijiaDeviceNav(const Keyboard_Class::KeysState& status);
// 主循环调用：应用异步查询结果并重绘
void updateMijiaApp();
