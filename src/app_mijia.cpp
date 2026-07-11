#include "app_mijia.h"
#include "app_common.h"
#include "app_config.h"
#include "app_header.h"
#include "app_colors.h"
#include "app_icons.h"
#include "app_mijia_ui.h"
#include "app_device_icons.h"
#include "mijia_control.h"
#include <WiFi.h>
#include <cctype>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static int mijiaDeviceIdx = 0;
static bool mijiaOverviewMode = false;
static bool mijiaOverviewGridMode = false;
static bool mijiaHelpVisible = false;
static int mijiaOverviewScrollIdx = 0;
static int mijiaOverviewEntryDeviceIdx = 0;
static MijiaUiState mijiaUi{};
static int mijiaRefreshGen = 0;
static volatile bool mijiaRefreshTaskRunning = false;
static volatile bool mijiaRefreshTimedOut = false;
static volatile bool mijiaNeedRedraw = false;
static uint32_t mijiaRefreshDeadlineMs = 0;

static constexpr uint32_t MIJIA_REFRESH_TIMEOUT_MS = 1000;
static constexpr uint32_t MIJIA_WIFI_TIMEOUT_MS = 12000;

enum class MijiaWifiPhase : uint8_t {
    IDLE,
    CONNECTING, // 联网中，不计入设备查询超时
    READY,
    FAILED,
};

static MijiaWifiPhase mijiaWifiPhase = MijiaWifiPhase::IDLE;
static uint32_t mijiaWifiDeadlineMs = 0;
static char mijiaNetStatus[32] = "";
static bool mijiaControlInitialized = false;
static int mijiaRenderedDeviceIdx = -1;
static MijiaUiState mijiaRenderedUi{};
static char mijiaRenderedNetStatus[32] = "";
static MijiaUiState mijiaOverviewUi[MIJIA_DEVICE_MAX];
static int mijiaOverviewRefreshQueue[MIJIA_GRID_PAGE_SIZE];
static int mijiaOverviewRefreshQueueLen = 0;
static int mijiaOverviewRefreshQueuePos = 0;

struct MijiaRefreshJob {
    int gen;
    int device_idx;
    uint32_t deadline_ms;
    MijiaDevice device;
    bool overview_cache = false;
};

static void scheduleMijiaRefresh();
static void requestMijiaRefresh();
static void scheduleMijiaOverviewRefreshJob();
static void requestMijiaOverviewPageRefresh();
static void drawMijiaHelpPage();
static void drawMijiaGridHelpPage();
static void invalidateMijiaControlSurface();
static void applyMijiaControlRefresh(bool force_full = false);
static void drawMijiaOverview(int& y);
static void refreshMijiaGridSelection(int old_idx, int new_idx);
static void refreshMijiaListSelection(int old_idx, int new_idx);
static bool handleMijiaGridSelectionNav(const Keyboard_Class::KeysState& status);
static bool handleMijiaListSelectionNav(const Keyboard_Class::KeysState& status);
static const MijiaDevice* getCurrentMijiaDevice();

// 是否已连上 config 中的 WiFi
static bool isMijiaConfigWifiConnected() {
    const AppConfig& cfg = getAppConfig();
    if (!cfg.loaded || cfg.wifi_ssid[0] == '\0') {
        return false;
    }
    return WiFi.status() == WL_CONNECTED && WiFi.SSID() == cfg.wifi_ssid;
}

// 联网阶段在 UI 上显示的网络状态（nullptr 表示不显示）
static const char* getMijiaNetworkStatusForUi() {
    if (mijiaUi.power_known || mijiaNetStatus[0] == '\0') {
        return nullptr;
    }
    return mijiaNetStatus;
}

