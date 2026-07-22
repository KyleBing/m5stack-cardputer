# 获取米家设备 Token

局域网 miIO 与 BLE 设备都需要从小米云拿到 `token`（BLE 还需 `ble.key`）。推荐使用开源工具：

[Xiaomi Cloud Tokens Extractor](https://github.com/PiotrMachowski/Xiaomi-cloud-tokens-extractor)

登录小米云账号（邮箱 / 小米 ID + 密码，或扫码），选择服务器区域（国内一般选 `cn`，也可留空扫描全部区域），即可列出账号下全部设备。

## 工具输出示例

```text
NAME:     Xiaomi Wireless Switch (Bluetooth)
ID:       blt.3.1fe2te3rokg00
BLE KEY:  8ac1c06d5989a8618c1158488a1b46b7
MAC:      18:C2:3C:28:45:44
IP:       112.232.158.153
TOKEN:    9d2d1a7c337fb00ddcb1f60c
MODEL:    lumi.remote.mcn001
```

## 字段对照

| Extractor | `config.json` `devices[]` | 说明 |
|-----------|---------------------------|------|
| `NAME` | `name` / `name_zh` | 可改成自己习惯的英文 / 中文名 |
| `ID` | `id` | 原样复制；BLE 多为 `blt.3.…` |
| `TOKEN` | `token` | 必填 |
| `MODEL` | `model` | 必填；固件按此分类控制能力 |
| `MAC` | `mac` | 建议填写；BLE 设备必填 |
| `BLE KEY` | `ble.key` | 仅 BLE 设备需要 |
| `IP` | `ip` | Wi-Fi 设备填**局域网** IP；纯 BLE 可留空 |

可选：`hotkey`（`a`–`z` / `0`–`9`，`q` 保留）用于 [Mijia](./mijia) 快捷跳转。

## 改成固件格式

这一步用 ai 处理就好，很方便。

### BLE 设备（有 `BLE KEY`）

上例对应：

```json
{
  "name": "Switch",
  "name_zh": "蓝牙无线开关",
  "ip": "",
  "token": "9d2d1a7c337fb00ddcb1f60c",
  "model": "lumi.remote.mcn001",
  "id": "blt.3.1fe2te3rokg00",
  "mac": "18:C2:3C:28:45:44",
  "ble": {
    "key": "8ac1c06d5989a8618c1158488a1b46b7"
  }
}
```

注意：

- `BLE KEY` → `ble.key`（对象嵌套，不是顶层字符串）
- Extractor 里的 `IP` 经常是公网 / 云端地址，对本地 BLE 扫描无用，`ip` 留空即可
- `MAC` 保持 `AA:BB:CC:DD:EE:FF` 形式

### Wi-Fi 设备（无 `BLE KEY`）

```json
{
  "name": "DeskLamp",
  "name_zh": "台灯",
  "ip": "192.168.31.85",
  "token": "28398e0747a0adb0dca2b5c028c039c3",
  "model": "yeelink.light.lamp2",
  "id": "434412341",
  "mac": "B4:60:ED:03:2E:8A",
  "hotkey": "l"
}
```

注意：

- 不要照抄 Extractor 的公网 `IP`；填路由器里看到的局域网地址（与 Cardputer 同一网段）
- 无 `BLE KEY` 时不要写 `ble` 字段

## 写入配置

把对象放进 `config.json` 的 `devices` 数组，或通过 [Config](./config) Web 页面添加 / 编辑。保存后重启或重新进入 [Mijia](./mijia) 即可。

## 常见问题

- **登录失败**：确认用的是米家 / 小米云账号，不是石头等第三方 App 账号；可改用扫码登录；注意区域日限频（约 3～5 次 2FA）
- **能列出但控制失败**：检查 `token` / `model` 是否抄错；Wi-Fi 设备核对局域网 `ip`；BLE 设备核对 `mac` 与 `ble.key`
- **运行方式**：Windows 可下 release 里的 `token_extractor.exe`；其它平台见仓库 [README](https://github.com/PiotrMachowski/Xiaomi-cloud-tokens-extractor)
