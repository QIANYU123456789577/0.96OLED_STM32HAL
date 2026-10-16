/**
 * test_main.c - 驱动功能验证程序（仅用于本机 GCC 验证，不属于交付驱动）
 * 运行 OLED_Init + OLED_Test，并将最终帧缓冲导出为 framebuffer.bin
 */
#include "OLED.h"
#include <stdio.h>

/* 模拟 CubeMX 工程中的全局句柄定义 */
I2C_HandleTypeDef hi2c1;

int main(void)
{
    FILE *f;

    if (OLED_Init() != 0) {
        printf("OLED_Init FAILED (I2C no response)\n");
        return 1;
    }
    OLED_Test();

    f = fopen("framebuffer.bin", "wb");
    if (f == NULL) {
        printf("cannot open framebuffer.bin\n");
        return 1;
    }
    fwrite(OLED_Buffer, 1, sizeof(OLED_Buffer), f);
    fclose(f);
    printf("OLED driver test OK, framebuffer dumped\n");
    return 0;
}
