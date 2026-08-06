#include "Int_Motor.h"

volatile Motor_Para motor_para;

//!========================================================================================================================================================================

/**
 * @brief 设置电机速度
 *
 * TIM2计数时钟 = 84MHz / (PSC + 1) = 84MHz / 84 = 1MHz，每计数一次为1us。
 * STEP脉冲频率 = 1000000 / (ARR + 1)，因此 ARR = 1000000 / speed - 1。
 * 当前使用1/8细分，1600个STEP脉冲为电机转动1圈。
 *
 * @param speed STEP脉冲频率，单位：step/s
 */
static void __Int_Motor_Set_Speed(uint32_t speed)
{
    uint32_t period_count;

    /* 防止除零 */
    if (speed == 0U)
    {
        return;
    }

    /* 计算一个STEP脉冲周期需要的TIM2计数值 */
    period_count = MOTOR_TIM_COUNTER_FREQUENCY / speed;

    /* 保证周期至少有2个计数值，避免ARR或CCR出现无效值 */
    if (period_count < 2U)
    {
        period_count = 2U;
    }

    /*
     * 当前采用直接更新ARR/CCR1的方式。加速时ARR会变小，如果CNT已经超过
     * 新ARR，定时器可能要等待很久才溢出，因此先检查并把CNT归零。
     */
    if (__HAL_TIM_GET_COUNTER(&htim2) >= period_count)
    {
        __HAL_TIM_SET_COUNTER(&htim2, 0U);
    }

    __HAL_TIM_SET_AUTORELOAD(&htim2, period_count - 1U);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, period_count / 2U);

    /* 停止状态下装载新参数，并保证下一次从完整PWM周期开始 */
    if ((htim2.Instance->CR1 & TIM_CR1_CEN) == 0U)
    {
        htim2.Instance->EGR = TIM_EGR_UG;
        __HAL_TIM_SET_COUNTER(&htim2, 0U);
    }
}

//!========================================================================================================================================================================

/**
 * @brief 装载一次梯形加减速运动的公共参数
 *
 * KEY1回零、KEY2/KEY3固定圈数、KEY4/KEY5连续运动均使用这里的参数。
 */
static void __Int_Motor_Prepare_Trapezoid(void)
{
    motor_para.speed.start_speed = MOTOR_TRAPEZOID_START_SPEED;
    motor_para.speed.target_speed = MOTOR_TRAPEZOID_MAX_SPEED;
    motor_para.speed.current_speed = motor_para.speed.start_speed;
    motor_para.speed.acceleration = MOTOR_TRAPEZOID_ACCELERATION;
    motor_para.speed.deceleration = MOTOR_TRAPEZOID_DECELERATION;
    motor_para.speed_phase = MOTOR_SPEED_PHASE_ACCEL;
    motor_para.speed_update_tick = HAL_GetTick();
}

//!========================================================================================================================================================================

/**
 * @brief 根据当前速度计算减速到启动速度需要的步数
 *
 * 使用公式：s = (v² - v0²) / (2a)。
 *
 * @return 需要预留的减速步数
 */
static uint32_t __Int_Motor_Calculate_Decel_Step(void)
{
    uint64_t current_speed_square;
    uint64_t start_speed_square;
    uint64_t denominator;

    if ((motor_para.speed.deceleration == 0U) ||
        (motor_para.speed.current_speed <= motor_para.speed.start_speed))
    {
        return 0U;
    }

    current_speed_square =
        (uint64_t)motor_para.speed.current_speed * motor_para.speed.current_speed;
    start_speed_square =
        (uint64_t)motor_para.speed.start_speed * motor_para.speed.start_speed;
    denominator = (uint64_t)2U * motor_para.speed.deceleration;

    /* 向上取整，确保给减速阶段预留足够的步数 */
    return (uint32_t)((current_speed_square - start_speed_square + denominator - 1U) /
                      denominator);
}

//!========================================================================================================================================================================

/**
 * @brief 获取固定圈数运动的剩余步数
 */
static uint32_t __Int_Motor_Get_Remaining_Step(void)
{
    if (motor_para.step.current_step >= motor_para.step.target_step)
    {
        return 0U;
    }

    return motor_para.step.target_step - motor_para.step.current_step;
}

