# Mijia 米家

主菜单按键：`m`

本地 miIO（局域网）与 BLE 米家设备控制：详情、列表、宫格、编组、设备快捷键。

## 截图

::: tip 截图预留
将截图放到 `docs/public/shots/`，文件名如下。补全后刷新文档即可显示。
:::

**详情页**

![mijia-detail](/shots/mijia-detail.png)

**列表**

![mijia-list](/shots/mijia-list.png)

**宫格**

![mijia-grid](/shots/mijia-grid.png)

**编组**

![mijia-group](/shots/mijia-group.png)

**快捷选择 / 编辑热键**

![mijia-quick](/shots/mijia-quick.png)
![mijia-hotkey](/shots/mijia-hotkey.png)

**帮助**

![mijia-help](/shots/mijia-help.png)

## 快捷键

### 公共

| 按键 | 作用 |
|------|------|
| `ESC` / `GO` | 返回主菜单 |
| `h` | 打开 / 关闭 Help |
| `o` | 开 |
| `i` | 关 |
| `t` / **BtnA** | 切换开关 |
| `r` | 刷新状态（传感器为 BLE 扫描） |
| `q` | 快捷选择页 |
| `Fn` + `q` | 编辑当前设备热键 |
| `l` | 列表视图 |
| `g` | 宫格视图 |
| `d` | 编组视图 |
| `;` `,` / ↑← · `.` `/` / ↓→ | 切换设备 / 翻页（视当前视图） |
| `[` `]` | 宫格 / 快捷页翻页 |

### 列表 List

| 按键 | 作用 |
|------|------|
| 方向 / `;,.` `/` | 翻页 |
| `l` | 返回详情（或保持列表导航，以 tip 为准） |
| `g` | 切到宫格 |

底栏 tip 示例：页码 `pN/M` · 箭头 page · `l` · `g`

### 宫格 Grid

| 按键 | 作用 |
|------|------|
| 方向键 | 移动选中格 |
| `[` `]` | 翻页 |
| `1`–`9` | 直接选中对应格 |
| `o` / `i` / `t` / BtnA | 开 / 关 / 切换 |

### 编组 Group

| 按键 | 作用 |
|------|------|
| `,` `.` | 切换编组 |
| `o` / `i` / `t` | 整组开 / 关 / 切换 |
| `-` `=` / `1` `0` | 若组成员均为灯：亮度调节 / 预设 |
| `r` | 刷新 |
| `[` `]` | 翻组成员页 |

### 快捷选择 Quick（`q`）

| 按键 | 作用 |
|------|------|
| `q` | 返回 |
| `[` `]` | 翻页 |
| 已配置的 `a`–`z` / `0`–`9` | 跳到对应设备（`q` 保留给本页） |

### 热键编辑（`Fn+q`）

| 按键 | 作用 |
|------|------|
| `a`–`z` / `0`–`9` | 输入热键 |
| **BtnA** | 保存；冲突时确认替换 |

### 详情页 · 按设备类型

#### 灯 LIGHT（`yeelink.light.*`）

| 按键 | 作用 |
|------|------|
| `-` `=` | 亮度 − / + |
| `1` / `9` / `0` | 亮度预设（约 1% / 90% / 100%） |
| `[` `]` | 色温（支持 CT 的型号） |
| `j` `k` | 色相（`bslamp2` / `color8` / `color2`） |

#### 风扇 FAN_P5（`dmaker.fan.p5`）

| 按键 | 作用 |
|------|------|
| `-` `=` | 风速 |
| `w` | 摇头 |
| `m` | 模式 |
| `a` | 摆角 |
| `1` / `9` / `0` | 档位预设 |

#### 风扇 FAN_GENERIC（model 含 `.fan.`）

| 按键 | 作用 |
|------|------|
| `1`–`4` | 档位 |

#### 空气净化器 F20（`dmaker.airpurifier.f20`）

| 按键 | 作用 |
|------|------|
| `1`–`5` | 模式 |
| `-` `=` | 风速 |

#### 空气炸锅 AIR_FRYER

| 按键 | 作用 |
|------|------|
| `-` `=` | 温度 |
| `[` `]` | 时间 |

#### 温湿度 / BLE 事件（SENSOR_HT · BLE_EVENT）

| 按键 | 作用 |
|------|------|
| `r` | BLE 扫描刷新 |

#### 插座 PLUG / 其它 GENERIC

仅开关与刷新。

## 使用说明

### 配置设备

在 `config.json` 的 `devices[]` 中填写 `name` / `name_zh` / `id` / `ip` / `token` / `model` 等；BLE 传感器还需 `mac` 与 `ble.key`。也可在 [Config](./config) Web 中管理。

可选字段 `hotkey`（`a`–`z` / `0`–`9`，`q` 保留）用于一键跳转；列表 / 宫格名称旁会显示彩色热键字母。

