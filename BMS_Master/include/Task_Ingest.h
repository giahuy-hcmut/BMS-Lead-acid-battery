/**
 * @file      Task_Ingest.h
 * @author    Do Dang Gia Huy
 * @brief     Task that copies received CAN frames into the data store.
 * @date      2026-02-10
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 */

#ifndef TASK_INGEST_H_
#define TASK_INGEST_H_

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
 * @brief  Blocks on the CAN queue and writes each frame into the store.
 * @param[in] pvParameters  Unused, required by the FreeRTOS signature.
 * @note   Pure data plumbing: holds no state, makes no decisions, and sleeps
 *         at 0% CPU when no frame is waiting. Split out of Task_Logic so the
 *         safety monitor runs on its own fixed cadence instead of being
 *         paced by frame arrival.
 * @return None.
 */
void Task_Ingest_Run(void *pvParameters);

/**********************
 *      MACROS
 **********************/

#endif /*TASK_INGEST_H_*/