//!========================================================================================================================================================================

/**
 * @brief 设置电机运行方向
 *
 * @param dir 电机运行方向
 */
static void __Int_Motor_Set_Dir(Motor_Dir dir)
{
    if (dir == MOTOR_DIR_HAND)
    {
        /* 手轮方向 */
        HAL_GPIO_WritePin(STEPPER_1_DIR_GPIO_Port, STEPPER_1_DIR_Pin, GPIO_PIN_RESET);
    }
    else
    {
        /* 电机方向 */
        HAL_GPIO_WritePin(STEPPER_1_DIR_GPIO_Port, STEPPER_1_DIR_Pin, GPIO_PIN_SET);
    }

    motor_para.dir = dir;
}

//!========================================================================================================================================================================

/**
 * @brief 设置电机运行圈数
 *
 * @param revolution 运行圈数
 */
static void __Int_Motor_Set_Revolution(double revolution)
{
    if (revolution > 28.0)
    {
        revolution = 28.0;
    }

    if (revolution < 0.0)
    {
        revolution = 1.0;
    }

    /* 根据圈数计算目标STEP脉冲数 */
    motor_para.step.target_step = (uint32_t)(revolution * STEP_PER_REVOLUTION);
    motor_para.revolution = revolution;
}

//!========================================================================================================================================================================

/**
 * @brief 初始化电机控制参数
 */
void Int_Motor_Init(void)
{
    motor_para.dir = MOTOR_DIR_HAND;
    motor_para.step.target_step = 0U;
    motor_para.step.current_step = 0U;

    /* 连续运动梯形加减速参数 */
    motor_para.speed.start_speed = MOTOR_TRAPEZOID_START_SPEED;
    motor_para.speed.target_speed = MOTOR_TRAPEZOID_MAX_SPEED;
    motor_para.speed.current_speed = 0U;
    motor_para.speed.acceleration = MOTOR_TRAPEZOID_ACCELERATION;
    motor_para.speed.deceleration = MOTOR_TRAPEZOID_DECELERATION;

    motor_para.state = MOTOR_STATE_INIT;
    motor_para.speed_phase = MOTOR_SPEED_PHASE_STOP;
    motor_para.speed_update_tick = HAL_GetTick();
    motor_para.revolution = 0.0;

    /* PA4为X轴方向控制引脚，默认设置为手轮方向 */
    HAL_GPIO_WritePin(STEPPER_1_DIR_GPIO_Port, STEPPER_1_DIR_Pin, GPIO_PIN_RESET);

    /*
     * 关闭ARR和CCR1预装载，速度变化时直接更新TIM2当前工作寄存器。
     * 本项目采用主循环每10ms更新一次速度，直接写入ARR/CCR1可以确保
     * 梯形算法计算出的新频率立即生效，避免只修改预装载寄存器但PWM
     * 频率没有变化的问题。
     */
    CLEAR_BIT(htim2.Instance->CR1, TIM_CR1_ARPE);
    __HAL_TIM_DISABLE_OCxPRELOAD(&htim2, TIM_CHANNEL_1);

    /* 初始化时确保PWM和电机使能均处于关闭状态 */
    HAL_TIM_PWM_Stop_IT(&htim2, TIM_CHANNEL_1);
    HAL_GPIO_WritePin(STEPPER_ALL_EN_GPIO_Port, STEPPER_ALL_EN_Pin, GPIO_PIN_RESET);
}

//!========================================================================================================================================================================

/**
 * @brief 启动TIM2 PWM输出，驱动步进电机运动
 */
