# 项目长期记忆

## 字模生成管线（重要约定）
- `OLED_Font.h` 中 6x8 字模是手工设计的（源数据在 `tools/font6x8_data.py`），8x16 字模由 `tools/gen_fonts.py` 从 `C:/Windows/Fonts/CascadiaMono.ttf` 渲染生成（当前配置：字号 18、列宽 8、行数 16、基线行 13、阈值 110）。
- 改字形/字号后必须重跑 `python tools/gen_fonts.py`（会同时重生成 OLED_Font.h 与全部 preview_*.png，并做回读校验）。
- 运行环境：Pillow  venv 在 `D:/WorkBuddyData/.workbuddy/binaries/python/envs/default/Scripts/python.exe`（从 C 盘 `.workbuddy/binaries/python/envs/default` 创建会被重定向到 D 盘）。
- `render_glyph()` 的基线对齐逻辑是易错点：源图锚点为 la（y=0 是升部顶），基线在 y=ascent，裁剪后基线距顶 `(ascent - t)`；`yoff = baseline - (ascent - t)`，纵向压缩后还需按压缩比折算。2026-10-16 修过此处漏偏移的 bug。

## PC 端验证链（改驱动/字模后必跑）
1. `gcc -std=c99 -Wall -Wextra -I tools/test_stub -I . tools/test_stub/hal_stub.c tools/test_stub/test_main.c OLED.c -o tools/test_stub/oled_test.exe`
2. `cd tools/test_stub && ./oled_test.exe`（导出 framebuffer.bin）
3. `python tools/render_dump.py`（C 导出与 Python 模拟逐字节比对，应输出"完全一致"）
