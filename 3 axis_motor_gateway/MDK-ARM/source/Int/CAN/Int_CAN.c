#include "Int_CAN.h"

/**
 * @brief 初始化CAN
 *
 */
void Int_CAN_Init(void)
{

    // 1.配置设备过滤器
    CAN_FilterTypeDef can_filter_config;
    can_filter_config.FilterActivation = CAN_FILTER_ENABLE;   // 激活过滤器
    can_filter_config.FilterBank = 0;                         // 14个过滤器，0-13
    can_filter_config.FilterFIFOAssignment = CAN_FilterFIFO0; // 分配邮箱0
    can_filter_config.FilterMode = CAN_FILTERMODE_IDLIST;     // 匹配ID列表
    can_filter_config.FilterIdHigh = (CAN_MOTOR_ID << 5);
    can_filter_config.FilterIdLow = 0;
    can_filter_config.FilterMaskIdHigh = 0;
    can_filter_config.FilterMaskIdLow = 0;
    can_filter_config.FilterScale = CAN_FILTERSCALE_32BIT; // ID位宽
    HAL_CAN_ConfigFilter(&hcan, &can_filter_config);
    // CAN通信需要启动
    HAL_CAN_Start(&hcan);
}

//!============================================================================================================================================

/**
 * @brief 发送CAN数据
 *
 * @param data  数据指针
 * @param len   数据长度
 */
void Int_CAN_SendData(uint8_t *data, uint32_t len)
{
    // 发数据前，先检查发送邮箱是否为空

    uint8_t cnt = 0;

    uint32_t t0 = HAL_GetTick();
    while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) == 0)
    {
        if (HAL_GetTick() - t0 > 5)
        {
            return;
            /* code */
        }

        /* code */
    }

    /* code */

    CAN_TxHeaderTypeDef can_message_header;
    uint32_t mailbox_num = 0;

    can_message_header.DLC = len;              // 数据长度，最大8字节，最小1字节
    can_message_header.StdId = CAN_GATEWAY_ID; // 标准ID
    can_message_header.IDE = CAN_ID_STD;       // 标准帧
    can_message_header.RTR = CAN_RTR_DATA;     // 数据帧

    // 将发送的数据放置到发送邮箱
    HAL_CAN_AddTxMessage(&hcan, &can_message_header, data, &mailbox_num);
    // printf("Int_CAN_SendData mailbox_num = %d\n", mailbox_num);

    /* code */
}

//!============================================================================================================================================

/**
 * @brief 接收CAN数据
 *
 * @param data  数据指针
 * @param len   数据长度
 */
void Int_CAN_ReceiveData(CAN_Message *message, uint32_t *len)
{

    // 接收数据前，先检查接收邮箱是否为空

    // while (1)
    // {
    *len = HAL_CAN_GetRxFifoFillLevel(&hcan, CAN_RX_FIFO0);
    if (*len == 0)
    {
        return;
        /* code */
    }
    if (*len > 3)
    {
        *len = 3;
        /* code */
    }

    /* code */

    for (uint8_t i = 0; i < *len; i++)
    {
        CAN_RxHeaderTypeDef can_message_header;
        // 将要接收的消息存到接收邮箱队列0中
        HAL_CAN_GetRxMessage(&hcan, CAN_RX_FIFO0, &can_message_header, message[i].data);
        message[i].stdID = can_message_header.StdId;
        message[i].data_len = can_message_header.DLC;
        /* code */
    }
}

/* code */

/* code */
// }