// 进入米家后先非阻塞联网，成功后再启动设备查询
static void startMijiaWifiConnect() {
    const AppConfig& cfg = getAppConfig();
    mijiaNetStatus[0] = '\0';

    if (!cfg.loaded || cfg.wifi_ssid[0] == '\0') {
        mijiaWifiPhase = MijiaWifiPhase::FAILED;
        strncpy(mijiaUi.status, "未配置WiFi", sizeof(mijiaUi.status));
        strncpy(mijiaNetStatus, "未配置WiFi", sizeof(mijiaNetStatus));
        mijiaNeedRedraw = true;
        return;
    }

    if (isMijiaConfigWifiConnected()) {
        mijiaWifiPhase = MijiaWifiPhase::READY;
        strncpy(mijiaNetStatus, "已连接", sizeof(mijiaNetStatus));
        requestMijiaRefresh();
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(cfg.wifi_ssid, cfg.wifi_password);
    mijiaWifiPhase = MijiaWifiPhase::CONNECTING;
    mijiaWifiDeadlineMs = millis() + MIJIA_WIFI_TIMEOUT_MS;
    strncpy(mijiaUi.status, "正在连接网络", sizeof(mijiaUi.status));
    strncpy(mijiaNetStatus, "正在连接网络", sizeof(mijiaNetStatus));
    mijiaRefreshDeadlineMs = 0;
    mijiaNeedRedraw = true;
}

// 主循环轮询 WiFi，联网成功后再开始设备查询（不计入设备超时）
static void updateMijiaWifiConnect() {
    if (mijiaWifiPhase != MijiaWifiPhase::CONNECTING) {
        return;
    }

    if (isMijiaConfigWifiConnected()) {
        mijiaWifiPhase = MijiaWifiPhase::READY;
        strncpy(mijiaNetStatus, "已连接", sizeof(mijiaNetStatus));
        mijiaNeedRedraw = true;
        requestMijiaRefresh();
        return;
    }

    if (static_cast<int32_t>(millis() - mijiaWifiDeadlineMs) >= 0) {
        mijiaWifiPhase = MijiaWifiPhase::FAILED;
        strncpy(mijiaUi.status, "连接失败", sizeof(mijiaUi.status));
        strncpy(mijiaNetStatus, "连接失败", sizeof(mijiaNetStatus));
        mijiaRefreshDeadlineMs = 0;
        mijiaNeedRedraw = true;
    }
}

// 离开控制页后下次需整页初始化
static void invalidateMijiaControlSurface() {
    mijiaControlInitialized = false;
    mijiaRenderedDeviceIdx = -1;
    mijiaRenderedNetStatus[0] = '\0';
    mijiaResetUiState(mijiaRenderedUi);
}

// 记录当前已绘制到屏幕上的控制页状态
static void snapshotMijiaRenderedPanel(const char* net_status) {
    mijiaRenderedDeviceIdx = mijiaDeviceIdx;
    mijiaRenderedUi = mijiaUi;
    if (net_status != nullptr) {
        strncpy(mijiaRenderedNetStatus, net_status, sizeof(mijiaRenderedNetStatus) - 1);
        mijiaRenderedNetStatus[sizeof(mijiaRenderedNetStatus) - 1] = '\0';
    } else {
        mijiaRenderedNetStatus[0] = '\0';
    }
}

// 图标仅随开关态变化
static bool mijiaPanelIconVisualChanged(const MijiaUiState& old_ui, const MijiaUiState& new_ui) {
    const bool old_active = old_ui.power_known && old_ui.power_on;
    const bool new_active = new_ui.power_known && new_ui.power_on;
    return old_ui.power_known != new_ui.power_known || old_active != new_active;
}

// 右栏状态与控制区是否需重绘
static bool mijiaPanelControlsVisualChanged(const MijiaUiState& old_ui, const MijiaUiState& new_ui) {
    return old_ui.extra_known != new_ui.extra_known || old_ui.bright != new_ui.bright ||
           old_ui.color_temp != new_ui.color_temp || old_ui.ct_known != new_ui.ct_known ||
           old_ui.ct_min != new_ui.ct_min || old_ui.ct_max != new_ui.ct_max ||
           old_ui.speed != new_ui.speed || old_ui.roll != new_ui.roll || old_ui.mode != new_ui.mode ||
           old_ui.fan_level != new_ui.fan_level || old_ui.aqi != new_ui.aqi;
}

static bool mijiaPanelRightVisualChanged(const MijiaUiState& old_ui, const MijiaUiState& new_ui,
                                         const char* old_net, const char* new_net) {
    const bool old_inline = mijiaPanelShowsInlineStatus(old_ui.status, old_ui.power_known);
    const bool new_inline = mijiaPanelShowsInlineStatus(new_ui.status, new_ui.power_known);
    if (old_inline != new_inline) {
        return true;
    }
    if (new_inline && strcmp(old_ui.status, new_ui.status) != 0) {
        return true;
    }
    if (strcmp(old_net != nullptr ? old_net : "", new_net != nullptr ? new_net : "") != 0) {
        return true;
    }
    if (old_ui.power_known != new_ui.power_known) {
        return true;
    }
    return mijiaPanelControlsVisualChanged(old_ui, new_ui);
}

// 控制页局部刷新：仅重绘变化的图标/右栏区域
static void applyMijiaControlRefresh(const bool force_full) {
    const AppConfig& cfg = getAppConfig();
    const MijiaDevice* dev = getCurrentMijiaDevice();
    const int panel_y = APP_CONTENT_Y;

    if (!cfg.loaded || dev == nullptr) {
        if (!mijiaControlInitialized) {
            beginAppScreen("Mijia");
            mijiaControlInitialized = true;
        } else {
            clearAppContentArea();
        }
        int y = panel_y;
        drawInfoLine(APP_CONTENT_X, y, "cfg", "none");
        drawInfoLine(APP_CONTENT_X, y, "hint", "press u web");
        invalidateMijiaControlSurface();
        mijiaControlInitialized = true;
        return;
    }

    const MijiaDevKind kind = mijiaClassifyModel(dev->model);
    const char* net = getMijiaNetworkStatusForUi();
    const MijiaPanelLayout layout = calcMijiaPanelLayout(panel_y, APP_CONTENT_X);
    const bool device_changed = mijiaRenderedDeviceIdx != mijiaDeviceIdx;
    const bool icon_dirty = force_full || !mijiaControlInitialized || device_changed ||
                            mijiaPanelIconVisualChanged(mijiaRenderedUi, mijiaUi);
    const bool right_dirty =
        force_full || !mijiaControlInitialized || device_changed ||
        mijiaPanelRightVisualChanged(mijiaRenderedUi, mijiaUi, mijiaRenderedNetStatus, net);

    if (!icon_dirty && !right_dirty) {
        return;
    }

    if (force_full || !mijiaControlInitialized || device_changed) {
        if (!mijiaControlInitialized) {
            beginAppScreen("Mijia");
            mijiaControlInitialized = true;
        } else {
            clearAppContentArea();
        }
        drawMijiaDevicePanel(dev, kind, mijiaDeviceIdx, cfg.device_count, mijiaUi, APP_CONTENT_X,
                             panel_y, net);
        snapshotMijiaRenderedPanel(net);
        return;
    }

    if (icon_dirty) {
        M5Cardputer.Display.fillRect(layout.icon_x, layout.icon_y, layout.icon_px, layout.icon_px,
                                     BLACK);
        drawMijiaPanelIcon(dev, kind, layout, mijiaUi);
    }
    if (right_dirty) {
        const int clear_h = M5Cardputer.Display.height() - layout.right_top_y;
        M5Cardputer.Display.fillRect(layout.info_x, layout.right_top_y, layout.info_w, clear_h,
                                     BLACK);
        drawMijiaPanelRightColumn(dev, kind, layout, mijiaUi, net);
    }
    snapshotMijiaRenderedPanel(net);
}

// 按当前模式重绘控制页、概览或帮助页
static void redrawMijiaScreen() {
    if (mijiaHelpVisible) {
        invalidateMijiaControlSurface();
        if (mijiaOverviewGridMode) {
            drawMijiaGridHelpPage();
        } else {
            drawMijiaHelpPage();
        }
        return;
    }
    if (mijiaOverviewMode) {
        invalidateMijiaControlSurface();
        beginAppScreen("Mijia");
        int y = APP_CONTENT_Y;
        drawMijiaOverview(y);
        return;
    }
    applyMijiaControlRefresh(false);
}

static const MijiaDevice* getCurrentMijiaDevice() {
    const AppConfig& cfg = getAppConfig();
    if (!cfg.loaded || cfg.device_count == 0) {
        return nullptr;
    }
    if (mijiaDeviceIdx < 0) {
        mijiaDeviceIdx = 0;
    }
    if (mijiaDeviceIdx >= cfg.device_count) {
        mijiaDeviceIdx = cfg.device_count - 1;
    }
    return &cfg.devices[mijiaDeviceIdx];
}

// 后台任务结束后按需继续拉取最新设备
static void finishMijiaRefreshTask(const int job_gen) {
    mijiaRefreshTaskRunning = false;
    if (job_gen != mijiaRefreshGen && !mijiaRefreshTimedOut) {
        scheduleMijiaRefresh();
    }
}

// 后台任务：查询设备状态，结果仅在与当前 gen/索引一致时写回
static void mijiaRefreshTaskFn(void* arg) {
    MijiaRefreshJob* job = static_cast<MijiaRefreshJob*>(arg);
    const int job_gen = job->gen;
    const int job_idx = job->device_idx;
    const uint32_t job_deadline_ms = job->deadline_ms;
    const bool overview_cache = job->overview_cache;
    const MijiaDevice device = job->device;
    delete job;

    if (job_gen != mijiaRefreshGen || mijiaRefreshTimedOut) {
        finishMijiaRefreshTask(job_gen);
        if (overview_cache) {
            scheduleMijiaOverviewRefreshJob();
        }
        vTaskDelete(nullptr);
        return;
    }

    if (!isMijiaConfigWifiConnected()) {
        if (!overview_cache && job_gen == mijiaRefreshGen && job_idx == mijiaDeviceIdx) {
            strncpy(mijiaUi.status, "wifi fail", sizeof(mijiaUi.status));
            mijiaRefreshDeadlineMs = 0;
            mijiaNeedRedraw = true;
        }
        finishMijiaRefreshTask(job_gen);
        if (overview_cache) {
            scheduleMijiaOverviewRefreshJob();
        }
        vTaskDelete(nullptr);
        return;
    }

    if (job_gen != mijiaRefreshGen || mijiaRefreshTimedOut) {
        finishMijiaRefreshTask(job_gen);
        if (overview_cache) {
            scheduleMijiaOverviewRefreshJob();
        }
        vTaskDelete(nullptr);
        return;
    }

    MijiaUiState temp{};
    mijiaResetUiState(temp);
    mijiaRefreshDevice(&device, temp);

    const bool job_timed_out =
        job_deadline_ms != 0 && static_cast<int32_t>(millis() - job_deadline_ms) >= 0;
    if (mijiaRefreshTimedOut) {
        // UI 层已先判定超时，丢弃晚到结果
    } else if (job_timed_out && job_gen == mijiaRefreshGen && job_idx == mijiaDeviceIdx) {
        mijiaRefreshTimedOut = true;
        mijiaRefreshGen++;
        mijiaRefreshDeadlineMs = 0;
        strncpy(mijiaUi.status, "timeout", sizeof(mijiaUi.status));
        mijiaNeedRedraw = true;
    } else if (!job_timed_out && job_gen == mijiaRefreshGen) {
        if (overview_cache) {
            mijiaOverviewUi[job_idx] = temp;
            mijiaNeedRedraw = true;
        } else if (job_idx == mijiaDeviceIdx) {
            mijiaUi = temp;
            mijiaRefreshDeadlineMs = 0;
            mijiaNetStatus[0] = '\0';
            mijiaNeedRedraw = true;
        }
    }

    finishMijiaRefreshTask(job_gen);
    if (overview_cache) {
        scheduleMijiaOverviewRefreshJob();
    }
    vTaskDelete(nullptr);
}

// 启动一次异步状态查询（若已有任务在跑则等其结束后链式触发）
static void scheduleMijiaRefresh() {
    if (mijiaRefreshTaskRunning) {
        return;
    }

    const MijiaDevice* dev = getCurrentMijiaDevice();
    if (dev == nullptr) {
        strncpy(mijiaUi.status, "no device", sizeof(mijiaUi.status));
        mijiaUi.power_known = false;
        mijiaRefreshDeadlineMs = 0;
        mijiaNeedRedraw = true;
        return;
    }

    auto* job = new MijiaRefreshJob{};
    job->gen = mijiaRefreshGen;
    job->device_idx = mijiaDeviceIdx;
    job->device = *dev;
    job->deadline_ms = mijiaRefreshDeadlineMs;

    mijiaRefreshTaskRunning = true;
    if (xTaskCreate(mijiaRefreshTaskFn, "mijia_ref", 8192, job, 1, nullptr) != pdPASS) {
        delete job;
        mijiaRefreshTaskRunning = false;
        mijiaRefreshDeadlineMs = 0;
        strncpy(mijiaUi.status, "task fail", sizeof(mijiaUi.status));
        mijiaUi.power_known = false;
        mijiaNeedRedraw = true;
    }
}

// 请求刷新当前设备（不阻塞按键处理；需 WiFi 已就绪）
static void requestMijiaRefresh() {
    if (mijiaWifiPhase != MijiaWifiPhase::READY && !isMijiaConfigWifiConnected()) {
        return;
    }
    mijiaWifiPhase = MijiaWifiPhase::READY;
    mijiaRefreshTimedOut = false;
    mijiaRefreshGen++;
    mijiaRefreshDeadlineMs = millis() + MIJIA_REFRESH_TIMEOUT_MS;
    const bool was_query = strcmp(mijiaUi.status, "query...") == 0;
    strncpy(mijiaUi.status, "query...", sizeof(mijiaUi.status));
    if (!was_query) {
        mijiaNeedRedraw = true;
    }
    scheduleMijiaRefresh();
}

// 状态查询超过 1s 就判定超时，晚到结果会被 gen 丢弃
static void updateMijiaRefreshTimeout() {
    if (mijiaRefreshTimedOut || mijiaRefreshDeadlineMs == 0) {
        return;
    }
    if (static_cast<int32_t>(millis() - mijiaRefreshDeadlineMs) < 0) {
        return;
    }

    mijiaRefreshTimedOut = true;
    mijiaRefreshGen++;
    mijiaRefreshDeadlineMs = 0;
    strncpy(mijiaUi.status, "timeout", sizeof(mijiaUi.status));
    mijiaNeedRedraw = true;
}

// 立即切换设备并异步拉状态
static void switchMijiaDevice(const int delta, const int device_count) {
    mijiaDeviceIdx = (mijiaDeviceIdx + delta + device_count) % device_count;
    mijiaResetUiState(mijiaUi);
    strncpy(mijiaUi.status, "query...", sizeof(mijiaUi.status));
    applyMijiaControlRefresh(true);
    requestMijiaRefresh();
}

// 设置当前设备开关
static void setMijiaPower(const bool on) {
    const MijiaDevice* dev = getCurrentMijiaDevice();
    if (dev == nullptr) {
        strncpy(mijiaUi.status, "no device", sizeof(mijiaUi.status));
        return;
    }

    if (!ensureConfigWifi()) {
        strncpy(mijiaUi.status, "wifi fail", sizeof(mijiaUi.status));
        applyMijiaControlRefresh(false);
        return;
    }

    mijiaSetDevicePower(dev, mijiaUi, on);
    if (mijiaDeviceIdx >= 0 && mijiaDeviceIdx < MIJIA_DEVICE_MAX) {
        mijiaOverviewUi[mijiaDeviceIdx] = mijiaUi;
    }
    applyMijiaControlRefresh(false);
}

// 宫格概览：依次刷新当前页设备状态
static void scheduleMijiaOverviewRefreshJob() {
    if (mijiaOverviewRefreshQueuePos >= mijiaOverviewRefreshQueueLen) {
        return;
    }
    if (mijiaRefreshTaskRunning) {
        return;
    }

    const AppConfig& cfg = getAppConfig();
    const int idx = mijiaOverviewRefreshQueue[mijiaOverviewRefreshQueuePos++];
    if (!cfg.loaded || idx < 0 || idx >= cfg.device_count) {
        scheduleMijiaOverviewRefreshJob();
        return;
    }

    auto* job = new MijiaRefreshJob{};
    job->gen = mijiaRefreshGen;
    job->device_idx = idx;
    job->device = cfg.devices[idx];
    job->deadline_ms = millis() + MIJIA_REFRESH_TIMEOUT_MS;
    job->overview_cache = true;

    mijiaRefreshTaskRunning = true;
    if (xTaskCreate(mijiaRefreshTaskFn, "mijia_ref", 8192, job, 1, nullptr) != pdPASS) {
        delete job;
        mijiaRefreshTaskRunning = false;
        scheduleMijiaOverviewRefreshJob();
    }
}

static void requestMijiaOverviewPageRefresh() {
    if (!mijiaOverviewMode || !mijiaOverviewGridMode) {
        return;
    }
    if (mijiaWifiPhase != MijiaWifiPhase::READY && !isMijiaConfigWifiConnected()) {
        return;
    }

    const AppConfig& cfg = getAppConfig();
    if (!cfg.loaded || cfg.device_count == 0) {
        return;
    }

    mijiaOverviewRefreshQueueLen = 0;
    mijiaOverviewRefreshQueuePos = 0;
    for (int slot = 0; slot < MIJIA_GRID_PAGE_SIZE; slot++) {
        const int idx = mijiaOverviewScrollIdx + slot;
        if (idx >= cfg.device_count) {
            break;
        }
        mijiaOverviewRefreshQueue[mijiaOverviewRefreshQueueLen++] = idx;
        mijiaResetUiState(mijiaOverviewUi[idx]);
        strncpy(mijiaOverviewUi[idx].status, "query...", sizeof(mijiaOverviewUi[idx].status));
    }
    scheduleMijiaOverviewRefreshJob();
}

// 宫格选中设备后 i/o 快捷开关
static void setMijiaOverviewPower(const int device_idx, const bool on) {
    const AppConfig& cfg = getAppConfig();
    if (!cfg.loaded || device_idx < 0 || device_idx >= cfg.device_count) {
        return;
    }
    if (!ensureConfigWifi()) {
        return;
    }

    const MijiaDevice* dev = &cfg.devices[device_idx];
    MijiaUiState& state = mijiaOverviewUi[device_idx];
    mijiaSetDevicePower(dev, state, on);
    if (device_idx == mijiaDeviceIdx) {
        mijiaUi.power_known = state.power_known;
        mijiaUi.power_on = state.power_on;
        strncpy(mijiaUi.status, state.status, sizeof(mijiaUi.status));
    }
    refreshMijiaGridSelection(device_idx, device_idx);
}

// 概览每页设备数：列表 3 / 宫格 9
static int getMijiaOverviewVisibleCount() {
    return mijiaOverviewGridMode ? MIJIA_GRID_PAGE_SIZE : MIJIA_LIST_VISIBLE_COUNT;
}

static int getMijiaOverviewPageCount(const int device_count) {
    const int visible = getMijiaOverviewVisibleCount();
    if (device_count <= 0) {
        return 1;
    }
    return (device_count + visible - 1) / visible;
}

// 切换列表/宫格时同步滚动位置到当前选中设备所在页
static void syncMijiaOverviewScroll() {
    const AppConfig& cfg = getAppConfig();
    if (!cfg.loaded || cfg.device_count <= 0) {
        mijiaOverviewScrollIdx = 0;
        return;
    }
    const int visible = getMijiaOverviewVisibleCount();
    mijiaOverviewScrollIdx = (mijiaDeviceIdx / visible) * visible;
    if (mijiaOverviewGridMode) {
        const int page_count = getMijiaOverviewPageCount(cfg.device_count);
        int page = mijiaOverviewScrollIdx / visible;
        if (page < 0) {
            page = 0;
        }
        if (page >= page_count) {
            page = page_count - 1;
        }
        mijiaOverviewScrollIdx = page * visible;
        return;
    }
    const int max_scroll = cfg.device_count > visible ? cfg.device_count - visible : 0;
    if (mijiaOverviewScrollIdx > max_scroll) {
        mijiaOverviewScrollIdx = max_scroll;
    }
    if (mijiaOverviewScrollIdx < 0) {
        mijiaOverviewScrollIdx = 0;
    }
}

// 进入概览（列表或宫格）
static void enterMijiaOverview(const bool grid_mode) {
    mijiaOverviewMode = true;
    mijiaOverviewGridMode = grid_mode;
    mijiaOverviewEntryDeviceIdx = mijiaDeviceIdx;
    syncMijiaOverviewScroll();
    if (mijiaDeviceIdx >= 0 && mijiaDeviceIdx < MIJIA_DEVICE_MAX && mijiaUi.power_known) {
        mijiaOverviewUi[mijiaDeviceIdx] = mijiaUi;
    }
    if (grid_mode) {
        requestMijiaOverviewPageRefresh();
    }
}

// 退出概览回到控制页
static void exitMijiaOverview() {
    const bool device_changed = mijiaDeviceIdx != mijiaOverviewEntryDeviceIdx;
    mijiaOverviewMode = false;
    mijiaOverviewGridMode = false;
    if (device_changed) {
        mijiaResetUiState(mijiaUi);
        strncpy(mijiaUi.status, "query...", sizeof(mijiaUi.status));
        requestMijiaRefresh();
    }
}

// 宫格紧贴 header 顶边
static int getMijiaGridOriginY() {
    return APP_HEADER_H;
}

// 概览列表每项高度：均分内容区（扣除底栏提示）
static int getMijiaOverviewItemHeight() {
    constexpr int hint_h = 12;
    constexpr int gap = 4;
    const int avail = M5Cardputer.Display.height() - APP_CONTENT_Y - hint_h;
    const int total_gap = gap * (MIJIA_LIST_VISIBLE_COUNT - 1);
    return (avail - total_gap) / MIJIA_LIST_VISIBLE_COUNT;
}

// 概览列表图标边长：随行高缩放，不超过设计上限
static int getMijiaOverviewIconPx(const int item_h) {
    const int fit = item_h - 2;
    return fit < MIJIA_LIST_ICON_PX ? fit : MIJIA_LIST_ICON_PX;
}

static int getMijiaOverviewPage(const int device_count) {
    const int visible = getMijiaOverviewVisibleCount();
    if (device_count <= 0) {
        return 0;
    }
    return mijiaOverviewScrollIdx / visible;
}

// 概览翻页：宫格按整页跳转，列表支持逐条滚动
static bool handleMijiaOverviewNav(const int delta) {
    const AppConfig& cfg = getAppConfig();
    if (!cfg.loaded || cfg.device_count <= 1) {
        return false;
    }

    const int visible = getMijiaOverviewVisibleCount();
    const int page_count = getMijiaOverviewPageCount(cfg.device_count);

    if (mijiaOverviewGridMode) {
        if (page_count <= 1) {
            return false;
        }
        int page = mijiaOverviewScrollIdx / visible;
        page = (page + delta + page_count) % page_count;
        mijiaOverviewScrollIdx = page * visible;
        mijiaDeviceIdx = mijiaOverviewScrollIdx;
        if (mijiaDeviceIdx >= cfg.device_count) {
            mijiaDeviceIdx = cfg.device_count - 1;
        }
        if (mijiaOverviewGridMode) {
            requestMijiaOverviewPageRefresh();
        }
        redrawMijiaScreen();
        return true;
    }

    if (page_count <= 1) {
        mijiaDeviceIdx = (mijiaDeviceIdx + delta + cfg.device_count) % cfg.device_count;
        mijiaOverviewScrollIdx = (mijiaDeviceIdx / visible) * visible;
        redrawMijiaScreen();
        return true;
    }

    int page = getMijiaOverviewPage(cfg.device_count);
    page = (page + delta + page_count) % page_count;
    mijiaOverviewScrollIdx = page * visible;
    mijiaDeviceIdx = mijiaOverviewScrollIdx;
    if (mijiaDeviceIdx >= cfg.device_count) {
        mijiaDeviceIdx = cfg.device_count - 1;
    }
    redrawMijiaScreen();
    return true;
}

// 宫格底栏提示高度与分隔线
static constexpr int MIJIA_GRID_HINT_H = 12;
static constexpr int MIJIA_GRID_BOTTOM_DIVIDER = 1;

// 垂直方向：; . 及 HID 上/下（Cardputer 方向键布局）
static int getOverviewVerticalDelta(const Keyboard_Class::KeysState& status) {
    int delta = 0;
    for (const uint8_t hid : status.hid_keys) {
        if (hid == 0x52 || hid == 0x33) {
            delta = -1;
        } else if (hid == 0x51 || hid == 0x37) {
            delta = 1;
        }
    }
    for (const char c : status.word) {
        if (c == ';') {
            delta = -1;
        } else if (c == '.') {
            delta = 1;
        }
    }
    return delta;
}

// 水平方向：, / 及 HID 左/右（Cardputer 方向键布局）
static int getOverviewHorizontalDelta(const Keyboard_Class::KeysState& status) {
    int delta = 0;
    for (const uint8_t hid : status.hid_keys) {
        if (hid == 0x50 || hid == 0x36) {
            delta = -1;
        } else if (hid == 0x4F || hid == 0x38) {
            delta = 1;
        }
    }
    for (const char c : status.word) {
        if (c == ',') {
            delta = -1;
        } else if (c == '/') {
            delta = 1;
        }
    }
    return delta;
}

// 列表上下选中，跨页时同步 scroll
static bool handleMijiaListSelectionNav(const Keyboard_Class::KeysState& status) {
    const int drow = getOverviewVerticalDelta(status);
    if (drow == 0) {
        return false;
    }

    const AppConfig& cfg = getAppConfig();
    if (!cfg.loaded || cfg.device_count <= 1) {
        return false;
    }

    const int new_idx = mijiaDeviceIdx + drow;
    if (new_idx < 0 || new_idx >= cfg.device_count || new_idx == mijiaDeviceIdx) {
        return false;
    }

    const int visible = MIJIA_LIST_VISIBLE_COUNT;
    const int old_idx = mijiaDeviceIdx;
    const int old_scroll = mijiaOverviewScrollIdx;
    mijiaDeviceIdx = new_idx;
    mijiaOverviewScrollIdx = (new_idx / visible) * visible;

    if (mijiaOverviewScrollIdx != old_scroll) {
        redrawMijiaScreen();
    } else {
        refreshMijiaListSelection(old_idx, new_idx);
    }
    return true;
}

// [ ] 翻页：-1 上一页，0 无，1 下一页
static int getMijiaOverviewBracketDelta(const Keyboard_Class::KeysState& status) {
    for (const char c : status.word) {
        if (c == '[') {
            return -1;
        }
        if (c == ']') {
            return 1;
        }
    }
    return 0;
}

// 宫格方向键选中：上下左右 / ; , . /
static bool handleMijiaGridSelectionNav(const Keyboard_Class::KeysState& status) {
    const AppConfig& cfg = getAppConfig();
    if (!cfg.loaded || cfg.device_count <= 1) {
        return false;
    }

    int dcol = 0;
    int drow = 0;
    for (const uint8_t hid : status.hid_keys) {
        switch (hid) {
            case 0x50:
            case 0x36:
                dcol = -1;
                break;
            case 0x4F:
            case 0x38:
                dcol = 1;
                break;
            case 0x52:
            case 0x33:
                drow = -1;
                break;
            case 0x51:
            case 0x37:
                drow = 1;
                break;
            default:
                break;
        }
    }
    for (const char c : status.word) {
        if (c == ',') {
            dcol = -1;
        } else if (c == '/') {
            dcol = 1;
        } else if (c == ';') {
            drow = -1;
        } else if (c == '.') {
            drow = 1;
        }
    }
    if (dcol == 0 && drow == 0) {
        return false;
    }

    int new_idx = mijiaDeviceIdx;
    if (drow != 0) {
        new_idx += drow * MIJIA_GRID_COLS;
    } else {
        new_idx += dcol;
    }
    if (new_idx < 0 || new_idx >= cfg.device_count || new_idx == mijiaDeviceIdx) {
        return false;
    }

    const int old_idx = mijiaDeviceIdx;
    const int old_scroll = mijiaOverviewScrollIdx;
    mijiaDeviceIdx = new_idx;
    mijiaOverviewScrollIdx = (new_idx / MIJIA_GRID_PAGE_SIZE) * MIJIA_GRID_PAGE_SIZE;

    if (mijiaOverviewScrollIdx != old_scroll) {
        requestMijiaOverviewPageRefresh();
        redrawMijiaScreen();
    } else {
        refreshMijiaGridSelection(old_idx, new_idx);
    }
    return true;
}

// 估算底栏按键提示宽度
static int mijiaMeasureHintItem(const KeyHintItem& item) {
    const char letter = static_cast<char>(toupper(static_cast<unsigned char>(item.key)));
    const char str[2] = {letter, '\0'};
    M5Cardputer.Display.setTextSize(1);
    const int tw = M5Cardputer.Display.textWidth(str);
    constexpr int pad_x = 2;
    const int badge_w = tw + pad_x * 2 + 3;
    return badge_w + M5Cardputer.Display.textWidth(item.text);
}

// 宫格底栏：左侧 on/off，右侧 h help
static void drawMijiaGridBottomHints(const AppConfig& cfg) {
    const int hint_y = M5Cardputer.Display.height() - MIJIA_GRID_HINT_H;
    const int screen_w = M5Cardputer.Display.width();
    M5Cardputer.Display.fillRect(APP_CONTENT_X, hint_y, screen_w - APP_CONTENT_X * 2,
                                 MIJIA_GRID_HINT_H, BLACK);

    int cx = APP_CONTENT_X;
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);

    if (cfg.loaded && cfg.device_count > 1) {
        const int page_count = getMijiaOverviewPageCount(cfg.device_count);
        if (page_count > 1) {
            char pos_buf[12];
            snprintf(pos_buf, sizeof(pos_buf), "p%d/%d",
                     getMijiaOverviewPage(cfg.device_count) + 1, page_count);
            M5Cardputer.Display.setCursor(cx, hint_y);
            M5Cardputer.Display.print(pos_buf);
            cx += M5Cardputer.Display.textWidth(pos_buf) + 6;
        }
    }

    cx += drawKeyBadge(cx, hint_y, 'o', 1);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
    M5Cardputer.Display.setCursor(cx, hint_y);
    M5Cardputer.Display.print("on ");
    cx += M5Cardputer.Display.textWidth("on ");
    cx += drawKeyBadge(cx, hint_y, 'i', 1);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
    M5Cardputer.Display.setCursor(cx, hint_y);
    M5Cardputer.Display.print("off");

    const KeyHintItem help_item = {'h', "help"};
    const int help_w = mijiaMeasureHintItem(help_item);
    const int hx = screen_w - APP_CONTENT_X - help_w;
    cx = hx + drawKeyBadge(hx, hint_y, 'h', 1);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
    M5Cardputer.Display.setCursor(cx, hint_y);
    M5Cardputer.Display.print("help");
}

