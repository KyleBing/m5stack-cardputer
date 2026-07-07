#pragma once

// 启动 Web 配网：已连路由器时用 STA IP，否则开 AP 热点
bool startConfigWebServer();

// 停止 Web 服务；AP 模式会尝试恢复 config 中的 WiFi
void stopConfigWebServer();

// loop 中轮询 HTTP 请求
void handleConfigWebServer();

bool isConfigWebServerRunning();

// 当前是否通过路由器局域网 IP 提供配置页
bool isConfigWebStaMode();

const char* getConfigWebApSsid();
const char* getConfigWebApPass();
const char* getConfigWebUrl();
const char* getConfigWebStatus();

void drawWebApp();
void enterWebApp();
