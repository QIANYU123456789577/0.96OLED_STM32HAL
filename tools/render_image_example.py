#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""render_image_example.py - 验证 README 中的示例图片取模数据（心形 16x16）"""
import os
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.dirname(HERE)

# 与 README.md 中完全一致的取模数据（按页取模：每 8 行一组，每字节 8 个水平像素，高位在左）
IMG_HEART = [
    0x00, 0x00,   # 第 0 行
    0x38, 0x38,   # 第 1 行
    0x7C, 0x7C,   # 第 2 行
    0xFE, 0xF8,   # 第 3 行
    0xFE, 0xF8,   # 第 4 行
    0xFE, 0xF8,   # 第 5 行
    0x3F, 0xF0,   # 第 6 行
    0x1F, 0xE0,   # 第 7 行
    0x0F, 0xC0,   # 第 8 行
    0x07, 0x80,   # 第 9 行
    0x03, 0x00,   # 第 10 行
    0x00, 0x00,   # 第 11 行
    0x00, 0x00,   # 第 12 行
    0x00, 0x00,   # 第 13 行
    0x00, 0x00,   # 第 14 行
    0x00, 0x00,   # 第 15 行
]


def main():
    scale = 20
    img = Image.new("RGB", (16 * scale, 16 * scale), (8, 10, 24))
    d = ImageDraw.Draw(img)
    for row in range(16):
        for c in range(16):
            if IMG_HEART[row * 2 + c // 8] & (0x80 >> (c % 8)):
                d.rectangle([c * scale, row * scale,
                             c * scale + scale - 1, row * scale + scale - 1],
                            fill=(0, 214, 200))
    img.save(os.path.join(OUT_DIR, "preview_image_example.png"))
    print("saved preview_image_example.png")


if __name__ == "__main__":
    main()