// 绘制概览底栏按键提示
static void drawMijiaOverviewHints(const AppConfig& cfg) {
    if (mijiaOverviewGridMode) {
        drawMijiaGridBottomHints(cfg);
        return;
    }

    const int hint_y = M5Cardputer.Display.height() - 12;
    int cx = APP_CONTENT_X;

    if (cfg.loaded && cfg.device_count > 1) {
        const int page_count = getMijiaOverviewPageCount(cfg.device_count);
        if (page_count > 1) {
            char pos_buf[12];
            snprintf(pos_buf, sizeof(pos_buf), "p%d/%d",
                     getMijiaOverviewPage(cfg.device_count) + 1, page_count);
            M5Cardputer.Display.setTextSize(1);
            M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
            M5Cardputer.Display.setCursor(cx, hint_y);
            M5Cardputer.Display.print(pos_buf);
            cx += M5Cardputer.Display.textWidth(pos_buf) + 6;
        }
    }

    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);

    cx += drawArrowBadge(cx, hint_y, 1);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
    M5Cardputer.Display.setCursor(cx, hint_y);
    M5Cardputer.Display.print("page ");
    cx += M5Cardputer.Display.textWidth("page ");

    cx += drawKeyBadge(cx, hint_y, 'l', 1);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
    M5Cardputer.Display.setCursor(cx, hint_y);
    M5Cardputer.Display.print("back ");
    cx += M5Cardputer.Display.textWidth("back ");
    cx += drawKeyBadge(cx, hint_y, 'g', 1);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
    M5Cardputer.Display.setCursor(cx, hint_y);
    M5Cardputer.Display.print("grid");
}

