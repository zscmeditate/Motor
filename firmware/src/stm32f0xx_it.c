// ============================================================
// STM32F0xx 中断向量处理
// ============================================================
#include "stm32f0xx_hal.h"

void NMI_Handler(void) { }
void HardFault_Handler(void) { while (1) { } }
void SVC_Handler(void) { }
void PendSV_Handler(void) { }
void SysTick_Handler(void) { HAL_IncTick(); }

// EXTI4_15: PA9/PA10 限位
void EXTI4_15_IRQHandler(void) {
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_9);
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_10);
}
