/**
 * @file      Task_Sleep.c
 * @author    Do Dang Gia Huy
 * @brief     Heartbeat timeout check and WFI entry.
 * @date      2026-06-30
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 */

/**********************
 *      INCLUDES
 **********************/

#include "Task_Sleep.h"

#include "BMS_CAN.h"
#include "BSP_Led.h"

#include "main.h"

/**********************
 *      DEFINES
 **********************/

/* Silence from the master for this long means the vehicle is parked. The
 * master sends every 5 ms, so 5 s is three orders of magnitude of slack and
 * cannot be reached by a burst of lost frames. */
#define HEARTBEAT_TIMEOUT_MS    5000U

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
 * @brief  Enter sleep mode while the master heartbeat is absent.
 * @return None.
 */
void Task_Sleep_Run(void)
{
    if ((HAL_GetTick() - BMS_CAN_GetLastRxTick()) > HEARTBEAT_TIMEOUT_MS)
    {
        BSP_Led_On();           /* LED on = sleeping */

        /* The SysTick interrupt would wake the core every millisecond and
         * defeat the point, so suspend it across the WFI. */
        HAL_SuspendTick();
        HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
        /* The core resumes here on the CAN RX interrupt. */
        HAL_ResumeTick();
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
