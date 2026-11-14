#include "stm32f0xx_hal.h"

// ============================================================
// HAL MSP 初始化 —— 由 HAL_*_Init 自动回调
// ============================================================

void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM3) {
        __HAL_RCC_TIM3_CLK_ENABLE();
    }
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) {
    (void)file; (void)line;
    for (;;) { }
}
#endif
