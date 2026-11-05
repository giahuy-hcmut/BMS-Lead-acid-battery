/**
 * @file      ds18b20.c
 * @author    Do Dang Gia Huy
 * @brief     Bit-banged 1-Wire transport and DS18B20 command sequences.
 * @date      2026-03-03
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 *
 * Timing follows the DS18B20 datasheet (Maxim/Analog Devices, rev 2019),
 * "1-WIRE SIGNALING" section. All slot lengths are driven by a free-running
 * timer polled by DS18B20_DelayUs(), so they are NOT protected against
 * interrupts: an ISR firing inside a slot stretches it. The read slot is the
 * only one where that matters and is commented at its call site.
 */

/**********************
 *      INCLUDES
 **********************/

#include "ds18b20.h"

/**********************
 *      DEFINES
 **********************/

/* --- Bus timing, microseconds (datasheet "1-WIRE SIGNALING") --- */
#define DS18B20_RESET_LOW_US        480U    /* master reset pulse, >= 480 us  */
#define DS18B20_PRESENCE_WAIT_US     80U    /* sample point of the presence   */
#define DS18B20_RESET_RECOVERY_US   400U    /* rest of the 960 us reset slot  */

#define DS18B20_WRITE1_LOW_US         1U    /* pull low, then release at once */
#define DS18B20_WRITE1_HIGH_US       60U
#define DS18B20_WRITE0_LOW_US        60U    /* hold low for the whole slot    */
#define DS18B20_WRITE0_HIGH_US        1U

#define DS18B20_READ_INIT_LOW_US      2U    /* master starts the read slot    */
#define DS18B20_READ_SAMPLE_US        5U    /* see DS18B20_ReadByte()         */
#define DS18B20_READ_TAIL_US         55U    /* slot >= 60 us + 1 us recovery  */

/* --- ROM and function commands --- */
#define DS18B20_CMD_SKIP_ROM        0xCCU
#define DS18B20_CMD_CONVERT_T       0x44U
#define DS18B20_CMD_READ_SCRATCH    0xBEU

/* --- Misc --- */
#define DS18B20_BITS_PER_BYTE         8U
#define DS18B20_PRESENCE_OK           1    /* sensor pulled the line low     */
#define DS18B20_PRESENCE_FAIL       (-1)   /* line stayed high: no sensor    */

/* Scratchpad temperature is a 16-bit two's complement count of 1/16 degree. */
#define DS18B20_COUNTS_PER_DEGREE  16.0f

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void    DS18B20_DelayUs(uint16_t us);
static int8_t  DS18B20_Reset(void);
static void    DS18B20_WriteByte(uint8_t data);
static uint8_t DS18B20_ReadByte(void);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief  Start the microsecond time base and release the 1-Wire line.
 * @return None.
 */
void DS18B20_Init(void)
{
    HAL_TIM_Base_Start(DS18B20_TIM);

    /* Open-drain high = bus idle. Do this before the first transaction so a
     * sensor powered up earlier is not held in reset. */
    HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_SET);
}

/**
 * @brief  Issue SKIP ROM + CONVERT T so the sensor starts a measurement.
 * @return None.
 */
void DS18B20_Start_Conversion(void)
{
    (void)DS18B20_Reset();
    DS18B20_WriteByte(DS18B20_CMD_SKIP_ROM);
    DS18B20_WriteByte(DS18B20_CMD_CONVERT_T);
}

/**
 * @brief  Read the scratchpad and convert the raw count to degrees Celsius.
 * @return Temperature in degrees Celsius, or DS18B20_TEMP_ERROR_C when the
 *         sensor does not answer the reset pulse.
 */
