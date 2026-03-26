#include "DHT11.h"

/* Busy-wait microsecond delay, same method as DS18B20 */
static void DHT11_DelayUs(uint32_t us)
{
    uint32_t i;
    uint32_t j;
    uint32_t loop = (SystemCoreClock / 1000000U) / 5U + 1U;
    for (i = 0; i < us; i++)
    {
        for (j = 0; j < loop; j++)
        {
            __NOP();
        }
    }
}

static void DHT11_SetOutput(void)
{
    gpio_init(DHT11_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, DHT11_PIN);
}

static void DHT11_SetInput(void)
{
    gpio_init(DHT11_PORT, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, DHT11_PIN);
}

static void DHT11_WriteLine(bit_status status)
{
    gpio_bit_write(DHT11_PORT, DHT11_PIN, status);
}

static bit_status DHT11_ReadLine(void)
{
    return gpio_input_bit_get(DHT11_PORT, DHT11_PIN);
}

uint8_t DHT11_Init(void)
{
    rcu_periph_clock_enable(DHT11_GPIO_CLK);
    DHT11_SetInput();
    return 1U;
}

/*
 * Read 40-bit data from DHT11.
 * Returns 1 on success, 0 on failure (no response or checksum error).
 * humidity/temperature are integer parts only (DHT11 decimal is always 0).
 */
uint8_t DHT11_ReadData(uint8_t *humidity, uint8_t *temperature)
{
    uint8_t data[5] = {0U};
    uint8_t i;
    uint8_t bit;
    uint16_t timeout;

    /* --- Start signal: pull low ≥18ms, then release --- */
    DHT11_SetOutput();
    DHT11_WriteLine(RESET);
    DHT11_DelayUs(20000U);      /* 20ms low */
    DHT11_WriteLine(SET);
    DHT11_DelayUs(30U);         /* 20-40us high */
    DHT11_SetInput();

    /* --- Wait for DHT11 response low (~80us) --- */
    timeout = 0U;
    while (DHT11_ReadLine() == SET)
    {
        DHT11_DelayUs(1U);
        if (++timeout > 100U)
        {
            return 0U;          /* No response */
        }
    }

    /* --- Wait for DHT11 response high (~80us) --- */
    timeout = 0U;
    while (DHT11_ReadLine() == RESET)
    {
        DHT11_DelayUs(1U);
        if (++timeout > 100U)
        {
            return 0U;
        }
    }

    /* --- Wait for response high to end --- */
    timeout = 0U;
    while (DHT11_ReadLine() == SET)
    {
        DHT11_DelayUs(1U);
        if (++timeout > 100U)
        {
            return 0U;
        }
    }

    /* --- Read 40 bits (5 bytes) --- */
    for (i = 0U; i < 40U; i++)
    {
        /* Each bit starts with ~50us low */
        timeout = 0U;
        while (DHT11_ReadLine() == RESET)
        {
            DHT11_DelayUs(1U);
            if (++timeout > 60U)
            {
                return 0U;
            }
        }

        /* Sample after 40us: high = '1', low = '0' */
        DHT11_DelayUs(40U);
        bit = (DHT11_ReadLine() == SET) ? 1U : 0U;

        /* Consume remaining high time */
        timeout = 0U;
        while (DHT11_ReadLine() == SET)
        {
            DHT11_DelayUs(1U);
            if (++timeout > 80U)
            {
                return 0U;
            }
        }

        data[i / 8U] = (uint8_t)(data[i / 8U] << 1U) | bit;
    }

    /* --- Checksum verify --- */
    if (data[4] != (uint8_t)(data[0] + data[1] + data[2] + data[3]))
    {
        return 0U;
    }

    *humidity    = data[0];
    *temperature = data[2];
    return 1U;
}
