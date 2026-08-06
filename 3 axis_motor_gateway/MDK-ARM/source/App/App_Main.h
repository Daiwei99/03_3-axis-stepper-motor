#ifndef __APP_MAIN_H__
#define __APP_MAIN_H__

#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "stdio.h"
#include "Int_CAN.h"
#include "Int_MQTT.h"
#include "cJSON.h"
#include "string.h"

void App_Main(void);


#endif /* __APP_MAIN_H__ */
