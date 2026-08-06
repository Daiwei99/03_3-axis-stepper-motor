#ifndef __INT_KEY_H__
#define __INT_KEY_H__

#include "stm32f4xx_hal.h"
#include "gpio.h"

typedef enum{
    KEY_NONE,
    KEY1,
    KEY2,
    KEY3,
    KEY4,
    KEY5,
}KEY_TYPE;


/**
 * @brief 扫描按键
 * 
 * @return KEY_TYPE 
 */
KEY_TYPE Int_KEY_Scan(void);


#endif /* __INT_KEY_H__ */
