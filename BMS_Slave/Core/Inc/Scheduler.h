/*
 * Scheduler.h
 *
 *  Created on: Feb 10, 2026
 *      Author: User
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>

typedef struct {
    void (*pTask)(void);  // Con trỏ hàm
    uint32_t Delay;
    uint32_t Period;
    uint8_t RunMe;
} sTask;

// Hàm khởi tạo và quản lý
void SCH_Init(void);
void SCH_Update(void);    // Gọi trong ngắt SysTick
void SCH_Dispatch(void);  // Gọi trong while(1)
void SCH_Add_Task(void (*pFunction)(void), uint32_t DELAY, uint32_t PERIOD);

#endif