static void __Int_Motor_Move_Start(void)
{
    /* 每次启动前把TIM2计数器归零，使第一个PWM周期完整输出 */
    __HAL_TIM_SET_COUNTER(&htim2, 0U);

    /* 使能电机驱动，当前硬件逻辑为PG0拉高有效 */
    HAL_GPIO_WritePin(STEPPER_ALL_EN_GPIO_Port, STEPPER_ALL_EN_Pin, GPIO_PIN_SET);

    /*
     * 只有固定圈数运动需要逐脉冲计数，因此仅该模式开启TIM2通道中断。
     * 连续运动和回零运动只启动PWM，避免高频无用中断影响HAL_GetTick和加速计算。
     */
    if (motor_para.state == MOTOR_STATE_REVOLUTION_RUN)
    {
        HAL_TIM_PWM_Start_IT(&htim2, TIM_CHANNEL_1);
    }
    else
    {
        HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
    }
}

//!========================================================================================================================================================================

/**
 * @brief 关闭TIM2 PWM输出并关闭电机使能
 */
static void __Int_Motor_Move_Stop(void)
{
    /* 同时兼容普通PWM和带中断PWM两种启动方式 */
    __HAL_TIM_DISABLE_IT(&htim2, TIM_IT_CC1);
    HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
    HAL_GPIO_WritePin(STEPPER_ALL_EN_GPIO_Port, STEPPER_ALL_EN_Pin, GPIO_PIN_RESET);
}

//!========================================================================================================================================================================

/**
 * @brief TIM2 PWM脉冲完成回调函数
 *
 * 固定圈数运动时，每完成一个STEP脉冲就在这里累计一次步数。
 *
 * @param htim 触发回调的定时器句柄
 */
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    /* 只处理TIM2通道1，避免其他定时器或通道误进入X轴计步逻辑 */
    if ((htim->Instance != TIM2) ||
        (htim->Channel != HAL_TIM_ACTIVE_CHANNEL_1))
    {
        return;
    }

    if (motor_para.state == MOTOR_STATE_REVOLUTION_RUN)
    {
        motor_para.step.current_step++;

        if (motor_para.step.current_step >= motor_para.step.target_step)
        {
            __Int_Motor_Move_Stop();
            motor_para.step.current_step = 0U;
            motor_para.speed.current_speed = 0U;
            motor_para.speed_phase = MOTOR_SPEED_PHASE_STOP;
            motor_para.state = MOTOR_STATE_STOPPED;
        }
    }
}

//!========================================================================================================================================================================

/**
 * @brief X轴零点光电开关下降沿中断回调函数
 *
 * @param GPIO_Pin 触发中断的GPIO引脚
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == X_ZERO_Pin)
    {
        /* 限位触发时必须立即停止，不能继续执行减速过程 */
        __Int_Motor_Move_Stop();
        motor_para.step.current_step = 0U;
        motor_para.speed.current_speed = 0U;
        motor_para.speed_phase = MOTOR_SPEED_PHASE_STOP;
        motor_para.state = MOTOR_STATE_HOMING;

        /* 停止后把方向切换为离开零点的电机方向 */
        __Int_Motor_Set_Dir(MOTOR_DIR_MOTOR);
    }
}

//!========================================================================================================================================================================

/**
 * @brief 向手轮方向移动指定圈数
 */
void Int_Motor_Move_To_Hand(void)
{
    if ((motor_para.state == MOTOR_STATE_REVOLUTION_RUN) ||
        (motor_para.state == MOTOR_STATE_CONTINUOUS_RUN) ||
        (motor_para.state == MOTOR_STATE_HOMING_RUN))
    {
        return;
    }

    /* 零点传感器未触发时，才允许继续向手轮方向运动 */
    if (HAL_GPIO_ReadPin(X_ZERO_GPIO_Port, X_ZERO_Pin) == GPIO_PIN_SET)
    {
        __Int_Motor_Prepare_Trapezoid();
        __Int_Motor_Set_Speed(motor_para.speed.current_speed);
        __Int_Motor_Set_Dir(MOTOR_DIR_HAND);
        __Int_Motor_Set_Revolution(15.0);

        motor_para.step.current_step = 0U;
        motor_para.state = MOTOR_STATE_REVOLUTION_RUN;
        __Int_Motor_Move_Start();
    }
}

//!========================================================================================================================================================================

/**
 * @brief 向电机方向移动指定圈数
 */
