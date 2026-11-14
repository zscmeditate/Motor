#include "stm32f0xx_hal.h"
#include "motor.h"
#include "pins.h"

// —— 外设句柄 ——
TIM_HandleTypeDef htim3;    // CH1=MOTOR_FI, CH2=MOTOR_BI

// 本地按键点动时的默认占空比 (0..1000)
#define BUTTON_PWM 600

// ============================================================
// 时钟配置 —— STM32F030 内部 HSI 8MHz → PLL×6 = 48MHz
// F0 的 PLL 用 PREDIV (不是 PLLDIV)
// ============================================================
void SystemClock_Config(void) {
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    osc.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState            = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState        = RCC_PLL_ON;
    osc.PLL.PLLSource       = RCC_PLLSOURCE_HSI;   // F0: HSI/2 = 4MHz (固定分频)
    osc.PLL.PLLMUL          = RCC_PLL_MUL12;       // 4MHz × 12 = 48MHz
    osc.PLL.PREDIV          = RCC_PREDIV_DIV1;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        while (1) { }
    }

    clk.ClockType       = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
    clk.SYSCLKSource    = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider   = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider  = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_1) != HAL_OK) {
        while (1) { }
    }
}

// ============================================================
// GPIO 初始化
// ============================================================
static void MX_GPIO_Init(void) {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};

    // PA4 PA5: CW_START / CCW_START 按键输入 (外部 5.1K 上拉, 按下接地)
    g.Mode      = GPIO_MODE_INPUT;
    g.Pull      = GPIO_PULLUP;
    g.Alternate = 0;
    g.Pin       = GPIO_PIN_4 | GPIO_PIN_5;
    HAL_GPIO_Init(GPIOA, &g);

    // PA6 PA7: MOTOR_FI / MOTOR_BI → SS6285M (TIM3_CH1/CH2, AF1)
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF1_TIM3;
    g.Pin       = GPIO_PIN_6 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOA, &g);

    // PA9 PA10: 左右限位输入 (上拉 + EXTI 双边沿)
    g.Mode      = GPIO_MODE_IT_RISING_FALLING;
    g.Pull      = GPIO_PULLUP;
    g.Alternate = 0;
    g.Speed     = GPIO_SPEED_FREQ_LOW;
    g.Pin       = GPIO_PIN_9 | GPIO_PIN_10;
    HAL_GPIO_Init(GPIOA, &g);
    HAL_NVIC_SetPriority(EXTI4_15_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);

    // PA0 (433_D1) PA3 (433_D0) PB1 (433_D2): 433M 数据位输入
    // 模块空闲低、按键高；下拉避免模块启动期间输入悬空造成误触发
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLDOWN;
    g.Pin  = GPIO_PIN_0 | GPIO_PIN_3;
    HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_1;
    HAL_GPIO_Init(GPIOB, &g);

    // PA1: MOTOR_CUR 模拟输入 (ADC1_IN1)
    g.Mode = GPIO_MODE_ANALOG;
    g.Pull = GPIO_NOPULL;
    g.Pin  = GPIO_PIN_1;
    HAL_GPIO_Init(GPIOA, &g);

    // PA13 PA14: SWD —— F0 默认保留,不需要配置
}

// ============================================================
// TIM3: MOTOR_FI (CH1/PA6) + MOTOR_BI (CH2/PA7)
// 48MHz / 48 = 1MHz, 周期 1000 → 1kHz PWM
// ============================================================
static void MX_TIM3_Init(void) {
    htim3.Instance           = TIM3;
    htim3.Init.Prescaler     = 47;
    htim3.Init.Period        = 1000;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.CounterMode   = TIM_COUNTERMODE_UP;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_PWM_Init(&htim3) != HAL_OK) { while (1) { } }

    TIM_OC_InitTypeDef oc = {0};
    oc.OCMode      = TIM_OCMODE_PWM1;
    oc.Pulse       = 0;
    oc.OCPolarity  = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode  = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim3, &oc, TIM_CHANNEL_1) != HAL_OK) { while (1) { } }
    if (HAL_TIM_PWM_ConfigChannel(&htim3, &oc, TIM_CHANNEL_2) != HAL_OK) { while (1) { } }
}

// ============================================================
// EXTI 回调 —— 限位开关
// ============================================================
static void remote_limit_stop(void);

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == GPIO_PIN_9) {
        motor_left_limit_isr();
        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9) == GPIO_PIN_SET) remote_limit_stop();
    }
    if (GPIO_Pin == GPIO_PIN_10) {
        motor_right_limit_isr();
        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_10) == GPIO_PIN_SET) remote_limit_stop();
    }
}

// ============================================================
// 433M 遥控 —— D0=PA3 正转, D1=PA0 反转, D2=PB1 停止
// 模块空闲输出低电平, 检测上升沿(按键按下)作为一次指令
// ============================================================
#define REMOTE_PWM   600        // 遥控指令的默认占空比 (0..1000)
#define REMOTE_DEBOUNCE_SAMPLES  2   // 连续 2 次采样一致(约 20ms)才认
#define REMOTE_STARTUP_IDLE_SAMPLES 50 // 上电后三路连续空闲 500ms 才允许接收
#define REVERSE_DELAY_MS 500            // 正反转切换前强制停车时间

typedef struct {
    uint8_t stable;   // 已消抖电平
    uint8_t count;    // 连续不同采样计数
    bool    edge;     // 本次出现上升沿(按下)
} remote_key_t;

static remote_key_t key_d0, key_d1, key_d2;
static bool         remote_running = false;
static motor_dir_t  remote_dir     = MOTOR_IDLE;
static bool         remote_armed   = false;
static uint8_t      remote_idle_count = 0;
static motor_dir_t  remote_pending_dir = MOTOR_IDLE;
static uint32_t     reverse_started = 0;
static bool         local_active = false;
static motor_dir_t  local_dir = MOTOR_IDLE;

