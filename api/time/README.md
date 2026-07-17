# Time App · 校时数据格式

> 实现：`src/app_rtc.cpp`  
> Time App **没有**业务 HTTP JSON。校时走 **SNTP（UDP/123）**；固件层消费的是解析后的本地时间结构。

## 文档

| 文件 | 说明 |
|------|------|
| [ntp-request.json](./ntp-request.json) | 客户端发出的 SNTP 请求含义（逻辑字段） |
| [ntp-response.json](./ntp-response.json) | 服务器返回 / 固件解析后的数据格式 |
| [ntp-sync.md](./ntp-sync.md) | 流程、服务器列表、超时与 RTC 约定 |

## 传输层（SNTP）

| 项 | 值 |
|----|-----|
| 协议 | SNTP / NTP v4（RFC 5905 子集） |
| 传输 | UDP |
| 端口 | **123** |
| 报文长度 | 48 bytes（标准） |
| 主机 | `ntp.aliyun.com`、`pool.ntp.org`、`time.windows.com` |

由 ESP-IDF `configTzTime` / SNTP 客户端代发，应用层不组包。

## 应用层「返回」

成功后 `getLocalTime` / `readCurrentTime` 得到本地 `struct tm`，UI 格式化为：

- 时间：`HH:MM:SS`
- 日期：`YYYY-MM-DD`
- 来源标签：`NTP` 或 `RTC`

详见 [ntp-response.json](./ntp-response.json)。
