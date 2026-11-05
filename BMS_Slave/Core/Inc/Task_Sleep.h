/**
 * @file      Task_Sleep.h
 * @author    Do Dang Gia Huy
 * @brief     Low-power idle when the master stops transmitting.
 * @date      2026-06-30
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 */

#ifndef TASK_SLEEP_H_
#define TASK_SLEEP_H_

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
 * @brief  Enter sleep mode while the master heartbeat is absent.
 * @note   Registered with the scheduler at a 1000 ms period. When it does
 *         sleep, the call does not return until a CAN RX interrupt wakes the
 *         core, so the whole schedule is suspended with it - which is the
 *         intent, since there is nothing to measure without a master.
 * @return None.
 */
void Task_Sleep_Run(void);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*TASK_SLEEP_H_*/
