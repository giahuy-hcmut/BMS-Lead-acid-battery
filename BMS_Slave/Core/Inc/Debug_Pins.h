/*
 * Debug_Pins.h
 *
 *  Timing-debug pins for logic-analyzer verification of task scheduling.
 *  Each task drives one PA line HIGH while it runs; a logic analyzer then
 *  reads period, execution time and (absence of) overlap directly off the
 *  waveform - external evidence, independent of SCH_GetOverrunCount().
 *
 *  Pins are PA0..PA5, all free on this board (SWD = PA13/PA14, ADC = PB1,
 *  DS18B20 = PB7, CAN = PB8/PB9, LED = PC13, OSC = PD0/PD1 - none clash).
 *
 *  Logic-analyzer channel map:
 *      D0 PA0  SysTick        1 ms
 *      D1 PA1  Scheduler tick 5 ms
 *      D2 PA2  Task_SOC       5 ms
 *      D3 PA3  Task_Voltage   50 ms
 *      D4 PA4  Task_CAN       1000 ms
 *      D5 PA5  Task_Temp      1000 ms
 *
 *  Comment out DBG_TIMING below to compile every pin access away to nothing:
 *  the production build then carries zero overhead and leaves PA0..PA5 free.
 */
#ifndef DEBUG_PINS_H
#define DEBUG_PINS_H

#include "main.h"   /* GPIOA, GPIO_PIN_x, HAL_GPIO_WritePin, HAL_GPIO_Init */

/* ---- master switch: comment this line out for a production build ---- */
#define DBG_TIMING

#ifdef DBG_TIMING

#define DBG_SYSTICK   GPIO_PIN_0   /* D0 */
#define DBG_SCH       GPIO_PIN_1   /* D1 */
#define DBG_SOC       GPIO_PIN_2   /* D2 */
#define DBG_VOLT      GPIO_PIN_3   /* D3 */
#define DBG_CAN       GPIO_PIN_4   /* D4 */
#define DBG_TEMP      GPIO_PIN_5   /* D5 */

#define DBG_ALL (DBG_SYSTICK | DBG_SCH | DBG_SOC | DBG_VOLT | DBG_CAN | DBG_TEMP)

/* Bat den (HIGH) / Tat den (LOW). Cung la ham HAL ban da quen dung. */
#define DBG_SET(pin)  HAL_GPIO_WritePin(GPIOA, (pin), GPIO_PIN_SET)
#define DBG_CLR(pin)  HAL_GPIO_WritePin(GPIOA, (pin), GPIO_PIN_RESET)

/* Cau hinh PA0..PA5 thanh chan xuat. Goi 1 lan tu MX_GPIO_Init, sau khi
 * clock cua GPIOA da bat. */
static inline void DBG_Pins_Init(void)
{
    GPIO_InitTypeDef g = {0};
    g.Pin   = DBG_ALL;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &g);
    HAL_GPIO_WritePin(GPIOA, DBG_ALL, GPIO_PIN_RESET);
}

#else  /* !DBG_TIMING - compile everything away */

#define DBG_SET(pin)     ((void)0)
#define DBG_CLR(pin)     ((void)0)
#define DBG_Pins_Init()  ((void)0)

#endif /* DBG_TIMING */

#endif /* DEBUG_PINS_H */
