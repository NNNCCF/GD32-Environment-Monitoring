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
static volatile uint16_t g_mq2_adc_raw  = 0U;
static volatile uint8_t  g_ui_page      = 0U;
static volatile uint8_t  g_alarm_temp   = 0U;
static volatile uint8_t  g_alarm_humi   = 0U;
static volatile uint8_t  g_alarm_active = 0U;

#define SENSOR_AVG_WINDOW 8U
#define UI_PAGE_COUNT     3U

#define TEMP_ALARM_LOW_C    10U
#define TEMP_ALARM_HIGH_C   35U
#define HUMI_ALARM_LOW_PCT  30U
#define HUMI_ALARM_HIGH_PCT 80U

#define ZH_WENSHI_SHUJU   "\xCE\xC2\xCA\xAA\xCA\xFD\xBE\xDD"
#define ZH_WENDU          "\xCE\xC2\xB6\xC8"
#define ZH_SHIDU          "\xCA\xAA\xB6\xC8"
#define ZH_QITI_SHUJU     "\xC6\xF8\xCC\xE5\xCA\xFD\xBE\xDD"
#define ZH_NONGDU_BAIFENBI "\xC5\xA8\xB6\xC8\xB0\xD9\xB7\xD6\xB1\xC8"
#define ZH_YUANSHIZHI     "\xD4\xAD\xCA\xBC\xD6\xB5"
#define ZH_BAOJING_XINXI  "\xB1\xA8\xBE\xAF\xD0\xC5\xCF\xA2"
#define ZH_WENDU_BAOJING  "\xCE\xC2\xB6\xC8\xB1\xA8\xBE\xAF"
#define ZH_WENDU_ZHENGCHANG "\xCE\xC2\xB6\xC8\xD5\xFD\xB3\xA3"
#define ZH_SHIDU_BAOJING  "\xCA\xAA\xB6\xC8\xB1\xA8\xBE\xAF"
#define ZH_SHIDU_ZHENGCHANG "\xCA\xAA\xB6\xC8\xD5\xFD\xB3\xA3"
#define ZH_ZHISHIDENG     "\xD6\xB8\xCA\xBE\xB5\xC6"
#define ZH_KAI            "\xBF\xAA"
#define ZH_GUAN           "\xB9\xD8"

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
        g_mq2_adc_raw = adc_raw;

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
        if (g_alarm_active != 0U)
        {
            LED1_On();
        }
        else
        {
            LED1_Off();
        }

        prvWatchdogBeat(WDG_TASK_LED);
        vTaskDelay(100);
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
            g_ui_page = (uint8_t)((g_ui_page + 1U) % UI_PAGE_COUNT);
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
            uint8_t temp_alarm = ((avg_temp < TEMP_ALARM_LOW_C) || (avg_temp > TEMP_ALARM_HIGH_C)) ? 1U : 0U;
            uint8_t humi_alarm = ((avg_humi < HUMI_ALARM_LOW_PCT) || (avg_humi > HUMI_ALARM_HIGH_PCT)) ? 1U : 0U;

            g_temperature = (uint8_t)avg_temp;
            g_humidity    = (uint8_t)avg_humi;
            g_alarm_temp  = temp_alarm;
            g_alarm_humi  = humi_alarm;
            g_alarm_active = (uint8_t)((temp_alarm != 0U) || (humi_alarm != 0U));
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
    uint16_t last_adc_raw = 0xFFFFU;
    uint8_t  last_page = 0xFFU;
    uint8_t  last_alarm = 0xFFU;
    uint8_t  last_alarm_temp = 0xFFU;
    uint8_t  last_alarm_humi = 0xFFU;

    for( ;; )
    {
        uint8_t  temp  = g_temperature;
        uint8_t  humi  = g_humidity;
        uint8_t  mq2   = g_mq2_percent;
        uint16_t adc   = g_mq2_adc;
        uint16_t adc_raw = g_mq2_adc_raw;
        uint16_t mv_avg = (uint16_t)(((uint32_t)adc * 3300U) / 4095U);
        uint16_t mv_raw = (uint16_t)(((uint32_t)adc_raw * 3300U) / 4095U);
        uint8_t  page  = g_ui_page;
        uint8_t  alarm = g_alarm_active;
        uint8_t  alarm_temp = g_alarm_temp;
        uint8_t  alarm_humi = g_alarm_humi;
        uint8_t  dirty = 0U;

        if ((page != last_page) ||
            (temp != last_temp) ||
            (humi != last_humi) ||
            (mq2 != last_mq2) ||
            (adc != last_adc) ||
            (adc_raw != last_adc_raw) ||
            (alarm != last_alarm) ||
            (alarm_temp != last_alarm_temp) ||
            (alarm_humi != last_alarm_humi))
        {
            OLED_Clear();

            if (page == 0U)
            {
                OLED_ShowChinese(32, 0, (char *)ZH_WENSHI_SHUJU);
                OLED_ReverseArea(0, 0, 128, 16);

                OLED_ShowChinese(0, 16, (char *)ZH_WENDU);
                snprintf(buf, sizeof(buf), ":%3d C", temp);
                OLED_ShowString(32, 16, buf, OLED_8X16);

                OLED_ShowChinese(0, 32, (char *)ZH_SHIDU);
                snprintf(buf, sizeof(buf), ":%3d %%", humi);
                OLED_ShowString(32, 32, buf, OLED_8X16);
            }
            else if (page == 1U)
            {
                OLED_ShowChinese(32, 0, (char *)ZH_QITI_SHUJU);
                OLED_ReverseArea(0, 0, 128, 16);

                OLED_ShowChinese(0, 16, (char *)ZH_NONGDU_BAIFENBI);
                snprintf(buf, sizeof(buf), ":%3d %%", mq2);
                OLED_ShowString(80, 16, buf, OLED_8X16);

                OLED_ShowChinese(0, 32, (char *)ZH_YUANSHIZHI);
                snprintf(buf, sizeof(buf), ":%4umV", mv_raw);
                OLED_ShowString(48, 32, buf, OLED_8X16);

                OLED_ShowString(0, 48, "AVG mV:", OLED_8X16);
                snprintf(buf, sizeof(buf), "%4u", mv_avg);
                OLED_ShowString(64, 48, buf, OLED_8X16);
            }
            else
            {
                OLED_ShowChinese(32, 0, (char *)ZH_BAOJING_XINXI);
                OLED_ReverseArea(0, 0, 128, 16);

                if (alarm_temp != 0U)
                {
                    OLED_ShowChinese(0, 16, (char *)ZH_WENDU_BAOJING);
                }
                else
                {
                    OLED_ShowChinese(0, 16, (char *)ZH_WENDU_ZHENGCHANG);
                }
                snprintf(buf, sizeof(buf), " %3dC", temp);
                OLED_ShowString(80, 16, buf, OLED_8X16);

                if (alarm_humi != 0U)
                {
                    OLED_ShowChinese(0, 32, (char *)ZH_SHIDU_BAOJING);
                }
                else
                {
                    OLED_ShowChinese(0, 32, (char *)ZH_SHIDU_ZHENGCHANG);
                }
                snprintf(buf, sizeof(buf), " %3d%%", humi);
                OLED_ShowString(80, 32, buf, OLED_8X16);

                OLED_ShowChinese(0, 48, (char *)ZH_ZHISHIDENG);
                OLED_ShowChinese(64, 48, (char *)((alarm != 0U) ? ZH_KAI : ZH_GUAN));
            }

            last_page = page;
            last_temp = temp;
            last_humi = humi;
            last_mq2  = mq2;
            last_adc  = adc;
            last_adc_raw = adc_raw;
            last_alarm = alarm;
            last_alarm_temp = alarm_temp;
            last_alarm_humi = alarm_humi;
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
