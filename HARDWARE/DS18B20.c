#include "DS18B20.h"

/* ------------------------------------------------------------------ */
/* Internal helpers                                                     */
/* ------------------------------------------------------------------ */

static void DS18B20_DelayUs(uint32_t us)
{
    uint32_t i, j;
    uint32_t loop = (SystemCoreClock / 1000000U) / 5U + 1U;
    for (i = 0U; i < us; i++)
        for (j = 0U; j < loop; j++)
            __NOP();
}

static void DS18B20_SetOutput(void)
{
    gpio_init(DS18B20_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, DS18B20_PIN);
}

static void DS18B20_SetInput(void)
{
    gpio_init(DS18B20_PORT, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, DS18B20_PIN);
}

static void DS18B20_WriteLine(bit_status status)
{
    gpio_bit_write(DS18B20_PORT, DS18B20_PIN, status);
}

static bit_status DS18B20_ReadLine(void)
{
    return gpio_input_bit_get(DS18B20_PORT, DS18B20_PIN);
}

/* Reset pulse. Returns 1 if presence pulse detected, 0 if no device. */
static uint8_t DS18B20_Reset(void)
{
    uint16_t timeout;

    DS18B20_SetOutput();
    DS18B20_WriteLine(RESET);
    DS18B20_DelayUs(500U);      /* Pull low >=480us */
    DS18B20_WriteLine(SET);
    DS18B20_SetInput();
    DS18B20_DelayUs(60U);       /* Wait for device to pull low (15-60us after release) */

    /* Wait for presence pulse (low) */
    timeout = 0U;
    while (DS18B20_ReadLine() == SET)
    {
        DS18B20_DelayUs(1U);
        if (++timeout > 100U)
            return 0U;
    }

    /* Wait for presence pulse to end */
    timeout = 0U;
    while (DS18B20_ReadLine() == RESET)
    {
        DS18B20_DelayUs(1U);
        if (++timeout > 300U)
            return 0U;
    }

    return 1U;
}

static void DS18B20_WriteBit(uint8_t bit)
{
    DS18B20_SetOutput();
    DS18B20_WriteLine(RESET);
    if (bit)
    {
        DS18B20_DelayUs(5U);    /* Pull low 5us → write '1' */
        DS18B20_WriteLine(SET);
        DS18B20_DelayUs(60U);   /* Complete 60us slot */
    }
    else
    {
        DS18B20_DelayUs(65U);   /* Pull low 65us → write '0' */
        DS18B20_WriteLine(SET);
        DS18B20_DelayUs(5U);    /* Recovery */
    }
}

static uint8_t DS18B20_ReadBit(void)
{
    uint8_t bit;

    DS18B20_SetOutput();
    DS18B20_WriteLine(RESET);
    DS18B20_DelayUs(3U);        /* Pull low 3us to initiate read slot */
    DS18B20_SetInput();
    DS18B20_DelayUs(10U);       /* Sample at ~13us from start (must be <15us) */
    bit = (DS18B20_ReadLine() == SET) ? 1U : 0U;
    DS18B20_DelayUs(50U);       /* Complete 60us slot */
    return bit;
}

static void DS18B20_WriteByte(uint8_t byte)
{
    uint8_t i;
    for (i = 0U; i < 8U; i++)
    {
        DS18B20_WriteBit(byte & 0x01U);
        byte >>= 1U;            /* LSB first */
    }
}

static uint8_t DS18B20_ReadByte(void)
{
    uint8_t i, byte = 0U;
    for (i = 0U; i < 8U; i++)
        byte |= (uint8_t)(DS18B20_ReadBit() << i); /* LSB first */
    return byte;
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

uint8_t DS18B20_Init(void)
{
    rcu_periph_clock_enable(DS18B20_GPIO_CLK);
    DS18B20_SetInput();
    return DS18B20_Reset();
}

/*
 * Issue Convert T (0x44). After this call the caller must wait >=750ms
 * (12-bit resolution) before calling DS18B20_ReadTemp().
 * In FreeRTOS: use vTaskDelay(800) between the two calls.
 */
uint8_t DS18B20_StartConvert(void)
{
    if (DS18B20_Reset() == 0U)
        return 0U;

    DS18B20_WriteByte(0xCCU);   /* Skip ROM */
    DS18B20_WriteByte(0x44U);   /* Convert T */
    DS18B20_SetInput();         /* Release bus; pull-up holds DQ HIGH during conversion */
    return 1U;
}

/*
 * Read scratchpad and convert to 0.1°C units.
 * e.g. 25.6°C → *temp_x10 = 256, -10.5°C → *temp_x10 = -105
 */
uint8_t DS18B20_ReadTemp(int16_t *temp_x10)
{
    uint8_t lsb, msb;
    int16_t raw;

    if (DS18B20_Reset() == 0U)
        return 0U;

    DS18B20_WriteByte(0xCCU);   /* Skip ROM */
    DS18B20_WriteByte(0xBEU);   /* Read Scratchpad */
    lsb = DS18B20_ReadByte();   /* Byte 0: Temp LSB */
    msb = DS18B20_ReadByte();   /* Byte 1: Temp MSB */

    /* 16-bit two's-complement, LSB = 0.0625°C → ×10 then /16 for 0.1°C units */
    raw = (int16_t)((uint16_t)((uint16_t)msb << 8U) | lsb);
    *temp_x10 = (int16_t)((raw * 10) / 16);
    return 1U;
}
