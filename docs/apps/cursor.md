# Cursor 用量

主菜单按键：`c`

通过配置中的 `cursor.token` 拉取 Cursor 用量：摘要、最近记录、日 / 周 / 月图表。

## 截图

![cursor-summary](/shots/cursor-summary.png)
![cursor-chart](/shots/cursor-chart.png)
![cursor-help](/shots/cursor-help.png)

## 快捷键

| 按键 | 作用 |
|------|------|
| 方向键 · `;,.` `/` | 翻页 / 切视图 |
| `r` | 刷新数据 |
| `s` | 摘要 Summary |
| `u` | 最新 Latest |
| `d` / `w` / `m` | 日 / 周 / 月图表 |
| `[` `]` | 在记录间翻动 |
| **BtnGO** | 关屏（省电看图时） |
| `h` | Help |

## 使用说明

1. 在 `config.json` 或 Config Web 写入有效的 Cursor API token。
2. 需已连接 WiFi；首次请求会预解析 DNS，失败时自动重试。
3. Summary 看总量；`d`/`w`/`m` 看趋势；`u` 看最近几条。
4. 无网或 token 无效时界面会提示错误，检查 [WiFi](./wifi) 与配置后按 `r`。
