# Config 配网

主菜单按键：`u`

启动 SoftAP 或接入已有局域网，用浏览器打开 Web 页面管理 `config.json`：设备、编组、WiFi、亮度、红外默认、截图下载等。

## 截图

![config-ap](/shots/config-ap.png)
![config-lan](/shots/config-lan.png)
![config-help](/shots/config-help.png)

## 快捷键

| 状态 | 按键 | 作用 |
|------|------|------|
| 连接中 / AP | `a` | 跳过 LAN，切到 AP 热点 |
| LAN 已就绪 | `a` | 切换为 AP 热点模式 |
| 失败 | 重新进入 `u` | 重试 |
| 任意 | `h` | Help |
| Help 内 | `a` | AP |
| Help 内 | `l` | 重试 LAN |

返回菜单：`ESC` / `GO`。

## 使用说明

1. 进入 App 后优先尝试用已保存的 WiFi 连 LAN；失败或按 `a` 则开 SoftAP。
2. 屏上会显示 IP 或热点 SSID；用手机 / 电脑浏览器访问该地址。
3. Web 常见入口：
   - 设备与编组编辑
   - `/wifi`：多 WiFi 档案（最多 5 条）与 Active
   - `/shots`：截图预览、下载、清空 TF / Flash
   - `/about`：固件版本信息
   - RGB565 烘焙：`POST /bake-rgb565`（现场生成图标 bake 文件）
4. 修改保存后写入 LittleFS；部分项（如反色、音量）会立即生效。

配网完成后可按 `ESC` 回菜单，再进 [Mijia](./mijia) / [WiFi](./wifi) 使用。
