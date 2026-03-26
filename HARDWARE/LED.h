#ifndef __LED_H
#define __LED_H

#include "gd32f10x.h"

/* LED1 Hardware Definition */
#define LED1_RCU    RCU_GPIOC
#define LED1_PORT   GPIOC
#define LED1_PIN    GPIO_PIN_13

void LED_Init(void);
void LED1_On(void);
void LED1_Off(void);
void LED1_Toggle(void);

#endif
