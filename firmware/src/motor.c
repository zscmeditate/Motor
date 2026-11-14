#include "motor.h"
#include "pins.h"
#include "stm32f0xx_hal.h"

// 外设句柄 (由 main.c 的 HAL init 提供)
extern TIM_HandleTypeDef htim3;    // CH1 = MOTOR_FI (正转), CH2 = MOTOR_BI (反转)

static motor_state_t state = {
    .dir       = MOTOR_IDLE,
    .pwm       = 0,
    .max_pwm   = 1000,
    .left_hit  = false,
    .right_hit = false,
};

// —— 初始化 PWM TIM 和 GPIO ——
void motor_init(void) {
    // 上电先读一次限位: 常闭(NC)开关常态接地(低),断开(碰到限位)为高
    state.left_hit  = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9)  == GPIO_PIN_SET);
    state.right_hit = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_10) == GPIO_PIN_SET);

    // 两路占空比清零后再启动,避免上电瞬间电机抖动
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 0);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
}

// —— 启动电机 ——
void motor_start(motor_dir_t dir, uint16_t pwm) {
    if (pwm > state.max_pwm) pwm = state.max_pwm;

    // 限位保护
    if ((dir == MOTOR_CW  && state.left_hit)  ||
        (dir == MOTOR_CCW && state.right_hit)) {
        motor_stop();
        return;
    }

    state.dir = dir;
    state.pwm = pwm;

    switch (dir) {
        case MOTOR_CW:      // MOTOR_FI = PWM, MOTOR_BI = 0
            __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pwm);
            __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 0);
            break;
        case MOTOR_CCW:     // MOTOR_BI = PWM, MOTOR_FI = 0
            __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
            __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, pwm);
            break;
        default:
            motor_stop();
    }
}

// —— 停车 ——
void motor_stop(void) {
    state.dir = MOTOR_IDLE;
    state.pwm = 0;
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 0);
}

// —— 运行中调节 PWM ——
void motor_set_pwm(uint16_t pwm) {
    if (pwm > state.max_pwm) pwm = state.max_pwm;
    state.pwm = pwm;

    switch (state.dir) {
        case MOTOR_CW:  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pwm); break;
        case MOTOR_CCW: __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, pwm); break;
        default: break;
    }
}

motor_state_t motor_get_state(void) { return state; }

// —— 限位中断处理 (NC 常闭开关: 常态低, 碰到断开为高) ——
void motor_left_limit_isr(void) {
    state.left_hit = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9) == GPIO_PIN_SET);
    if (state.left_hit) motor_stop();
}

void motor_right_limit_isr(void) {
    state.right_hit = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_10) == GPIO_PIN_SET);
    if (state.right_hit) motor_stop();
}
