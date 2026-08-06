#ifndef __INT_MQTT_H__
#define __INT_MQTT_H__

#include "stm32f1xx_hal.h"
#include "MQTTClient.h"
#include "Int_W5500.h"
#include "socket.h"
#include "string.h"
#include "semphr.h"

#define SOCKET_NUM 0

/**
 * @brief 初始化MQTT
 *
 */
void Int_MQTT_Init(void);

/**
 * @brief 发送MQTT数据
 *
 * @param data 数据指针
 * @param len 数据长度
 */
void Int_MQTT_SendData(uint8_t *data, uint16_t len);


/**
 * @brief 刷新MQTT连接
 *
 */
void Int_MQTT_Refresh(void);


#endif /* __INT_MQTT_H__ */
