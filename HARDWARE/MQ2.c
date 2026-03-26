#include "MQ2.h"

void MQ2_Init(void)
{
    /* Enable GPIO and ADC clock */
    rcu_periph_clock_enable(MQ2_GPIO_RCU);
    rcu_periph_clock_enable(MQ2_ADC_RCU);
    
    /* ADC clock configuration */
    /* GD32F103 ADC clock max 14MHz. APB2 is usually SystemCoreClock. 
       If SystemCoreClock is 108MHz, DIV8 = 13.5MHz. */
    rcu_adc_clock_config(RCU_CKADC_CKAPB2_DIV8);

    /* Configure GPIO as analog input */
    gpio_init(MQ2_PORT, GPIO_MODE_AIN, GPIO_OSPEED_50MHZ, MQ2_PIN);

    /* ADC configuration */
    adc_deinit(ADC0);
    /* ADC mode config */
    adc_mode_config(ADC_MODE_FREE); 
    /* ADC data alignment config */
    adc_data_alignment_config(ADC0, ADC_DATAALIGN_RIGHT);
    /* ADC channel length config */
    adc_channel_length_config(ADC0, ADC_REGULAR_CHANNEL, 1);
    /* ADC regular channel config */ 
    adc_regular_channel_config(ADC0, 0, MQ2_ADC_CHANNEL, ADC_SAMPLETIME_55POINT5);
    /* ADC trigger config */
    adc_external_trigger_source_config(ADC0, ADC_REGULAR_CHANNEL, ADC0_1_2_EXTTRIG_REGULAR_NONE);
    adc_external_trigger_config(ADC0, ADC_REGULAR_CHANNEL, ENABLE);

    /* Enable ADC interface */
    adc_enable(ADC0);
    
    /* ADC calibration and reset calibration */
    adc_calibration_enable(ADC0);
}

uint16_t MQ2_GetData(void)
{
    /* ADC software trigger enable */
    adc_software_trigger_enable(ADC0, ADC_REGULAR_CHANNEL);

    /* Wait the end of conversion */
    while(!adc_flag_get(ADC0, ADC_FLAG_EOC));

    /* Clear the EOC flag */
    adc_flag_clear(ADC0, ADC_FLAG_EOC);

    /* Return the ADC value */
    return adc_regular_data_read(ADC0);
}
