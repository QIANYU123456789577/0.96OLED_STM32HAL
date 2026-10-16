/**
 * @file OLED.c
 * @brief SSD1306 128x64 OLED 驱动实现（I2C 接口，基于 STM32 HAL 库）
 */

#include "OLED.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include "OLED_Font.h"

/* 帧缓冲：[页][列]，每 bit 对应一个像素，布局与 SSD1306 显存一致 */
uint8_t OLED_Buffer[OLED_PAGE_NUM][OLED_WIDTH];

/* I2C 句柄（可通过 OLED_AttachI2C 在运行时指定） */
static I2C_HandleTypeDef *OLED_I2C = OLED_I2C_HANDLE;

void OLED_AttachI2C(I2C_HandleTypeDef *hi2c)
{
    if (hi2c != NULL) {
        OLED_I2C = hi2c;
    }
}

/* ============================ 底层 I2C 读写 ============================
 * SSD1306 的 I2C 传输以 1 字节控制字节开头：
 *     0x00 + 命令字节...   命令（Co=0, D/C#=0）
 *     0x40 + 数据字节...   数据（Co=0, D/C#=1）
 * 移植到 SPI / 标准库 / 其他平台时，只需重写下面三个函数。
 * ====================================================================== */

static HAL_StatusTypeDef OLED_WriteCmd(uint8_t cmd)
{
    uint8_t buf[2];
    buf[0] = 0x00;
    buf[1] = cmd;
    return HAL_I2C_Master_Transmit(OLED_I2C, OLED_I2C_ADDR, buf, 2, OLED_TIMEOUT);
}

static HAL_StatusTypeDef OLED_WriteCmds(const uint8_t *cmds, uint16_t num)
{
    uint8_t buf[17];
    while (num > 0) {
        uint16_t n = (num > 16) ? 16 : num;   /* 每次最多 16 条命令，与 SSD1306 内部缓冲匹配 */
        buf[0] = 0x00;
        memcpy(buf + 1, cmds, n);
        if (HAL_I2C_Master_Transmit(OLED_I2C, OLED_I2C_ADDR, buf, n + 1, OLED_TIMEOUT) != HAL_OK) {
            return HAL_ERROR;
        }
        cmds += n;
        num -= n;
    }
    return HAL_OK;
}

static HAL_StatusTypeDef OLED_WriteData(const uint8_t *data, uint16_t num)
{
    /* 控制字节 0x40 作为 8 位"内存地址"传入，随后连续写入数据 */
    return HAL_I2C_Mem_Write(OLED_I2C, OLED_I2C_ADDR, 0x40, I2C_MEMADD_SIZE_8BIT,
                             (uint8_t *)data, num, OLED_TIMEOUT);
}

/* ================================ 初始化 ================================ */

int OLED_Init(void)
{
    /* SSD1306 128x64 标准初始化序列（水平寻址模式） */
    static const uint8_t init_seq[] = {
        0xAE,         /* 关闭显示（初始化过程中） */
        0xD5, 0x80,   /* 时钟分频比 1，振荡器频率默认 */
        0xA8, 0x3F,   /* 多路复用率 1/64 */
        0xD3, 0x00,   /* 显示偏移 0 */
        0x40,         /* 起始行 0 */
        0x8D, 0x14,   /* 电荷泵使能（模块无外部升压电路时必需） */
        0x20, 0x00,   /* 内存寻址模式：水平寻址 */
        0xA1,         /* 段重映射：列 127 -> SEG0（画面左右颠倒时改为 0xA0） */
        0xC8,         /* COM 扫描方向：反向（画面上下颠倒时改为 0xC0） */
        0xDA, 0x12,   /* COM 引脚硬件配置（128x64） */
        0x81, 0xCF,   /* 对比度 */
        0xD9, 0xF1,   /* 预充电周期 */
        0xDB, 0x30,   /* V_COMH 反激电平 */
        0xA4,         /* 全局显示开：输出跟随显存内容 */
        0xA6,         /* 正常显示（1 亮 0 灭）；反色可用 0xA7 */
        0xAF,         /* 开启显示 */
    };

    HAL_Delay(100);    /* 上电后等待 SSD1306 内部电源稳定 */
    if (OLED_WriteCmds(init_seq, sizeof(init_seq)) != HAL_OK) {
        return -1;
    }
    OLED_Clear();
    OLED_Refresh();
    return 0;
}

