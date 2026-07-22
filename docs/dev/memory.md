# 内存说明

Cardputer（ESP32-S3）可用的运行时内存有限。HTTPS、BLE、大 JSON、整图缓冲等都会吃掉 **内部 Heap**；碎片化时「总空闲还够、连续块不够」同样会失败。本文说明本固件里的内存分区、分配习惯、常见不足场景，以及如何在设备上查看并解读数值。

> 数值随固件版本、当前打开的 App、WiFi/BLE 状态波动；下文阈值以源码为准，属经验门槛而非硬件硬限。

## 内存分区（本机看到的几类）

| 类型 | 是什么 | 主要用途 |
|------|--------|----------|
| **Heap** | 片内 SRAM 动态堆 | `malloc` / `new` / Arduino `String` / TLS / WiFi / BLE / FreeRTOS 栈外缓冲 |
| **PSRAM** | 外置伪静态 RAM（若硬件有） | 大缓冲可放此处；本固件多数路径仍走内部 Heap |
| **Sketch** | Flash 里固件分区占用 | 编译出的 `.bin` 大小；与运行时 Heap 无关 |
| **LittleFS** | Flash 文件系统分区 | `config.json`、图标、日志等；满了是「盘满」，不是 Heap OOM |

Info 的 Memory 页把上述几项做成进度条，并额外显示 **Max Alloc** / **Min Free**（见下文）。

### Heap vs 碎片

- **Free Heap**（`ESP.getFreeHeap()`）：当前空闲字节总和。
- **Max Alloc**（`ESP.getMaxAllocHeap()`）：当前能一次 `malloc` 成功的**最大连续**块。

TLS / mbedTLS、较大 JSON、整图 RGB565 往往需要一块连续内存。因此：

```
Free 够大 + Max Alloc 很小  → 典型碎片，HTTPS / 大块分配仍会失败
```

**Min Free**（`ESP.getMinFreeHeap()`）是开机以来空闲堆曾降到的最低值，用来判断历史上是否逼近危险区。

## 固件里的分配机制（习惯）

本仓库没有统一的「内存池框架」，而是按场景选策略：

### 1. 栈 / 静态 / 全局

短生命周期、大小固定的小对象优先放栈或 `static`/`BSS`，避免反复 `malloc`。

### 2. 按需 `malloc`，离开再 `free`

大块、非常驻资源进入 App 时分配，退出释放，避免占满 Heap：

| 场景 | 做法 |
|------|------|
| 红外空调图标缓存 | 进入 IR 时 `malloc`，离开 `free`（`app_ir.cpp`） |
| 设备图标缩放 | 临时整图缓冲；1:1 则按行推屏不占整图 RAM |
| 截屏行缓冲 | 行级 `malloc`，失败返回 `oom heap=…` |
| 网页文件列表 | `FmEntry` 数组按上限 `malloc`，失败提示「内存不足」 |

### 3. 流式 / 分块，避免一次拼大缓冲

| 场景 | 做法 |
|------|------|
| Cursor 图表 | `pageSize=200` + 流式解析，降低大 JSON OOM |
| Config Web HTML | 分块发送，不拼巨大 `String` |

### 4. 动作前先看 Heap / Max Alloc

部分功能在「贵操作」前主动检查，不足则跳过并打日志，避免误报成网络错误：

| 功能 | 大致门槛（源码常量） | 不足时表现 |
|------|----------------------|------------|
| Cursor HTTPS | Free ≥ **48 KB** 且 Max Alloc ≥ **24 KB** | 跳过请求；日志 `http skip lowmem` |
| Cursor 建 fetch task | Free ≥ **90 KB**（栈约 32 KB） | 提示 `auth lowmem` |
| Config 开 AP | Free ≥ **40 KB** 且 Max Alloc ≥ **20 KB** | 跳过开 AP，串口 `[web] ap skip lowmem` |
| 米家 BLE 扫描 | Free ≥ **28 KB** | 不启扫描 |

### 5. Flash 与 RAM 分工

图标优先 **LittleFS 上的 `.rgb565` 直推**，减少解码临时堆；少量高频图标可进 App 时 RAM 缓存。详见 [图片处理与烘焙](./images)。

## 可能遇到的内存不足

### 用户可见现象

| 现象 | 常见原因 |
|------|----------|
| Cursor 提示 **`auth lowmem`** | Heap / Max Alloc 低于 HTTPS 或 task 门槛；不一定是 token 错 |
| Cursor `auth -1` / 连接类失败 | 有时是 TLS 分配失败被误报；低内存时固件会优先判 lowmem |
| Config 网页打不开 / 无法开 AP | 当前空闲或连续块不够开 SoftAP + HTTP |
| 米家扫描无结果、偶发卡顿 | BLE + WiFi 叠加；扫描前 Heap 过低会直接放弃 |
| 截屏失败 `oom` | 行缓冲 `malloc` 失败 |
| IR 图标偶发变慢 | 缓存 `malloc` 失败时回退 PNG / 逐次读 Flash |
| 网页文件管理报「内存不足」 | 目录项数组分配失败 |

