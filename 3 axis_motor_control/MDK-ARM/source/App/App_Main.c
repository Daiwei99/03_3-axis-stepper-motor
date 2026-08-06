#include "App_Main.h"

uint32_t cnt = 0;
void App_Main(void)
{

    printf("App Main Start...!\n");

    Com_DWT_Init();

    // 初始化电机
    Int_Motor_Init();
    Int_CAN_Init();

    HAL_Delay(1000);

    KEY_TYPE key = KEY_NONE;

    while (1)
    {
        key = Int_KEY_Scan();
        if (key != KEY_NONE)
        {

            if (key == KEY1)
            {

                // 回零操作
                // Int_Motor_Move_Stop();
                Int_Motor_Move_To_Homing();
            }
            if (key == KEY2)
            {
                // 向电机移动]
                // Int_Motor_Move_Stop();
                Int_Motor_Move_To_Motor();
            }
            if (key == KEY3)
            {
                // 向手轮移动
                // Int_Motor_Move_Stop();
                Int_Motor_Move_To_Hand();
            }
            if (key == KEY4)
            {
                // Int_Motor_Move_To_Motor_Point();
            }
            if (key == KEY5)
            {

                // Int_Motor_Move_To_Hand_Point();
            }

            key = KEY_NONE;

            /* code */
        }
        cnt++;


        if (cnt == 8)
        {
            //每100ms获取网关数据
            Int_Motor_Refresh();
            cnt = 0;
            /* code */
        }



        // if (cnt == 100)
        // {
        //     CAN_Message can_message[3] = {0};
        //     uint32_t len = 0;
        //     Int_CAN_ReceiveData(can_message, &len);
        //     if (len > 0)
        //     {
        //         for (uint8_t i = 0; i < len; i++)
        //         {
        //             printf("can_messageData= %s\n", can_message[i].data);
        //         }

        //         /* code */
        //     }

        //     cnt = 0;
        //     /* code */
        // }

        HAL_Delay(10);

        /* code */
    }

    // // 移动电机
    // Int_Motor_Move();
}