void OLED_Refresh(void)
{
    /* 设置水平寻址的列/页范围：列 0~127，页 0~7，随后连续写入 1024 字节 */
    static const uint8_t addr_seq[] = {
        0x21, 0x00, 0x7F,   /* 列地址指针范围 */
        0x22, 0x00, 0x07,   /* 页地址指针范围 */
    };
    OLED_WriteCmds(addr_seq, sizeof(addr_seq));
    OLED_WriteData(&OLED_Buffer[0][0], sizeof(OLED_Buffer));
}

void OLED_Clear(void)
{
    memset(OLED_Buffer, 0, sizeof(OLED_Buffer));
}

void OLED_Fill(uint8_t color)
{
    memset(OLED_Buffer, (color != OLED_OFF) ? 0xFF : 0x00, sizeof(OLED_Buffer));
}

void OLED_ClearArea(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
    uint8_t i, j;
    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            OLED_SetPixel((int16_t)(x + i), (int16_t)(y + j), OLED_OFF);
        }
    }
}

/* ================================ 绘图原语 ================================ */

void OLED_SetPixel(int16_t x, int16_t y, uint8_t color)
{
    uint8_t page, bit;
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) {
        return;
    }
    page = (uint8_t)(y >> 3);
    bit  = (uint8_t)(y & 0x07);
    if (color != OLED_OFF) {
        OLED_Buffer[page][x] |= (uint8_t)(1 << bit);
    } else {
        OLED_Buffer[page][x] &= (uint8_t)~(1 << bit);
    }
}

uint8_t OLED_GetPixel(int16_t x, int16_t y)
{
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) {
        return 0;
    }
    return (uint8_t)((OLED_Buffer[y >> 3][x] >> (y & 0x07)) & 0x01);
}

/* 在 (x,y) 处横向绘制 width 个像素，data 最高位对应最左侧像素 */
static void OLED_DrawByte(uint8_t x, uint8_t y, uint8_t data, uint8_t width, uint8_t color)
{
    uint8_t page = (uint8_t)(y >> 3);
    uint8_t bit  = (uint8_t)(y & 0x07);
    uint8_t i;
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) {
        return;
    }
    for (i = 0; i < width; i++) {
        if ((data & (0x80 >> i)) == 0) {
            continue;
        }
        if ((uint8_t)(x + i) >= OLED_WIDTH) {
            break;
        }
        if (color != OLED_OFF) {
            OLED_Buffer[page][x + i] |= (uint8_t)(1 << bit);
        } else {
            OLED_Buffer[page][x + i] &= (uint8_t)~(1 << bit);
        }
    }
}

void OLED_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color)
{
    int16_t dx = (x1 > x0) ? (int16_t)(x1 - x0) : (int16_t)(x0 - x1);
    int16_t dy = (y1 > y0) ? (int16_t)(y0 - y1) : (int16_t)(y1 - y0);   /* 注意取负 */
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx + dy;
    int16_t e2;

    for (;;) {
        OLED_SetPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        e2 = (int16_t)(2 * err);
        if (e2 >= dy) {
            err += dy;
            x0 = (int16_t)(x0 + sx);
        }
        if (e2 <= dx) {
            err += dx;
            y0 = (int16_t)(y0 + sy);
        }
    }
}

void OLED_DrawRect(int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t color)
{
    if (w == 0 || h == 0) {
        return;
    }
    OLED_DrawLine(x, y, (int16_t)(x + w - 1), y, color);
    OLED_DrawLine(x, (int16_t)(y + h - 1), (int16_t)(x + w - 1), (int16_t)(y + h - 1), color);
    OLED_DrawLine(x, y, x, (int16_t)(y + h - 1), color);
    OLED_DrawLine((int16_t)(x + w - 1), y, (int16_t)(x + w - 1), (int16_t)(y + h - 1), color);
}

