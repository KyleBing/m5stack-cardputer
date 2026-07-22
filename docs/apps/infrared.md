# Infrared 红外

主菜单按键：`x`

通过 GPIO44 发射红外，支持 **TV** 与 **AC** 两类遥控。默认类别与品牌来自配置 `infrared`。

## 截图

![infrared-tv](/shots/infrared-tv.png)
![infrared-ac](/shots/infrared-ac.png)
![infrared-help](/shots/infrared-help.png)

## 快捷键

主界面无底栏 tip（按键印在面板上）；完整说明见 `h` Help。

| 按键 | 作用 |
|------|------|
| `h` | Help |
| `Tab` | 切换品牌 |
| `t` | TV ↔ AC |
| `p` | 电源 |
| `-` | TV：音量− · AC：温度− |
| `=` | （对称调节，见面板） |
| `[` | TV：频道相关 |
| **BtnA** / `Space` / `Enter` | 发送当前动作 |
| TV：`m` / `i` | Mute / Input 等 |
| AC：`m` / `f` | 模式 / 风速 |

具体动作以屏上按键垫与 Help 为准。

## 使用说明

### TV

支持品牌：Samsung、Sony、LG、Panasonic、NEC。

常见动作：Power、Vol±、Mute、Ch±、Input。选好品牌与动作后发送。

### AC

支持品牌：Midea、Gree、Haier、AUX、Hisense、Xiaomi。

- 模式图标：制冷 / 制热 / 除湿 / 送风 / Auto  
- 顶栏风速图标：auto / min / low / med / high / max  
- 调节温度、模式、风速后发送整帧状态  

默认项可在 [Options](./options) 或 Config Web 修改：`infrared.default`（`tv`/`ac`）、`tv_brand`、`ac_brand`。

> 红外协议因机型而异；无效时请换品牌或确认发射头对准接收窗。
