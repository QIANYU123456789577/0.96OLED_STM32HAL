/**
 * @file main_example.c
 * @brief SSD1306 OLED 驱动集成示例（STM32F103 + CubeMX HAL，I2C1）
 *
 * 使用方法：
 *   1. CubeMX 中使能 I2C1（Fast Mode，400kHz），生成代码；
 *   2. 将本文件中的 main() 及私有函数整合进你的 main.c
 *      （或直接对照本文件修改你的 main.c）；
 *   3. 确保 OLED.h / OLED.c / OLED_Font.h 已加入工程；
 *   4. 注意调用顺序：MX_I2C1_Init() 必须在 OLED_Init() 之前。
 *
 * 详细说明见同目录 README.md。
 */

#include "main.h"
#include "OLED.h"

/* 私有函数原型 */
static void OLED_UpdateUI(void);

/* 运行计数（跨函数访问，设为文件级变量） */
static uint16_t g_count = 0;

/* 示例：16x16 心形图标取模数据（按页取模，格式见 README 第 6 节） */
static const uint8_t Img_Heart[32] = {
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

int main(void)
{
    uint32_t tick = 0;

    /* ---- CubeMX 生成的初始化（顺序不要变） ---- */
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C1_Init();          /* I2C 必须在 OLED_Init 之前初始化 */

    /* ---- OLED 初始化 ---- */
    if (OLED_Init() != 0) {
        /* 返回 -1 表示 I2C 无响应：检查接线、从机地址、上拉电阻 */
        Error_Handler();
    }

    /* 可选：上电自检演示（弹跳小球 + 功能展示），调试通过后注释掉 */
    // OLED_Test();
    // HAL_Delay(2000);

    /* 初始画面 */
    OLED_Clear();
    OLED_ShowString(0, 0, "OLED Demo", OLED_FONT_8X16, OLED_ON);
    OLED_ShowString(0, 20, "init ok", OLED_FONT_6X8, OLED_ON);
    OLED_Refresh();
    HAL_Delay(1000);

    while (1) {
        /* 每 500ms 更新一次画面（全屏刷新在 400kHz 下约 30ms，不要过于频繁） */
        if (HAL_GetTick() - tick >= 500) {
            tick = HAL_GetTick();
            g_count++;
            OLED_UpdateUI();
        }
    }
}

/**
 * @brief 更新一帧画面：先清屏，再绘制，最后统一刷新
 *        所有绘制函数都只写帧缓冲，OLED_Refresh() 才真正送上屏
 */
static void OLED_UpdateUI(void)
{
    /* 标题行：8x16 字体（每行 16 字符） */
    OLED_Clear();
    OLED_ShowString(0, 0, "STM32+SSD1306", OLED_FONT_8X16, OLED_ON);

    /* 数据行：6x8 字体（每行 21 字符） */
    OLED_Printf(0, 22, OLED_FONT_6X8, OLED_ON, "cnt:%u", g_count);
    OLED_ShowString(0, 32, "V:", OLED_FONT_6X8, OLED_ON);
    OLED_ShowFloat(12, 32, 3.26f, 2, 1, OLED_FONT_6X8, OLED_ON);
    OLED_ShowString(48, 32, "T:", OLED_FONT_6X8, OLED_ON);
    OLED_ShowFloat(60, 32, 25.5f, 1, 2, OLED_FONT_6X8, OLED_ON);

    /* 进度条：外框 + 实心矩形（0~100 循环） */
    OLED_DrawRect(0, 44, 101, 9, OLED_ON);
    OLED_FillRect(2, 46, (uint8_t)(g_count % 101), 5, OLED_ON);

    /* 16x16 心形图标（DrawImage 的 h 必须为 8 的倍数） */
    OLED_DrawImage(108, 44, 16, 16, Img_Heart, OLED_ON);

    /* 一次性推送到屏幕 */
    OLED_Refresh();
}
