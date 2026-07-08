# 构建前将设备 PNG 从 assets/img 复制到 data/img（LittleFS 烧录目录）
Import("env")
import os
import shutil

PROJECT_DIR = env["PROJECT_DIR"]
SRC_DIR = os.path.join(PROJECT_DIR, "assets", "img")
DST_DIR = os.path.join(PROJECT_DIR, "data", "img")

# 与 app_device_icons.cpp 中映射一致
ICON_FILES = [
    "fan@2x.png",
    "air_normal@2x.png",
    "switch_on@2x.png",
    "default@2x.png",
]


def copy_device_icons(source, target, env):
    os.makedirs(DST_DIR, exist_ok=True)
    copied = 0
    for name in ICON_FILES:
        src = os.path.join(SRC_DIR, name)
        dst = os.path.join(DST_DIR, name)
        if not os.path.isfile(src):
            print(f"WARN: missing device icon {src}")
            continue
        shutil.copy2(src, dst)
        copied += 1
    print(f"device icons: copied {copied} file(s) to data/img")


# 脚本加载时先复制一次，保证 buildfs / uploadfs 能打包到 data/img
copy_device_icons(None, None, env)
env.AddPreAction("buildfs", copy_device_icons)
env.AddPreAction("uploadfs", copy_device_icons)
env.AddPreAction("upload", copy_device_icons)
