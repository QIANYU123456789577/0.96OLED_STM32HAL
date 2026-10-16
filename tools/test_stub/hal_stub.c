/**
 * hal_stub.c - 测试用 HAL 桩实现（仅用于本机 GCC 编译验证，不属于交付驱动）
 */
#include "stm32f1xx_hal.h"

HAL_StatusTypeDef HAL_Init(void)
{
    return HAL_OK;
}

HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *hi2c, uint16_t DevAddress,
                                          uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
    (void)hi2c;
    (void)DevAddress;
    (void)pData;
    (void)Size;
    (void)Timeout;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *hi2c, uint16_t DevAddress,
                                    uint16_t MemAddress, uint16_t MemAddSize,
                                    uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
    (void)hi2c;
    (void)DevAddress;
    (void)MemAddress;
    (void)MemAddSize;
    (void)pData;
    (void)Size;
    (void)Timeout;
    return HAL_OK;
}

void HAL_Delay(uint32_t Delay)
{
    (void)Delay;
}

uint32_t HAL_GetTick(void)
{
    static uint32_t tick = 0;
    tick++;
    return tick;
}
