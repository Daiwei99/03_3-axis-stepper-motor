#include "Int_Motor.h"

volatile Motor_Para motor_para;

// 网页/网关指定的连续运动目标速度（0 = 未指定，用默认 6X）
static volatile double s_remote_speed = 0.0;

//!========================================================================================================================================================================

/**
 * @brief 设置电机速度,step/s
 *
 *        TIM2 计数时钟 = 84MHz / (PSC+1) = 84MHz / 84 = 1MHz(每个计数 1us)
 *        脉冲频率 f = 1000000 / (ARR+1)  ==>  ARR = 1000000 / speed - 1
 *        1600 step/s = 1 圈/s(1/8 细分:200 * 8 = 1600)
 *
 * @param speed 每秒脉冲数,step/s
 */
void __Int_Motor_Set_Speed(double speed)
{
    // 防止除零
    if (speed == 0)
    {
        return;
    }

    // __HAL_TIM_SetAutoreload(&htim2, 0);

    htim2.Instance->CR1 |= TIM_CR1_ARPE;
    // htim2.Instance->EGR |= TIM_EGR_UG;

    uint32_t arr = 1000000.0 / speed; // 一个完整周期占多少个计数(us)

    __HAL_TIM_SET_AUTORELOAD(&htim2, arr - 1);             // 设置自动重装载寄存器的值
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, arr / 2); // 设置比较寄存器（CCR）的值,50%占空比

    // ARR 已开启预装载(ARPE=1),新值要等更新事件才生效。
    // 定时器未运行时不会产生更新事件,这里手动产生一次,让新速度立即写入影子寄存器
    // if ((htim2.Instance->CR1 & TIM_CR1_CEN) == 0)
    // {
    //     htim2.Instance->EGR = TIM_EGR_UG;
    // }
}

//!========================================================================================================================================================================

/**
 * @brief 设置电机运行方向
 *
 * @param dir 电机运行方向
 */

