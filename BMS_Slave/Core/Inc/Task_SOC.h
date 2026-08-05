/*
 * Task_SOC.h
 *
 *  Created on: Aug 6, 2026
 *      Author: User
 *
 *  Runs the 2-state Kalman SOC estimator. MULTIRATE:
 *    Predict - every TASK_SOC_PERIOD_MS, driven by the pack current
 *    Update  - only when a FRESH voltage sample exists (~50 ms)
 *
 *  Both run in this one task, in main context. The cooperative scheduler runs
 *  a task to completion, so nothing can interleave with the filter state and
 *  no critical section is needed around it. Splitting Predict into the CAN ISR
 *  would let it corrupt the covariance matrix mid-Update.
 */

#ifndef TASK_SOC_H
#define TASK_SOC_H

#include <stdint.h>

// Chu ky dang ky voi scheduler. Day chi la NHIP MONG MUON - Task_SOC do dt
// thuc te bang HAL_GetTick(), vi scheduler chi dispatch 1 task moi tick nen
// task nay se mat nhip khi task khac toi han. Xem chu thich trong Task_SOC.c.
#define TASK_SOC_PERIOD_MS   5U

void Task_SOC_Run(void);

#endif /* TASK_SOC_H */
