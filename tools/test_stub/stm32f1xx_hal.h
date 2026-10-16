/**
 * stm32f1xx_hal.h - 测试用 HAL 桩头文件（仅用于本机 GCC 编译验证，不属于交付驱动）
 */
#ifndef STM32F1XX_HAL_H_STUB
#define STM32F1XX_HAL_H_STUB

#include <stdint.h>

typedef enum {
    HAL_OK = 0,
    HAL_ERROR = 1,
    HAL_BUSY = 2,
    HAL_TIMEOUT = 3
} HAL_StatusTypeDef;

HAL_StatusTypeDef HAL_Init(void);

typedef struct {
    int dummy;
} I2C_HandleTypeDef;

#define I2C_MEMADD_SIZE_8BIT ((uint16_t)0x0001)

HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *hi2c, uint16_t DevAddress,
                                          uint8_t *pData, uint16_t Size, uint32_t Timeout);
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *hi2c, uint16_t DevAddress,
                                    uint16_t MemAddress, uint16_t MemAddSize,
                                    uint8_t *pData, uint16_t Size, uint32_t Timeout);
void HAL_Delay(uint32_t Delay);
uint32_t HAL_GetTick(void);

#endif