void Int_Motor_Move_To_Motor(void)
{
    if ((motor_para.state == MOTOR_STATE_REVOLUTION_RUN) ||
        (motor_para.state == MOTOR_STATE_CONTINUOUS_RUN) ||
        (motor_para.state == MOTOR_STATE_HOMING_RUN))
    {
        return;
    }

    __Int_Motor_Prepare_Trapezoid();
    __Int_Motor_Set_Speed(motor_para.speed.current_speed);
    __Int_Motor_Set_Dir(MOTOR_DIR_MOTOR);
    __Int_Motor_Set_Revolution(15.0);

    motor_para.step.current_step = 0U;
    motor_para.state = MOTOR_STATE_REVOLUTION_RUN;
    __Int_Motor_Move_Start();
}

//!========================================================================================================================================================================

/**
 * @brief 电机向零点方向持续运动，直到零点光电开关触发
 */
void Int_Motor_Move_To_Homing(void)
{
    if ((motor_para.state == MOTOR_STATE_REVOLUTION_RUN) ||
        (motor_para.state == MOTOR_STATE_CONTINUOUS_RUN) ||
        (motor_para.state == MOTOR_STATE_HOMING_RUN))
    {
        return;
    }

    if (HAL_GPIO_ReadPin(X_ZERO_GPIO_Port, X_ZERO_Pin) == GPIO_PIN_SET)
    {
        /* 回零位置未知，因此先按照梯形曲线的加速段逐渐提高速度 */
        __Int_Motor_Prepare_Trapezoid();
        __Int_Motor_Set_Speed(motor_para.speed.current_speed);
        __Int_Motor_Set_Dir(MOTOR_DIR_HAND);

        motor_para.step.current_step = 0U;
        motor_para.state = MOTOR_STATE_HOMING_RUN;
        __Int_Motor_Move_Start();
    }
    else
    {
        motor_para.state = MOTOR_STATE_HOMING;
    }
}

//!========================================================================================================================================================================

/**
 * @brief 启动或保持连续运动
 *
 * 首次按下KEY4/KEY5时从启动速度开始运行，然后由Int_Motor_Process逐步加速。
 * 如果运行中按下相反方向按键，则先减速到停止，停止后才允许改变方向。
 *
 * @param dir 连续运动方向
 */
void Int_Motor_Move_Continuous(Motor_Dir dir)
{
    /* 拒绝无效方向参数 */
    if ((dir != MOTOR_DIR_HAND) && (dir != MOTOR_DIR_MOTOR))
    {
        return;
    }

    /* 固定圈数运动和回零运动期间，不允许连续运动打断当前任务 */
    if ((motor_para.state == MOTOR_STATE_REVOLUTION_RUN) ||
        (motor_para.state == MOTOR_STATE_HOMING_RUN))
    {
        return;
    }

    if (motor_para.state == MOTOR_STATE_CONTINUOUS_RUN)
    {
        if (motor_para.dir == dir)
        {
            /* 松开后尚未完全停止时，再次按下同方向按键则重新加速 */
            if (motor_para.speed_phase == MOTOR_SPEED_PHASE_DECEL)
            {
                motor_para.speed_phase = MOTOR_SPEED_PHASE_ACCEL;
                motor_para.speed_update_tick = HAL_GetTick();
            }

            return;
        }

        /* 禁止高速直接反转：先进入减速阶段，完全停止后再切换方向 */
        motor_para.speed_phase = MOTOR_SPEED_PHASE_DECEL;
        return;
    }

    /* 已经触发零点传感器时，不允许继续向手轮方向运动 */
    if ((dir == MOTOR_DIR_HAND) &&
        (HAL_GPIO_ReadPin(X_ZERO_GPIO_Port, X_ZERO_Pin) == GPIO_PIN_RESET))
    {
        motor_para.state = MOTOR_STATE_HOMING;
        return;
    }

    /* 从较低的启动速度开始，防止电机从静止状态直接跳到高速而失步 */
    __Int_Motor_Prepare_Trapezoid();
    __Int_Motor_Set_Dir(dir);
    __Int_Motor_Set_Speed(motor_para.speed.current_speed);

    motor_para.step.current_step = 0U;
    motor_para.speed_phase = MOTOR_SPEED_PHASE_ACCEL;
    motor_para.speed_update_tick = HAL_GetTick();
    motor_para.state = MOTOR_STATE_CONTINUOUS_RUN;

    __Int_Motor_Move_Start();
}

