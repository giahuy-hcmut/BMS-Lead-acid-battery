/**
 * @file      Task_Voltage.h
 * @author    Do Dang Gia Huy
 * @brief     Periodic battery voltage measurement and voltage protection.
 * @date      2026-02-10
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 */

#ifndef TASK_VOLTAGE_H_
#define TASK_VOLTAGE_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************
 *      INCLUDES
 *********************/

/**********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * @brief  Measure the terminal voltage, run the voltage checks and publish.
 * @note   Registered with the scheduler at a 50 ms period. Blocks for about
 *         0.7 ms while averaging the ADC samples.
 * @return None.
 */
void Task_Voltage_Run(void);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*TASK_VOLTAGE_H_*/
