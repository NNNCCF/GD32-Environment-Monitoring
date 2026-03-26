#include "UART.h"
#include "gd32f10x.h"

QueueHandle_t uart_rx_queue = NULL;

/* UART init function */
void uart_init(uint32_t baudrate)
{
    /* enable GPIO clock */
    rcu_periph_clock_enable(USART_GPIO_RCU);
    /* enable USART clock */
    rcu_periph_clock_enable(USART_CLK_RCU);

    /* connect port to USARTx_Tx */
    gpio_init(USART_TX_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, USART_TX_PIN);
    /* connect port to USARTx_Rx */
    gpio_init(USART_RX_PORT, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, USART_RX_PIN);

    /* USART configure */
    usart_deinit(USART_PORT);
    usart_baudrate_set(USART_PORT, baudrate);
    usart_receive_config(USART_PORT, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART_PORT, USART_TRANSMIT_ENABLE);
    usart_enable(USART_PORT);
    
    /* enable USART0 receive interrupt */
    usart_interrupt_enable(USART_PORT, USART_INT_RBNE);
    
    /* NVIC configure */
    nvic_irq_enable(USART0_IRQn, 11, 0); 
    
    /* Create FreeRTOS Queue */
    uart_rx_queue = xQueueCreate(128, sizeof(char));
}

/* UART send string function */
void usart_send_string(uint32_t usart_periph, char *string)
{
    while(*string)
    {
        usart_data_transmit(usart_periph, *string++);
        while(usart_flag_get(usart_periph, USART_FLAG_TBE) == RESET);
    }
}
