# OLED 显示屏驱动程序

基于 **STM32 HAL 库**的 **SSD1306 128×64 OLED**（I2C 接口）驱动程序，提供帧缓冲管理、绘图原语、ASCII 文本（6×8 / 8×16）、数字与小数显示、按页取模图片显示等完整功能。

## 1. 文件清单

| 文件 | 说明 |
| --- | --- |
| `OLED.h` | 对外接口与用户配置宏（I2C 句柄、从机地址、超时等） |
| `OLED.c` | 驱动实现（底层 I2C、初始化、帧缓冲、绘图、文本） |
| `OLED_Font.h` | ASCII 字模数据（6×8 手工点阵 + 8×16，字符范围 0x20~0x7E） |
| `README.md` | 本文档 |
| `example/main_example.c` | 完整集成示例（CubeMX 工程 main.c 写法，含 500ms 定时刷新、进度条、图标显示） |
| `preview_test.png` | `OLED_Test()` 上电演示的屏幕效果预览（由驱动实际输出渲染） |
| `preview_sheet_6x8.png` / `preview_sheet_8x16.png` | 全部字符的字模预览表 |
| `tools/` | 开发工具（字模再生成、PC 端编译验证，可选，见第 9 节） |

资源占用（参考值）：字模数据约 **2.3 KB Flash**，帧缓冲 **1 KB RAM**，代码约 2~3 KB Flash（视编译器与优化等级而定）。

## 2. 硬件与接线

适用模块：0.96 寸 SSD1306 128×64 OLED（I2C 版本，4Pin）。

| OLED 模块引脚 | STM32（默认 I2C1） | 说明 |
| --- | --- | --- |
| VCC | 3.3V | 多数模块兼容 3.3V / 5V |
| GND | GND | |
| SCL | PB6（I2C1_SCL） | |
| SDA | PB7（I2C1_SDA） | |

注意事项：

1. **上拉电阻**：I2C 需要 SCL/SDA 上拉。大多数模块已板载 4.7kΩ 上拉；若使用裸屏/自建电路，请务必外接。
2. **从机地址**：默认 `0x78`（即 7 位地址 0x3C）。少数模块为 0x3D（对应 `0x7A`），如无显示可用 I2C 扫描程序确认，然后修改 `OLED.h` 中的 `OLED_I2C_ADDR`。
3. **I2C 时钟**：建议 400 kHz（快速模式）。100 kHz 标准模式下刷新全屏约需 100 ms，请将 `OLED_TIMEOUT` 调大（如 200）或提高时钟。

## 3. 快速上手（STM32CubeMX）

1. **配置 I2C**：Pinout → Connectivity → I2C1 → Fast Mode（400 kHz），生成代码。
2. **加入驱动**：将 `OLED.h`、`OLED.c`、`OLED_Font.h` 复制到工程的 `Core/Inc` 与 `Core/Src`（或保持相对路径加入工程）。
3. **修改包含**（非 F1 系列时）：`OLED.h` 默认包含 `stm32f1xx_hal.h`。F4 系列改为 `stm32f4xx_hal.h`，或在编译选项中添加 `-DOLED_HAL_HEADER='"stm32f4xx_hal.h"'`。
4. **编写主程序**（完整可对照的示例见 `example/main_example.c`）：

```c
#include "main.h"
#include "OLED.h"

int main(void)
{
    /* CubeMX 自动生成的初始化 */
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C1_Init();          /* 确保在 OLED_Init 之前初始化 I2C */

    /* OLED 初始化 */
    if (OLED_Init() != 0) {
        /* I2C 无响应：检查接线、地址、上拉电阻 */
        while (1);
    }

    /* 上电自检演示：弹跳小球动画 + 功能展示（调试通过后可注释掉） */
    OLED_Test();

    /* 你的应用代码 */
    OLED_Clear();
    OLED_ShowString(0, 0, "Hello, OLED!", OLED_FONT_8X16, OLED_ON);
    OLED_ShowNum(0, 20, 2026, 4, OLED_FONT_6X8, OLED_ON);
    OLED_ShowFloat(0, 30, 3.14159f, 3, 1, OLED_FONT_6X8, OLED_ON);
    OLED_Printf(0, 40, OLED_FONT_6X8, OLED_ON, "T:%d V:%.2f", 25, 3.3f);
    OLED_DrawCircle(100, 50, 10, OLED_ON);
    OLED_Refresh();          /* 所有绘制都在帧缓冲中进行，最后统一刷新 */

    while (1) {
        /* 主循环 */
    }
}
```

## 4. API 说明

### 初始化与刷新

| 函数 | 说明 |
| --- | --- |
| `OLED_Init()` | 初始化 SSD1306，返回 0 成功 / -1 失败（I2C 无响应） |
| `OLED_AttachI2C(&hi2c)` | 运行时指定 I2C 句柄（可选，默认用编译期配置的句柄） |
| `OLED_Refresh()` | 将帧缓冲内容刷新到屏幕（修改显示内容后必须调用） |
| `OLED_Clear()` / `OLED_Fill(c)` | 清屏 / 全屏填充 |
| `OLED_ClearArea(x, y, w, h)` | 清除指定区域 |