//!========================================================================================================================================================================

/**
 * @brief 请求连续运动按照设定减速度逐渐停止
 */
void Int_Motor_Stop_Continuous(void)
{
    /* 只处理连续运动，不影响固定圈数运动和回零运动 */
    if (motor_para.state != MOTOR_STATE_CONTINUOUS_RUN)
    {
        return;
    }

    if (motor_para.speed_phase != MOTOR_SPEED_PHASE_DECEL)
    {
        motor_para.speed_phase = MOTOR_SPEED_PHASE_DECEL;
        motor_para.speed_update_tick = HAL_GetTick();
    }
}

//!========================================================================================================================================================================

/**
 * @brief 立即停止电机
 *
 * 该函数用于限位、急停或者从连续模式切换到其他运动模式。
 */
void Int_Motor_Stop_Immediate(void)
{
    __Int_Motor_Move_Stop();

    motor_para.step.current_step = 0U;
    motor_para.speed.current_speed = 0U;
    motor_para.speed_phase = MOTOR_SPEED_PHASE_STOP;
    motor_para.speed_update_tick = HAL_GetTick();
    motor_para.state = MOTOR_STATE_STOPPED;
}

//!========================================================================================================================================================================

/**
 * @brief KEY1～KEY5共用的梯形加减速周期处理函数
 *
 * 必须在while(1)主循环中持续调用。本函数每隔固定时间计算一次当前速度，
 * 再根据新的速度修改TIM2的ARR和CCR，从而改变STEP脉冲频率。
 */
Motor_State Int_Motor_Get_State(void)
{
    return motor_para.state;
}

//!========================================================================================================================================================================

