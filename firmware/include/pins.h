#ifndef PINS_H
#define PINS_H

// ============================================================
// 板子引脚映射 —— STM32F030F4P6TR (433M DC Motor Board)
// 来源: 原理图网表 (Board1 / 433M_DC_Motor) 2026-11-13
// ============================================================

// —— 电机驱动输出 ——
// PA6: MOTOR_FI → U1.2 (SS6285M FI)，TIM3_CH1 正转 PWM
// PA7: MOTOR_BI → U1.1 (SS6285M BI)，TIM3_CH2 反转 PWM
#define PIN_MOTOR_FI        GPIOA, GPIO_PIN_6
#define PIN_MOTOR_BI        GPIOA, GPIO_PIN_7

// —— 正/反转按键 (外部 5.1K 上拉 + 100nF，低=按下) ——
// PA4: CW_START  (P3)
// PA5: CCW_START (P4)
#define PIN_CW_START        GPIOA, GPIO_PIN_4
#define PIN_CCW_START       GPIOA, GPIO_PIN_5

// —— 限位开关 (NC 常闭：常态接地为低，碰到后断开为高=触发) ——
#define PIN_LEFT_LIMIT      GPIOA, GPIO_PIN_9   // EXTI9
#define PIN_RIGHT_LIMIT     GPIOA, GPIO_PIN_10  // EXTI10

// —— 电流采样 (ADC) ——
#define ADC_CHANNEL_MOTOR_CUR  ADC_CHANNEL_1   // PA1 ← U20 电流放大输出

// —— 433M 无线模块 (H1 排针, 电平输入) ——
// PA3 = D0，PA0 = D1，PB1 = D2（D3/VT 未接 MCU）
#define PIN_433_D0          GPIOA, GPIO_PIN_3
#define PIN_433_D1          GPIOA, GPIO_PIN_0
#define PIN_433_D2          GPIOB, GPIO_PIN_1

// —— 其他 ——
// PA2 = TP1 测试点（非 ADC 反馈）
// PA13 = SWDIO, PA14 = SWCLK (CubeMX 自动处理)

#endif /* PINS_H */
