#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
render_dump.py - 将 C 驱动导出的帧缓冲渲染为 PNG，并与 Python 模拟结果逐字节比对
用法: python render_dump.py
"""
import os
import sys
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.dirname(HERE)
C_DUMP = os.path.join(HERE, "test_stub", "framebuffer.bin")
PY_DUMP = os.path.join(OUT_DIR, "preview_test.bin")


def render(data, path, scale=6):
    assert len(data) == 1024, "帧缓冲必须为 1024 字节"
    img = Image.new("RGB", (128 * scale, 64 * scale), (8, 10, 24))
    d = ImageDraw.Draw(img)
    for page in range(8):
        for col in range(128):
            byte = data[page * 128 + col]
            if byte:
                for bit in range(8):
                    if byte & (1 << bit):
                        y = page * 8 + bit
                        d.rectangle([col * scale, y * scale,
                                     col * scale + scale - 1, y * scale + scale - 1],
                                    fill=(0, 214, 200))
    img.save(path)


def main():
    with open(C_DUMP, "rb") as f:
        c_data = f.read()
    with open(PY_DUMP, "rb") as f:
        py_data = f.read()

    render(c_data, os.path.join(OUT_DIR, "preview_from_c.png"))

    if c_data == py_data:
        print("帧缓冲逐字节比对: 完全一致 (%d 字节)" % len(c_data))
    else:
        diff = [i for i in range(1024) if c_data[i] != py_data[i]]
        print("帧缓冲比对: %d 字节不一致" % len(diff))
        for i in diff[:20]:
            print("  偏移 %3d (页 %d 列 %3d): C=0x%02X Python=0x%02X"
                  % (i, i // 128, i % 128, c_data[i], py_data[i]))
        sys.exit(1)


if __name__ == "__main__":
    main()
