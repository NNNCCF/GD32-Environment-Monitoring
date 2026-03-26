#ifndef __KEY_H
#define __KEY_H

#include "gd32f10x.h"

/* 按键定义
 * KEY1 -> PA0 (示例，请根据实际修改)
 * KEY2 -> PC13 (示例，请根据实际修改)
 * WK_UP -> PA0 (通常 WK_UP 接 PA0)
 */

#define KEY1_GPIO_CLK   RCU_GPIOE
#define KEY1_GPIO_PORT  GPIOE
#define KEY1_PIN        GPIO_PIN_6

/* 按键按下状态定义 */
#define KEY_ON  1
#define KEY_OFF 0

void KEY_Init(void);
uint8_t KEY_Scan(uint8_t mode);

#endif
