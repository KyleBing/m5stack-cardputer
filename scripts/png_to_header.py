# 将 PNG 转为 C 头文件，供 drawPng() 直接使用
Import("env")

import os
import struct

from pathlib import Path

PROJECT_DIR = env["PROJECT_DIR"]
SRC_PNG = Path(PROJECT_DIR) / "assets" / "img" / "logo.png"
OUT_H = Path(PROJECT_DIR) / "include" / "logo_png.h"
VAR_NAME = "logo_png"


def read_png_size(data: bytes) -> tuple[int, int] | None:
    # PNG IHDR：偏移 16/20 为宽高（大端）
    if len(data) < 24 or data[:8] != b"\x89PNG\r\n\x1a\n":
        return None
    width, height = struct.unpack(">II", data[16:24])
    return width, height


def read_content_bbox(path: Path, width: int, height: int) -> tuple[int, int, int, int]:
    # 非透明内容区域；失败时退回整图
    try:
        from PIL import Image

        bbox = Image.open(path).getbbox()
        if bbox:
            left, top, right, bottom = bbox
            return left, top, right - left, bottom - top
    except Exception:
        pass
    return 0, 0, width, height


def normalize_pixel_perfect(path: Path) -> None:
    # 仅平移像素居中，禁止 resize，保持原始纵横比
    try:
        from PIL import Image
    except ImportError:
        return

    image = Image.open(path).convert("RGBA")
    bbox = image.getbbox()
    if not bbox:
        return

    content = image.crop(bbox)
    canvas_w, canvas_h = image.size
    normalized = Image.new("RGBA", (canvas_w, canvas_h), (0, 0, 0, 0))
    offset_x = (canvas_w - content.width) // 2
    offset_y = (canvas_h - content.height) // 2
    normalized.paste(content, (offset_x, offset_y), content)
    normalized.save(path, optimize=False)


def write_header() -> None:
    if not SRC_PNG.is_file():
        return

    normalize_pixel_perfect(SRC_PNG)

    data = SRC_PNG.read_bytes()
    size = read_png_size(data)
    if size is None:
        return

    width, height = size
    content_x, content_y, content_w, content_h = read_content_bbox(SRC_PNG, width, height)

    lines = [
        f"// Auto-generated from assets/img/logo.png\n",
        "#pragma once\n\n",
        "#include <cstddef>\n",
        "#include <cstdint>\n\n",
        f"static constexpr int {VAR_NAME}_w = {width};\n",
        f"static constexpr int {VAR_NAME}_h = {height};\n",
        f"static constexpr int {VAR_NAME}_cx = {content_x};\n",
        f"static constexpr int {VAR_NAME}_cy = {content_y};\n",
        f"static constexpr int {VAR_NAME}_cw = {content_w};\n",
        f"static constexpr int {VAR_NAME}_ch = {content_h};\n\n",
        f"static const uint8_t {VAR_NAME}[] = {{\n",
    ]

    for index, byte in enumerate(data):
        if index % 12 == 0:
            lines.append("  ")
        lines.append(f"0x{byte:02x}, ")
        if index % 12 == 11:
            lines.append("\n")

    if len(data) % 12 != 0:
        lines.append("\n")

    lines.append("};\n\n")
    lines.append(f"static const size_t {VAR_NAME}_len = {len(data)};\n")
    OUT_H.write_text("".join(lines), encoding="utf-8")


write_header()
