#ifndef __INT_MOTOR_H__
#define __INT_MOTOR_H__

#include "stm32f4xx_hal.h"
#include "stdio.h"
#include "gpio.h"
#include "Com_Delay.h"
#include "Com_DWT.h"
#include "tim.h"

#define STEP_PER_REVOLUTION 1600U
#define SPEED_BASE         1600U

/* X轴运动通用梯形加减速参数，单位分别为 step/s、step/s^2、ms */
#define MOTOR_TRAPEZOID_START_SPEED    1600U
#define MOTOR_TRAPEZOID_MAX_SPEED      SPEED_MULTI_8X
#define MOTOR_TRAPEZOID_ACCELERATION   4000U
#define MOTOR_TRAPEZOID_DECELERATION   4000U
#define MOTOR_SPEED_CONTROL_PERIOD_MS   10U

/* 速度调试输出：当前设为1，通过UART5周期输出速度和TIM2周期 */
#define MOTOR_SPEED_DEBUG_ENABLE        1U
#define MOTOR_SPEED_DEBUG_PERIOD_MS     200U

/* TIM2 分频后的计数频率为1MHz，用于根据速度计算ARR */
#define MOTOR_TIM_COUNTER_FREQUENCY     1000000U

typedef enum
{
    SPEED_MULTI_1X = 1U * SPEED_BASE, // 1 倍速
    SPEED_MULTI_2X = 2U * SPEED_BASE,// 2 倍速
    SPEED_MULTI_3X = 3U * SPEED_BASE,// 3 倍速
    SPEED_MULTI_4X = 4U * SPEED_BASE,// 4 倍速
    SPEED_MULTI_5X = 5U * SPEED_BASE,// 5 倍速
    SPEED_MULTI_6X = 6U * SPEED_BASE,// 6 倍速
    SPEED_MULTI_7X = 7U * SPEED_BASE,// 7 倍速
    SPEED_MULTI_8X = 8U * SPEED_BASE,// 8 倍速
    SPEED_MULTI_9X = 9U * SPEED_BASE,// 9 倍速
    SPEED_MULTI_10X = 10U * SPEED_BASE,// 10 倍速
    SPEED_MULTI_11X = 11U * SPEED_BASE,// 11 倍速
    SPEED_MULTI_12X = 12U * SPEED_BASE,// 12 倍速
    SPEED_MULTI_13X = 13U * SPEED_BASE,// 13 倍速
    SPEED_MULTI_14X = 14U * SPEED_BASE,// 14 倍速
} Motor_Speed_Mutiple; // 速度系数

// 电机运行方向
typedef enum
{
    MOTOR_DIR_HAND,  // 手轮方向
    MOTOR_DIR_MOTOR, // 电机方向
} Motor_Dir;

// 电机运行状态
typedef enum
{
    MOTOR_STATE_INIT,           // 初始状态
    MOTOR_STATE_REVOLUTION_RUN, // 圈数运行中
    MOTOR_STATE_CONTINUOUS_RUN, // 连续运行中
    MOTOR_STATE_HOMING_RUN,     // 回零运行中
    MOTOR_STATE_HOMING,         // 处于零点位置
    MOTOR_STATE_STOPPED,        // 已停止
} Motor_State;

// 梯形速度曲线所处阶段
typedef enum
{
    MOTOR_SPEED_PHASE_STOP,     // 已停止
    MOTOR_SPEED_PHASE_ACCEL,    // 加速阶段
    MOTOR_SPEED_PHASE_CONSTANT, // 匀速阶段
    MOTOR_SPEED_PHASE_DECEL,    // 减速阶段
} Motor_Speed_Phase;

// 电机运行距离
typedef struct
{
    uint32_t target_step;  // 目标步数
    uint32_t current_step; // 当前已经运行的步数
} Motor_Step;

// 电机运行速度
typedef struct
{
    uint32_t start_speed;  // 启动速度，单位step/s
    uint32_t target_speed; // 最大目标速度，单位step/s
    uint32_t current_speed;// 当前速度，单位step/s
    uint32_t acceleration; // 加速度，单位step/s^2
    uint32_t deceleration; // 减速度，单位step/s^2
} Motor_Speed;

// 电机配置属性
typedef struct
{
    Motor_Dir dir;               // 电机运行方向
    Motor_Step step;             // 电机运行步长
    Motor_Speed speed;           // 电机运行速度
    Motor_State state;           // 电机运行状态
    Motor_Speed_Phase speed_phase;// 梯形加减速所处阶段
    uint32_t speed_update_tick;   // 上一次更新速度的系统时间
    double revolution;           // 运行圈数
} Motor_Para;

/**
 * @brief 初始化电机
 */
void Int_Motor_Init(void);

/**
 * @brief 向手轮方向按照梯形速度曲线移动指定圈数
 */
void Int_Motor_Move_To_Hand(void);

/**
 * @brief 向电机方向按照梯形速度曲线移动指定圈数
 */
void Int_Motor_Move_To_Motor(void);

/**
 * @brief 电机按照加速曲线回零，光电开关触发后立即停止
 */
void Int_Motor_Move_To_Homing(void);

/**
 * @brief 启动或保持连续运动；首次启动时从启动速度逐渐加速
 *
 * @param dir 电机运行方向
 */
void Int_Motor_Move_Continuous(Motor_Dir dir);

/**
 * @brief 请求连续运动减速停止，不会立即关闭PWM
 */
void Int_Motor_Stop_Continuous(void);

/**
 * @brief 立即停止电机，用于限位、急停或切换到其他运动模式
 */
void Int_Motor_Stop_Immediate(void);

/**
 * @brief KEY1～KEY5共用的梯形加减速周期处理函数，需要放在主循环中反复调用
 */
Motor_State Int_Motor_Get_State(void);

void Int_Motor_Process(void);

#endif /* __INT_MOTOR_H__ */
