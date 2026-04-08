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
static volatile uint16_t g_mq2_adc      = 0U;

#define SENSOR_AVG_WINDOW 8U

typedef struct
{
    uint16_t samples[SENSOR_AVG_WINDOW];
    uint32_t sum;
    uint8_t  index;
    uint8_t  count;
} sensor_avg_queue_t;

static void sensor_avg_queue_init(sensor_avg_queue_t *queue);
static uint16_t sensor_avg_queue_push_and_get_avg(sensor_avg_queue_t *queue, uint16_t sample);

/* Task function prototype */
void vTaskMQ2( void * pvParameters );
void vTaskLED( void * pvParameters );
void vTaskKEY( void * pvParameters );
void vTaskUartCmd( void * pvParameters );
void vTaskDHT11( void * pvParameters );
void vTaskOLED( void * pvParameters );
void vTaskWDG( void * pvParameters );

typedef enum
{
    WDG_TASK_MQ2 = 0,
    WDG_TASK_LED,
    WDG_TASK_KEY,
    WDG_TASK_DHT11,
    WDG_TASK_OLED,
    WDG_TASK_COUNT
} wdg_task_id_t;

static void prvHardwareInit(void);
static BaseType_t prvCreateAppTasks(void);
static void prvFatalError(void);
static void prvWatchdogBeat(wdg_task_id_t task_id);
static BaseType_t prvWatchdogAllHealthy(void);

static volatile TickType_t g_wdg_heartbeat[WDG_TASK_COUNT] = { 0 };

#define WDG_TIMEOUT_MQ2_TICKS     pdMS_TO_TICKS(3000U)
#define WDG_TIMEOUT_LED_TICKS     pdMS_TO_TICKS(4500U)
#define WDG_TIMEOUT_KEY_TICKS     pdMS_TO_TICKS(500U)
#define WDG_TIMEOUT_DHT11_TICKS   pdMS_TO_TICKS(4500U)
#define WDG_TIMEOUT_OLED_TICKS    pdMS_TO_TICKS(2000U)
#define WDG_SUPERVISOR_PERIOD     pdMS_TO_TICKS(500U)

static void sensor_avg_queue_init(sensor_avg_queue_t *queue)
{
    (void)memset(queue, 0, sizeof(*queue));
}

static uint16_t sensor_avg_queue_push_and_get_avg(sensor_avg_queue_t *queue, uint16_t sample)
{
    if (queue->count < SENSOR_AVG_WINDOW)
    {
        queue->samples[queue->index] = sample;
        queue->sum += sample;
        queue->count++;
    }
    else
    {
        queue->sum -= queue->samples[queue->index];
        queue->samples[queue->index] = sample;
        queue->sum += sample;
    }

    queue->index++;
    if (queue->index >= SENSOR_AVG_WINDOW)
    {
        queue->index = 0U;
    }

    return (uint16_t)(queue->sum / queue->count);
}

int main(void)
{
    prvHardwareInit();

    if (prvCreateAppTasks() != pdPASS)
    {
        prvFatalError();
    }

    vTaskStartScheduler();

    /* Scheduler should never return. If it does, heap/port configuration is wrong. */
    prvFatalError();
}

static void prvHardwareInit(void)
{
    /* FreeRTOS on Cortex-M expects all implemented priority bits to be preemption bits. */
    nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0);

    uart_init(115200);
    LED_Init();
    MQ2_Init();
    KEY_Init();
    DHT11_Init();
    OLED_Init();

    /* FWDGT: IRC40K ~40kHz, DIV256 -> 156.25Hz, reload 1250 -> timeout ~8s */
    rcu_osci_on(RCU_IRC40K);
    rcu_osci_stab_wait(RCU_IRC40K);
    fwdgt_config(1250U, FWDGT_PSC_DIV256);
    fwdgt_enable();
}