void OLED_FillRect(int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t color)
{
    uint8_t i, j;
    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            OLED_SetPixel((int16_t)(x + i), (int16_t)(y + j), color);
        }
    }
}

void OLED_DrawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                       int16_t x2, int16_t y2, uint8_t color)
{
    OLED_DrawLine(x0, y0, x1, y1, color);
    OLED_DrawLine(x1, y1, x2, y2, color);
    OLED_DrawLine(x2, y2, x0, y0, color);
}

void OLED_DrawCircle(int16_t cx, int16_t cy, uint8_t r, uint8_t color)
{
    int16_t x = 0;
    int16_t y = r;
    int16_t d = (int16_t)(3 - 2 * r);   /* 中点画圆法判据 */

    while (x <= y) {
        OLED_SetPixel((int16_t)(cx + x), (int16_t)(cy + y), color);
        OLED_SetPixel((int16_t)(cx - x), (int16_t)(cy + y), color);
        OLED_SetPixel((int16_t)(cx + x), (int16_t)(cy - y), color);
        OLED_SetPixel((int16_t)(cx - x), (int16_t)(cy - y), color);
        OLED_SetPixel((int16_t)(cx + y), (int16_t)(cy + x), color);
        OLED_SetPixel((int16_t)(cx - y), (int16_t)(cy + x), color);
        OLED_SetPixel((int16_t)(cx + y), (int16_t)(cy - x), color);
        OLED_SetPixel((int16_t)(cx - y), (int16_t)(cy - x), color);
        if (d < 0) {
            d = (int16_t)(d + 4 * x + 6);
        } else {
            d = (int16_t)(d + 4 * (x - y) + 10);
            y--;
        }
        x++;
    }
}

/* 整数平方根（牛顿逐位逼近，避免引入浮点库） */
static uint16_t OLED_ISqrt(uint32_t v)
{
    uint32_t x = v;
    uint32_t c = 0;
    uint32_t d = 1U << 30;

    while (d > x) {
        d >>= 2;
    }
    while (d != 0) {
        if (x >= c + d) {
            x -= c + d;
            c = (c >> 1) + d;
        } else {
            c >>= 1;
        }
        d >>= 2;
    }
    return (uint16_t)c;
}

void OLED_FillCircle(int16_t cx, int16_t cy, uint8_t r, uint8_t color)
{
    int16_t dy;
    uint32_t r2 = (uint32_t)r * (uint32_t)r;

    for (dy = -(int16_t)r; dy <= (int16_t)r; dy++) {
        uint16_t dx = OLED_ISqrt(r2 - (uint32_t)(dy * dy));
        OLED_DrawLine((int16_t)(cx - dx), (int16_t)(cy + dy),
                      (int16_t)(cx + dx), (int16_t)(cy + dy), color);
    }
}

void OLED_DrawImage(uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                    const uint8_t *img, uint8_t color)
{
    uint8_t row, col;
    if (img == NULL || w == 0 || h == 0 || (h % 8) != 0) {
        return;
    }
    /* img 布局：每 8 行为一组（页），组内逐行、每字节 8 个水平像素、高位在左 */
    for (row = 0; row < (uint8_t)(h / 8); row++) {
        for (col = 0; col < w; col++) {
            OLED_DrawByte((uint8_t)(x + col), (uint8_t)(y + row * 8),
                          img[row * w + col], 8, color);
        }
    }
}

void OLED_DrawBitmap(const uint8_t *bmp)
{
    if (bmp != NULL) {
        memcpy(OLED_Buffer, bmp, sizeof(OLED_Buffer));
    }
}

/* ================================ 文本显示 ================================ */

void OLED_ShowChar(uint8_t x, uint8_t y, char ch, uint8_t font, uint8_t color)
{
    uint8_t rows, width, i;
    const uint8_t *glyph;
    uint8_t c = (uint8_t)ch;

    if (c < 0x20 || c > 0x7E) {
        c = '?';    /* 不可显示字符用问号代替 */
    }
    c = (uint8_t)(c - 0x20);

    if (font == OLED_FONT_6X8) {
        glyph = OLED_Font6x8[c];
        rows = 8;
        width = 6;
    } else {
        glyph = OLED_Font8x16[c];
        rows = 16;
        width = 8;
    }
    for (i = 0; i < rows; i++) {
        OLED_DrawByte(x, (uint8_t)(y + i), glyph[i], width, color);
    }
}

