/**
 * @file      Task_SOC.h
 * @author    Do Dang Gia Huy
 * @brief     Multirate Kalman state-of-charge estimation task.
 * @date      2026-08-06
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 *
 * Runs the 2-state Kalman SOC estimator at two different rates:
 *   - Predict, every TASK_SOC_PERIOD_MS, driven by the pack current;
 *   - Update, only when a FRESH voltage sample exists, about every 50 ms.
 *
 * Both run inside this one task, in main context. The cooperative scheduler
 * runs a task to completion, so nothing can interleave with the filter state
 * and no critical section is needed around it. Moving Predict into the CAN
 * ISR would let it corrupt the covariance matrix in the middle of an Update.
 */

#ifndef TASK_SOC_H_
#define TASK_SOC_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************
 *      INCLUDES
 *********************/

#include <stdint.h>

/**********************
 *      DEFINES
 *********************/

/** Period registered with the scheduler, in milliseconds.
 * This is the INTENDED rate. Task_SOC measures the real interval with
 * HAL_GetTick() rather than assuming this value, because the release instant
 * jitters whenever a longer task runs first. See the comment in Task_SOC.c. */
#define TASK_SOC_PERIOD_MS   5U

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * @brief  Run one Predict step, and an Update when a fresh voltage arrived.
 * @note   The first call only initialises the filter and returns; it waits
 *         for the first voltage sample to invert the OCV curve.
 * @return None.
 */
void Task_SOC_Run(void);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*TASK_SOC_H_*/