void __Int_Motor_Set_Dir(Motor_Dir dir)
{
    if (dir == MOTOR_DIR_HAND)
    {
        // 手轮方向
        HAL_GPIO_WritePin(STEPPER_1_DIR_GPIO_Port, STEPPER_1_DIR_Pin, GPIO_PIN_RESET);
        /* code */
    }
    else
    {
        // 电机方向
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
void __Int_Motor_Set_Revolution(double revolution)
{
    if (revolution > 28)
    {
        revolution = 28;
        /* code */
    }
    if (revolution < 0)
    {
        revolution = 1;
        /* code */
    }

    // 计算步数
    motor_para.step.target = revolution * STEP_PER_REVOLUTION;

    motor_para.revolution = revolution;

    double vmax = motor_para.speed.target_speed;
    double vinit = SPEED_MULTI_2X;

    motor_para.speed.min = vinit;
    motor_para.speed.current_speed = vinit;
    motor_para.speed.acc = vinit;
    motor_para.speed.dec = vinit * 2;

    motor_para.step.acc_step = (vmax * vmax - vinit * vinit) / (2 * motor_para.speed.acc);
    motor_para.step.dec_step = (vmax * vmax - vinit * vinit) / (2 * motor_para.speed.dec);
    if ((motor_para.step.acc_step + motor_para.step.dec_step) > motor_para.step.target)
    {
        double tmp1 = motor_para.step.target * 2 * motor_para.speed.acc * motor_para.speed.dec;
        double tmp2 = motor_para.speed.acc + motor_para.speed.dec;
        double tmp3 = vinit * vinit;

        double vnew_max = (tmp1 / tmp2) + tmp3;

        motor_para.step.acc_step = (vnew_max - vinit * vinit) / (2 * motor_para.speed.acc);
        motor_para.step.dec_step = motor_para.step.target - motor_para.step.acc_step;
        motor_para.step.const_step = 0;

        /* code */
    }
    else
    {
        motor_para.step.const_step = motor_para.step.target - (motor_para.step.acc_step + motor_para.step.dec_step);
    }

    printf("target step  = %d\n", motor_para.step.target);
    printf("acc step  = %d\n", motor_para.step.acc_step);
    printf("dec step  = %d\n", motor_para.step.dec_step);
    printf("const step  = %d\n", motor_para.step.const_step);
}
//!============================================================================================================================================

void __Int_Motor_Set_Continus_Speed()
{

    // ★ 远端指定了就用它，没指定（按键操作）仍用默认 6X
    double vmax = (s_remote_speed > 0.0) ? s_remote_speed : (double)SPEED_MULTI_6X;
    double vinit = SPEED_MULTI_2X;

    // ★ 目标速度低于起步速度时（网页滑块可拉到 160），把起步压到目标以下，
    //    否则会出现"起步就超过目标"，加速段逻辑没有意义
    if (vinit > vmax)
    {
        vinit = vmax;
    }

    motor_para.speed.min = vinit;
    motor_para.speed.target_speed = vmax;
    motor_para.speed.current_speed = vinit;
    motor_para.speed.acc = SPEED_MULTI_4X;
    motor_para.speed.dec = SPEED_MULTI_4X * 3;
}

//!========================================================================================================================================================================

/**
 * @brief 初始化---电机
 *
 */
void Int_Motor_Init(void)
{
    motor_para.state = MOTOR_STATE_INIT;

    // 基本参数配置:PA4:方向(顺时针，逆时针) ==》 丝杆滑台（前进，后退）
    HAL_GPIO_WritePin(STEPPER_1_DIR_GPIO_Port, STEPPER_1_DIR_Pin, GPIO_PIN_RESET);
}

//!========================================================================================================================================================================

/**
 * @brief 使用定时器输出PWM方波信号，驱动电机的移动
 *
 */

void __Int_Motor_Move_Start(void)
{

    // 使能开关:PG0(拉高有效)
    HAL_GPIO_WritePin(STEPPER_ALL_EN_GPIO_Port, STEPPER_ALL_EN_Pin, GPIO_PIN_SET);

    printf("Motor Start Move\n");
    // motor_para.state = MOTOR_STATE_RUN;

    // 启动定时器
    HAL_TIM_PWM_Start_IT(&htim2, TIM_CHANNEL_1);
}
//!========================================================================================================================================================================

void __Int_Motor_Move_Stop(void)
{

    // 关闭定时器
    HAL_TIM_PWM_Stop_IT(&htim2, TIM_CHANNEL_1);

    printf("Motor End Move\n");

    // 关闭电机使能
    HAL_GPIO_WritePin(STEPPER_ALL_EN_GPIO_Port, STEPPER_ALL_EN_Pin, GPIO_PIN_RESET);
}

//!========================================================================================================================================================================

/**
 * @brief 定时器PWM脉冲完成回调函数====》当PWM方波高电平发送完毕后，会触发当前的回调函数
 *        但是PWM方波整个周期信号并没有发送完毕，还要发送无效电平信号
 *
 * @param htim
 */
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{

    if (htim->Instance != TIM2)
    {
        return;
        /* code */
    }

    // 圈数运行
    if (motor_para.state == MOTOR_STATE_REVOLUTION_RUN)
    {
        motor_para.step.current++;
        if (motor_para.step.current >= motor_para.step.target)
        {
            motor_para.step.current = 0;
            __Int_Motor_Move_Stop();
            motor_para.state = MOTOR_STATE_STOPPED;
            /* code */
        }
        else
        {

            if (motor_para.step.current < motor_para.step.acc_step)
            {

                motor_para.speed.current_speed += (motor_para.speed.acc / motor_para.speed.current_speed);
                if (motor_para.speed.current_speed > motor_para.speed.target_speed)
                {
                    motor_para.speed.current_speed = motor_para.speed.target_speed;
                    /* code */
                }

                __Int_Motor_Set_Speed(motor_para.speed.current_speed);

                /* code */
            }
            else
            {
                if (motor_para.step.const_step == 0)
                {
                    motor_para.speed.current_speed -= (motor_para.speed.dec / motor_para.speed.current_speed);
                    if (motor_para.speed.current_speed < motor_para.speed.min)
                    {
                        motor_para.speed.current_speed = motor_para.speed.min;
                        /* code */
                    }
                    __Int_Motor_Set_Speed(motor_para.speed.current_speed);
                    /* code */
                }
                else
                {
                    if (motor_para.step.current >= (motor_para.step.acc_step + motor_para.step.const_step))
                    {
                        motor_para.speed.current_speed -= (motor_para.speed.dec / motor_para.speed.current_speed);
                        if (motor_para.speed.current_speed < motor_para.speed.min)
                        {
                            motor_para.speed.current_speed = motor_para.speed.min;
                            /* code */
                        }
                        __Int_Motor_Set_Speed(motor_para.speed.current_speed);
                        /* code */
                    }
                }
            }

            /* code */
        }

        /* code */
    }
    // 持续运行：梯形加减速
    else
    {
        if (motor_para.state == MOTOR_STATE_POINT_RUN)
        {
            if ((motor_para.speed.current_speed < motor_para.speed.target_speed && motor_para.speed_phase == MOTOR_SPEED_ACC))
            {
                // 加速
                motor_para.speed.current_speed += (motor_para.speed.acc / motor_para.speed.current_speed);

                if (motor_para.speed.current_speed > motor_para.speed.target_speed)
                {
                    motor_para.speed.current_speed = motor_para.speed.target_speed;
                    /* code */
                }

                __Int_Motor_Set_Speed(motor_para.speed.current_speed);
                /* code */
            }

            else if (motor_para.speed_phase == MOTOR_SPEED_DEC)
            {

                // 减速
                motor_para.speed.current_speed -= (motor_para.speed.dec / motor_para.speed.current_speed);
                if (motor_para.speed.current_speed <= motor_para.speed.min)
                {
                    motor_para.speed.current_speed = motor_para.speed.min;
                    __Int_Motor_Set_Speed(motor_para.speed.current_speed);

                    __Int_Motor_Move_Stop();

                    motor_para.state = MOTOR_STATE_STOPPED;
                    motor_para.speed_phase = MOTOR_SPEED_FIRST_PRESS;
                    /* code */
                }
                else
                {
                    __Int_Motor_Set_Speed(motor_para.speed.current_speed);
                }

                /* code */
            }
        } // 新增：回零加速段
        else if (motor_para.state == MOTOR_STATE_HOMING_RUN)
        {

            if (motor_para.speed.current_speed < motor_para.speed.target_speed)
            {
                // David Austin 递推：Δv = acc / v
                motor_para.speed.current_speed += (motor_para.speed.acc / motor_para.speed.current_speed);
                if (motor_para.speed.current_speed > motor_para.speed.target_speed)
                {
                    motor_para.speed.current_speed = motor_para.speed.target_speed;
                }
                __Int_Motor_Set_Speed(motor_para.speed.current_speed);
            }

            /* code */
        }
    }
}

//!========================================================================================================================================================================

/**
 * @brief 当光电开关触发（下降沿）时，会触发当前的回调函数
 *
 * @param GPIO_Pin
 * @return __weak
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{

    if (GPIO_Pin == X_ZERO_Pin)
    {
        // 进行回零操作
        __Int_Motor_Move_Stop();
        motor_para.step.current = 0;
        motor_para.state = MOTOR_STATE_HOMING;
        __Int_Motor_Set_Dir(MOTOR_DIR_MOTOR); // 换成电机方向
        /* code */
    }
}