static BaseType_t prvCreateAppTasks(void)
{
    BaseType_t status = pdPASS;

    status &= xTaskCreate(vTaskWDG,    "TaskWDG",  configMINIMAL_STACK_SIZE,        NULL, 4, NULL);
    status &= xTaskCreate(vTaskMQ2,    "TaskMQ2",  configMINIMAL_STACK_SIZE + 128U, NULL, 2, NULL);
    status &= xTaskCreate(vTaskLED,    "TaskLED",  configMINIMAL_STACK_SIZE,        NULL, 2, NULL);
    status &= xTaskCreate(vTaskKEY,    "TaskKEY",  configMINIMAL_STACK_SIZE,        NULL, 2, NULL);
    status &= xTaskCreate(vTaskDHT11,  "TaskDHT11",configMINIMAL_STACK_SIZE + 128U, NULL, 2, NULL);
    status &= xTaskCreate(vTaskOLED,   "TaskOLED", configMINIMAL_STACK_SIZE * 4U,   NULL, 2, NULL);

    if (uart_rx_queue != NULL)
    {
        status &= xTaskCreate(vTaskUartCmd, "TaskUart", configMINIMAL_STACK_SIZE + 128U, NULL, 3, NULL);
    }

    return status;
}

static void prvFatalError(void)
{
    volatile uint32_t delay = 0U;

    taskDISABLE_INTERRUPTS();

    while (1)
    {
        LED1_Toggle();
        for (delay = 0U; delay < 200000U; ++delay)
        {
            __NOP();
        }
    }
}

static void prvWatchdogBeat(wdg_task_id_t task_id)
{
    g_wdg_heartbeat[task_id] = xTaskGetTickCount();
}

static BaseType_t prvWatchdogAllHealthy(void)
{
    static const TickType_t timeout_ticks[WDG_TASK_COUNT] =
    {
        WDG_TIMEOUT_MQ2_TICKS,
        WDG_TIMEOUT_LED_TICKS,
        WDG_TIMEOUT_KEY_TICKS,
        WDG_TIMEOUT_DHT11_TICKS,
        WDG_TIMEOUT_OLED_TICKS
    };
    TickType_t now = xTaskGetTickCount();
    UBaseType_t i;

    for (i = 0; i < (UBaseType_t)WDG_TASK_COUNT; ++i)
    {
        TickType_t last = g_wdg_heartbeat[i];

        if ((last == 0U) || ((now - last) > timeout_ticks[i]))
        {
            return pdFALSE;
        }
    }

    return pdTRUE;
}

void vTaskMQ2( void * pvParameters )
{
    uint16_t adc_value;
    uint16_t adc_raw;
    uint8_t gas_percent;
    uint32_t voltage_mv;
    char buffer[48];
    sensor_avg_queue_t mq2_avg_queue;

    sensor_avg_queue_init(&mq2_avg_queue);
    
    for( ;; )
    {
        adc_raw = MQ2_GetData();
        adc_value = sensor_avg_queue_push_and_get_avg(&mq2_avg_queue, adc_raw);
        /* 
           ADC is 12-bit (0-4095). 
           Percentage = (adc_value / 4095) * 100 
        */
        gas_percent = (uint8_t)((adc_value * 100U) / 4095U);
        voltage_mv = ((uint32_t)adc_value * 3300U) / 4095U;
        g_mq2_adc = adc_value;

        g_mq2_percent = gas_percent;  // 共享给 OLED 显示
        
        snprintf(buffer, sizeof(buffer), "MQ2: %d%%, %lu.%03luV\r\n",
                 gas_percent,
                 voltage_mv / 1000U,
                 voltage_mv % 1000U);
        usart_send_string(USART0, buffer);
        prvWatchdogBeat(WDG_TASK_MQ2);

        vTaskDelay(1000);
    }
}

