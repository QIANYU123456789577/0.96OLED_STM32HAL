/**
 * main.h - 测试用 CubeMX 风格 main.h 桩（仅用于 PC 端编译验证示例代码，不属于交付驱动）
 */
#ifndef MAIN_H
#define MAIN_H

#include "stm32f1xx_hal.h"

/* CubeMX 生成的函数（测试桩声明） */
void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_I2C1_Init(void);
void Error_Handler(void);

extern I2C_HandleTypeDef hi2c1;

#endif
