#ifndef __PWM_H
#define __PWM_H

#include "gd32f10x.h"

/* 
 * PWM 呼吸灯配置
 * 用户请在此修改引脚和定时器配置
 * 示例配置：使用 TIMER2_CH0 (PA6)
 */

/* GPIO 配置 */
#define PWM_GPIO_CLK    RCU_GPIOA
#define PWM_GPIO_PORT   GPIOA
#define PWM_PIN         GPIO_PIN_6

/* 定时器配置 */
#define PWM_TIMER_CLK   RCU_TIMER2
#define PWM_TIMER       TIMER2
#define PWM_CHANNEL     TIMER_CH_0

/* 重映射配置 (如果不使用重映射，请注释掉此宏) */
// #define PWM_REMAP       GPIO_TIMER2_PARTIAL_REMAP

void PWM_Init(uint16_t arr, uint16_t psc);
void PWM_SetDutyCycle(uint16_t duty);

#endif
