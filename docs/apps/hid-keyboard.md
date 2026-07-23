# Keyboard

主菜单按键：`k`

将 Cardputer 作为 **USB** 或 **BLE** HID 键盘，向主机发送按键。本 App **不能**用 `ESC` 退回菜单，请用 **BtnGO**。

## 截图

**USB / BLE**

<div class="shot-row">

![hid-usb](/shots/app_hidkeyboard_005.png)
![hid-ble](/shots/app_hidkeyboard_003.png)
![hid-ble-paired](/shots/app_hidkeyboard_002.png)

</div>

**Hosts 主机列表**

<div class="shot-row">

![hid-hosts-empty](/shots/app_hidkeyboard_001.png)
![hid-hosts](/shots/app_hidkeyboard_004.png)

</div>

**Help**

<div class="shot-row">

![hid-help](/shots/app_hidkb_001.png)

</div>

## 快捷键

### 模式与退出

| 按键 | 作用 |
|------|------|
| `Fn` + `u` | USB HID |
| `Fn` + `b` | BLE HID |
| `Fn` + `p` | BLE 主机列表（切换 / 配对） |
| **BtnGO** | 退出回主菜单（并断开 BLE） |
| `h` | Help |

### BLE 主机列表（`Fn+p`）

最多保存 **5** 台已配对主机；同时只连接一台。

| 按键 | 作用 |
|------|------|
| `1`–`5` / `;` `,` `.` `/` | 选择槽位 |
| Enter / Space | 切换到该主机（断开当前；目标机需在蓝牙里点一下 `Cardputer KB`） |
| `n` | 新配对（需有空槽；会拒绝旧主机抢连） |
| `d` | 删除当前槽位配对 |
| `p` / `h` | 关闭列表 |

### Fn 层（Help 第 2 页）

| 按键 | 作用 |
|------|------|
| `Fn` + `` ` `` | Esc |
| `Fn` + Backspace | Delete |
| `Fn` + `;` `,` `.` `/` | 方向键 |
| `Fn` + `1`–`0` | F1–F10 |
| `Fn` + `-` `=` | F11 / F12 |
| `Fn` + `A`/`a` | CapsLock |
| `Fn` + 修饰键 | 右侧修饰键映射（见 Help） |

## 使用说明

1. 用数据线连电脑选 USB，或 `Fn+b` 开 BLE 键盘后在主机端配对（`Fn+p` → `n`）。
2. 多台主机：`Fn+p` 打开列表，选槽后 Enter 切换。`reconnecting #N` 表示正在等目标电脑自动回连（多数几秒内成功；不行再在蓝牙里点一下 `Cardputer KB`）。
3. 新配对按 `n`：会拒绝旧主机抢连，再在新电脑上搜索配对。
4. 普通字符直接敲击；功能键走 Fn 层。
5. 退出务必 **BtnGO**（会断开 BLE）；避免与发送给主机的 Esc 混淆。
