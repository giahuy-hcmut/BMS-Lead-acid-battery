/*
 * Scheduler.c
 *
 *  Created on: Feb 10, 2026
 *      Author: User
 */


#include "Scheduler.h"

#define MAX_TASKS 5 // Số lượng task tối đa
sTask SCH_tasks_G[MAX_TASKS];
uint8_t current_index = 0;

void SCH_Init(void) {
    current_index = 0;
}

void SCH_Add_Task(void (*pFunction)(void), uint32_t DELAY, uint32_t PERIOD) {
    if (current_index < MAX_TASKS) {
        SCH_tasks_G[current_index].pTask = pFunction;
        SCH_tasks_G[current_index].Delay = DELAY;
        SCH_tasks_G[current_index].Period = PERIOD;
        SCH_tasks_G[current_index].RunMe = 0;
        current_index++;
    }
}

void SCH_Update(void) {
    for (int i = 0; i < current_index; i++) {
        if (SCH_tasks_G[i].Delay > 0) {
            SCH_tasks_G[i].Delay--;
        } else {
            SCH_tasks_G[i].Delay = SCH_tasks_G[i].Period;
            SCH_tasks_G[i].RunMe += 1;
        }
    }
}

void SCH_Dispatch(void) {
    for (int i = 0; i < current_index; i++) {
        if (SCH_tasks_G[i].RunMe > 0) {
            (*SCH_tasks_G[i].pTask)(); // Chạy hàm Task
            SCH_tasks_G[i].RunMe -= 1;
        }
    }
}
