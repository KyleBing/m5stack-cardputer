#pragma once

#include <cstddef>
#include <WString.h>

// 截图目录（LittleFS）
static constexpr const char* SHOT_DIR = "/shot";

// 把当前屏存为 /shot/app_<slug>_NNN.bmp（序号自动递增）
// 成功时 out_name 写入文件名（不含路径）；失败时 err 有原因
bool saveScreenshotToFlash(const char* app_slug, char* out_name, size_t out_name_len, char* err,
                           size_t err_len);

// 全局热键：Fn+s 截图（任意界面）；返回 true 表示已消费按键
bool tryHandleScreenshotHotkey();

// 统计 /shot 下截图数量
int countScreenshots();

// 统计 /shot 下截图占用字节数
size_t screenshotsUsedBytes();

// LittleFS 总容量 / 已用 / 剩余（失败时写 0）
void getFlashDataSpace(size_t* total, size_t* used, size_t* free_bytes);

// 删除全部截图，返回删除个数
int clearAllScreenshots();

// 删除最后一张截图（.last 或按时间最新），成功返回 true
bool deleteLastScreenshot();

// 开机恢复：上次启动崩溃则删最后一张；空间不足则继续删到可用
// 并打上 boot_pending；setup 成功结束须调 markScreenshotBootOk()
void recoverScreenshotsOnBoot();

// setup 正常跑完后调用，清除 boot_pending
void markScreenshotBootOk();

// 校验并打开截图文件路径（仅允许 /shot/app_*.bmp）
bool isSafeShotPath(const String& uri);
