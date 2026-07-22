# Time 时间

主菜单按键：`t`

四个子模式：**Uptime**、**Clock**、**Countdown**、**Stopwatch**；支持 Pure 纯净显示（隐藏 tip / 多余 UI）。

## 截图

![time-uptime](/shots/time-uptime.png)
![time-clock](/shots/time-clock.png)
![time-countdown](/shots/time-countdown.png)
![time-stopwatch](/shots/time-stopwatch.png)
![time-help](/shots/time-help.png)

## 快捷键

### 模式切换（Help 汇总）

| 按键 | 作用 |
|------|------|
| `u` | Uptime 运行时长 |
| `t` | Clock 时钟 |
| `c` | Countdown 倒计时 |
| `s` | Stopwatch 秒表 |
| `p` | Pure 纯净显示开关 |
| `r` | 同步时间 / 重置（视模式） |
| **BtnGO** | 开始 / 暂停 / 继续 |
| `h` | Help |

### Uptime

| 按键 | 作用 |
|------|------|
| `p` | Pure |
| `h` | Help |

### Clock

| 按键 | 作用 |
|------|------|
| `r` | NTP 同步（需 WiFi） |
| `p` | Pure |
| `h` | Help |

### Countdown · 设置 SETUP

| 按键 | 作用 |
|------|------|
| 方向键 | 调节时分秒字段 |
| `0`–`9` | 数字输入 |
| **BtnGO** | 开始 |
| `p` / `h` | Pure / Help |

### Countdown · 运行 / 暂停

| 按键 | 作用 |
|------|------|
| **BtnGO** | 暂停 / 继续 |
| `r` | 重置 |
| `p` / `h` | Pure / Help |

### Stopwatch

| 按键 | 作用 |
|------|------|
| **BtnGO** | 开始 / 暂停 / 继续 |
| `r` | 重置 |
| `p` / `h` | Pure / Help |

## 使用说明

1. 默认进入模式由配置 `time.default`（如 `up`）决定；`time.pure` 可默认开启 Pure。
2. 时钟依赖 RTC；有网时可 `r` 做 NTP，时区见配置 `timezone`（如 `CST-8`）。
3. 倒计时到期会响铃提示（音量受 `sound.volume` 影响）；后台计时在离开 App 后仍可能继续，到期弹窗提醒。
4. Pure 适合把 Cardputer 当桌面时钟 / 秒表使用。