// 宫格单元：左图标，右侧第一行序号+状态，第二行设备名
static void drawMijiaOverviewGridCell(const MijiaDevice& entry, const int device_idx, const int x,
                                      const int y, const int cell_w, const int cell_h,
                                      const bool selected) {
    const MijiaDevKind kind = mijiaClassifyModel(entry.model);
    const uint16_t num_color = selected ? APP_COLOR_OK : APP_COLOR_HINT;
    const uint16_t name_color = selected ? APP_COLOR_OK : APP_COLOR_TEXT;
    const MijiaUiState& ui = mijiaOverviewUi[device_idx];
    const bool icon_active = ui.power_known && ui.power_on;

    constexpr int pad = 2;
    constexpr int text_gap = 4;
    constexpr int text_line_gap = 1;
    constexpr int num_status_gap = 4;
    int icon_px = cell_h - pad * 2;
    if (icon_px > DEVICE_ICON_LIST_PX) {
        icon_px = DEVICE_ICON_LIST_PX;
    }
    if (icon_px < MIJIA_ICON_BASE) {
        icon_px = MIJIA_ICON_BASE;
    }

    const int icon_x = x + pad;
    const int icon_y = y + (cell_h - icon_px) / 2;
    const float png_scale = static_cast<float>(icon_px) / DEVICE_ICON_LIST_PX;
    const int vector_scale = icon_px / MIJIA_ICON_BASE;
    drawMijiaDeviceIconForList(&entry, kind, icon_x, icon_y,
                               selected ? APP_COLOR_OK : APP_COLOR_HINT, icon_active,
                               vector_scale > 0 ? vector_scale : 1, png_scale);

    const int text_x = icon_x + icon_px + text_gap;
    const int text_w = cell_w - (text_x - x) - pad;
    const int num_h = infoLineHeight(1);
    const int name_h = infoLineHeight(1);
    const int line1_h = num_h > MIJIA_TAG_H ? num_h : MIJIA_TAG_H;
    const int text_block_h = line1_h + text_line_gap + name_h;
    const int text_y = y + (cell_h - text_block_h) / 2;

    M5Cardputer.Display.setTextSize(1);
    char num_buf[8];
    snprintf(num_buf, sizeof(num_buf), "%d", device_idx + 1);
    M5Cardputer.Display.setTextColor(num_color, BLACK);
    M5Cardputer.Display.setCursor(text_x, text_y + (line1_h - num_h) / 2);
    M5Cardputer.Display.print(num_buf);
    const int num_w = M5Cardputer.Display.textWidth(num_buf);

    const int status_x = text_x + num_w + num_status_gap;
    const int status_y = text_y + (line1_h - MIJIA_TAG_H) / 2;
    if (!ui.power_known) {
        drawMijiaStatusTag(status_x, status_y, "?", false, APP_COLOR_MUTED, 1);
    } else if (ui.power_on) {
        drawMijiaStatusTag(status_x, status_y, "ON", true, APP_COLOR_OK, 1);
    } else {
        drawMijiaStatusTag(status_x, status_y, "OFF", true, APP_COLOR_LABEL, 1);
    }

    const char* raw_name = entry.name[0] != '\0' ? entry.name : "device";
    char name_buf[24];
    strncpy(name_buf, raw_name, sizeof(name_buf) - 1);
    name_buf[sizeof(name_buf) - 1] = '\0';
    while (name_buf[0] != '\0' && M5Cardputer.Display.textWidth(name_buf) > text_w) {
        name_buf[strlen(name_buf) - 1] = '\0';
    }
    M5Cardputer.Display.setTextColor(name_color, BLACK);
    M5Cardputer.Display.setCursor(text_x, text_y + line1_h + text_line_gap);
    M5Cardputer.Display.print(name_buf);

    if (selected) {
        M5Cardputer.Display.drawRoundRect(x, y, cell_w, cell_h, 2, APP_COLOR_OK);
    }
}