//!========================================================================================================================================================================

/**
 * @brief 移动---手轮方向
 *
 */
void Int_Motor_Move_To_Hand(void)
{

    if (motor_para.state == MOTOR_STATE_REVOLUTION_RUN || motor_para.state == MOTOR_STATE_HOMING_RUN || motor_para.state == MOTOR_STATE_POINT_RUN)
    {
        return;
        /* code */
    }

    if (HAL_GPIO_ReadPin(X_ZERO_GPIO_Port, X_ZERO_Pin) == GPIO_PIN_SET)
    {
        // 配置参数
        // 3 RPS
        // __Int_Motor_Set_Speed(SPEED_MULTI_4X); // 一秒钟转几圈
        motor_para.speed.target_speed = SPEED_MULTI_5X;
        __Int_Motor_Set_Dir(MOTOR_DIR_HAND); // 方向
        __Int_Motor_Set_Revolution(10);      // 距离
        motor_para.state = MOTOR_STATE_REVOLUTION_RUN;
        // 开始移动
        __Int_Motor_Move_Start();
        /* code */
    }
}

//!========================================================================================================================================================================

/**
 * @brief 移动---电机方向
 *
 */
void Int_Motor_Move_To_Motor(void)
{

    if (motor_para.state == MOTOR_STATE_REVOLUTION_RUN || motor_para.state == MOTOR_STATE_HOMING_RUN || motor_para.state == MOTOR_STATE_POINT_RUN)
    {
        return;
        /* code */
    }

    // 配置参数
    // 3 RPS
    // __Int_Motor_Set_Speed(SPEED_MULTI_4X); // 一秒钟转几圈
    motor_para.speed.target_speed = SPEED_MULTI_5X;
    __Int_Motor_Set_Dir(MOTOR_DIR_MOTOR); // 方向
    __Int_Motor_Set_Revolution(10);       // 距离
    motor_para.state = MOTOR_STATE_REVOLUTION_RUN;
    // 开始移动
    __Int_Motor_Move_Start();
}

//!========================================================================================================================================================================

