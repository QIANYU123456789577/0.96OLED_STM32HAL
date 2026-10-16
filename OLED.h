/**
 * @file OLED.h
 * @brief SSD1306 128x64 OLED 驱动（I2C 接口，基于 STM32 HAL 库）
 *
 * 功能概述：
 *   - 初始化 / 清屏 / 全屏刷新 / 区域清除
 *   - 绘图原语：点、直线、矩形、三角形、圆（空心/实心）
 *   - 文本：ASCII 字符（6x8 / 8x16）、字符串（支持 \n \r 与自动换行）、
 *     有符号整数、小数、格式化输出
 *   - 图像：按页取模的图片显示、全屏位图
 *   - 显示控制：对比度、亮灭、反色、180 度翻转
 *
 * 硬件依赖性说明：
 *   所有硬件访问集中在 OLED.c 的 OLED_WriteCmd() / OLED_WriteCmds() /
 *   OLED_WriteData() 三个静态函数中。移植到 SPI 接口、标准库或其他 MCU
 *   平台时，只需重写这三个函数，上层 API 无需改动。
 *
 * 默认适配：
 *   - 0.96 寸 SSD1306 128x64 OLED 模块（I2C，从机地址 0x3C，即 0x78）
 *   - STM32 HAL 库（F1/F4/F7/G0/H7 等均可，见下方 OLED_HAL_HEADER）
 */

#ifndef __OLED_H
#define __OLED_H

#include <stdint.h>

/* HAL 头文件：默认为 STM32F1 系列。其他系列有两种改法：
 * 1) 直接修改下面的字符串，例如改为 "stm32f4xx_hal.h"；
 * 2) 在工程编译选项中添加 -DOLED_HAL_HEADER='"stm32f4xx_hal.h"'
 */
#ifndef OLED_HAL_HEADER
#define OLED_HAL_HEADER "stm32f1xx_hal.h"
#endif
#include OLED_HAL_HEADER

/* ------------------------------ 用户配置区 ------------------------------ */

/* 使用的 I2C 句柄，默认为 hi2c1（CubeMX 默认生成）。
 * 使用其他句柄时，在包含本文件之前定义，例如：
 *     #define OLED_I2C_HANDLE &hi2c2
 * 或在编译选项中添加 -DOLED_I2C_HANDLE='&hi2c2'
 * 也可以在运行时调用 OLED_AttachI2C(&hi2c2) 动态指定。
 */
#ifndef OLED_I2C_HANDLE
extern I2C_HandleTypeDef hi2c1;
#define OLED_I2C_HANDLE &hi2c1
#endif

/* SSD1306 I2C 从机地址：0x3C << 1 = 0x78。
 * 少数模块地址为 0x3D（即 0x7A），可用 I2C 扫描程序确认后修改此处。 */
#ifndef OLED_I2C_ADDR
#define OLED_I2C_ADDR 0x78
#endif

/* I2C 传输超时时间（ms）。
 * 400kHz 快速模式下刷新全屏约需 30ms；若 I2C 时钟为 100kHz 标准模式，
 * 传输 1024 字节约需 100ms，请将超时调大（如 200）或提高 I2C 时钟。 */
#ifndef OLED_TIMEOUT
#define OLED_TIMEOUT 100
#endif

/* ----------------------------------------------------------------------- */

/* 屏幕参数（SSD1306 128x64） */
#define OLED_WIDTH             128
#define OLED_HEIGHT            64
#define OLED_PAGE_NUM          (OLED_HEIGHT / 8)   /* 页数，每页 8 像素 */

/* 字体尺寸 */
#define OLED_FONT_6X8          6      /* 6x8  字体：每行 21 字符，共 8 行 */
#define OLED_FONT_8X16         16     /* 8x16 字体：每行 16 字符，共 4 行 */

/* 单色定义（1 = 点亮，0 = 熄灭） */
#define OLED_ON                1
#define OLED_OFF               0

#ifdef __cplusplus
extern "C" {
#endif

/* 帧缓冲，布局：[页][列]，与 SSD1306 显存一致，暴露以便高级用法 */
extern uint8_t OLED_Buffer[OLED_PAGE_NUM][OLED_WIDTH];

/* ------------------------------- 初始化与刷新 ------------------------------- */

int      OLED_Init(void);                              /* 初始化，返回 0 成功，-1 失败（I2C 无响应） */
void     OLED_AttachI2C(I2C_HandleTypeDef *hi2c);      /* 运行时指定 I2C 句柄（可选） */
void     OLED_Refresh(void);                           /* 将帧缓冲内容刷新到屏幕 */
void     OLED_Clear(void);                             /* 清屏（帧缓冲全部熄灭） */
void     OLED_ClearArea(uint8_t x, uint8_t y, uint8_t w, uint8_t h); /* 清除指定区域 */
void     OLED_Fill(uint8_t color);                     /* 全屏填充（OLED_ON / OLED_OFF） */

/* -------------------------------- 基本绘图 -------------------------------- */

void     OLED_SetPixel(int16_t x, int16_t y, uint8_t color);   /* 画点，坐标自动裁剪 */
uint8_t  OLED_GetPixel(int16_t x, int16_t y);                  /* 读点，返回 0/1 */
void     OLED_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color);
void     OLED_DrawRect(int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t color);
void     OLED_FillRect(int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t color);
void     OLED_DrawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                           int16_t x2, int16_t y2, uint8_t color);
void     OLED_DrawCircle(int16_t cx, int16_t cy, uint8_t r, uint8_t color);
void     OLED_FillCircle(int16_t cx, int16_t cy, uint8_t r, uint8_t color);
void     OLED_DrawImage(uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                        const uint8_t *img, uint8_t color);    /* 按页取模图片，h 须为 8 的倍数 */
void     OLED_DrawBitmap(const uint8_t *bmp);                  /* 全屏位图，1024 字节，同帧缓冲布局 */

/* -------------------------------- 文本显示 -------------------------------- */

void     OLED_ShowChar(uint8_t x, uint8_t y, char ch, uint8_t font, uint8_t color);
void     OLED_ShowString(uint8_t x, uint8_t y, const char *str, uint8_t font, uint8_t color);
void     OLED_ShowNum(uint8_t x, uint8_t y, int32_t num, uint8_t len, uint8_t font, uint8_t color);
void     OLED_ShowFloat(uint8_t x, uint8_t y, float num, uint8_t frac_num, uint8_t int_len,
                        uint8_t font, uint8_t color);
void     OLED_Printf(uint8_t x, uint8_t y, uint8_t font, uint8_t color, const char *fmt, ...);

/* -------------------------------- 显示控制 -------------------------------- */

void     OLED_SetContrast(uint8_t value);      /* 对比度 0~255 */
void     OLED_DisplayOn(uint8_t on);           /* 1 开显示，0 关闭（休眠） */
void     OLED_Invert(uint8_t invert);          /* 1 反色显示，0 正常 */
void     OLED_SetFlip(uint8_t flip);           /* 1 显示旋转 180 度，0 正常 */

/* --------------------------------- 演示 ---------------------------------- */

void     OLED_Test(void);                      /* 上电演示：弹跳小球动画 + 静态功能展示 */

#ifdef __cplusplus
}
#endif

#endif /* __OLED_H */