### 绘图

| 函数 | 说明 |
| --- | --- |
| `OLED_SetPixel(x, y, c)` / `OLED_GetPixel(x, y)` | 画点 / 读点，坐标自动裁剪 |
| `OLED_DrawLine(x0, y0, x1, y1, c)` | 直线（Bresenham） |
| `OLED_DrawRect(x, y, w, h, c)` / `OLED_FillRect(...)` | 矩形边框 / 实心矩形 |
| `OLED_DrawTriangle(x0, y0, x1, y1, x2, y2, c)` | 三角形边框 |
| `OLED_DrawCircle(cx, cy, r, c)` / `OLED_FillCircle(...)` | 圆（中点画圆法） |
| `OLED_DrawImage(x, y, w, h, img, c)` | 按页取模图片，`h` 须为 8 的倍数（见第 6 节） |
| `OLED_DrawBitmap(bmp)` | 全屏位图，1024 字节，布局同帧缓冲 |

### 文本

| 函数 | 说明 |
| --- | --- |
| `OLED_ShowChar(x, y, ch, font, c)` | 单字符 |
| `OLED_ShowString(x, y, str, font, c)` | 字符串，支持 `\n`（换行）、`\r`（回到行首）与自动换行 |
| `OLED_ShowNum(x, y, num, len, font, c)` | 有符号整数；`len` 为总位数（含负号），不足高位补 0，传 0 按实际位数 |
| `OLED_ShowFloat(x, y, num, frac, int_len, font, c)` | 小数；`frac` 小数位数（0~7），`int_len` 整数部分最小位数（含负号），四舍五入 |
| `OLED_Printf(x, y, font, c, fmt, ...)` | 格式化输出，内部缓冲 40 字节 |

`font` 取 `OLED_FONT_6X8`（每行 21 字符 × 8 行）或 `OLED_FONT_8X16`（每行 16 字符 × 4 行）；`c` 取 `OLED_ON` / `OLED_OFF`（`OLED_OFF` 相当于擦除）。

### 显示控制

| 函数 | 说明 |
| --- | --- |
| `OLED_SetContrast(v)` | 对比度 0~255，默认 0xCF |
| `OLED_DisplayOn(on)` | 1 开显示 / 0 关闭（休眠，显存内容保留） |
| `OLED_Invert(inv)` | 1 反色 / 0 正常 |
| `OLED_SetFlip(flip)` | 1 显示旋转 180° / 0 正常（用于模块倒装场景） |

## 5. 中文显示

驱动本身只含 ASCII 字模。显示中文的方法：用取模软件生成 16×16（或 8×16 等）点阵，以按页取模数组存放，再用 `OLED_DrawImage()` 显示：

```c
/* "中" 字 16x16 点阵（示例格式，实际字模请用取模软件生成） */
const uint8_t han_zhong[32] = { /* 32 字节，见第 6 节格式说明 */ };
OLED_DrawImage(0, 0, 16, 16, han_zhong, OLED_ON);
```

字宽 16 像素、行高 16 像素即可排版中文界面（16 列 × 4 行）。

## 6. 图像取模格式（重点）

`OLED_DrawImage()` 使用的取模格式与 SSD1306 显存一致：

- **按页取模**：图像按 8 像素行分为若干"页"，高 `h` 必须是 8 的倍数；
- **逐行式（横向）**：每字节表示**一行中从左到右的 8 个水平像素**；
- **高位在前**：字节最高位（bit7）= 该行最左侧像素；
- 数据顺序：第 0 页的第 0 行、第 1 行 …… 第 7 行，然后第 1 页的第 0 行 ……，每行 `w` 字节。

以 PCtoLCD2000 取模为例，选项设置为：**字节横向（逐行式）＋ 高位在前**，即可直接得到本驱动可用的数组。

示例：16×16 心形图标（`OLED_DrawImage(x, y, 16, 16, img, OLED_ON)`）：

```c
const uint8_t Img_Heart[32] = {
    0x00, 0x00,   /* 第 0 行  */
    0x38, 0x38,   /* 第 1 行  */
    0x7C, 0x7C,   /* 第 2 行  */
    0xFE, 0xF8,   /* 第 3 行  */
    0xFE, 0xF8,   /* 第 4 行  */
    0xFE, 0xF8,   /* 第 5 行  */
    0x3F, 0xF0,   /* 第 6 行  */
    0x1F, 0xE0,   /* 第 7 行  */
    0x0F, 0xC0,   /* 第 8 行  */
    0x07, 0x80,   /* 第 9 行  */
    0x03, 0x00,   /* 第 10 行 */
    0x00, 0x00,   /* 第 11 行 */
    0x00, 0x00,   /* 第 12 行 */
    0x00, 0x00,   /* 第 13 行 */
    0x00, 0x00,   /* 第 14 行 */
    0x00, 0x00,   /* 第 15 行 */
};
```

