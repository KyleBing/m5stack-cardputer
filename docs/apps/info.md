# Info 信息

主菜单按键：`i`

只读系统信息，共 5 页：Memory / Chip / Fw / Net / Run。

## 截图

**Memory / Chip / Firmware / Network / Runtime**

<div class="shot-row">

![info-memory](/shots/app_info_001.png)
![info-chip](/shots/app_info_002.png)
![info-fw](/shots/app_info_003.png)
![info-net](/shots/app_info_004.png)
![info-run](/shots/app_info_005.png)

</div>

## 快捷键

| 按键 | 作用 |
|------|------|
| `[` `]` | 上一页 / 下一页 |
| 方向键等翻页键 | 翻页 |

底栏显示 `N/5` 页码。

## 使用说明

| 页 | 内容 |
|----|------|
| Memory | Heap / PSRAM / Sketch / LittleFS 用量与进度条 |
| Chip | 芯片型号、特性 |
| Fw | 固件版本、编译时间等 |
| Net | WiFi / IP / RSSI 等 |
| Run | 运行时长等相关 |

排查内存或配网问题时优先看 Memory 与 Net。字段含义、分配机制与常见不足场景见 [内存说明](/dev/memory)。
