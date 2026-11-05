/**
 * @file      ds18b20.h
 * @author    Do Dang Gia Huy
 * @brief     Single-drop 1-Wire driver for the DS18B20 temperature sensor.
 * @date      2026-03-03
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 *
 * One sensor per bus, so every transaction uses SKIP ROM and no ROM search
 * is implemented. Conversion is split in two calls on purpose: the sensor
 * needs up to 750 ms at 12-bit resolution, which must not be spent blocking
 * inside a scheduler slot. Start the conversion in one task activation and
 * read the result in a later one.
 */

#ifndef DS18B20_H_
#define DS18B20_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************
 *      INCLUDES
 *********************/

#include "stm32f1xx_hal.h"

/**********************
 *      DEFINES
 *********************/

/* Returned instead of a temperature when the sensor does not answer the reset
 * pulse. -127 C is outside the -55..+125 C range the part can report, so it
 * cannot collide with a real reading. */
#define DS18B20_TEMP_ERROR_C    (-127.0f)

/* 1-Wire data line. The pin is configured as open-drain by MX_GPIO_Init(). */
#define DS18B20_PORT    GPIOB
#define DS18B20_PIN     GPIO_PIN_7

/* Timer used as the microsecond time base for the 1-Wire bit slots.
 * It must already be configured for a 1 MHz count rate. */
extern TIM_HandleTypeDef htim4;
#define DS18B20_TIM     (&htim4)

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * @brief  Start the microsecond time base and release the 1-Wire line.
 * @note   Call once at start-up, before any other function of this module.
 * @return None.
 */
void DS18B20_Init(void);

/**
 * @brief  Issue SKIP ROM + CONVERT T so the sensor starts a measurement.
 * @note   Non-blocking with respect to the conversion itself: the result is
 *         only valid after the conversion time of the configured resolution
 *         (up to 750 ms at 12 bit) has elapsed.
 * @return None.
 */
void DS18B20_Start_Conversion(void);

/**
 * @brief  Read the scratchpad and convert the raw count to degrees Celsius.
 * @return Temperature in degrees Celsius, or DS18B20_TEMP_ERROR_C when the
 *         sensor does not answer the reset pulse.
 */
float DS18B20_Read_Temperature(void);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*DS18B20_H_*/