效果见 `preview_image_example.png`（可运行第 9 节脚本重新生成）。

## 7. 移植说明

驱动对硬件的全部依赖集中在 `OLED.c` 的三个静态函数中：

```c
static HAL_StatusTypeDef OLED_WriteCmd(uint8_t cmd);          /* 写命令 */
static HAL_StatusTypeDef OLED_WriteCmds(const uint8_t *cmds, uint16_t num); /* 写多条命令 */
static HAL_StatusTypeDef OLED_WriteData(const uint8_t *data, uint16_t num); /* 写显示数据 */
```

- **更换 MCU 系列**（F4/F7/G0/H7 等）：修改 `OLED_HAL_HEADER` 即可，其他代码不用动。
- **更换 I2C 句柄**（如 hi2c2）：`#define OLED_I2C_HANDLE &hi2c2`，或运行时调用 `OLED_AttachI2C(&hi2c2)`。
- **移植到 SPI 接口**：SSD1306 的 SPI 时序为 D/C# 数据/命令选择 + SCLK + SDIN + CS，只需把上述三个函数改为 GPIO 模拟或 HAL SPI 实现（D/C#=0 写命令、D/C#=1 写数据），上层 API 完全不用改。
- **移植到标准库 / Arduino / 其他平台**：同样只重写这三个函数，并将 `OLED.c` 中的 `HAL_*` 依赖去掉。

## 8. 常见问题（FAQ）

**Q1：屏幕完全无反应。**
按顺序检查：① VCC/GND 是否接好；② SCL/SDA 是否接反、是否接在 I2C 引脚上；③ 模块是否有上拉电阻；④ 从机地址是 0x78 还是 0x7A；⑤ `MX_I2C1_Init()` 是否在 `OLED_Init()` 之前调用。可用 `OLED_Init()` 的返回值判断 I2C 是否无响应（返回 -1）。

**Q2：画面花屏、条纹、闪烁。**
多为 I2C 速度过快或超时不足：降低 I2C 时钟至 100 kHz 并将 `OLED_TIMEOUT` 调大；或检查杜邦线质量、缩短线长、确认上拉电阻。

**Q3：画面上下或左右颠倒。**
初始化序列中 `0xA1/0xC8` 控制重映射方向：左右颠倒改为 `0xA0`，上下颠倒改为 `0xC0`；也可调用 `OLED_SetFlip(1)` 动态旋转 180°。

**Q4：屏幕很暗或对比度低。**
调用 `OLED_SetContrast()` 调整（0~255）；确认模块供电充足（VCC 建议 3.3V 以上，部分模块需 5V 供电，注意电平匹配）。

**Q5：编译报错 `hi2c1` 未定义。**
工程使用的 I2C 句柄不叫 `hi2c1`。按第 7 节说明定义 `OLED_I2C_HANDLE`，或调用 `OLED_AttachI2C()`。

**Q6：1.3 寸屏（SH1106）能用吗？**
SH1106 与 SSD1306 命令基本兼容，但列地址有 2 列偏移。可将 `OLED_Refresh()` 中列地址范围 `0x21, 0x00, 0x7F` 改为 `0x21, 0x02, 0x81` 后尝试。

**Q7：`OLED_Printf` 使程序体积增大很多。**
`vsnprintf` 会引入格式化代码。如对 Flash 敏感，可去掉 `OLED_Printf`（删除函数及其调用）以节省数 KB。

## 9. 开发工具（tools/，可选）

以下脚本用于字模维护与 PC 端验证，**不是驱动运行所必需的**：

| 脚本 | 用途 |
| --- | --- |
| `tools/gen_fonts.py` | 重新生成 `OLED_Font.h` 及全部预览图（需 Pillow） |
| `tools/font6x8_data.py` | 6×8 手工点阵源数据（修改字形后重新运行 gen_fonts.py） |
| `tools/test_stub/` | PC 端 GCC 编译与功能验证（桩 HAL + 测试主程序） |
| `tools/render_dump.py` | 将驱动导出的帧缓冲渲染为 PNG，并与模拟结果逐字节比对 |
| `tools/render_image_example.py` | 生成第 6 节心形示例的预览图 |

PC 端验证方法（本仓库交付前已通过）：

```bash
# 1. 编译并运行驱动（导出帧缓冲 framebuffer.bin）
gcc -std=c99 -Wall -Wextra -I tools/test_stub -I . \
    tools/test_stub/hal_stub.c tools/test_stub/test_main.c OLED.c \
    -o tools/test_stub/oled_test.exe
cd tools/test_stub && ./oled_test.exe && cd ..

# 2. 与 Python 模拟结果逐字节比对（应输出"完全一致"）
python tools/render_dump.py
```

## 10. 演示效果

`OLED_Test()` 上电演示的屏幕效果（由本驱动在 PC 端实际运行后渲染，见 `preview_test.png`）：

- 第一行 8×16 大字体标题；
- 6×8 小字体信息行（含 `OLED_ShowFloat`、`OLED_ShowNum` 实际输出）；
- 边框、对角线、空心圆、实心圆等绘图原语。
