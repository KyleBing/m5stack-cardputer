# WiFi

主菜单按键：`w`

查看当前连接、在已保存档案间切换、扫描附近热点并输入密码连接。配置支持最多 **5** 条 `wifis[]`，由 `wifi_active` 指定当前档案。

## 截图

**已连接**

<div class="shot-row">

![wifi-connected](/shots/app_wifi_001.png)

</div>

**已保存档案**

<div class="shot-row">

![wifi-saved](/shots/app_wifi_002.png)

</div>

**Help**

<div class="shot-row">

![wifi-help](/shots/app_wifi_003.png)

</div>

## 快捷键

| 状态 | 按键 | 作用 |
|------|------|------|
| 已连接 | `r` | 刷新状态 |
| 已连接 | `c` | 更换 / 选择档案 |
| 已连接 | `s` | 扫描 |
| 无档案 / 空闲 | `s` | 扫描 |
| 选择已保存 | `1`–`4`（及列表项）· 方向键 | 选档案 |
| 选择已保存 | `s` | 去扫描 |
| 输入密码 | 字母数字 · Enter | 连接 |
| 输入密码 | Del / Backspace | 删字 |
| 失败 | `r` | 重试 |
| 失败 | `c` | 更换 |
| 任意 | `h` | Help |

## 使用说明

1. **Connected**：显示 SSID、IP、RSSI 等；`r` 刷新，`c` 切档案，`s` 扫网。
2. **Pick saved**：从已保存列表选一条设为 active 并连接。
3. **Scan → Password**：选中热点后输入密码，Enter 连接；成功后可写入配置（与 Config Web `/wifi` 同源）。
4. 其它 App 只使用当前 **active** 档案，不会自动轮询多 SSID。

旧版顶层 `"wifi":{ssid,password}` 启动时会自动迁移为 `wifis[]`。
