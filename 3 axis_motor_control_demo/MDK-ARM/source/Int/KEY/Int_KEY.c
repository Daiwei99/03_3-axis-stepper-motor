#include "Int_KEY.h"

#define READ_KEY1 HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin)
#define READ_KEY2 HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin)
#define READ_KEY3 HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin)
#define READ_KEY4 HAL_GPIO_ReadPin(KEY4_GPIO_Port, KEY4_Pin)
#define READ_KEY5 HAL_GPIO_ReadPin(KEY5_GPIO_Port, KEY5_Pin)

/* 按键消抖时间，单位：ms */
#define KEY_DEBOUNCE_TIME_MS 10U

/**
 * @brief 非阻塞按键扫描
 *
 * 按键硬件采用上拉输入，按下时为低电平。
 *
 * KEY1/KEY2/KEY3：采用“按下事件”方式，只在稳定按下的瞬间返回一次，
 *                 避免固定圈数运动被长按重复触发。
 * KEY4/KEY5：采用“电平状态”方式，按住期间持续返回对应按键，
 *            松开后返回KEY_NONE，用于实现按住运行、松开减速停止。
 *
 * 本函数不使用HAL_Delay，也不使用while等待松手，避免阻塞主循环，
 * 使Int_Motor_Process()能够持续执行梯形加减速速度更新。
 *
 * @return 当前按键事件或当前按键状态
 */
KEY_TYPE Int_KEY_Scan(void)
{
    static KEY_TYPE stable_key = KEY_NONE;    /* 已经确认的稳定按键 */
    static KEY_TYPE candidate_key = KEY_NONE; /* 等待消抖确认的候选按键 */
    static uint32_t candidate_tick = 0U;      /* 候选按键开始变化的时间 */

    KEY_TYPE raw_key = KEY_NONE;
    uint32_t now_tick = HAL_GetTick();

    /* 按KEY1到KEY5的顺序读取原始按键电平 */
    if (READ_KEY1 == GPIO_PIN_RESET)
    {
        raw_key = KEY1;
    }
    else if (READ_KEY2 == GPIO_PIN_RESET)
    {
        raw_key = KEY2;
    }
    else if (READ_KEY3 == GPIO_PIN_RESET)
    {
        raw_key = KEY3;
    }
    else if (READ_KEY4 == GPIO_PIN_RESET)
    {
        raw_key = KEY4;
    }
    else if (READ_KEY5 == GPIO_PIN_RESET)
    {
        raw_key = KEY5;
    }

    /* 原始按键状态发生变化，重新开始消抖计时 */
    if (raw_key != candidate_key)
    {
        candidate_key = raw_key;
        candidate_tick = now_tick;
    }

    /* 候选状态持续稳定达到消抖时间后，更新稳定按键状态 */
    if ((candidate_key != stable_key) &&
        ((now_tick - candidate_tick) >= KEY_DEBOUNCE_TIME_MS))
    {
        stable_key = candidate_key;

        /* KEY1/KEY2/KEY3只在按下确认的瞬间产生一次按键事件 */
        if ((stable_key == KEY1) ||
            (stable_key == KEY2) ||
            (stable_key == KEY3))
        {
            return stable_key;
        }
    }

    /* KEY4/KEY5按住期间持续报告按键状态 */
    if ((stable_key == KEY4) || (stable_key == KEY5))
    {
        return stable_key;
    }

    /* 没有按键按下，或者没有新的KEY1/KEY2/KEY3按下事件 */
    return KEY_NONE;
}
