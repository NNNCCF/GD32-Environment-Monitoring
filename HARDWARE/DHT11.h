#ifndef __DHT11_H
#define __DHT11_H

#include "gd32f10x.h"

/* DHT11 data pin configuration - modify as needed */
#define DHT11_GPIO_CLK  RCU_GPIOB
#define DHT11_PORT      GPIOB
#define DHT11_PIN       GPIO_PIN_8

uint8_t DHT11_Init(void);
uint8_t DHT11_ReadData(uint8_t *humidity, uint8_t *temperature);

#endif /* __DHT11_H */
