#include "KEY.h"

/*!
    \brief      初始化按键GPIO
    \param[in]  none
    \param[out] none
    \retval     none
*/
void KEY_Init(void)
{
    /* 开启GPIO时钟 */
    rcu_periph_clock_enable(KEY1_GPIO_CLK);

    /* 配置GPIO模式：输入上拉/下拉/浮空 
       通常按键接地时配置为上拉输入 (IPU)
       按键接VCC时配置为下拉输入 (IPD)
    */
    gpio_init(KEY1_GPIO_PORT, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, KEY1_PIN);
}

/*!
    \brief      按键扫描函数
    \param[in]  mode: 0-不支持连按; 1-支持连按
    \param[out] none
    \retval     按键值 (0-无按键, 1-KEY1按下)
*/
uint8_t KEY_Scan(uint8_t mode)
{
    static uint8_t key_up = 1; // 按键松开标志
    
    if (mode) key_up = 1; // 支持连按
    
    if (key_up && (gpio_input_bit_get(KEY1_GPIO_PORT, KEY1_PIN) == RESET))
    {
        // 检测到按键按下
        // 软件消抖：简单延时或依赖调用间隔
        // 这里假设调用者会处理消抖或调用间隔足够
        
        /* 简单的阻塞式消抖 (不推荐在FreeRTOS任务中长时间阻塞，但在简单驱动中常见) */
        /* 更好的方式是使用FreeRTOS的软件定时器或状态机，这里提供基础实现 */
        
        key_up = 0;
        return 1; // KEY1 按下
    }
    else if (gpio_input_bit_get(KEY1_GPIO_PORT, KEY1_PIN) == SET)
    {
        key_up = 1;
    }
    
    return 0; // 无按键按下
}