void OLED_ShowString(uint8_t x, uint8_t y, const char *str, uint8_t font, uint8_t color)
{
    uint8_t step = (font == OLED_FONT_6X8) ? 6 : 8;   /* 字宽即行高 */

    while (*str != '\0') {
        if (*str == '\n') {
            x = 0;
            y = (uint8_t)(y + step);
        } else if (*str == '\r') {
            x = 0;
        } else {
            OLED_ShowChar(x, y, *str, font, color);
            x = (uint8_t)(x + step);
            if ((uint8_t)(x + step) > OLED_WIDTH) {   /* 自动换行 */
                x = 0;
                y = (uint8_t)(y + step);
            }
        }
        if ((uint8_t)(y + step) > OLED_HEIGHT) {      /* 超出屏幕则停止绘制 */
            break;
        }
        str++;
    }
}

/* 显示无符号整数，len 为最小位数（不足补前导零，0 表示按实际位数），返回结束时的 x */
static uint8_t OLED_ShowUint(uint8_t x, uint8_t y, uint32_t num, uint8_t len,
                             uint8_t font, uint8_t color)
{
    uint8_t step = (font == OLED_FONT_6X8) ? 6 : 8;
    char tmp[12];
    uint8_t nd = 0;
    uint8_t zeros;

    do {
        tmp[nd++] = (char)('0' + (num % 10U));
        num /= 10U;
    } while (num != 0);

    zeros = (len > nd) ? (uint8_t)(len - nd) : 0;
    while (zeros > 0) {
        OLED_ShowChar(x, y, '0', font, color);
        x = (uint8_t)(x + step);
        zeros--;
    }
    while (nd > 0) {
        nd--;
        OLED_ShowChar(x, y, tmp[nd], font, color);
        x = (uint8_t)(x + step);
    }
    return x;
}

void OLED_ShowNum(uint8_t x, uint8_t y, int32_t num, uint8_t len, uint8_t font, uint8_t color)
{
    uint8_t step = (font == OLED_FONT_6X8) ? 6 : 8;
    uint8_t neg = (num < 0) ? 1 : 0;
    uint32_t n;

    if (neg) {
        n = (uint32_t)(-(num + 1)) + 1U;   /* 避免 INT32_MIN 取负溢出 */
    } else {
        n = (uint32_t)num;
    }
    if (neg) {
        OLED_ShowChar(x, y, '-', font, color);
        x = (uint8_t)(x + step);
    }
    OLED_ShowUint(x, y, n, (len > neg) ? (uint8_t)(len - neg) : 0, font, color);
}

void OLED_ShowFloat(uint8_t x, uint8_t y, float num, uint8_t frac_num, uint8_t int_len,
                    uint8_t font, uint8_t color)
{
    uint8_t step = (font == OLED_FONT_6X8) ? 6 : 8;
    uint8_t neg = 0;
    uint8_t i;
    float v = num;
    uint32_t scale = 1;
    uint32_t ip;
    uint32_t fp;

    if (frac_num > 7) {
        frac_num = 7;
    }
    if (v < 0.0f) {
        neg = 1;
        v = -v;
    }
    for (i = 0; i < frac_num; i++) {
        scale *= 10U;
    }
    ip = (uint32_t)v;
    fp = (uint32_t)((v - (float)ip) * (float)scale + 0.5f);   /* 四舍五入 */
    if (fp >= scale) {          /* 进位，如 9.99 保留 1 位 -> 10.0 */
        ip++;
        fp -= scale;
    }

    if (neg) {
        OLED_ShowChar(x, y, '-', font, color);
        x = (uint8_t)(x + step);
    }
    x = OLED_ShowUint(x, y, ip, (int_len > neg) ? (uint8_t)(int_len - neg) : 0, font, color);

    if (frac_num > 0) {
        char ftmp[8];
        uint8_t fn = 0;
        uint8_t fi;
        OLED_ShowChar(x, y, '.', font, color);
        x = (uint8_t)(x + step);
        do {
            ftmp[fn++] = (char)('0' + (fp % 10U));
            fp /= 10U;
        } while (fp != 0);
        for (fi = fn; fi < frac_num; fi++) {    /* 补前导零，如 .05 */
            OLED_ShowChar(x, y, '0', font, color);
            x = (uint8_t)(x + step);
        }
        while (fn > 0) {
            fn--;
            OLED_ShowChar(x, y, ftmp[fn], font, color);
            x = (uint8_t)(x + step);
        }
    }
}

