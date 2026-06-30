/*
 * Task_Sleep.c
 *
 *  Created on: Jun 30, 2026
 *      Author: User
 */

#include "Task_Sleep.h"
#include "Shared_Data.h"
#include "main.h"

#define HEARTBEAT_TIMEOUT_MS  5000U

void Task_Sleep_Run(void) {
    if ((HAL_GetTick() - lastHeartbeatTick) > HEARTBEAT_TIMEOUT_MS) {
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET); /* LED ON = sleeping */
        HAL_SuspendTick();
        HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
        /* CPU sleeps here — wakes on CAN RX interrupt */
        HAL_ResumeTick();
    }
}