// 任一限位触发都取消遥控保持和待执行换向，松开后不得自行恢复
static void remote_limit_stop(void) {
    if (remote_running) reverse_started = HAL_GetTick();
    remote_running = false;
    remote_pending_dir = MOTOR_IDLE;
    // remote_dir 保留最后运行方向，用于停车后 500ms 内的反向启动保护
}

static bool remote_read_d0(void) { return HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3) == GPIO_PIN_SET; }
static bool remote_read_d1(void) { return HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET; }
static bool remote_read_d2(void) { return HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1) == GPIO_PIN_SET; }

static void remote_key_update(remote_key_t *k, bool raw) {
    k->edge = false;
    if (raw == k->stable) {
        k->count = 0;
        return;
    }
    if (++k->count >= REMOTE_DEBOUNCE_SAMPLES) {
        k->count  = 0;
        k->stable = raw;
        if (raw) k->edge = true;    // 上升沿 = 一次按键
    }
}

// 收到相反方向：从实际停车时刻起至少等待 500ms 才允许启动
static void remote_request_dir(motor_dir_t dir) {
    uint32_t now = HAL_GetTick();

    // 等待期间再次改变方向，以最新命令为准并重新计时
    if (remote_pending_dir != MOTOR_IDLE) {
        if (remote_pending_dir != dir) {
            remote_pending_dir = dir;
            reverse_started = now;
        }
        return;
    }

    if (remote_dir != MOTOR_IDLE && remote_dir != dir) {
        if (remote_running) {
            remote_running = false;
            remote_pending_dir = dir;
            reverse_started = now;
            return;
        }

        // 点动按键松开后再按反向，也不能绕过停车后的 500ms 保护
        if ((uint32_t)(now - reverse_started) < REVERSE_DELAY_MS) {
            remote_pending_dir = dir;
            return;
        }
    }

    remote_dir = dir;
    remote_running = true;
}

// 上电时先把当前电平记为稳定值,避免模块开机即有输出被误当成按键
static void remote_init(void) {
    key_d0.stable = remote_read_d0();
    key_d1.stable = remote_read_d1();
    key_d2.stable = remote_read_d2();
    remote_running = false;
    remote_dir     = MOTOR_IDLE;
    remote_armed   = false;
    remote_idle_count = 0;
    remote_pending_dir = MOTOR_IDLE;
    reverse_started = 0;
    local_active = false;
    local_dir = MOTOR_IDLE;
}

// 每 10ms 调用一次；本地按键优先时只计时，不接收遥控新命令
static void remote_poll(bool accept_commands) {
    // 无符号减法可正确处理 HAL_GetTick() 溢出
    if (remote_pending_dir != MOTOR_IDLE &&
        (uint32_t)(HAL_GetTick() - reverse_started) >= REVERSE_DELAY_MS) {
        remote_dir = remote_pending_dir;
        remote_pending_dir = MOTOR_IDLE;
        remote_running = true;
    }

    bool d0 = remote_read_d0();
    bool d1 = remote_read_d1();
    bool d2 = remote_read_d2();

    // 接收模块上电可能有输出脉冲：先等三路持续低电平，再响应新按键
    if (!remote_armed) {
        key_d0.stable = d0;
        key_d1.stable = d1;
        key_d2.stable = d2;
        key_d0.count = key_d1.count = key_d2.count = 0;
        key_d0.edge = key_d1.edge = key_d2.edge = false;

        if (!d0 && !d1 && !d2) {
            if (remote_idle_count < REMOTE_STARTUP_IDLE_SAMPLES) remote_idle_count++;
            if (remote_idle_count >= REMOTE_STARTUP_IDLE_SAMPLES) remote_armed = true;
        } else {
            remote_idle_count = 0;
        }
        return;
    }

    remote_key_update(&key_d0, d0);
    remote_key_update(&key_d1, d1);
    remote_key_update(&key_d2, d2);

    if (!accept_commands) return;

    if (key_d0.edge) remote_request_dir(MOTOR_CW);
    if (key_d1.edge) remote_request_dir(MOTOR_CCW);
    if (key_d2.edge) {
        if (remote_running) reverse_started = HAL_GetTick();
        remote_running = false;
        remote_pending_dir = MOTOR_IDLE;
    }
}

// ============================================================
// 主函数
// ============================================================
int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_TIM3_Init();

    motor_init();
    motor_stop();   // 上电安全停车
    remote_init();

    while (1) {
        bool cw  = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4) == GPIO_PIN_RESET);
        bool ccw = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_RESET);
        bool local_pressed = (cw != ccw);  // 同时按下或都松开时均不启动

        remote_poll(!local_pressed);

        if (local_pressed) {
            motor_dir_t dir = cw ? MOTOR_CW : MOTOR_CCW;

            // 新按下或改变方向时才发命令，避免等待期间反复重置换向计时
            if (!local_active || local_dir != dir) {
                remote_request_dir(dir);
                local_dir = dir;
                local_active = true;
            }
        } else if (local_active) {
            // 本地按键为点动：松开即停；保留最后方向用于快速反向保护
            if (remote_running) reverse_started = HAL_GetTick();
            remote_running = false;
            remote_pending_dir = MOTOR_IDLE;
            local_active = false;
            local_dir = MOTOR_IDLE;
        }

        if (remote_running) {
            motor_start(remote_dir, local_active ? BUTTON_PWM : REMOTE_PWM);
        } else {
            motor_stop();
        }

        HAL_Delay(10);
    }
}
