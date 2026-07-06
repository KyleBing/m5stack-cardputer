#pragma once

#include <cstdint>

struct MiioResult {
    bool ok;
    char message[96];
};

// 解析 32 位 hex token
bool miioParseTokenHex(const char* hex, uint8_t token[16]);

// 查询 power（get_prop，返回 on/off）
MiioResult miioGetPower(const char* ip, const char* token_hex, bool& on);

// 设置 power（set_power on/off，yeelink 等设备必须用此接口）
MiioResult miioSetPower(const char* ip, const char* token_hex, bool on);
