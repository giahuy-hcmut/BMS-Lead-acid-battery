/**
 * @file      Task_CAN.h
 * @author    Do Dang Gia Huy
 * @brief     Periodic transmission of this slave's status frame.
 * @date      2026-02-10
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 */

#ifndef TASK_CAN_H_
#define TASK_CAN_H_

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
 * @brief  Pack the current snapshot into CAN_SLAVE_ID and transmit it.
 * @note   Registered with the scheduler at a 1000 ms period, with a start
 *         delay of 300 + SLAVE_INDEX * 100 ms so the five slaves stay phase
 *         staggered and do not contend for the bus on the same tick.
 * @return None.
 */
void Task_CAN_Run(void);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*TASK_CAN_H_*/
