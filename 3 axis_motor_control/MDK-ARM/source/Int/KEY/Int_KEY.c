#include "Int_KEY.h"

extern Motor_Para motor_para;

#define READ_KEY1 HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin)
#define READ_KEY2 HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin)
#define READ_KEY3 HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin)
#define READ_KEY4 HAL_GPIO_ReadPin(KEY4_GPIO_Port, KEY4_Pin)
#define READ_KEY5 HAL_GPIO_ReadPin(KEY5_GPIO_Port, KEY5_Pin)

/**
 * @brief 扫描按键
 *
 * @return KEY_TYPE
 */
KEY_TYPE Int_KEY_Scan(void)
{

    if (READ_KEY1 == GPIO_PIN_RESET || READ_KEY2 == GPIO_PIN_RESET || READ_KEY3 == GPIO_PIN_RESET || READ_KEY4 == GPIO_PIN_RESET || READ_KEY5 == GPIO_PIN_RESET)
    {
        HAL_Delay(10); // 延时消抖

        // 扫描按键1
        if (READ_KEY1 == GPIO_PIN_RESET)
        {
            // KEY1被按下，需要确认是否抬起
            while (READ_KEY1 == GPIO_PIN_RESET)
            {
                /* code */
            }
            return KEY1;

            /* code */
        }

        // 扫描按键2
        if (READ_KEY2 == GPIO_PIN_RESET)
        {
            // KEY2被按下，需要确认是否抬起
            while (READ_KEY2 == GPIO_PIN_RESET)
            {
                /* code */
            }
            return KEY2;

            /* code */
        }

        // 扫描按键3
        if (READ_KEY3 == GPIO_PIN_RESET)
        {
            // KEY3被按下，需要确认是否抬起
            while (READ_KEY3 == GPIO_PIN_RESET)
            {
                /* code */
            }
            return KEY3;

            /* code */
        }

        // 扫描按键4
        if (READ_KEY4 == GPIO_PIN_RESET)
        {

            // 启动电机
            motor_para.speed_phase = MOTOR_SPEED_FIRST_PRESS;
            Int_Motor_Move_To_Motor_Point();

            // KEY4被按下，需要确认是否抬起
            while (READ_KEY4 == GPIO_PIN_RESET)
            {

                /* code */
            }
            // 停止电机
            motor_para.speed_phase = MOTOR_SPEED_DEC;
            // Int_Motor_Move_Stop();
            return KEY4;

            /* code */
        }

        // 扫描按键5
        if (READ_KEY5 == GPIO_PIN_RESET)
        {
            // 启动电机
            motor_para.speed_phase = MOTOR_SPEED_FIRST_PRESS;
            Int_Motor_Move_To_Hand_Point();

            // KEY5被按下，需要确认是否抬起
            while (READ_KEY5 == GPIO_PIN_RESET)
            {

                /* code */
            }
            // 停止电机
            motor_para.speed_phase = MOTOR_SPEED_DEC;
            // Int_Motor_Move_Stop();
            return KEY5;

            /* code */
        }

        /* code */
    }

    // 没有按键按下
    return KEY_NONE;
}