struct MijiaGridLayout {
    int grid_y;
    int content_w;
    int avail_h;
    int cell_w;
    int cell_h;
    int gap;
};

// 计算宫格布局参数
static MijiaGridLayout getMijiaGridLayout() {
    MijiaGridLayout layout{};
    layout.gap = 2;
    layout.grid_y = getMijiaGridOriginY();
    layout.content_w = M5Cardputer.Display.width() - APP_CONTENT_X * 2;
    layout.avail_h =
        M5Cardputer.Display.height() - layout.grid_y - MIJIA_GRID_HINT_H - MIJIA_GRID_BOTTOM_DIVIDER;
    layout.cell_w = (layout.content_w - (MIJIA_GRID_COLS - 1) * layout.gap) / MIJIA_GRID_COLS;
    layout.cell_h = (layout.avail_h - (MIJIA_GRID_ROWS - 1) * layout.gap) / MIJIA_GRID_ROWS;
    return layout;
}

// 宫格末行与底栏提示之间的分隔线
static void drawMijiaGridBottomDivider(const MijiaGridLayout& layout) {
    const int y = layout.grid_y + layout.avail_h;
    M5Cardputer.Display.drawFastHLine(APP_CONTENT_X, y, layout.content_w, MIJIA_DIVIDER_COLOR);
}

// 绘制宫格分割线
static void drawMijiaGridDividers(const MijiaGridLayout& layout) {
    for (int col = 1; col < MIJIA_GRID_COLS; col++) {
        const int vx = APP_CONTENT_X + col * (layout.cell_w + layout.gap) - layout.gap / 2;
        M5Cardputer.Display.drawFastVLine(vx, layout.grid_y, layout.avail_h, MIJIA_DIVIDER_COLOR);
    }
    for (int row = 1; row < MIJIA_GRID_ROWS; row++) {
        const int hy = layout.grid_y + row * (layout.cell_h + layout.gap) - layout.gap / 2;
        M5Cardputer.Display.drawFastHLine(APP_CONTENT_X, hy, layout.content_w, MIJIA_DIVIDER_COLOR);
    }
}

// 设备索引转当前页宫格 slot，不在当前页返回 -1
static int mijiaGridSlotForIdx(const int device_idx) {
    if (device_idx < mijiaOverviewScrollIdx ||
        device_idx >= mijiaOverviewScrollIdx + MIJIA_GRID_PAGE_SIZE) {
        return -1;
    }
    return device_idx - mijiaOverviewScrollIdx;
}

// 局部刷新宫格选中态（仅重绘变更的两个格子）
static void refreshMijiaGridSelection(const int old_idx, const int new_idx) {
    if (old_idx == new_idx) {
        return;
    }

    const int old_slot = mijiaGridSlotForIdx(old_idx);
    const int new_slot = mijiaGridSlotForIdx(new_idx);
    if (old_slot < 0 && new_slot < 0) {
        redrawMijiaScreen();
        return;
    }

    const MijiaGridLayout layout = getMijiaGridLayout();
    const AppConfig& cfg = getAppConfig();

    const auto paint_slot = [&](const int slot) {
        if (slot < 0 || slot >= MIJIA_GRID_PAGE_SIZE) {
            return;
        }
        const int idx = mijiaOverviewScrollIdx + slot;
        if (idx >= cfg.device_count) {
            return;
        }
        const int row = slot / MIJIA_GRID_COLS;
        const int col = slot % MIJIA_GRID_COLS;
        const int cx = APP_CONTENT_X + col * (layout.cell_w + layout.gap);
        const int cy = layout.grid_y + row * (layout.cell_h + layout.gap);
        M5Cardputer.Display.fillRect(cx, cy, layout.cell_w, layout.cell_h, BLACK);
        drawMijiaOverviewGridCell(cfg.devices[idx], idx, cx, cy, layout.cell_w, layout.cell_h,
                                  idx == mijiaDeviceIdx);
    };

    paint_slot(old_slot);
    paint_slot(new_slot);
    drawMijiaGridDividers(layout);
    drawMijiaGridBottomDivider(layout);
}

struct MijiaListLayout {
    int item_h;
    int item_gap;
    int line_w;
};

// 计算列表布局参数
static MijiaListLayout getMijiaListLayout() {
    MijiaListLayout layout{};
    layout.item_h = getMijiaOverviewItemHeight();
    layout.item_gap = 4;
    layout.line_w = M5Cardputer.Display.width() - APP_CONTENT_X * 2;
    return layout;
}

// 设备索引转当前页列表 slot，不在当前页返回 -1
static int mijiaListSlotForIdx(const int device_idx) {
    if (device_idx < mijiaOverviewScrollIdx ||
        device_idx >= mijiaOverviewScrollIdx + MIJIA_LIST_VISIBLE_COUNT) {
        return -1;
    }
    return device_idx - mijiaOverviewScrollIdx;
}

// 绘制列表项间分隔线
static void drawMijiaListDividers(const MijiaListLayout& layout, const int device_count) {
    int item_y = APP_CONTENT_Y;
    for (int i = 0; i < MIJIA_LIST_VISIBLE_COUNT - 1; i++) {
        const int idx = mijiaOverviewScrollIdx + i;
        if (idx + 1 >= device_count) {
            break;
        }
        const int line_y = item_y + layout.item_h + layout.item_gap / 2;
        M5Cardputer.Display.drawFastHLine(APP_CONTENT_X, line_y, layout.line_w, MIJIA_DIVIDER_COLOR);
        item_y += layout.item_h + layout.item_gap;
    }
}

// 宫格概览：3x3 整页分页，末页不足 9 台留空
static void drawMijiaOverviewGrid() {
    const AppConfig& cfg = getAppConfig();
    syncMijiaOverviewScroll();

    const MijiaGridLayout layout = getMijiaGridLayout();

    drawMijiaGridDividers(layout);
    drawMijiaGridBottomDivider(layout);

    for (int row = 0; row < MIJIA_GRID_ROWS; row++) {
        for (int col = 0; col < MIJIA_GRID_COLS; col++) {
            const int slot = row * MIJIA_GRID_COLS + col;
            const int idx = mijiaOverviewScrollIdx + slot;
            const int cx = APP_CONTENT_X + col * (layout.cell_w + layout.gap);
            const int cy = layout.grid_y + row * (layout.cell_h + layout.gap);
            if (idx < cfg.device_count) {
                drawMijiaOverviewGridCell(cfg.devices[idx], idx, cx, cy, layout.cell_w, layout.cell_h,
                                          idx == mijiaDeviceIdx);
            }
        }
    }

    drawMijiaOverviewHints(cfg);
}

