# 功能目录

按主菜单分类浏览。每个 App 文档均包含：**简介与截图预留**、**快捷键**、**使用说明**。

截图命名：`docs/public/shots/{app}-{子功能}.png`。

## 智能家居

| 键 | App | 说明 |
|----|-----|------|
| `m` | [Mijia](./mijia) | 米家局域网内控制 |
| — | [获取设备 Token](./mijia-token) | 用云端工具导出 token / ble.key，转成 `config.json` 格式 |
| `x` | [Infrared](./infrared) | TV / AC 红外遥控（GPIO44） |

## 网络与配置

| 键 | App | 说明 |
|----|-----|------|
| `u` | [Config](./config) | 通过 AP、LAN 局域网 web 配置固件配置 config.json |
| `w` | [WiFi](./wifi) | 连接、扫描、切换已保存 wifi（最多 5 条） |
| `b` | [BLE](./ble) | 扫描附近 BLE 设备列表 |

## 时间与电源

| 键 | App | 说明 |
|----|-----|------|
| `t` | [Time](./time) | 钟表功能，系统运行时长 / 时钟 / 倒计时 / 秒表 |
| `p` | [Battery](./battery) | 实时电量，过去12小时的电量变化图表 |
| `s` | [Sleep](./sleep) | 进入浅睡 / 深睡 |

## 效率工具

| 键 | App | 说明 |
|----|-----|------|
| `c` | [Cursor](./cursor) | Cursor 用量摘要、日、星期、月用量图表 |
| `k` | [HID Keyboard](./hid-keyboard) | USB / BLE HID 键盘 |
| `j` | [Morse](./morse) | 摩斯电码发声 |

## 系统与信息

| 键 | App | 说明 |
|----|-----|------|
| `o` | [Options](./options) | 系统层面的配置，屏幕亮度、声音、时间、红外 |
| `i` | [Info](./info) | 查看 内存 / 芯片 / 固件 / 网络 / 运行信息 |
| `v` | [Version](./version) | 版本 关于 |

## 硬件调试与演示

| 键 | App | 说明 |
|----|-----|------|
| `g` | [IMU](./imu) | IMU(芯片BMI270) 姿态可视化 |
| `l` | [RGB LED](./rgb-led) | 板载 LED 测试 |
| `r` | [Mic](./mic) | 实时波形 + VU + 增益 展示 |
| `d` | [Display](./display) | 屏幕显示测试，色块 / 格 / 线条 |
| `a` | [Icons](./icons) | 固件内的图标查看 |
| `f` | [Font](./font) | 字体预览 |
| `n` / `e` | [I2C](./i2c) | 内部 / 外部 I2C 扫描 |
