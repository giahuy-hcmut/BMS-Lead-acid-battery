/*
 * Task_Sleep.c
 *
 *  Created on: Jun 30, 2026
 *      Author: User
 */

#include "Task_Sleep.h"
#include "BMS_CAN.h"
#include "BSP_Led.h"
#include "main.h"

#define HEARTBEAT_TIMEOUT_MS  5000U

void Task_Sleep_Run(void) {
    if ((HAL_GetTick() - BMS_CAN_GetLastRxTick()) > HEARTBEAT_TIMEOUT_MS) {
        BSP_Led_On();                                  /* LED ON = sleeping */
        HAL_SuspendTick();
        HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
        /* CPU sleeps here — wakes on CAN RX interrupt */
        HAL_ResumeTick();
    }
}
