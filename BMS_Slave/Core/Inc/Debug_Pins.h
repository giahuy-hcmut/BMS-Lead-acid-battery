/**
 * @file      Debug_Pins.h
 * @author    Do Dang Gia Huy
 * @brief     Timing-debug pins for logic-analyzer verification of the
 *            task schedule.
 * @date      2026-02-10
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 *
 * Each task drives one PA line HIGH while it runs, so a logic analyzer reads
 * period, execution time and the absence of overlap straight off the
 * waveform. That is external evidence, independent of SCH_GetOverrunCount().
 *
 * The pins are PA0..PA5, all free on this board: SWD is PA13/PA14, the ADC
 * input PB1, the DS18B20 PB7, CAN PB8/PB9, the LED PC13 and the oscillator
 * PD0/PD1, so none of them clash.
 *
 * Logic-analyzer channel map:
 *   D0  PA0  SysTick          1 ms
 *   D1  PA1  Scheduler tick   5 ms
 *   D2  PA2  Task_SOC         5 ms
 *   D3  PA3  Task_Voltage     50 ms
 *   D4  PA4  Task_CAN         1000 ms
 *   D5  PA5  Task_Temperature 1000 ms
 *
 * Comment DBG_TIMING out to compile every pin access away to nothing: the
 * production build then carries zero overhead and leaves PA0..PA5 free.
 */

#ifndef DEBUG_PINS_H_
#define DEBUG_PINS_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************
 *      INCLUDES
 *********************/

#include "main.h"   /* GPIOA, GPIO_PIN_x, HAL_GPIO_WritePin, HAL_GPIO_Init */

/**********************
 *      DEFINES
 *********************/

/** Master switch: comment this line out for a production build. */
#define DBG_TIMING

#ifdef DBG_TIMING

#define DBG_SYSTICK     GPIO_PIN_0      /**< D0 */
#define DBG_SCH         GPIO_PIN_1      /**< D1 */
#define DBG_SOC         GPIO_PIN_2      /**< D2 */
#define DBG_VOLT        GPIO_PIN_3      /**< D3 */
#define DBG_CAN         GPIO_PIN_4      /**< D4 */
#define DBG_TEMP        GPIO_PIN_5      /**< D5 */

/** Every debug pin, for the one-shot GPIO configuration. */
#define DBG_ALL         (DBG_SYSTICK | DBG_SCH | DBG_SOC | DBG_VOLT | \
                         DBG_CAN | DBG_TEMP)

#endif /* DBG_TIMING block */

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

#ifdef DBG_TIMING

/**
 * @brief  Configure PA0..PA5 as push-pull outputs and drive them low.
 * @note   Call once from MX_GPIO_Init(), after the GPIOA clock is enabled.
 *         Defined static inline so the header stays self-contained and the
 *         debug build needs no extra translation unit.
 * @return None.
 */
static inline void DBG_Pins_Init(void)
{
    GPIO_InitTypeDef init = {0};

    init.Pin   = DBG_ALL;
    init.Mode  = GPIO_MODE_OUTPUT_PP;
    init.Pull  = GPIO_NOPULL;
    init.Speed = GPIO_SPEED_FREQ_HIGH;

    HAL_GPIO_Init(GPIOA, &init);
    HAL_GPIO_WritePin(GPIOA, DBG_ALL, GPIO_PIN_RESET);
}

#else   /* !DBG_TIMING */

#define DBG_Pins_Init()     ((void)0)

#endif  /* DBG_TIMING block */

/**********************
 *      MACROS
 **********************/

#ifdef DBG_TIMING

/** Raise the pin: the task starts. */
#define DBG_SET(pin)    HAL_GPIO_WritePin(GPIOA, (pin), GPIO_PIN_SET)

/** Lower the pin: the task ends. */
#define DBG_CLR(pin)    HAL_GPIO_WritePin(GPIOA, (pin), GPIO_PIN_RESET)

#else   /* !DBG_TIMING - compile every access away */

#define DBG_SET(pin)    ((void)0)
#define DBG_CLR(pin)    ((void)0)

#endif  /* DBG_TIMING block */

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*DEBUG_PINS_H_*/