### 容易把 Heap 打紧的组合

1. **WiFi 已连 + HTTPS（Cursor / 部分配网页）**：TLS 会话占用大，且要连续块。
2. **BLE（HID / 米家扫描）与 WiFi 同时活跃**：两套协议栈常驻，空闲骤降。
3. **大 JSON / 长 String 拼接**：Arduino `String` 反复扩容易碎片化。
4. **整图临时缓冲**：缩放绘制、未 bake 的大 PNG 解码。
5. **浅睡未真正释放**：浅睡侧重省电，**不会**像深睡重启那样清堆；唤醒后 Heap 状态基本延续。深睡唤醒会重启，堆重新开始。

### 碎片化 vs「真的用光」

- **真的用光**：Free Heap 持续很低（例如长期 &lt; 30–40 KB）。
- **碎片**：Free 看起来还有几十 KB，但 Max Alloc 只有十几 KB 甚至更低 → 大块分配仍失败。

缓解思路（使用侧）：退出占内存的 App → 关掉不需要的 BLE/WiFi 场景 → 必要时 **深睡或重启** 清碎片。开发侧：少用大 `String`、按需分配、流式解析、大块用完即 `free`。

## 如何查看系统内存

### 设备上：Info → Memory

1. 主菜单按 **`i`** 进入 [Info](/apps/info)。
2. 第一页即为 **Memory**（可用 `[` `]` 翻页）。

页面字段：

| 行 | 含义 | 怎么读 |
|----|------|--------|
| **Heap** `used/total xx%` | 片内堆已用 / 总量 | 百分比越高越紧；结合下面两行看 |
| **PSRAM** | 外置 RAM（无则不显示） | 有硬件才会出现；本固件主路径仍多依赖 Heap |
| **Sketch** | 固件占用 / 固件分区可用空间 | 变大说明固件变胖；与运行 OOM 无直接关系 |
| **LittleFS** | 文件系统已用 / 容量 | 满了会导致写配置/日志失败，不是 Heap OOM |
| **Max Alloc** | 当前最大连续可分配块 | **排查 HTTPS/大图时优先看这个** |
| **Min Free** | 历史最低空闲堆 | 曾跌得很低说明某段路径峰值危险 |

进度条颜色随占用升高变「更紧」（实现见 `app_info.cpp` 的 `memUsedBarColor`）。

### 串口 / 日志

- 开机日志会打 `heap=…`（`main` boot）。
- Cursor：LittleFS `/cursor.log`、错误轨 `/cursor.err`；Config Web 可看；内容常含 `heap=`、`max=`。
- 网页 / AP：`[web] ap skip lowmem heap=… max=…`。

开发时用 PlatformIO 串口监视（`115200`）即可对照。

### 代码里采样（调试用）

```cpp
ESP.getHeapSize();      // Heap 总量
ESP.getFreeHeap();      // 当前空闲
ESP.getMaxAllocHeap();  // 最大连续块
ESP.getMinFreeHeap();   // 历史最低空闲
ESP.getPsramSize();     // 0 = 无 PSRAM
ESP.getFreePsram();
```

Info 页的 used 计算方式：`used = total - free`（与上述 API 一致）。

## 数值解读（经验）

以下针对 **Heap / Max Alloc**，在「已连 WiFi、未额外开 BLE」的常见空闲状态作参考：

| 指标 | 较宽松 | 需留意 | 高风险 |
|------|--------|--------|--------|
| Free Heap | ≳ 100 KB | 40–90 KB | ≲ 40 KB |
| Max Alloc | ≳ 40 KB | 20–40 KB | ≲ 24 KB（HTTPS 门槛附近） |

对照本固件门槛：

- 要跑 **Cursor 拉取**：尽量保证 Free ≳ **90 KB**（建 task），且 Max Alloc ≳ **24 KB**。
- 只开 **Config AP**：Free ≳ **40 KB**、Max Alloc ≳ **20 KB**。
- **米家 BLE 扫描**：Free ≳ **28 KB**。

**Sketch / LittleFS** 百分比高：关心的是「还能不能烧更大固件 / 还能不能写文件」，不要和 Heap OOM 混为一谈。

## 排查建议（短流程）

1. 复现问题前打开 **Info → Memory**，记下 Heap%、**Max Alloc**、**Min Free**。
2. 若 Max Alloc 明显小于 Free，优先怀疑**碎片**；退出 App 或重启后再试。
3. Cursor / 网页失败时看 `/cursor.log` 或串口是否已有 `lowmem`，避免只查 token / 路由。
4. 改固件时：大块按需分配、离开释放；禁止在热路径拼超大 `String`；需要连续大块的操作前复用现有 `getFreeHeap` / `getMaxAllocHeap` 检查模式。

## 相关入口

- 界面：[Info](/apps/info)（`i`）
- 实现：`src/app_info.cpp`（采样与 Memory 页）
- 低内存防护示例：`src/app_cursor.cpp`、`src/app_web.cpp`、`src/mijia_ble.cpp`
- 资源与 RAM 策略：[图片处理与烘焙](./images)
