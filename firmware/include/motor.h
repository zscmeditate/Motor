#ifndef MOTOR_H
#define MOTOR_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================
// 电机驱动 API
// ============================================================

typedef enum {
    MOTOR_IDLE = 0,     // 停止
    MOTOR_CW,           // 正转 (MOTOR_FI / PA6 / TIM3_CH1)
    MOTOR_CCW,          // 反转 (MOTOR_BI / PA7 / TIM3_CH2)
} motor_dir_t;

typedef struct {
    motor_dir_t dir;        // 当前方向
    uint16_t    pwm;        // PWM 占空比 0..1000 (0%..100%)
    uint16_t    max_pwm;    // 最大 PWM (限功率)
    bool        left_hit;   // 左限位触发
    bool        right_hit;  // 右限位触发
} motor_state_t;

// —— 初始化 ——
void motor_init(void);

// —— 控制 ——
void motor_start(motor_dir_t dir, uint16_t pwm);   // pwm: 0..1000
void motor_stop(void);
void motor_set_pwm(uint16_t pwm);

// —— 状态 ——
motor_state_t motor_get_state(void);

// —— 限位中断回调 (由 HAL EXTI 中断调用) ——
void motor_left_limit_isr(void);
void motor_right_limit_isr(void);

#endif /* MOTOR_H */
