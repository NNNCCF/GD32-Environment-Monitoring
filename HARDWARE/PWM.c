#include "PWM.h"

/*!
    \brief      初始化 PWM (定时器模式)
    \param[in]  arr: 自动重装载值 (周期 - 1)
    \param[in]  psc: 预分频值 (分频系数 - 1)
    \param[out] none
    \retval     none
*/
void PWM_Init(uint16_t arr, uint16_t psc)
{
    timer_oc_parameter_struct timer_ocintpara;
    timer_parameter_struct timer_initpara;

    /* 开启时钟 */
    rcu_periph_clock_enable(PWM_GPIO_CLK);
    rcu_periph_clock_enable(PWM_TIMER_CLK);
    rcu_periph_clock_enable(RCU_AF);

    /* 配置 GPIO 为复用推挽输出 */
    gpio_init(PWM_GPIO_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, PWM_PIN);

#ifdef PWM_REMAP
    /* 如果定义了重映射，执行重映射配置 */
    gpio_pin_remap_config(PWM_REMAP, ENABLE);
#endif

    /* 复位定时器 */
    timer_deinit(PWM_TIMER);

    /* 定时器基本配置 */
    timer_initpara.prescaler         = psc;
    timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection  = TIMER_COUNTER_UP;
    timer_initpara.period            = arr;
    timer_initpara.clockdivision     = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0;
    timer_init(PWM_TIMER, &timer_initpara);

    /* 通道配置 (PWM模式1) */
    timer_ocintpara.outputstate  = TIMER_CCX_ENABLE;
    timer_ocintpara.outputnstate = TIMER_CCXN_DISABLE;
    timer_ocintpara.ocpolarity   = TIMER_OC_POLARITY_HIGH;
    timer_ocintpara.ocnpolarity  = TIMER_OCN_POLARITY_HIGH;
    timer_ocintpara.ocidlestate  = TIMER_OC_IDLE_STATE_LOW;
    timer_ocintpara.ocnidlestate = TIMER_OCN_IDLE_STATE_LOW;

    timer_channel_output_config(PWM_TIMER, PWM_CHANNEL, &timer_ocintpara);

    /* 配置通道为 PWM 模式 0 (CNV < CHxVAL 输出有效电平) 
       或者 PWM 模式 1 (CNV < CHxVAL 输出有效电平)
       注意：GD32 库中 PWM0 对应 STM32 的 PWM Mode 1
    */
    timer_channel_output_pulse_value_config(PWM_TIMER, PWM_CHANNEL, 0);
    timer_channel_output_mode_config(PWM_TIMER, PWM_CHANNEL, TIMER_OC_MODE_PWM0);
    timer_channel_output_shadow_config(PWM_TIMER, PWM_CHANNEL, TIMER_OC_SHADOW_DISABLE);

    /* 自动重装载影子寄存器使能 */
    timer_auto_reload_shadow_enable(PWM_TIMER);
    
    /* 使能定时器 */
    timer_enable(PWM_TIMER);
}

/*!
    \brief      设置 PWM 占空比
    \param[in]  duty: 比较值 (0 ~ arr)
    \param[out] none
    \retval     none
*/
void PWM_SetDutyCycle(uint16_t duty)
{
    timer_channel_output_pulse_value_config(PWM_TIMER, PWM_CHANNEL, duty);
}
