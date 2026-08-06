#ifndef __INT_W5500_H__
#define __INT_W5500_H__
#include "stm32f1xx_hal.h"
#include "wizchip_conf.h"
#include "gpio.h"
#include "socket.h"
#include "stdio.h"

#define W5500_SOCKET_NUM  0U
#define SERVER_PORT  777
#define SERVER_IP "192.168.48.41"

/**
 * @brief 初始化W5500
 * 
 */
void Int_W5500_Init(void);



/**
 * @brief 发送数据
 * 
 * @param data 
 * @param len 
 */
void Int_W5500_SendData(uint8_t *data, uint16_t len);



#endif /* __INT_W5500_H__ */
