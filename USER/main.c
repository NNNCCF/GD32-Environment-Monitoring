#include "gd32f10x.h"
#include "systick.h"
#include <stdio.h>
#include "main.h"

/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"
#include "MQ2.h"
#include "LED.h"
#include "KEY.h"
#include "DHT11.h"
#include "OLED.h"
#include "queue.h"
#include <string.h>
#include "UART.h"

/* Shared sensor data (volatile, uint8_t writes are atomic on Cortex-M3) */
static volatile uint8_t  g_temperature  = 0U;
static volatile uint8_t  g_humidity     = 0U;
static volatile uint8_t  g_mq2_percent  = 0U;

/* Task function prototype */
void vTaskMQ2( void * pvParameters );
void vTaskLED( void * pvParameters );
void vTaskKEY( void * pvParameters );
void vTaskUartCmd( void * pvParameters );
void vTaskDHT11( void * pvParameters );
void vTaskOLED( void * pvParameters );

int main(void)
{
    /* 1. System Init is called from startup file */
    /* Ensure NVIC priority group is set appropriately for FreeRTOS */
    /* FreeRTOS typically requires Priority Group 4 (4 bits for preemption priority) 
       on STM32/GD32 if not managed by the port. 
       However, the port usually handles it or assumes it. 
       Let's set it explicitly to be safe. */
    nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0);

    /* Initialize UART */
    uart_init(115200);
    /* Initialize LED */
    LED_Init();
    /* Initialize MQ2 (ADC) */
    MQ2_Init();
    /* Initialize KEY */
    KEY_Init();
    /* Initialize DHT11 */
    DHT11_Init();
    /* Initialize OLED */
    OLED_Init();

    /* 2. Create Task */
    xTaskCreate( vTaskMQ2,   "TaskMQ2",   configMINIMAL_STACK_SIZE + 128, NULL, 2, NULL );
    xTaskCreate( vTaskLED,   "TaskLED",   configMINIMAL_STACK_SIZE,       NULL, 2, NULL );
    xTaskCreate( vTaskKEY,   "TaskKEY",   configMINIMAL_STACK_SIZE,       NULL, 2, NULL );
    xTaskCreate( vTaskDHT11,  "TaskDHT11",  configMINIMAL_STACK_SIZE + 128, NULL, 2, NULL );
    xTaskCreate( vTaskOLED,   "TaskOLED",   configMINIMAL_STACK_SIZE * 4,   NULL, 2, NULL );
    if (uart_rx_queue != NULL) {
        xTaskCreate( vTaskUartCmd, "TaskUart", configMINIMAL_STACK_SIZE + 128, NULL, 3, NULL );
    }

    /* 3. Start Scheduler */
    vTaskStartScheduler();

    while(1)
    {
        /* Should not reach here */
    }
}

void vTaskMQ2( void * pvParameters )
{
    uint16_t adc_value;
    uint8_t gas_percent;
    char buffer[32];
    
    for( ;; )
    {
        adc_value = MQ2_GetData();
        /* 
           ADC is 12-bit (0-4095). 
           Percentage = (adc_value / 4095) * 100 
        */
        gas_percent = (uint8_t)((adc_value * 100U) / 4095U);

        g_mq2_percent = gas_percent;  // 共享给 OLED 显示
        
        snprintf(buffer, sizeof(buffer), "MQ2: %d%%\r\n", gas_percent);
        usart_send_string(USART0, buffer);
        
        vTaskDelay(1000);
    }
}

void vTaskLED( void * pvParameters)
{
    for( ;; )
    {
			LED1_Toggle();
			vTaskDelay(2000);
    }
}

void vTaskKEY( void * pvParameters )
{
    uint8_t key_val;
    
    for( ;; )
    {
        key_val = KEY_Scan(0);
        
        if (key_val == 1) // KEY1 Pressed
        {
            LED1_Toggle();
        }
        
        vTaskDelay(20); // 20ms polling interval
    }
}

void vTaskUartCmd( void * pvParameters )
{
    char rx_buffer[64];
    uint8_t rx_index = 0;
    char data;

    for( ;; )
    {
        if(xQueueReceive(uart_rx_queue, &data, portMAX_DELAY) == pdPASS)
        {
            if(data == '\n' || data == '\r')
            {
                rx_buffer[rx_index] = '\0';
                if(rx_index > 0)
                {
                    if(strstr(rx_buffer, "LED:ON") != NULL) {
                        LED1_On();
                    } else if(strstr(rx_buffer, "LED:OFF") != NULL) {
                        LED1_Off();
                    } else if(strstr(rx_buffer, "LED:TOGGLE") != NULL) {
                        LED1_Toggle();
                    }
                    rx_index = 0;
                }
            }
            else
            {
                if(rx_index < sizeof(rx_buffer) - 1)
                {
                    rx_buffer[rx_index++] = data;
                }
                else
                {
                    rx_index = 0; // buffer overflow
                }
            }
        }
    }
}

void vTaskDHT11( void * pvParameters )
{
    uint8_t humidity;
    uint8_t temperature;
    char buffer[40];

    for( ;; )
    {
        uint8_t result;

        taskENTER_CRITICAL();
        result = DHT11_ReadData(&humidity, &temperature);
        taskEXIT_CRITICAL();

        if(result == 1U)
        {
            g_temperature = temperature;
            g_humidity    = humidity;
            snprintf(buffer, sizeof(buffer), "DHT11: Temp=%d C, Humi=%d%%\r\n", temperature, humidity);
        }
        else
        {
            snprintf(buffer, sizeof(buffer), "DHT11: Read failed\r\n");
        }
        usart_send_string(USART0, buffer);

        vTaskDelay(2000); /* DHT11 minimum sampling interval: 1s, use 2s */
    }
}

void vTaskOLED( void * pvParameters )
{
    char    buf[17];
    uint8_t last_temp = 0xFFU;
    uint8_t last_humi = 0xFFU;
    uint8_t last_mq2  = 0xFFU;

    /* Initial placeholders for all 4 lines (8x16, Y=0/16/32/48) */
    OLED_ShowString(0,  0, "DHT11 Sensor", OLED_8X16);
    OLED_ShowString(0, 16, "Temp:  -- C ", OLED_8X16);
    OLED_ShowString(0, 32, "Humi:  -- % ", OLED_8X16);
    OLED_ShowString(0, 48, "MQ2:   -- %", OLED_8X16);
    taskENTER_CRITICAL();
    OLED_Update();
    taskEXIT_CRITICAL();

    for( ;; )
    {
        uint8_t temp  = g_temperature;
        uint8_t humi  = g_humidity;
        uint8_t mq2   = g_mq2_percent;
        uint8_t dirty = 0U;

        if (temp != last_temp || humi != last_humi)
        {
            snprintf(buf, sizeof(buf), "Temp: %3d C ", temp);
            OLED_ShowString(0, 16, buf, OLED_8X16);
            snprintf(buf, sizeof(buf), "Humi: %3d %% ", humi);
            OLED_ShowString(0, 32, buf, OLED_8X16);
            last_temp = temp;
            last_humi = humi;
            dirty = 1U;
        }

        if (mq2 != last_mq2)
        {
            snprintf(buf, sizeof(buf), "MQ2:  %3d %%", mq2);
            OLED_ShowString(0, 48, buf, OLED_8X16);
            last_mq2 = mq2;
            dirty = 1U;
        }

        if (dirty)
        {
            taskENTER_CRITICAL();
            OLED_Update();
            taskEXIT_CRITICAL();
        }

        vTaskDelay(500);
    }
}