void OLED_Printf(uint8_t x, uint8_t y, uint8_t font, uint8_t color, const char *fmt, ...)
{
    char buf[40];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    OLED_ShowString(x, y, buf, font, color);
}

/* ================================ 显示控制 ================================ */

void OLED_SetContrast(uint8_t value)
{
    OLED_WriteCmd(0x81);
    OLED_WriteCmd(value);
}

void OLED_DisplayOn(uint8_t on)
{
    OLED_WriteCmd((on != OLED_OFF) ? 0xAF : 0xAE);
}

void OLED_Invert(uint8_t invert)
{
    OLED_WriteCmd((invert != OLED_OFF) ? 0xA7 : 0xA6);
}

void OLED_SetFlip(uint8_t flip)
{
    /* 段重映射与 COM 扫描方向同时翻转，画面旋转 180 度（显存内容不变） */
    OLED_WriteCmd((flip != OLED_OFF) ? 0xA0 : 0xA1);
    OLED_WriteCmd((flip != OLED_OFF) ? 0xC0 : 0xC8);
}

/* ================================= 演示 ================================== */

static void OLED_TestFrame(void)
{
    OLED_DrawRect(0, 0, 128, 64, OLED_ON);
    OLED_DrawLine(0, 0, 127, 63, OLED_ON);
    OLED_DrawCircle(96, 46, 12, OLED_ON);
    OLED_FillCircle(24, 46, 8, OLED_ON);
    OLED_ShowString(6, 6, "Hello, STM32!", OLED_FONT_8X16, OLED_ON);
    OLED_ShowString(6, 26, "SSD1306 128x64 I2C", OLED_FONT_6X8, OLED_ON);
    OLED_ShowString(6, 36, "T:", OLED_FONT_6X8, OLED_ON);
    OLED_ShowFloat(18, 36, 25.5f, 1, 2, OLED_FONT_6X8, OLED_ON);
    OLED_ShowString(42, 36, "C V:", OLED_FONT_6X8, OLED_ON);
    OLED_ShowFloat(66, 36, 3.26f, 2, 1, OLED_FONT_6X8, OLED_ON);
    OLED_ShowString(90, 36, "V", OLED_FONT_6X8, OLED_ON);
    OLED_ShowNum(6, 46, -1234, 6, OLED_FONT_6X8, OLED_ON);
    OLED_ShowString(6, 56, "oled_driver v1.0", OLED_FONT_6X8, OLED_ON);
}

void OLED_Test(void)
{
    int16_t cx = 20;
    int16_t cy = 20;
    int16_t dx = 3;
    int16_t dy = 2;
    uint16_t i;

    OLED_Clear();

    /* 第一阶段：弹跳小球动画（70 帧，约 2 秒） */
    for (i = 0; i < 70; i++) {
        OLED_Clear();
        OLED_DrawRect(0, 0, 128, 64, OLED_ON);
        OLED_FillCircle(cx, cy, 5, OLED_ON);
        OLED_Refresh();
        cx = (int16_t)(cx + dx);
        cy = (int16_t)(cy + dy);
        if (cx <= 6 || cx >= 121) {
            dx = (int16_t)(-dx);
        }
        if (cy <= 6 || cy >= 57) {
            dy = (int16_t)(-dy);
        }
        HAL_Delay(30);
    }

    /* 第二阶段：静态功能展示 */
    OLED_Clear();
    OLED_TestFrame();
    OLED_Refresh();
}
