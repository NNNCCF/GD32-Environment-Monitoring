#ifndef __MQ2_H
#define __MQ2_H

#include "gd32f10x.h"

/*
 * 硬件连接说明:
 * MQ2 VCC -> 5V (必须接5V以保证加热正常)
 * MQ2 GND -> GND
 * MQ2 AO  -> PA0 (模拟输出，接 ADC)
 * MQ2 DO  -> 悬空 (本程序使用ADC采集，无需连接DO)
 */

#define MQ2_GPIO_RCU    RCU_GPIOA
#define MQ2_ADC_RCU     RCU_ADC0
#define MQ2_PORT        GPIOA
#define MQ2_PIN         GPIO_PIN_0
#define MQ2_ADC_CHANNEL ADC_CHANNEL_0

void MQ2_Init(void);
uint16_t MQ2_GetData(void);

#endif
