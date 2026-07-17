# Cursor App API 约定

> 非官方接口，来自 `cursor.com` 仪表盘逆向。可能随时变更。  
> 实现：`src/app_cursor.cpp` / `include/app_cursor.h`  
> Token：`config.json` → `cursor_token`（`WorkosCursorSessionToken` 值）

## 鉴权

所有请求带 Cookie：

```
Cookie: WorkosCursorSessionToken=<cursor_token>
```

POST 还需：

```
Origin: https://cursor.com
Content-Type: application/json
```

Host：`https://cursor.com`

## 端点一览

| 方法 | 路径 | 用途 | UI |
|------|------|------|-----|
| GET | `/api/auth/me` | 取 `id`（userId） | 内部 |
| POST | `/api/dashboard/get-current-period-usage` | 周期用量（优先） | Summary：Auto/API/ond/reset |
| GET | `/api/usage-summary` | 周期用量（回退） | 同上 |
| POST | `/api/dashboard/get-filtered-usage-events` | 事件列表 | last 页 / 24h·7d·30d 图 |

## 字段 → 界面

### Summary（`s`）

| UI | 来源 |
|----|------|
| Auto % | `100 - planUsage.autoPercentUsed` |
| API % | `100 - planUsage.apiPercentUsed` |
| `ond $used/$limit` | `spendLimitUsage` 个人/池，或 `individualUsage.onDemand` |
| `reset Nd \| MM-DD` | `billingCycleEnd` |

金额单位均为**美分**，显示前 ÷100。

### last（`u`，切页再拉）

| UI | 来源 |
|----|------|
| 日期 / 时间 | `usageEventsDisplay[].timestamp`（ms） |
| 模型 | `.model` |
| token | `input+output+cacheWrite+cacheRead` |
| Inc 徽章 | `.kind` 含 `INCLUDED` / `Included` |

### 图表（`d`/`w`/`m`）

按事件 `timestamp` + `tokenUsage.totalCents`（或 `chargedCents`）累加到小时/日桶。

## 模板文件

| 文件 | 说明 |
|------|------|
| [auth-me.json](./auth-me.json) | `GET /api/auth/me` |
| [period-usage.json](./period-usage.json) | `POST .../get-current-period-usage` |
| [usage-summary.json](./usage-summary.json) | `GET /api/usage-summary` |
| [usage-events.json](./usage-events.json) | `POST .../get-filtered-usage-events` |
| [usage-events-request.json](./usage-events-request.json) | 事件列表请求体示例 |

## 拉取时机

- 进入 App：只拉 auth + period（摘要）
- 切到 last：再拉最近 10 条事件
- 切到图表：按需分页拉事件并聚合
