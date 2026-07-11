#pragma once

#include "M5Cardputer.h"
#include <WString.h>

void enterMijiaApp();
void drawMijiaApp();
void handleMijiaApp(const String& key);
// 概览导航：宫格方向键选设备、[ ] 翻页；列表上下选中、左右翻页
bool handleMijiaOverviewPageNav(const Keyboard_Class::KeysState& status);
// 控制页切换设备（方向键 / ; , . / / [ ]）
bool handleMijiaDeviceNav(const Keyboard_Class::KeysState& status);
// 主循环调用：应用异步查询结果并重绘
void updateMijiaApp();