// 绘制单项：序号 + 左图标（缩放）+ 右名称/型号
static void drawMijiaOverviewItem(const MijiaDevice& entry, const int device_idx, const int x,
                                  const int y, const int item_h, const bool selected) {
    const MijiaDevKind kind = mijiaClassifyModel(entry.model);
    const uint16_t name_color = selected ? APP_COLOR_OK : APP_COLOR_VALUE;
    const int num_h = infoLineHeight(2);
    const int num_y = y + (item_h - num_h) / 2;

    M5Cardputer.Display.setTextSize(2);
    char num_buf[8];
    snprintf(num_buf, sizeof(num_buf), "%d", device_idx + 1);
    M5Cardputer.Display.setTextColor(name_color, BLACK);
    M5Cardputer.Display.setCursor(x, num_y);
    M5Cardputer.Display.print(num_buf);
    const int content_x = x + M5Cardputer.Display.textWidth(num_buf) + MIJIA_LIST_NUM_MARGIN_R;

    const int icon_px = DEVICE_ICON_LIST_PX;
    const int vector_scale = icon_px / MIJIA_ICON_BASE;
    const int icon_y = y + (item_h - icon_px) / 2;
    drawMijiaDeviceIconForList(&entry, kind, content_x, icon_y, selected ? APP_COLOR_OK : APP_COLOR_HINT,
                               false, vector_scale > 0 ? vector_scale : 1, 1.0f);

    const int text_x = content_x + icon_px + 6;
    const int text_block_h = INFO_LINE_H * 2;
    const int text_y = y + (item_h - text_block_h) / 2;

    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(name_color, BLACK);
    M5Cardputer.Display.setCursor(text_x, text_y);
    if (entry.name[0] != '\0') {
        M5Cardputer.Display.print(entry.name);
    } else {
        M5Cardputer.Display.print("device");
    }

    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
    M5Cardputer.Display.setCursor(text_x, text_y + INFO_LINE_H);
    if (entry.model[0] != '\0') {
        M5Cardputer.Display.print(entry.model);
    } else {
        M5Cardputer.Display.print("-");
    }
}

// 局部刷新列表选中态（仅重绘变更的两项）
static void refreshMijiaListSelection(const int old_idx, const int new_idx) {
    if (old_idx == new_idx) {
        return;
    }

    const int old_slot = mijiaListSlotForIdx(old_idx);
    const int new_slot = mijiaListSlotForIdx(new_idx);
    if (old_slot < 0 && new_slot < 0) {
        redrawMijiaScreen();
        return;
    }

    const MijiaListLayout layout = getMijiaListLayout();
    const AppConfig& cfg = getAppConfig();

    const auto paint_slot = [&](const int slot) {
        if (slot < 0 || slot >= MIJIA_LIST_VISIBLE_COUNT) {
            return;
        }
        const int idx = mijiaOverviewScrollIdx + slot;
        if (idx >= cfg.device_count) {
            return;
        }
        const int item_y = APP_CONTENT_Y + slot * (layout.item_h + layout.item_gap);
        M5Cardputer.Display.fillRect(APP_CONTENT_X, item_y, layout.line_w, layout.item_h, BLACK);
        drawMijiaOverviewItem(cfg.devices[idx], idx, APP_CONTENT_X, item_y, layout.item_h,
                              idx == mijiaDeviceIdx);
    };

    paint_slot(old_slot);
    paint_slot(new_slot);
    drawMijiaListDividers(layout, cfg.device_count);
}

static void drawMijiaOverview(int& y) {
    const AppConfig& cfg = getAppConfig();

    if (!cfg.loaded || cfg.device_count == 0) {
        drawInfoLine(APP_CONTENT_X, y, "total", "0");
        drawInfoLine(APP_CONTENT_X, y, "hint", "press u web");
        static const KeyHintItem empty_items[] = {
            {'u', "web"},
            {'l', "back"},
        };
        drawKeyHintsRow(APP_CONTENT_X, M5Cardputer.Display.height() - 12, empty_items, 2, 1,
                        APP_COLOR_HINT);
        return;
    }

    if (mijiaOverviewGridMode) {
        drawMijiaOverviewGrid();
        y = M5Cardputer.Display.height() - 12;
        return;
    }

    const int visible = getMijiaOverviewVisibleCount();
    const int max_scroll = cfg.device_count > visible ? cfg.device_count - visible : 0;
    if (mijiaOverviewScrollIdx > max_scroll) {
        mijiaOverviewScrollIdx = max_scroll;
    }
    if (mijiaOverviewScrollIdx < 0) {
        mijiaOverviewScrollIdx = 0;
    }

    const int item_h = getMijiaOverviewItemHeight();
    constexpr int item_gap = 4;
    const MijiaListLayout layout = getMijiaListLayout();
    int item_y = APP_CONTENT_Y;
    for (int i = 0; i < visible; i++) {
        const int idx = mijiaOverviewScrollIdx + i;
        if (idx >= cfg.device_count) {
            break;
        }
        drawMijiaOverviewItem(cfg.devices[idx], idx, APP_CONTENT_X, item_y, item_h,
                              idx == mijiaDeviceIdx);
        item_y += item_h + item_gap;
    }
    drawMijiaListDividers(layout, cfg.device_count);
    y = item_y;
    drawMijiaOverviewHints(cfg);
}

bool handleMijiaOverviewPageNav(const Keyboard_Class::KeysState& status) {
    if (!mijiaOverviewMode || mijiaHelpVisible) {
        return false;
    }
    if (mijiaOverviewGridMode) {
        const int bracket = getMijiaOverviewBracketDelta(status);
        if (bracket != 0) {
            return handleMijiaOverviewNav(bracket);
        }
        return handleMijiaGridSelectionNav(status);
    }
    const int hdelta = getOverviewHorizontalDelta(status);
    if (hdelta != 0) {
        return handleMijiaOverviewNav(hdelta);
    }
    return handleMijiaListSelectionNav(status);
}

// 控制页切换设备
bool handleMijiaDeviceNav(const Keyboard_Class::KeysState& status) {
    if (mijiaOverviewMode || mijiaHelpVisible) {
        return false;
    }

    const AppConfig& cfg = getAppConfig();
    if (!cfg.loaded || cfg.device_count <= 1) {
        return false;
    }

    const int delta = getMenuNavDelta(status);
    if (delta == 0) {
        return false;
    }

    switchMijiaDevice(delta, cfg.device_count);
    return true;
}

static int mijiaHintLineStep(const int text_size) {
    return text_size == 2 ? INFO_LINE_H_2X : INFO_LINE_H;
}

// 帮助页：按总行数均分垂直空间，返回第 row 行绘制 y
static int mijiaHelpRowY(const int row, const int total_rows, const int text_size) {
    const int content_h = M5Cardputer.Display.height() - APP_CONTENT_Y;
    const int line_h = mijiaHintLineStep(text_size);
    if (total_rows <= 0) {
        return APP_CONTENT_Y;
    }
    const int slot_h = content_h / total_rows;
    return APP_CONTENT_Y + row * slot_h + (slot_h - line_h) / 2;
}

// 估算按键徽章 + 文案占用宽度
static int mijiaMeasureKeyHintItem(const KeyHintItem& item, const int text_size) {
    const int size = (text_size == 2) ? 2 : 1;
    const char letter = static_cast<char>(toupper(static_cast<unsigned char>(item.key)));
    const char str[2] = {letter, '\0'};
    M5Cardputer.Display.setTextSize(size);
    const int badge_w = M5Cardputer.Display.textWidth(str) + 4 + 3;
    M5Cardputer.Display.setTextSize(text_size);
    return badge_w + M5Cardputer.Display.textWidth(item.text);
}

// 估算换行后的行数
static int mijiaCountWrappedRows(const KeyHintItem* items, const int item_count,
                                 const int text_size, const int max_w) {
    if (items == nullptr || item_count <= 0) {
        return 0;
    }

    int rows = 1;
    int cx = 0;
    M5Cardputer.Display.setTextSize(text_size);
    const int space_w = M5Cardputer.Display.textWidth(" ");
    for (int i = 0; i < item_count; i++) {
        const int item_w = mijiaMeasureKeyHintItem(items[i], text_size);
        if (cx > 0 && cx + item_w > max_w) {
            rows++;
            cx = item_w;
        } else {
            if (cx > 0) {
                cx += space_w;
            }
            cx += item_w;
        }
    }
    return rows;
}

