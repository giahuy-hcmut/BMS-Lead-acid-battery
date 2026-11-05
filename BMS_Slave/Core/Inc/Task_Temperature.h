/**
 * @file      Task_Temperature.h
 * @author    Do Dang Gia Huy
 * @brief     Periodic battery temperature measurement and over-temperature
 *            protection.
 * @date      2026-03-03
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 */

#ifndef TASK_TEMPERATURE_H_
#define TASK_TEMPERATURE_H_

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
 * @brief  Read the previous conversion, publish it, and start the next one.
 * @note   Registered with the scheduler at a 1000 ms period. Blocks for about
 *         4.9 ms bit-banging the 1-Wire bus, which is why it carries the
 *         lowest priority of the schedule.
 * @return None.
 */
void Task_Temperature_Run(void);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*TASK_TEMPERATURE_H_*/
