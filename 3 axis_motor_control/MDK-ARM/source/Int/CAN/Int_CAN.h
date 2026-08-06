#ifndef __INT_CAN_H__
#define __INT_CAN_H__


#include "stm32f4xx_hal.h"
#include "can.h"
#include "stdio.h"

#define CAN_MOTOR_ID 1001
#define CAN_GATEWAY_ID 1002


typedef struct
{
    uint32_t stdID;
    uint8_t data[8];
    uint32_t data_len;

}CAN_Message;

/**
 * @brief 初始化CAN
 *
 */
void Int_CAN_Init(void);

/**
 * @brief 发送CAN数据
 *
 * @param data  数据指针
 * @param len   数据长度
 */
void Int_CAN_SendData(uint8_t *data,uint32_t len);


/**
 * @brief 接收CAN数据
 *
 * @param data  数据指针
 * @param len   数据长度
 */
void Int_CAN_ReceiveData(CAN_Message *message,uint32_t *len);

#endif /* __INT_CAN_H__ */