编组写在 `device_groups[]`，成员用设备 `id` 关联。

### 视图切换

- **详情**：单设备完整状态与类型专属控制  
- **列表 `l`**：密集浏览  
- **宫格 `g`**：图标网格，适合快速开关  
- **编组 `d`**：按房间 / 场景批量控制  

无设备时 tip 会提示按 `u` 进入配网。

### 通信方式

- 多数 Wi-Fi 设备：局域网 miIO（需与 Cardputer 同一网段）  
- 温湿度计、人体 / 遥控等：BLE 扫描读广播  

## 已支持的设备类型（model）

固件按 `model` 字符串分类（见 `mijiaClassifyModel`）。下表为**控制能力**分类；未列到的型号会落入 GENERIC（仍可开关，若图标能匹配则显示对应图标）。

| 类型 | 匹配规则（示例） | 能力概要 | 图标 |
|------|------------------|----------|------|
| LIGHT | `yeelink.light.*` | 开关、亮度；部分 CT / 色相 | 见下 |
| FAN_P5 | 精确 `dmaker.fan.p5` | 风速、摇头、模式、摆角 | ![fan](/assets/icon/device/fan.png) |
| FAN_GENERIC | model 含 `.fan.` | 1–4 档 | ![fan](/assets/icon/device/fan.png) |
| AIR_PURIFIER_F20 | 精确 `dmaker.airpurifier.f20` | 模式、风速、AQI 等 | ![airpurifier](/assets/icon/device/airpurifier.png) |
| AIR_FRYER | 含 `airfryer` 或 `.fryer.` | 温度、时间 | ![fryer](/assets/icon/device/fryer.png) |
| PLUG | 含 `.plug.` | 开关 | ![plug](/assets/icon/device/plug.png) |
| SENSOR_HT | 含 `sensor_ht` 或 `.ht.` | BLE 温湿度 | ![sensor_ht](/assets/icon/device/sensor_ht.png) |
| BLE_EVENT | 含 `.motion.` / `.remote.` | BLE 事件设备 | （默认图标） |
| GENERIC | 其它 | 基础开关 | ![default](/assets/icon/device/default.png) |

### 灯型号补充

| model 特征 | 额外能力 |
|------------|----------|
| `yeelink.light.lamp2` | 色温约 2500–4800 K |
| `yeelink.light.lamp1` / `lamp4` | 色温约 2700–5000 K |
| 含 `.mono` | 色温固定（不可调） |
| 含 `bslamp2` / `color8` / `color2` | `j`/`k` 调色相 |

配置示例：`yeelink.light.lamp2`、`miaomiaoce.sensor_ht.t2`。

### 设备图标资源

图标按 model **子串**匹配（长名优先），资源来自固件 `data/icon/device/`：

| basename | 离 | 开 | 典型 model 片段 |
|----------|----|----|-----------------|
| `airpurifier` | ![off](/assets/icon/device/airpurifier.png) | ![on](/assets/icon/device/airpurifier_active.png) | airpurifier |
| `bslamp2` | ![off](/assets/icon/device/bslamp2.png) | ![on](/assets/icon/device/bslamp2_active.png) | bslamp2 |
| `lamp2` | ![off](/assets/icon/device/lamp2.png) | ![on](/assets/icon/device/lamp2_active.png) | lamp2 |
| `light` | ![off](/assets/icon/device/light.png) | ![on](/assets/icon/device/light_active.png) | 含 light 的回退 |
| `fan` | ![off](/assets/icon/device/fan.png) | ![on](/assets/icon/device/fan_active.png) | fan |
| `fryer` | ![off](/assets/icon/device/fryer.png) | ![on](/assets/icon/device/fryer_active.png) | fryer |
| `plug` | ![off](/assets/icon/device/plug.png) | ![on](/assets/icon/device/plug_active.png) | plug |
| `sensor_ht` | ![off](/assets/icon/device/sensor_ht.png) | ![on](/assets/icon/device/sensor_ht_active.png) | sensor_ht |
| `camera` | ![off](/assets/icon/device/camera.png) | ![on](/assets/icon/device/camera_active.png) | camera |
| `cooker` | ![off](/assets/icon/device/cooker.png) | ![on](/assets/icon/device/cooker_active.png) | cooker |
| `juicer` | ![off](/assets/icon/device/juicer.png) | ![on](/assets/icon/device/juicer_active.png) | juicer |
| `wifispeaker` | ![off](/assets/icon/device/wifispeaker.png) | ![on](/assets/icon/device/wifispeaker_active.png) | wifispeaker |
| `default` | ![off](/assets/icon/device/default.png) | ![on](/assets/icon/device/default_active.png) | 未匹配 |

宫格 / 列表另有 `*_25w.png` 小图；设备上优先使用同名 `.rgb565` 烘焙文件以加速绘制。