float DS18B20_Read_Temperature(void)
{
    uint8_t  temp_lsb;
    uint8_t  temp_msb;
    uint16_t raw;

    if (DS18B20_Reset() == DS18B20_PRESENCE_FAIL)
    {
        return DS18B20_TEMP_ERROR_C;
    }

    DS18B20_WriteByte(DS18B20_CMD_SKIP_ROM);
    DS18B20_WriteByte(DS18B20_CMD_READ_SCRATCH);

    /* Only the first two scratchpad bytes are read; the remaining seven and
     * the CRC byte are discarded by the next reset pulse. */
    temp_lsb = DS18B20_ReadByte();
    temp_msb = DS18B20_ReadByte();

    raw = (uint16_t)(((uint16_t)temp_msb << 8) | (uint16_t)temp_lsb);

    return (float)raw / DS18B20_COUNTS_PER_DEGREE;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * @brief  Busy-wait by polling the free-running microsecond timer.
 * @param  us  Number of microseconds to wait.
 * @note   Not interrupt-protected: an ISR firing during the wait lengthens it.
 * @return None.
 */
static void DS18B20_DelayUs(uint16_t us)
{
    __HAL_TIM_SET_COUNTER(DS18B20_TIM, 0);
    while (__HAL_TIM_GET_COUNTER(DS18B20_TIM) < us)
    {
        /* Wait. */
    }
}

/**
 * @brief  Send a reset pulse and sample the presence pulse.
 * @return DS18B20_PRESENCE_OK when a sensor answered, DS18B20_PRESENCE_FAIL
 *         when the line stayed high.
 */
static int8_t DS18B20_Reset(void)
{
    int8_t response;

    HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_RESET);
    DS18B20_DelayUs(DS18B20_RESET_LOW_US);

    HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_SET);
    DS18B20_DelayUs(DS18B20_PRESENCE_WAIT_US);

    /* A sensor answers by pulling the line low for 60..240 us. */
    if (HAL_GPIO_ReadPin(DS18B20_PORT, DS18B20_PIN) == GPIO_PIN_RESET)
    {
        response = DS18B20_PRESENCE_OK;
    }
    else
    {
        response = DS18B20_PRESENCE_FAIL;
    }

    DS18B20_DelayUs(DS18B20_RESET_RECOVERY_US);

    return response;
}

/**
 * @brief  Send one byte, least significant bit first.
 * @param  data  Byte to transmit.
 * @return None.
 */
static void DS18B20_WriteByte(uint8_t data)
{
    uint8_t i;

    for (i = 0U; i < DS18B20_BITS_PER_BYTE; i++)
    {
        if ((data & (uint8_t)(1U << i)) != 0U)
        {
            /* Write 1: pull low briefly, then release for the rest of the
             * slot. The sensor samples 15..60 us after the falling edge. */
            HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_RESET);
            DS18B20_DelayUs(DS18B20_WRITE1_LOW_US);
            HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_SET);
            DS18B20_DelayUs(DS18B20_WRITE1_HIGH_US);
        }
        else
        {
            /* Write 0: hold the line low across the whole sampling window. */
            HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_RESET);
            DS18B20_DelayUs(DS18B20_WRITE0_LOW_US);
            HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_SET);
            DS18B20_DelayUs(DS18B20_WRITE0_HIGH_US);
        }
    }
}

/**
 * @brief  Receive one byte, least significant bit first.
 * @return The byte read from the bus.
 */
static uint8_t DS18B20_ReadByte(void)
{
    uint8_t value = 0U;
    uint8_t i;

    for (i = 0U; i < DS18B20_BITS_PER_BYTE; i++)
    {
        HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_RESET);
        DS18B20_DelayUs(DS18B20_READ_INIT_LOW_US);
        HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_SET);

        /* The DS18B20 read slot closes 15 us after the line is pulled low, so
         * the sample must land before then. DS18B20_DelayUs() polls TIM4 and
         * is NOT interrupt-protected, so any ISR firing here stretches it.
         * This used to be a 10 us delay -> sampling at ~12 us, only 3 us of
         * margin. The CAN RX ISR alone is ~4 us and will run every 5 ms once
         * the master streams the current frame at 200 Hz, which would push the
         * sample past the window and corrupt bits.
         * Sampling at ~7 us leaves ~8 us of margin instead. */
        DS18B20_DelayUs(DS18B20_READ_SAMPLE_US);

        if (HAL_GPIO_ReadPin(DS18B20_PORT, DS18B20_PIN) == GPIO_PIN_SET)
        {
            value |= (uint8_t)(1U << i);
        }

        DS18B20_DelayUs(DS18B20_READ_TAIL_US);
    }

    return value;
}
