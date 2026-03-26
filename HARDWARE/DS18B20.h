#ifndef __DS18B20_H
#define __DS18B20_H

#include "gd32f10x.h"

/* DS18B20 data pin - PB9 */
#define DS18B20_GPIO_CLK  RCU_GPIOB
#define DS18B20_PORT      GPIOB
#define DS18B20_PIN       GPIO_PIN_9

/*
 * DS18B20_Init        - enable clock, check device present; returns 1 OK / 0 not found
 * DS18B20_StartConvert- issue Convert T command, then caller waits >=750ms
 * DS18B20_ReadTemp    - read scratchpad; temp_x10 in 0.1°C units (e.g. 256 = 25.6°C)
 *                       returns 1 OK / 0 error
 *
 * FreeRTOS usage:
 *   DS18B20_StartConvert();
 *   vTaskDelay(800);
 *   DS18B20_ReadTemp(&temp);
 */
uint8_t DS18B20_Init(void);
uint8_t DS18B20_StartConvert(void);
uint8_t DS18B20_ReadTemp(int16_t *temp_x10);

#endif /* __DS18B20_H */