static int mijiaCountHelpRows(const MijiaDevKind kind, const int max_w, const int text_size) {
    static const KeyHintItem action_items[] = {
        {'o', "on"},
        {'i', "off"},
        {'t', "toggle"},
        {'l', "list"},
        {'h', "help"},
    };

    int rows = mijiaCountWrappedRows(action_items, 5, text_size, max_w);
    rows += 1; // refresh + switch

    switch (kind) {
        case MijiaDevKind::LIGHT: {
            static const KeyHintItem bright_items[] = {{'-', "bright-"}, {'=', "bright+"}};
            static const KeyHintItem percent_items[] = {{'1', "10%"}, {'9', "90%"}, {'0', "100%"}};
            static const KeyHintItem ct_items[] = {{'[', "ct-"}, {']', "ct+"}};
            rows += mijiaCountWrappedRows(bright_items, 2, text_size, max_w);
            rows += mijiaCountWrappedRows(percent_items, 3, text_size, max_w);
            rows += mijiaCountWrappedRows(ct_items, 2, text_size, max_w);
            break;
        }
        case MijiaDevKind::FAN_P5: {
            static const KeyHintItem fan_items[] = {
                {'-', "spd-"},
                {'=', "spd+"},
                {'w', "roll"},
                {'m', "mode"},
            };
            rows += mijiaCountWrappedRows(fan_items, 4, text_size, max_w);
            break;
        }
        case MijiaDevKind::FAN_GENERIC: {
            static const KeyHintItem speed_items[] = {
                {'1', "lv1"},
                {'2', "lv2"},
                {'3', "lv3"},
                {'4', "lv4"},
            };
            rows += mijiaCountWrappedRows(speed_items, 4, text_size, max_w);
            break;
        }
        case MijiaDevKind::AIR_PURIFIER_F20: {
            static const KeyHintItem mode_items[] = {
                {'1', "mode1"},
                {'2', "mode2"},
                {'3', "mode3"},
                {'4', "mode4"},
                {'5', "mode5"},
            };
            static const KeyHintItem fan_items[] = {{'-', "fan-"}, {'=', "fan+"}};
            rows += mijiaCountWrappedRows(mode_items, 5, text_size, max_w);
            rows += mijiaCountWrappedRows(fan_items, 2, text_size, max_w);
            break;
        }
        default:
            break;
    }
    return rows;
}

// 绘制单个按键提示，返回占用宽度
static int mijiaDrawKeyHintItem(const int x, const int y, const KeyHintItem& item,
                                const int text_size, const uint16_t color) {
    int cx = x + drawKeyBadge(x, y, item.key, text_size);
    M5Cardputer.Display.setTextSize(text_size);
    M5Cardputer.Display.setTextColor(color, BLACK);
    M5Cardputer.Display.setCursor(cx, y);
    M5Cardputer.Display.print(item.text);
    return cx + M5Cardputer.Display.textWidth(item.text) - x;
}

// 按屏宽换行绘制按键提示，返回下一行索引
static int drawKeyHintsWrapped(const int x, const int start_row, const int total_rows,
                               const KeyHintItem* items, const int item_count,
                               const int text_size, const uint16_t color, const int max_w) {
    if (items == nullptr || item_count <= 0) {
        return start_row;
    }

    int row = start_row;
    int y = mijiaHelpRowY(row, total_rows, text_size);
    int cx = x;
    M5Cardputer.Display.setTextSize(text_size);
    const int space_w = M5Cardputer.Display.textWidth(" ");

    for (int i = 0; i < item_count; i++) {
        const int item_w = mijiaMeasureKeyHintItem(items[i], text_size);
        if (cx > x && cx + item_w > x + max_w) {
            row++;
            y = mijiaHelpRowY(row, total_rows, text_size);
            cx = x;
        }
        cx += mijiaDrawKeyHintItem(cx, y, items[i], text_size, color);
        if (i != item_count - 1) {
            M5Cardputer.Display.setCursor(cx, y);
            M5Cardputer.Display.print(" ");
            cx += space_w;
        }
    }
    return row + 1;
}

// refresh 与 switch 同一行
static void drawMijiaRefreshHelpRow(const int x, const int row, const int total_rows,
                                    const int text_size) {
    const int y = mijiaHelpRowY(row, total_rows, text_size);
    static const KeyHintItem refresh_item = {'r', "refresh"};
    int cx = x + mijiaDrawKeyHintItem(x, y, refresh_item, text_size, APP_COLOR_HINT);

    M5Cardputer.Display.setTextSize(text_size);
    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
    M5Cardputer.Display.setCursor(cx, y);
    M5Cardputer.Display.print("  ");
    cx += M5Cardputer.Display.textWidth("  ");

    cx += drawArrowBadge(cx, y, text_size);
    M5Cardputer.Display.setTextSize(text_size);
    M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
    M5Cardputer.Display.setCursor(cx, y);
    M5Cardputer.Display.print("switch");
}

// 灯：亮度 + 色温调节说明
static int drawMijiaLightHelpRows(const MijiaDevice* dev, const int start_row, const int total_rows,
                                  const int text_size, const int max_w) {
    static const KeyHintItem bright_items[] = {{'-', "bright-"}, {'=', "bright+"}};
    static const KeyHintItem percent_items[] = {{'1', "10%"}, {'9', "90%"}, {'0', "100%"}};
    static const KeyHintItem ct_items[] = {{'[', "ct-"}, {']', "ct+"}};
    int row = drawKeyHintsWrapped(APP_CONTENT_X, start_row, total_rows, bright_items, 2, text_size,
                                  APP_COLOR_HINT, max_w);
    row = drawKeyHintsWrapped(APP_CONTENT_X, row, total_rows, percent_items, 3, text_size,
                              APP_COLOR_HINT, max_w);
    if (dev != nullptr && mijiaLightSupportsCt(dev->model)) {
        row = drawKeyHintsWrapped(APP_CONTENT_X, row, total_rows, ct_items, 2, text_size,
                                  APP_COLOR_HINT, max_w);
    }
    return row;
}

// 按设备类型绘制操作帮助（text_size=2，垂直空间均分）
static void drawMijiaHelpContent(const MijiaDevice* dev, const int text_size) {
    const MijiaDevKind kind =
        dev != nullptr ? mijiaClassifyModel(dev->model) : MijiaDevKind::GENERIC;
    const int max_w = M5Cardputer.Display.width() - APP_CONTENT_X * 2;
    const int total_rows = mijiaCountHelpRows(kind, max_w, text_size);

    static const KeyHintItem action_items[] = {
        {'o', "on"},
        {'i', "off"},
        {'t', "toggle"},
        {'l', "list"},
        {'h', "help"},
    };

    int row = 0;
    row = drawKeyHintsWrapped(APP_CONTENT_X, row, total_rows, action_items, 5, text_size,
                              APP_COLOR_HINT, max_w);
    drawMijiaRefreshHelpRow(APP_CONTENT_X, row, total_rows, text_size);
    row++;

    switch (kind) {
        case MijiaDevKind::LIGHT:
            row = drawMijiaLightHelpRows(dev, row, total_rows, text_size, max_w);
            break;
        case MijiaDevKind::FAN_P5: {
            static const KeyHintItem fan_items[] = {
                {'-', "spd-"},
                {'=', "spd+"},
                {'w', "roll"},
                {'m', "mode"},
            };
            row = drawKeyHintsWrapped(APP_CONTENT_X, row, total_rows, fan_items, 4, text_size,
                                      APP_COLOR_HINT, max_w);
            break;
        }
        case MijiaDevKind::FAN_GENERIC: {
            static const KeyHintItem speed_items[] = {
                {'1', "lv1"},
                {'2', "lv2"},
                {'3', "lv3"},
                {'4', "lv4"},
            };
            row = drawKeyHintsWrapped(APP_CONTENT_X, row, total_rows, speed_items, 4, text_size,
                                      APP_COLOR_HINT, max_w);
            break;
        }
        case MijiaDevKind::AIR_PURIFIER_F20: {
            static const KeyHintItem mode_items[] = {
                {'1', "mode1"},
                {'2', "mode2"},
                {'3', "mode3"},
                {'4', "mode4"},
                {'5', "mode5"},
            };
            static const KeyHintItem fan_items[] = {{'-', "fan-"}, {'=', "fan+"}};
            row = drawKeyHintsWrapped(APP_CONTENT_X, row, total_rows, mode_items, 5, text_size,
                                      APP_COLOR_HINT, max_w);
            row = drawKeyHintsWrapped(APP_CONTENT_X, row, total_rows, fan_items, 2, text_size,
                                      APP_COLOR_HINT, max_w);
            break;
        }
        default:
            break;
    }
}

// 宫格概览操作帮助（按 H 在 grid 界面查看）
static void drawMijiaGridHelpContent(const int text_size) {
    const int max_w = M5Cardputer.Display.width() - APP_CONTENT_X * 2;
    static const KeyHintItem action_items[] = {
        {'g', "back"},
        {'l', "list"},
        {'h', "help"},
    };
    const int total_rows =
        2 + mijiaCountWrappedRows(action_items, 3, text_size, max_w);

    // 方向键选中
    {
        const int y = mijiaHelpRowY(0, total_rows, text_size);
        int cx = APP_CONTENT_X + drawArrowBadge(APP_CONTENT_X, y, text_size);
        cx += drawArrowUpDownBadge(cx, y, text_size);
        M5Cardputer.Display.setTextSize(text_size);
        M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
        M5Cardputer.Display.setCursor(cx, y);
        M5Cardputer.Display.print("sel");
    }

    // [ ] 翻页
    {
        const int y = mijiaHelpRowY(1, total_rows, text_size);
        int cx = APP_CONTENT_X + drawKeyBadge(APP_CONTENT_X, y, '[', text_size);
        cx += drawKeyBadge(cx, y, ']', text_size);
        M5Cardputer.Display.setTextSize(text_size);
        M5Cardputer.Display.setTextColor(APP_COLOR_HINT, BLACK);
        M5Cardputer.Display.setCursor(cx, y);
        M5Cardputer.Display.print("page");
    }

    drawKeyHintsWrapped(APP_CONTENT_X, 2, total_rows, action_items, 3, text_size, APP_COLOR_HINT,
                        max_w);
}

