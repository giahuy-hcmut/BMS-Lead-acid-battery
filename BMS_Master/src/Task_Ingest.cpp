/**
 * @file      Task_Ingest.cpp
 * @author    Do Dang Gia Huy
 * @brief     The queue-to-store copy loop.
 * @date      2026-02-10
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 */

/**********************
 *      INCLUDES
 **********************/

#include "Task_Ingest.h"

#include "System_Data.h"    /* canQueue, System_Update_Pack, BmsMessage */

/**********************
 *      DEFINES
 **********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief  Blocks on the CAN queue and writes each frame into the store.
 * @param[in] pvParameters  Unused.
 * @return None.
 */
void Task_Ingest_Run(void *pvParameters)
{
    BmsMessage msg;

    (void)pvParameters;

    for (;;)
    {
        if (xQueueReceive(canQueue, &msg, portMAX_DELAY) == pdTRUE)
        {
            System_Update_Pack(msg.can_id, msg.voltage, msg.temperature,
                               msg.status, msg.soc);
        }
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
