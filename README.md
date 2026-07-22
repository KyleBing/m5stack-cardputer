# Sparks
为 Cardputer-ADV 制作的个人固件，主要功能是米家设备控制、cursor 信息查看。

_该固件内容为全英文，英文缩写比较多，所以需要有良好的英语基础才能比较方便的使用。_



## 一、功能说明
基于 M5Stack 的库进行的开发，功能有：

| App | 英文名 | 快捷按键 | 功能 |
|-----|--------|----------|------|
| 米家 | Mijia | `m` | 设备状态查看、控制 |
| 时钟 | Time | `t` | 开机时间，实时时间，秒表，倒计时 |
| Cursor | Cursor Dashboard | `c` | Cursor 信息查看，TOKEN 余量，使用概况（24h,7d,30d） |
| 红外 | Infrared | `x` | 电视、空调红外遥控，适配主流品牌 |
| 麦克风 | Mic | `r` | 测试，波型查看 |
| IMU | IMU | `g` | BMI270 6轴参数查看 |
| WiFi | WiFi | `w` | WiFi 设置，支持多组配置选择 |
| AP/LAN | Config | `u` | Web 配置服务，针对 config.json 的修改 |
| 系统配置 | Options | `o` | 屏幕、音量，首选项配置 |
| 图标查看 | Icons | `a` | 固件内的图标查看 |
| 字体 | Font | `f` | 预览系统自带字体 |
| 休眠 | Sleep | `s` | 不关机的状态下，浅睡、深睡 |
| 摩斯密码 | Morse | `j` | 按键出摩斯码音频 |
| 键盘 | HID Keyboard | `k` | 蓝牙、USB 键盘 |
| LED | RGB LED | `l` | ESP-32 板载 LED 测试 |
| 蓝牙 | BLE | `b` | 附近蓝牙设备列表查看 |
| 屏幕 | Display | `d` | 屏幕测试，多背景色，线条，方块 |


## 二、文档
固件详细功能说明： [在线文档](https://kylebing.github.io/m5stack-cardputer-sparks/)

## 三、固件刷写

### 固件刷写说明

本项目推荐通过 [GitHub Releases](https://github.com/KyleBing/m5stack-cardputer-sparks/releases) 预编译固件刷写，支持 Windows、macOS、Linux。

#### 1. 下载固件

从 Releases 页面下载对应版本文件（如 `sparks-v1.0.0-*.bin`）：

| 文件 | 用途 |
|------|------|
| `*-merged.bin` | 整片烧录（推荐首次刷机 / M5Burner），地址 `0x0` |
| `*-firmware.bin` | 仅更新程序，地址 `0x10000` |
| `*-littlefs.bin` | 仅更新资源（图标等），地址 `0x670000` |

#### 2. 刷写工具

安装 **esptool**（任选其一）：

```bash
pip install -U esptool
```

也可使用 [M5Burner](https://docs.m5stack.com/en/download) 图形界面烧录 `*-merged.bin`（地址填 `0x0`）。

#### 3. 连接设备

USB 连接 Cardputer-ADV，确认串口后替换下方命令中的 `PORT`：

- Windows：`COM3` 等（设备管理器查看）
- macOS：`/dev/cu.usbmodem*`
- Linux：`/dev/ttyACM0` 或 `/dev/ttyUSB0`

#### 4. 刷写指令

**首次刷机 / 完全重置**（整片烧录，会格式化存储区）：

```bash
esptool.py --chip esp32s3 --port PORT --baud 921600 write_flash ^
  --flash_mode qio --flash_freq 80m --flash_size 8MB ^
  0x0 sparks-v1.0.0-merged.bin
```

> Windows CMD 用 `^` 续行；PowerShell / macOS / Linux 将 `^` 改为 `\`，或写成一行。

**常规升级**（保留 WiFi、配置等用户数据，仅更新程序）：

```bash
esptool.py --chip esp32s3 --port PORT --baud 921600 write_flash ^
  --flash_mode qio --flash_freq 80m --flash_size 8MB ^
  0x10000 sparks-v1.0.0-firmware.bin
```

**仅更新资源**（LittleFS，一般无需单独操作）：

```bash
esptool.py --chip esp32s3 --port PORT --baud 921600 write_flash ^
  --flash_mode qio --flash_freq 80m --flash_size 8MB ^
  0x670000 sparks-v1.0.0-littlefs.bin
```

将 `v1.0.0` 替换为实际版本号。

#### 5. PlatformIO 本地编译

```bash
pio run -e m5stack-cardputer -t upload      # 上传固件
pio run -e m5stack-cardputer -t uploadfs    # 上传 LittleFS
```

##### 常见问题

- 如未识别设备，请确认已安装 USB 驱动（详见设备说明书）。
- 刷写失败或设备启动异常，可按住设备按键复位后重试。
- 建议升级前备份重要配置；完全重置请使用 `*-merged.bin`。

详细图文教程与常见 FAQ 可见 [在线文档](https://kylebing.github.io/m5stack-cardputer-sparks/guide/getting-started)。


## 三、对 Cardputer 的喜爱
一直非常喜欢像素屏，尤其那种低功耗的单色像素屏，像诺基亚那种，靠反射光线看内容的更好。  
前端时间想自己攒一个小设备出来，带个低功耗的这种屏幕，然后实现一些自己感觉比较好玩的功能。后来算了算，弄下来还不如直接买个手表划算了，就没有再弄。  

但这个想法一直在，通过跟 gemini 的聊天，把我导向了 M5Stack 的相关产品，最初是看上了那个 [Stick](https://shop.m5stack.com/products/m5sticks3-esp32s3-mini-iot-dev-kit)。  
但非常不喜欢那种用四个方向键或更少键来导航菜单的操作，非常的繁琐，更何况是在那种非常廉价的按键上面去实现这种操作，感觉是反人类的交互设计。  
后来又看到 M5Stack 有 [Cardputer](https://shop.m5stack.com/products/m5stack-cardputer-adv-version-esp32-s3) 这个设备，要比 Stick 有更多的按键，这个不错。  
之前就非常喜欢黑莓手机里的全键盘每个按键对应一个应用的设计，Cardputer 有这么多按键就能非常方便的作为 app 启动器，直接按一个字母就进入一个固定 app，爽，当前固件就保留了黑莓里按 O 进入系统设置的操作。  

拿到这个设备之后感觉实现自己想法就有了地基一样，把自己喜欢的小工具做到了这上面，后面有更多新想法再慢慢往上加。  
Cardputer 这种基于 ESP32 的工具有一点是我非常喜欢的，就是秒开系统，要比安卓、linux 启动都要快，就很爽。

用 ai 能实现各种有意思的功能，就成了我近期最喜欢的玩具，我非常喜欢用它控制这种室内米家设备。

