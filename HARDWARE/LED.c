#include "LED.h"

/*!
    \brief      configure the LED GPIO
    \param[in]  none
    \param[out] none
    \retval     none
*/
void LED_Init(void)
{
    /* enable the LED clock */
    rcu_periph_clock_enable(LED1_RCU);

    /* configure LED GPIO port */ 
    gpio_init(LED1_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, LED1_PIN);

    /* turn off LED initially */
    gpio_bit_write(LED1_PORT, LED1_PIN, SET);
}

/*!
    \brief      turn on LED1
    \param[in]  none
    \param[out] none
    \retval     none
*/
void LED1_On(void)
{
    gpio_bit_write(LED1_PORT, LED1_PIN, RESET);
}

/*!
    \brief      turn off LED1
    \param[in]  none
    \param[out] none
    \retval     none
*/
void LED1_Off(void)
{
    gpio_bit_write(LED1_PORT, LED1_PIN, SET);
}

/*!
    \brief      toggle LED1
    \param[in]  none
    \param[out] none
    \retval     none
*/
void LED1_Toggle(void)
{
    gpio_bit_write(LED1_PORT, LED1_PIN, 
        (bit_status)(1-gpio_input_bit_get(LED1_PORT, LED1_PIN)));
}
