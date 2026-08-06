#ifndef __INT_MOTOR_H__
#define __INT_MOTOR_H__

#include "stm32f4xx_hal.h"
#include "stdio.h"
#include "gpio.h"
#include "Com_Delay.h"
#include "Com_DWT.h"
#include "tim.h"
#include "Int_CAN.h"

#define STEP_PER_REVOLUTION 1600
#define SPEED_BASE 1600

typedef enum
{
    SPEED_MULTI_1X = 1 * SPEED_BASE, // 1 倍速
    SPEED_MULTI_2X = 2 * SPEED_BASE,
    SPEED_MULTI_3X = 3 * SPEED_BASE,
    SPEED_MULTI_4X = 4 * SPEED_BASE,
    SPEED_MULTI_5X = 5 * SPEED_BASE,
    SPEED_MULTI_6X = 6 * SPEED_BASE,
    SPEED_MULTI_7X = 7 * SPEED_BASE,
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
    MOTOR_STATE_POINT_RUN,      // 持续运行中
    MOTOR_STATE_HOMING_RUN,     // 回零运行中
    MOTOR_STATE_HOMING,         // 处于零点位置
    MOTOR_STATE_STOPPED,        // 停止中

} Motor_State;

// 电机持续运行状态
typedef enum
{

    MOTOR_SPEED_FIRST_PRESS,
    MOTOR_SPEED_ACC,
    MOTOR_SPEED_DEC,
    MOTOR_SPEED_CONST,
    MOTOR_SPEED_STOP,
} Motor_Speed_Phase;

// 电机运行距离
typedef struct
{
    uint16_t target;
    uint16_t current;
    uint16_t acc_step;
    uint16_t dec_step;
    uint16_t const_step;

    /* data */
} Motor_Step;

// 电机运行速度
typedef struct
{
    double target_speed;  // 目标速度(MAX)
    double current_speed; // 当前速度(MIN)
    double min;
    double acc;
    double dec;

} Motor_Speed;

// 电机配置属性
typedef struct
{
    Motor_Dir dir;                 // 电机运行方向
    Motor_Step step;               // 电机运行步长
    Motor_Speed speed;             // 电机运行速度
    Motor_State state;             // 电机运行状态
    Motor_Speed_Phase speed_phase; // 电机持续运行状态
    double revolution;             // 运行圈数

    /* data */
} Motor_Para;

/**
 * @brief 初始化---电机
 *
 */
void Int_Motor_Init(void);

/**
 * @brief 移动---手轮方向
 *
 */
void Int_Motor_Move_To_Hand(void);

/**
 * @brief 移动---电机方向
 *
 */
void Int_Motor_Move_To_Motor(void);

/**
 * @brief 移动---回零方向
 *
 */
void Int_Motor_Move_To_Homing(void);

/**
 * @brief 移动---手轮方向
 *
 */
void Int_Motor_Move_To_Hand_Point(void);

/**
 * @brief 移动---电机方向
 *
 */
void Int_Motor_Move_To_Motor_Point(void);

/**
 * @brief 停止---电机
 *
 */
void Int_Motor_Move_Stop(void);

/**
 * @brief 打印步数
 *
 */
void Int_Motor_PrintInfo(void);

/**
 * @brief 刷新电机状态
 *
 */
void Int_Motor_Refresh(void);

#endif /* __INT_MOTOR_H__ */
