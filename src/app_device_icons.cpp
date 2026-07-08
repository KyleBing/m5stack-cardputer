#include "app_device_icons.h"
#include <FS.h>
#include <LittleFS.h>
#include "M5Cardputer.h"

// 与 assets/img 中文件名对应
static constexpr const char* ICON_FAN = "/img/fan@2x.png";
static constexpr const char* ICON_AIR_PURIFIER = "/img/air_normal@2x.png";
static constexpr const char* ICON_PLUG = "/img/switch_on@2x.png";
static constexpr const char* ICON_DEFAULT = "/img/default@2x.png";

const char* deviceIconPathForKind(const MijiaDevKind kind) {
    switch (kind) {
        case MijiaDevKind::FAN_P5:
        case MijiaDevKind::FAN_GENERIC:
            return ICON_FAN;
        case MijiaDevKind::AIR_PURIFIER_F20:
            return ICON_AIR_PURIFIER;
        case MijiaDevKind::PLUG:
            return ICON_PLUG;
        case MijiaDevKind::GENERIC:
            return ICON_DEFAULT;
        default:
            // 灯、空气炸锅等暂无对应 PNG，由矢量图标兜底
            return nullptr;
    }
}

bool deviceIconsAvailable() {
    return LittleFS.exists(ICON_FAN);
}

bool drawDevicePngFile(const char* path, const int x, const int y, const int size) {
    if (path == nullptr || path[0] == '\0' || size <= 0) {
        return false;
    }
    if (!LittleFS.exists(path)) {
        return false;
    }
    // scale 0 = 等比缩放到 maxWidth×maxHeight 区域内；middle_center 在方框内居中
    return M5Cardputer.Display.drawPngFile(LittleFS, path, x, y, size, size, 0, 0, 0.0f, 0.0f,
                                           lgfx::v1::datum_t::middle_center);
}

bool drawDevicePngIcon(const MijiaDevKind kind, const int x, const int y, const int size) {
    const char* path = deviceIconPathForKind(kind);
    if (path == nullptr) {
        return false;
    }
    return drawDevicePngFile(path, x, y, size);
}