/**
 * @brief 移动---回零方向
 *
 */
void Int_Motor_Move_To_Homing(void)
{

    if (motor_para.state == MOTOR_STATE_REVOLUTION_RUN || motor_para.state == MOTOR_STATE_HOMING_RUN || motor_para.state == MOTOR_STATE_POINT_RUN)
    {
        return;
        /* code */
    }

    if (HAL_GPIO_ReadPin(X_ZERO_GPIO_Port, X_ZERO_Pin) == GPIO_PIN_SET)
    {
        // __Int_Motor_Set_Speed(SPEED_MULTI_4X); // 一秒钟转几圈

        __Int_Motor_Set_Dir(MOTOR_DIR_HAND); // 方向
        // ★ 梯形加减速：从 vinit 平滑爬到 vmax，而不是一把拉到 8000
        double vmax = SPEED_MULTI_4X;  // 6400 目标（原来是 5X=8000，接近零点时降一档更稳）
        double vinit = SPEED_MULTI_1X; // 1600 起步，在突跳频率以内
        motor_para.speed.min = vinit;
        motor_para.speed.current_speed = vinit;
        motor_para.speed.target_speed = vmax;
        motor_para.speed.acc = SPEED_MULTI_4X; // 6400 step/s²
        motor_para.speed.dec = SPEED_MULTI_4X * 3;

        __Int_Motor_Set_Speed(vinit);
        motor_para.state = MOTOR_STATE_HOMING_RUN;
        // Start PWM pulse output; configuring speed/direction alone does not move the motor.
        __Int_Motor_Move_Start();
    }
}

//!========================================================================================================================================================================

/**
 * @brief 持续移动---手轮方向
 *
 */
void Int_Motor_Move_To_Hand_Point(void)
{

    if (motor_para.state == MOTOR_STATE_REVOLUTION_RUN || motor_para.state == MOTOR_STATE_HOMING_RUN || motor_para.state == MOTOR_STATE_POINT_RUN)
    {
        return;
        /* code */
    }

    if (HAL_GPIO_ReadPin(X_ZERO_GPIO_Port, X_ZERO_Pin) == GPIO_PIN_SET)
    {
        // 配置参数
        // 3 RPS
        // __Int_Motor_Set_Speed(SPEED_MULTI_3X); // 一秒钟转几圈
        __Int_Motor_Set_Dir(MOTOR_DIR_HAND); // 方向
        __Int_Motor_Set_Continus_Speed();
        __Int_Motor_Set_Speed(motor_para.speed.current_speed);
        motor_para.state = MOTOR_STATE_POINT_RUN;
        if (motor_para.speed_phase == MOTOR_SPEED_FIRST_PRESS)
        {
            motor_para.speed_phase = MOTOR_SPEED_ACC; /* code */
        }

        // 开始移动
        __Int_Motor_Move_Start();
        /* code */
    }
}

//!========================================================================================================================================================================

/**
 * @brief 持续移动---电机方向
 *
 */
void Int_Motor_Move_To_Motor_Point(void)
{

    if (motor_para.state == MOTOR_STATE_REVOLUTION_RUN || motor_para.state == MOTOR_STATE_HOMING_RUN || motor_para.state == MOTOR_STATE_POINT_RUN)
    {
        return;
        /* code */
    }

    // 配置参数
    // 3 RPS

    // __Int_Motor_Set_Speed(SPEED_MULTI_3X); // 一秒钟转几圈
    __Int_Motor_Set_Dir(MOTOR_DIR_MOTOR); // 方向
    __Int_Motor_Set_Continus_Speed();
    __Int_Motor_Set_Speed(motor_para.speed.current_speed);
    motor_para.state = MOTOR_STATE_POINT_RUN;
    if (motor_para.speed_phase == MOTOR_SPEED_FIRST_PRESS)
    {
        motor_para.speed_phase = MOTOR_SPEED_ACC;
        /* code */
    }

    // 开始移动
    __Int_Motor_Move_Start();
}

//!========================================================================================================================================================================

/**
 * @brief 停止---电机
 *
 */
void Int_Motor_Move_Stop(void)
{
    __Int_Motor_Move_Stop();
    motor_para.state = MOTOR_STATE_STOPPED;
}

//!============================================================================================================================================

/**
 * @brief 打印步数
 *
 */
void Int_Motor_PrintInfo(void)
{
}

//!============================================================================================================================================

/**
 * @brief 刷新电机状态
 *
 */
