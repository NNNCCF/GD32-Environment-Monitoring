#ifndef __UART_H
#define __UART_H

#include "gd32f10x.h"
#include "FreeRTOS.h"
#include "queue.h"

/* UART configuration */
#define USART_PORT      USART0
#define USART_GPIO_RCU  RCU_GPIOA
#define USART_CLK_RCU   RCU_USART0
#define USART_TX_PORT   GPIOA
#define USART_TX_PIN    GPIO_PIN_9
#define USART_RX_PORT   GPIOA
#define USART_RX_PIN    GPIO_PIN_10

/* External queue handle for RX data */
extern QueueHandle_t uart_rx_queue;

/* Function prototypes */
void uart_init(uint32_t baudrate);
void usart_send_string(uint32_t usart_periph, char *string);


#endif /* __UART_H */
