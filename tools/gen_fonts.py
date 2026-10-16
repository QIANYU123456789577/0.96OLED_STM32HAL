#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_fonts.py - 从系统 TrueType 字体生成 SSD1306 OLED 点阵字模（6x8 / 8x16）

用法:
    python gen_fonts.py

输出（写入脚本所在目录的上一级，即工程根目录）:
    OLED_Font.h                     C 语言字模数据（95 个 ASCII 字符 0x20~0x7E）
    preview_sheet_6x8.png / preview_sheet_8x16.png   全部字符预览表
    preview_test.png                OLED_Test() 屏幕效果预览（按驱动帧缓冲格式 1:1 模拟）

设计说明:
    - 每个字符 rows 行 x width 列；每字节最高位对应该行最左侧像素
    - 以字体真实基线对齐（font.getmetrics() 的 ascent），保证引号在上、g/p/y 尾部在下
    - 仅对超出目标尺寸的维度做压缩，避免整体变形
    - 生成后回读 OLED_Font.h 做一致性校验
"""

import os
import re
import sys
from PIL import Image, ImageDraw, ImageFont

from font6x8_data import FONT6X8

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.dirname(HERE)

FONT_MONO = "C:/Windows/Fonts/CascadiaMono.ttf"

CHARS = [chr(c) for c in range(0x20, 0x7F)]  # 0x20 ~ 0x7E，共 95 个字符

# 8x16 由 TrueType 字体渲染生成；(字体文件, 字号, 列宽, 行数, 基线行, 阈值)
# 字号 18 时大写字母墨高 12 行，配基线行 13：大写占第 1~12 行，
# g/p/q/y/j 等降部字符伸到第 15 行附近，与常见 8x16 点阵字模的满格竖向占位一致
FONTS_CFG = [
    (FONT_MONO, 18, 8, 16, 13, 110),
]


def art_to_bytes(art):
    """把 ASCII 点阵图（'#' 为亮点）转换为字节数组，每字节最高位 = 最左像素"""
    data = []
    for row in art:
        assert len(row) == 6, "6x8 字模每行必须为 6 列: %r" % row
        b = 0
        for c, ch in enumerate(row):
            if ch == "#":
                b |= 1 << (7 - c)
            else:
                assert ch == ".", "只允许 '#' 或 '.': %r" % row
        data.append(b)
    return data


def build_font_6x8_hand():
    """使用手工设计的 6x8 字模"""
    assert len(FONT6X8) == 95, "6x8 字模必须包含 95 个字符"
    data = {}
    for ch in CHARS:
        art = FONT6X8[ch]
        assert len(art) == 8, "6x8 字模必须为 8 行: %r" % ch
        data[ch] = art_to_bytes(art)
    print("字体 6x8: 手工设计字模，95 个字符")
    return data


def render_glyph(font, ascent, ch, width, rows, baseline, thresh=110):
    """渲染单个字符为 rows 行 x width 列点阵，按真实基线对齐。返回 (data, info)。"""
    Wc = (width + 6) * 4
    img = Image.new("L", (Wc, Wc), 0)
    d = ImageDraw.Draw(img)
    d.text((Wc // 2, 0), ch, font=font, fill=255)  # 默认锚点 la：y=0 为升部顶
    bbox = img.getbbox()
    if bbox is None:
        return [0] * rows, {"w": 0, "h": 0, "clip_top": 0, "clip_bottom": 0,
                            "squeezed": False}
    l, t, r, b = bbox
    glyph = img.crop((l, t, r, b))
    iw, ih = glyph.size
    squeezed = False
    if iw > width:
        glyph = glyph.resize((width, ih), Image.BOX)
        iw = width
        squeezed = True
    if ih > rows:
        glyph = glyph.resize((iw, rows), Image.BOX)
        ih = rows
        squeezed = True
    # 基线对齐：源图中基线在 y=ascent，裁剪图中基线距顶 (ascent - t) 行；
    # 若做过纵向压缩，基线位置需按压缩比例折算，目标基线行为 baseline
    src_h = b - t
    base_in_glyph = (ascent - t) * ih / src_h if src_h > 0 else 0.0
    yoff = int(round(baseline - base_in_glyph))
    xoff = (width - iw) // 2
    data = [0] * rows
    clip_top = clip_bottom = 0
    for sy in range(ih):
        ty = sy + yoff
        if ty < 0:
            clip_top += 1
            continue
        if ty >= rows:
            clip_bottom += 1
            continue
        for sx in range(iw):
            tx = xoff + sx
            if 0 <= tx < width and glyph.getpixel((sx, sy)) >= thresh:
                data[ty] |= 1 << (7 - tx)
    return data, {"w": iw, "h": ih, "clip_top": clip_top,
                  "clip_bottom": clip_bottom, "squeezed": squeezed}


def build_font(cfg):
    path, size, width, rows, baseline, thresh = cfg
    font = ImageFont.truetype(path, size)
    ascent, descent = font.getmetrics()
    print("字体 %s 字号 %d: ascent=%d descent=%d" % (os.path.basename(path), size, ascent, descent))
    data, stats = {}, {}
    for ch in CHARS:
        data[ch], stats[ch] = render_glyph(font, ascent, ch, width, rows, baseline, thresh)
    return data, stats


# ---------------------------------------------------------------- 生成 C 头文件
def gen_header(fonts):
    L = []
    L.append("#ifndef __OLED_FONT_H")
    L.append("#define __OLED_FONT_H")
    L.append("")
    L.append("#include <stdint.h>")
    L.append("")
    for (width, rows), data in fonts:
        L.append("/**")
        L.append(" * @brief  %dx%d ASCII 字模（字符范围 0x20~0x7E）" % (width, rows))
        L.append(" *         每字符 %d 字节；每字节最高位 = 该行最左侧像素，低位在右" % rows)
        L.append(" */")
        L.append("const uint8_t OLED_Font%dx%d[95][%d] = {" % (width, rows, rows))
        for ch in CHARS:
            hexs = ", ".join("0x%02X" % v for v in data[ch])
            esc = ch.replace("\\", "\\\\").replace('"', '\\"')
            L.append('    {%s}, /* 0x%02X \'%s\' */' % (hexs, ord(ch), esc))
        L.append("};")
        L.append("")
    L.append("#endif /* __OLED_FONT_H */")
    L.append("")
    return "\n".join(L)


# ---------------------------------------------------------------- 回读校验
def verify_header(path, fonts):
    text = open(path, "r", encoding="utf-8").read()
    ok = True
    for (width, rows), data in fonts:
        m = re.search(
            r"OLED_Font%dx%d\[95\]\[%d\]\s*=\s*\{(.*?)\};" % (width, rows, rows),
            text, re.S)
        if not m:
            print("  [FAIL] 未找到 OLED_Font%dx%d 定义" % (width, rows))
            ok = False
            continue
        entries = re.findall(r"\{([^{}]*)\}", m.group(1))
        if len(entries) != 95:
            print("  [FAIL] OLED_Font%dx%d 条目数 %d != 95" % (width, rows, len(entries)))
            ok = False
            continue
        for ch, entry in zip(CHARS, entries):
            vals = [int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{2})", entry)]
            if vals != data[ch]:
                print("  [FAIL] 字符 0x%02X '%s' 回读不一致" % (ord(ch), ch))
                ok = False
    return ok


# ---------------------------------------------------------------- 预览渲染
def make_sheet(data, width, rows, path):
    """全部字符排成预览表（每行 16 个）"""
    per_row = 16
    cw, chh = (width + 2) * 8, (rows + 4) * 8
    nrow = (len(CHARS) + per_row - 1) // per_row
    img = Image.new("RGB", (cw * per_row, chh * nrow), (10, 12, 20))
    d = ImageDraw.Draw(img)
    for i, ch in enumerate(CHARS):
        gx = (i % per_row) * cw
        gy = (i // per_row) * chh
        for r in range(rows):
            byte = data[ch][r]
            for c in range(width):
                if byte & (0x80 >> c):
                    d.rectangle([gx + c * 8, gy + r * 8, gx + c * 8 + 7, gy + r * 8 + 7],
                                fill=(0, 220, 200))
    img.save(path)


class OLEDSim:
    """与 OLED.c 帧缓冲逻辑一致的 128x64 模拟器（用于预览）"""

    def __init__(self):
        self.buf = [[0] * 128 for _ in range(8)]

    def set_pixel(self, x, y, c):
        if 0 <= x < 128 and 0 <= y < 64:
            if c:
                self.buf[y >> 3][x] |= 1 << (y & 7)
            else:
                self.buf[y >> 3][x] &= ~(1 << (y & 7))

    def draw_byte(self, x, y, data, width, color):
        for i in range(width):
            if data & (0x80 >> i):
                self.set_pixel(x + i, y, color)

    def show_char(self, x, y, ch, font):
        width, rows, data = font
        g = data[ch]
        for r in range(rows):
            self.draw_byte(x, y + r, g[r], width, 1)

    def show_string(self, x, y, s, font):
        width = font[0]
        for ch in s:
            self.show_char(x, y, ch, font)
            x += width

    def show_num(self, x, y, num, ln, font):
        neg = num < 0
        n = -num if neg else num
        s = str(n)
        pad = ln - len(s) - (1 if neg else 0)
        if pad > 0:
            s = "0" * pad + s
        if neg:
            s = "-" + s
        self.show_string(x, y, s, font)

    def draw_line(self, x0, y0, x1, y1, c):
        dx, dy = abs(x1 - x0), -abs(y1 - y0)
        sx = 1 if x0 < x1 else -1
        sy = 1 if y0 < y1 else -1
        err = dx + dy
        while True:
            self.set_pixel(x0, y0, c)
            if x0 == x1 and y0 == y1:
                break
            e2 = 2 * err
            if e2 >= dy:
                err += dy
                x0 += sx
            if e2 <= dx:
                err += dx
                y0 += sy

    def draw_rect(self, x, y, w, h, c):
        self.draw_line(x, y, x + w - 1, y, c)
        self.draw_line(x, y + h - 1, x + w - 1, y + h - 1, c)
        self.draw_line(x, y, x, y + h - 1, c)
        self.draw_line(x + w - 1, y, x + w - 1, y + h - 1, c)

    def fill_rect(self, x, y, w, h, c):
        for j in range(h):
            for i in range(w):
                self.set_pixel(x + i, y + j, c)

    def draw_circle(self, cx, cy, r, c):
        x, y, dd = 0, r, 3 - 2 * r
        while x <= y:
            for px, py in ((cx + x, cy + y), (cx - x, cy + y), (cx + x, cy - y), (cx - x, cy - y),
                           (cx + y, cy + x), (cx - y, cy + x), (cx + y, cy - x), (cx - y, cy - x)):
                self.set_pixel(px, py, c)
            if dd < 0:
                dd += 4 * x + 6
            else:
                dd += 4 * (x - y) + 10
                y -= 1
            x += 1

    def fill_circle(self, cx, cy, r, c):
        for dy in range(-r, r + 1):
            w2 = r * r - dy * dy
            dx = 0
            while (dx + 1) * (dx + 1) <= w2:
                dx += 1
            self.draw_line(cx - dx, cy + dy, cx + dx, cy + dy, c)

    def save_png(self, path, scale=6):
        img = Image.new("RGB", (128 * scale, 64 * scale), (8, 10, 24))
        d = ImageDraw.Draw(img)
        for page in range(8):
            for col in range(128):
                byte = self.buf[page][col]
                if byte:
                    for bit in range(8):
                        if byte & (1 << bit):
                            y = page * 8 + bit
                            d.rectangle([col * scale, y * scale,
                                         col * scale + scale - 1, y * scale + scale - 1],
                                        fill=(0, 214, 200))
        img.save(path)

    def save_bin(self, path):
        data = bytes(self.buf[page][col] for page in range(8) for col in range(128))
        with open(path, "wb") as f:
            f.write(data)


def render_test_preview(fonts, path, bin_path=None):
    """与 OLED.c 中 OLED_Test() 第二阶段完全相同的画面内容"""
    sim = OLEDSim()
    f6 = (6, 8, fonts[0][1])    # fonts[0] 为手工 6x8
    f8 = (8, 16, fonts[1][1])   # fonts[1] 为 8x16
    sim.draw_rect(0, 0, 128, 64, 1)
    sim.draw_line(0, 0, 127, 63, 1)
    sim.draw_circle(96, 46, 12, 1)
    sim.fill_circle(24, 46, 8, 1)
    sim.show_string(6, 6, "Hello, STM32!", f8)
    sim.show_string(6, 26, "SSD1306 128x64 I2C", f6)
    sim.show_string(6, 36, "T:", f6)
    sim.show_string(18, 36, "25.5", f6)
    sim.show_string(42, 36, "C V:", f6)
    sim.show_string(66, 36, "3.26", f6)
    sim.show_string(90, 36, "V", f6)
    sim.show_num(6, 46, -1234, 6, f6)
    sim.show_string(6, 56, "oled_driver v1.0", f6)
    sim.save_png(path)
    if bin_path:
        sim.save_bin(bin_path)


def main():
    fonts = []
    # 6x8：手工字模
    data6 = build_font_6x8_hand()
    fonts.append(((6, 8), data6))
    make_sheet(data6, 6, 8, os.path.join(OUT_DIR, "preview_sheet_6x8.png"))
    # 8x16：TrueType 渲染
    for cfg in FONTS_CFG:
        data, stats = build_font(cfg)
        width, rows = cfg[2], cfg[3]
        fonts.append(((width, rows), data))
        sq = [ch for ch in CHARS if stats[ch]["squeezed"]]
        ct = max(stats[ch]["clip_top"] for ch in CHARS)
        cb = max(stats[ch]["clip_bottom"] for ch in CHARS)
        print("字体 %dx%d: 最大墨宽 %d, 最大墨高 %d, 顶部裁切 %d 行, 底部裁切 %d 行, 压缩字形: %s"
              % (width, rows, max(stats[ch]["w"] for ch in CHARS),
                 max(stats[ch]["h"] for ch in CHARS), ct, cb,
                 "".join(sq) if sq else "无"))
        make_sheet(data, width, rows,
                   os.path.join(OUT_DIR, "preview_sheet_%dx%d.png" % (width, rows)))

    header = gen_header(fonts)
    out = os.path.join(OUT_DIR, "OLED_Font.h")
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        f.write(header)
    print("已生成:", out)

    if verify_header(out, fonts):
        print("回读校验: 通过")
    else:
        print("回读校验: 失败!")
        sys.exit(1)

    render_test_preview(fonts, os.path.join(OUT_DIR, "preview_test.png"),
                        os.path.join(OUT_DIR, "preview_test.bin"))
    print("预览图已生成")


if __name__ == "__main__":
    main()