void Int_Motor_Refresh(void)
{

    // 原来只是在state == MOTOR_STATE_INIT时，才需要刷新状态，改成无条件收（停止/回零指令运行中也要能进来）
    CAN_Message message[3] = {0};
    uint32_t n = 0;
    Int_CAN_ReceiveData(message, &n); // 改成非阻塞

    for (uint32_t i = 0; i < n; i++)
    {
        uint8_t *data = message[i].data;
        if (message[i].data_len < 8)
        {
            continue;
            /* code */
        }
        uint8_t cmd = data[0];
        uint8_t dir = data[1];
        uint16_t dist_01mm = (uint16_t)data[2] | (uint16_t)data[3] << 8;                                                       // 小端
        uint32_t speed = (uint32_t)data[4] | ((uint32_t)data[5] << 8) | ((uint32_t)data[6] << 16) | ((uint32_t)data[7] << 24); // 小端

        switch (cmd)
        {
        case 2:

            // 停止
            // 持续运动中：不要直接切脉冲，改成进入减速段
            // 让PWM回调里那段现成的减速代码把速度平滑降到min再停
            if (motor_para.state == MOTOR_STATE_POINT_RUN)
            {
                motor_para.speed_phase = MOTOR_SPEED_DEC;
                /* code */
            }
            else
            {
                Int_Motor_Move_Stop(); // 其他状态（圈数/回零）保持原来的立即停
            }

            s_remote_speed = 0.0; // ★ 新增：清掉，KEY 操作回到默认 6X
            break;
        case 3:
            Int_Motor_Move_To_Homing();
            break;
        case 1: // 持续移动指令
            if (speed == 0)
            {
                break;
                /* code */
            }
            // motor_para.speed.target_speed = speed;
            s_remote_speed = (double)speed;
            dir ? Int_Motor_Move_To_Motor_Point() : Int_Motor_Move_To_Hand_Point();
            break;

        case 5: // 移动指令
        {

            if (dist_01mm == 0 || speed == 0)
            {
                break;
                /* code */
            }
            if (motor_para.state == MOTOR_STATE_REVOLUTION_RUN || motor_para.state == MOTOR_STATE_POINT_RUN || motor_para.state == MOTOR_STATE_HOMING_RUN)
            {
                break; // 运动中不接新定位
            }
            motor_para.speed.target_speed = speed;
            __Int_Motor_Set_Dir(dir ? MOTOR_DIR_MOTOR : MOTOR_DIR_HAND);
            __Int_Motor_Set_Revolution(dist_01mm / 80.0); // 一圈=8mm=80个0.1mm
            motor_para.step.current = 0;
            motor_para.state = MOTOR_STATE_REVOLUTION_RUN;
            // 开始移动
            __Int_Motor_Move_Start();
            break;
            /* code */
        }

        default:
            break;
        }

        /* code */
    }

    uint8_t data[8] = {0};
    // byte0:是否运行中
    uint8_t running = (motor_para.state == MOTOR_STATE_REVOLUTION_RUN || motor_para.state == MOTOR_STATE_HOMING_RUN || motor_para.state == MOTOR_STATE_POINT_RUN) ? 1 : 0;
    // byte1：回零阶段。★ 12工程没有细分阶段(无 homing_phase 字段)，
    //        只能用 state 粗映射：1=回零中 / 6=已在零点 / 0=其他
    uint8_t phase = 0;
    if (motor_para.state == MOTOR_STATE_HOMING_RUN)
    {
        phase = 1; // 网页显示"快速接近"
        /* code */
    }
    else if (motor_para.state == MOTOR_STATE_HOMING)
    {

        phase = 6; // 网页显示"已在零点"
    }

    // byte2~3：当前速度。current_speed 是 double，转窄类型前必须钳位
    double spd_d = motor_para.speed.current_speed;
    if (spd_d < 0.0)
    {
        spd_d = 0.0;

        /* code */
    }
    if (spd_d > 65535.0)
    {
        spd_d = 65535.0;
        /* code */
    }
    uint16_t sp = (uint16_t)spd_d;

    // byte4~7：当前位置。step.current 本身是 uint16_t，转 int32 安全
    int32_t pos = (int32_t)motor_para.step.current;
    data[0] = running;
    data[1] = phase;
    data[2] = (uint8_t)sp;
    data[3] = (uint8_t)(sp >> 8);
    data[4] = (uint8_t)pos;
    data[5] = (uint8_t)(pos >> 8);
    data[6] = (uint8_t)(pos >> 16);
    data[7] = (uint8_t)(pos >> 24);

    Int_CAN_SendData(data, 8);
}
