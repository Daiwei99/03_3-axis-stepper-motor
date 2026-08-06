#include "App_Main.h"

void App_Main(void)
{
    KEY_TYPE key = KEY_NONE;
#if MOTOR_SPEED_DEBUG_ENABLE
    KEY_TYPE last_key = KEY_NONE;
#endif

    printf("App Main Start...!\n");

    Com_DWT_Init();

    /* 初始化X轴电机 */
    Int_Motor_Init();

    HAL_Delay(1000);

    while (1)
    {
        key = Int_KEY_Scan();

#if MOTOR_SPEED_DEBUG_ENABLE
        /* 只在稳定按键状态发生变化时输出，检查KEY4/KEY5是否一直保持按下状态 */
        if (key != last_key)
        {
            printf("key=%d tick=%lu\r\n",
                   (int)key,
                   (unsigned long)HAL_GetTick());
            last_key = key;
        }
#endif

        switch (key)
        {
        case KEY1:
            /* KEY1执行回零，先立即终止可能存在的连续运动 */
            Int_Motor_Stop_Immediate();
            Int_Motor_Move_To_Homing();
            break;

        case KEY2:
            /* KEY2向电机方向移动固定圈数 */
            Int_Motor_Stop_Immediate();
            Int_Motor_Move_To_Motor();
            break;

        case KEY3:
            /* KEY3向手轮方向移动固定圈数 */
            Int_Motor_Stop_Immediate();
            Int_Motor_Move_To_Hand();
            break;

        case KEY4:
            /* 按住KEY4：向电机方向连续运动，并逐渐加速 */
            if (Int_Motor_Get_State() == MOTOR_STATE_HOMING_RUN)
            {
                /* KEY1回零没有结束时，允许KEY4手动打断回零并重新进入连续运动 */
                Int_Motor_Stop_Immediate();
            }
            Int_Motor_Move_Continuous(MOTOR_DIR_MOTOR);
            break;

        case KEY5:
            /* 按住KEY5：向手轮方向连续运动，并逐渐加速 */
            if (Int_Motor_Get_State() == MOTOR_STATE_HOMING_RUN)
            {
                /* KEY1回零没有结束时，允许KEY5手动打断回零并重新进入连续运动 */
                Int_Motor_Stop_Immediate();
            }
            Int_Motor_Move_Continuous(MOTOR_DIR_HAND);
            break;

        case KEY_NONE:
        default:
            /* 松开KEY4/KEY5：请求电机按照设定减速度逐渐停止 */
            Int_Motor_Stop_Continuous();
            break;
        }

        /* 周期更新连续运动速度，必须在主循环中持续调用 */
        Int_Motor_Process();
    }
}