// 宫格概览帮助页
static void drawMijiaGridHelpPage() {
    beginAppScreen("Help");
    drawMijiaGridHelpContent(2);
}

// 按 H 切换显示的帮助页
static void drawMijiaHelpPage() {
    beginAppScreen("Help");
    const MijiaDevice* dev = getCurrentMijiaDevice();
    drawMijiaHelpContent(dev, 2);
}

void drawMijiaApp() {
    if (mijiaOverviewMode) {
        beginAppScreen("Mijia");
        M5Cardputer.Display.setTextSize(1);
        int y = APP_CONTENT_Y;
        drawMijiaOverview(y);
        return;
    }
    applyMijiaControlRefresh(true);
}

void enterMijiaApp() {
    mijiaDeviceIdx = 0;
    mijiaOverviewMode = false;
    mijiaOverviewGridMode = false;
    mijiaHelpVisible = false;
    mijiaOverviewScrollIdx = 0;
    mijiaOverviewRefreshQueueLen = 0;
    mijiaOverviewRefreshQueuePos = 0;
    for (int i = 0; i < MIJIA_DEVICE_MAX; i++) {
        mijiaResetUiState(mijiaOverviewUi[i]);
    }
    mijiaWifiPhase = MijiaWifiPhase::IDLE;
    mijiaWifiDeadlineMs = 0;
    mijiaRefreshDeadlineMs = 0;
    mijiaNetStatus[0] = '\0';
    invalidateMijiaControlSurface();
    mijiaResetUiState(mijiaUi);
    applyMijiaControlRefresh(true);
    startMijiaWifiConnect();
}

void updateMijiaApp() {
    updateMijiaWifiConnect();
    updateMijiaRefreshTimeout();
    if (mijiaNeedRedraw) {
        mijiaNeedRedraw = false;
        redrawMijiaScreen();
    }
}

void handleMijiaApp(const String& key) {
    if (key == "h") {
        if (mijiaOverviewGridMode || !mijiaOverviewMode) {
            mijiaHelpVisible = !mijiaHelpVisible;
            redrawMijiaScreen();
        }
        return;
    }
    if (mijiaHelpVisible) {
        return;
    }

    if (key == "l") {
        if (!mijiaOverviewMode) {
            enterMijiaOverview(false);
        } else if (mijiaOverviewGridMode) {
            mijiaOverviewGridMode = false;
            mijiaHelpVisible = false;
            syncMijiaOverviewScroll();
        } else {
            exitMijiaOverview();
        }
        redrawMijiaScreen();
        return;
    }
    if (key == "g") {
        if (!mijiaOverviewMode) {
            enterMijiaOverview(true);
        } else if (mijiaOverviewGridMode) {
            mijiaHelpVisible = false;
            exitMijiaOverview();
        } else {
            mijiaOverviewGridMode = true;
            syncMijiaOverviewScroll();
            requestMijiaOverviewPageRefresh();
        }
        redrawMijiaScreen();
        return;
    }
    if (mijiaOverviewMode) {
        if (mijiaOverviewGridMode && (key == "i" || key == "o")) {
            setMijiaOverviewPower(mijiaDeviceIdx, key == "o");
            return;
        }

        const AppConfig& cfg = getAppConfig();
        // 宫格数字键选中当前页设备
        if (mijiaOverviewGridMode && key.length() == 1 &&
            key[0] >= '1' && key[0] < static_cast<char>('1' + MIJIA_GRID_PAGE_SIZE)) {
            const int pick = key[0] - '1';
            if (pick < MIJIA_GRID_PAGE_SIZE && cfg.loaded) {
                const int idx = mijiaOverviewScrollIdx + pick;
                if (idx < cfg.device_count && idx != mijiaDeviceIdx) {
                    const int old_idx = mijiaDeviceIdx;
                    mijiaDeviceIdx = idx;
                    refreshMijiaGridSelection(old_idx, idx);
                }
            }
        }
        return;
    }

    const AppConfig& cfg = getAppConfig();
    if (!cfg.loaded || cfg.device_count == 0) {
        return;
    }

    const MijiaDevice* dev = getCurrentMijiaDevice();
    if (dev == nullptr) {
        return;
    }

    const MijiaDevKind kind = mijiaClassifyModel(dev->model);
    bool handled = true;

    if (key == "o") {
        if (!ensureConfigWifi()) {
            strncpy(mijiaUi.status, "wifi fail", sizeof(mijiaUi.status));
        } else {
            setMijiaPower(true);
        }
    } else if (key == "i") {
        if (!ensureConfigWifi()) {
            strncpy(mijiaUi.status, "wifi fail", sizeof(mijiaUi.status));
        } else {
            setMijiaPower(false);
        }
    } else if (key == "t") {
        if (!ensureConfigWifi()) {
            strncpy(mijiaUi.status, "wifi fail", sizeof(mijiaUi.status));
        } else {
            setMijiaPower(!mijiaUi.power_on);
        }
    } else if (key == "r") {
        if (mijiaWifiPhase == MijiaWifiPhase::FAILED || mijiaWifiPhase == MijiaWifiPhase::IDLE) {
            startMijiaWifiConnect();
        } else {
            applyMijiaControlRefresh(false);
            requestMijiaRefresh();
        }
    } else if (key == "," || key == ";") {
        switchMijiaDevice(-1, cfg.device_count);
        return;
    } else if (key == "." || key == "/") {
        switchMijiaDevice(1, cfg.device_count);
        return;
    } else if (kind == MijiaDevKind::LIGHT && key == "-") {
        if (!ensureConfigWifi()) {
            strncpy(mijiaUi.status, "wifi fail", sizeof(mijiaUi.status));
        } else {
            mijiaAdjustBright(dev, mijiaUi, -10);
        }
    } else if (kind == MijiaDevKind::LIGHT && (key == "=" || key == "+")) {
        if (!ensureConfigWifi()) {
            strncpy(mijiaUi.status, "wifi fail", sizeof(mijiaUi.status));
        } else {
            mijiaAdjustBright(dev, mijiaUi, 10);
        }
    } else if (kind == MijiaDevKind::LIGHT && key.length() == 1 && key[0] >= '0' && key[0] <= '9') {
        if (!ensureConfigWifi()) {
            strncpy(mijiaUi.status, "wifi fail", sizeof(mijiaUi.status));
        } else {
            const int percent = key[0] == '0' ? 100 : (key[0] - '0') * 10;
            mijiaSetBrightPercent(dev, mijiaUi, percent);
        }
    } else if (kind == MijiaDevKind::LIGHT && mijiaLightSupportsCt(dev->model) && key == "[") {
        if (!ensureConfigWifi()) {
            strncpy(mijiaUi.status, "wifi fail", sizeof(mijiaUi.status));
        } else {
            mijiaAdjustColorTemp(dev, mijiaUi, -100);
        }
    } else if (kind == MijiaDevKind::LIGHT && mijiaLightSupportsCt(dev->model) && key == "]") {
        if (!ensureConfigWifi()) {
            strncpy(mijiaUi.status, "wifi fail", sizeof(mijiaUi.status));
        } else {
            mijiaAdjustColorTemp(dev, mijiaUi, 100);
        }
    } else if (kind == MijiaDevKind::FAN_P5) {
        if (!ensureConfigWifi()) {
            strncpy(mijiaUi.status, "wifi fail", sizeof(mijiaUi.status));
        } else if (key == "-") {
            mijiaAdjustFanP5Speed(dev, mijiaUi, -10);
        } else if (key == "=" || key == "+") {
            mijiaAdjustFanP5Speed(dev, mijiaUi, 10);
        } else if (key == "w") {
            mijiaToggleFanP5Roll(dev, mijiaUi);
        } else if (key == "m") {
            mijiaToggleFanP5Mode(dev, mijiaUi);
        } else {
            handled = false;
        }
    } else if (kind == MijiaDevKind::FAN_GENERIC && key.length() == 1 && key[0] >= '1' &&
               key[0] <= '4') {
        if (!ensureConfigWifi()) {
            strncpy(mijiaUi.status, "wifi fail", sizeof(mijiaUi.status));
        } else {
            mijiaSetFanSpeedLevel(dev, mijiaUi, key[0] - '0');
        }
    } else if (kind == MijiaDevKind::AIR_PURIFIER_F20) {
        if (!ensureConfigWifi()) {
            strncpy(mijiaUi.status, "wifi fail", sizeof(mijiaUi.status));
        } else if (key.length() == 1 && key[0] >= '1' && key[0] <= '5') {
            mijiaSetPurifierMode(dev, mijiaUi, key[0] - '1');
        } else if (key == "-") {
            mijiaAdjustPurifierFanLevel(dev, mijiaUi, -1);
        } else if (key == "=" || key == "+") {
            mijiaAdjustPurifierFanLevel(dev, mijiaUi, 1);
        } else {
            handled = false;
        }
    } else {
        handled = false;
    }

    if (handled) {
        applyMijiaControlRefresh(false);
    }
}