void Int_Motor_Process(void)
{
    uint32_t now_tick;
    uint32_t elapsed_ms;
    uint32_t speed_change;
    uint32_t remaining_step = 0U;
    uint32_t decel_step = 0U;
    uint32_t control_margin_step = 0U;

    /* KEY1～KEY5对应的三种运动状态都需要周期更新速度 */
    if ((motor_para.state != MOTOR_STATE_CONTINUOUS_RUN) &&
        (motor_para.state != MOTOR_STATE_REVOLUTION_RUN) &&
        (motor_para.state != MOTOR_STATE_HOMING_RUN))
    {
        return;
    }

    now_tick = HAL_GetTick();
    elapsed_ms = now_tick - motor_para.speed_update_tick;

    if (elapsed_ms < MOTOR_SPEED_CONTROL_PERIOD_MS)
    {
        return;
    }

    /*
     * 如果主循环偶尔阻塞很久，最多只按100ms计算一次速度变化，
     * 防止一次更新产生过大的速度跳变。
     */
    if (elapsed_ms > 100U)
    {
        elapsed_ms = 100U;
    }

    motor_para.speed_update_tick = now_tick;

    /*
     * KEY2/KEY3属于固定距离运动，可以根据剩余步数提前判断减速点。
     * 当剩余步数小于“理论减速步数+一个控制周期的余量”时进入减速段。
     */
    if ((motor_para.state == MOTOR_STATE_REVOLUTION_RUN) &&
        ((motor_para.speed_phase == MOTOR_SPEED_PHASE_ACCEL) ||
         (motor_para.speed_phase == MOTOR_SPEED_PHASE_CONSTANT)))
    {
        remaining_step = __Int_Motor_Get_Remaining_Step();
        decel_step = __Int_Motor_Calculate_Decel_Step();
        control_margin_step =
            (motor_para.speed.current_speed * MOTOR_SPEED_CONTROL_PERIOD_MS) / 1000U + 1U;

        if (remaining_step <= (decel_step + control_margin_step))
        {
            motor_para.speed_phase = MOTOR_SPEED_PHASE_DECEL;
        }
    }

    switch (motor_para.speed_phase)
    {
    case MOTOR_SPEED_PHASE_ACCEL:
        /* 速度增量 = 加速度 × 经过时间 */
        speed_change = (motor_para.speed.acceleration * elapsed_ms) / 1000U;

        if (speed_change == 0U)
        {
            speed_change = 1U;
        }

        if (motor_para.speed.current_speed >= motor_para.speed.target_speed)
        {
            motor_para.speed.current_speed = motor_para.speed.target_speed;
            motor_para.speed_phase = MOTOR_SPEED_PHASE_CONSTANT;
        }
        else if (speed_change >=
                 (motor_para.speed.target_speed - motor_para.speed.current_speed))
        {
            motor_para.speed.current_speed = motor_para.speed.target_speed;
            motor_para.speed_phase = MOTOR_SPEED_PHASE_CONSTANT;
        }
        else
        {
            motor_para.speed.current_speed += speed_change;
        }

        __Int_Motor_Set_Speed(motor_para.speed.current_speed);
        break;

    case MOTOR_SPEED_PHASE_CONSTANT:
        /* 已达到目标速度，保持当前PWM频率不变 */
        break;

    case MOTOR_SPEED_PHASE_DECEL:
        /* 速度减量 = 减速度 × 经过时间 */
        speed_change = (motor_para.speed.deceleration * elapsed_ms) / 1000U;

        if (speed_change == 0U)
        {
            speed_change = 1U;
        }

        if (motor_para.state == MOTOR_STATE_REVOLUTION_RUN)
        {
            /*
             * 固定距离运动不能在这里提前关闭PWM，否则步数会不足。
             * 速度降低到启动速度后保持低速，最终由PWM回调在目标步数处精确停止。
             */
            if (motor_para.speed.current_speed <=
                (motor_para.speed.start_speed + speed_change))
            {
                motor_para.speed.current_speed = motor_para.speed.start_speed;
            }
            else
            {
                motor_para.speed.current_speed -= speed_change;
            }

            __Int_Motor_Set_Speed(motor_para.speed.current_speed);
        }
        else if (motor_para.state == MOTOR_STATE_CONTINUOUS_RUN)
        {
            /* 连续运动减速到启动速度附近后即可关闭PWM */
            if (motor_para.speed.current_speed <=
                (motor_para.speed.start_speed + speed_change))
            {
                __Int_Motor_Move_Stop();
                motor_para.step.current_step = 0U;
                motor_para.speed.current_speed = 0U;
                motor_para.speed_phase = MOTOR_SPEED_PHASE_STOP;
                motor_para.state = MOTOR_STATE_STOPPED;
            }
            else
            {
                motor_para.speed.current_speed -= speed_change;
                __Int_Motor_Set_Speed(motor_para.speed.current_speed);
            }
        }
        else
        {
            /* 回零运动的停止位置由光电开关决定，触发后在EXTI回调中立即停止 */
            motor_para.speed_phase = MOTOR_SPEED_PHASE_ACCEL;
        }
        break;

    case MOTOR_SPEED_PHASE_STOP:
    default:
        /* 状态异常时执行保护性停止 */
        __Int_Motor_Move_Stop();
        motor_para.speed.current_speed = 0U;
        motor_para.speed_phase = MOTOR_SPEED_PHASE_STOP;
        motor_para.state = MOTOR_STATE_STOPPED;
        break;
    }

#if MOTOR_SPEED_DEBUG_ENABLE
    /*
     * 调试时每200ms输出一次速度，避免高频printf阻塞主循环。
     * 正常加速时speed应逐渐增大，ARR应逐渐减小。
     */
    {
        static uint32_t debug_tick = 0U;

        if ((now_tick - debug_tick) >= MOTOR_SPEED_DEBUG_PERIOD_MS)
        {
            debug_tick = now_tick;
            printf("state=%d phase=%d speed=%lu arr=%lu ccr=%lu\r\n",
                   (int)motor_para.state,
                   (int)motor_para.speed_phase,
                   (unsigned long)motor_para.speed.current_speed,
                   (unsigned long)__HAL_TIM_GET_AUTORELOAD(&htim2),
                   (unsigned long)__HAL_TIM_GET_COMPARE(&htim2, TIM_CHANNEL_1));
        }
    }
#endif
}