void vTaskLED( void * pvParameters)
{
    for( ;; )
    {
			LED1_Toggle();
			prvWatchdogBeat(WDG_TASK_LED);
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

        prvWatchdogBeat(WDG_TASK_KEY);
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
    sensor_avg_queue_t humi_avg_queue;
    sensor_avg_queue_t temp_avg_queue;

    sensor_avg_queue_init(&humi_avg_queue);
    sensor_avg_queue_init(&temp_avg_queue);

    for( ;; )
    {
        uint8_t result;

        result = DHT11_ReadData(&humidity, &temperature);

        if(result == 1U)
        {
            uint16_t avg_temp = sensor_avg_queue_push_and_get_avg(&temp_avg_queue, temperature);
            uint16_t avg_humi = sensor_avg_queue_push_and_get_avg(&humi_avg_queue, humidity);

            g_temperature = (uint8_t)avg_temp;
            g_humidity    = (uint8_t)avg_humi;
            snprintf(buffer, sizeof(buffer), "DHT11: Temp=%d C, Humi=%d%%\r\n", g_temperature, g_humidity);
        }
        else
        {
            snprintf(buffer, sizeof(buffer), "DHT11: Read failed\r\n");
        }
        usart_send_string(USART0, buffer);
        prvWatchdogBeat(WDG_TASK_DHT11);

        vTaskDelay(2000); /* DHT11 minimum sampling interval: 1s, use 2s */
    }
}

void vTaskOLED( void * pvParameters )
{
    char     buf[17];
    uint8_t  last_temp = 0U;
    uint8_t  last_humi = 0U;
    uint8_t  last_mq2  = 0xFFU;
    uint16_t last_adc  = 0xFFFFU;

    /* Initial placeholders for all 4 lines (8x16, Y=0/16/32/48) */
    OLED_ShowString(0,  0, "Temp:  -- C ", OLED_8X16);
    OLED_ShowString(0, 16, "Humi:  -- % ", OLED_8X16);
    OLED_ShowString(0, 32, "MQ2:   -- % ", OLED_8X16);
    OLED_ShowString(0, 48, "ADC: -.---V", OLED_8X16);
    taskENTER_CRITICAL();
    OLED_Update();
    taskEXIT_CRITICAL();

    for( ;; )
    {
        uint8_t  temp  = g_temperature;
        uint8_t  humi  = g_humidity;
        uint8_t  mq2   = g_mq2_percent;
        uint16_t adc   = g_mq2_adc;
        uint8_t  dirty = 0U;

        if (temp != last_temp)
        {
            snprintf(buf, sizeof(buf), "Temp: %3d C ", temp);
            OLED_ShowString(0, 0, buf, OLED_8X16);
            last_temp = temp;
            dirty = 1U;
        }

        if (humi != last_humi)
        {
            snprintf(buf, sizeof(buf), "Humi: %3d %% ", humi);
            OLED_ShowString(0, 16, buf, OLED_8X16);
            last_humi = humi;
            dirty = 1U;
        }

        if (mq2 != last_mq2)
        {
            snprintf(buf, sizeof(buf), "MQ2:  %3d %%", mq2);
            OLED_ShowString(0, 32, buf, OLED_8X16);
            last_mq2 = mq2;
            dirty = 1U;
        }

        if (adc != last_adc)
        {
            uint32_t voltage_mv = ((uint32_t)adc * 3300U) / 4095U;
            snprintf(buf, sizeof(buf), "ADC: %1lu.%03luV ",
                     voltage_mv / 1000U,
                     voltage_mv % 1000U);
            OLED_ShowString(0, 48, buf, OLED_8X16);
            last_adc = adc;
            dirty = 1U;
        }

        if (dirty)
        {
            taskENTER_CRITICAL();
            OLED_Update();
            taskEXIT_CRITICAL();
        }

        prvWatchdogBeat(WDG_TASK_OLED);
        vTaskDelay(500);
    }
}

void vTaskWDG( void * pvParameters )
{
    for( ;; )
    {
        if (prvWatchdogAllHealthy() == pdTRUE)
        {
            fwdgt_counter_reload();
        }

        vTaskDelay(WDG_SUPERVISOR_PERIOD);
    }
}
