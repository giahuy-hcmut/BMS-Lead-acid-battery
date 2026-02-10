/*
 * Scheduler.h
 *
 *  Created on: Feb 10, 2026
 *      Author: User
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "main.h" // Chứa HAL và các định nghĩa uint32_t

// CẤU HÌNH
#define SCH_MAX_TASKS   10
#define NO_TASK_ID      0xFF  // Đánh dấu không có task nào
#define SCH_TICK_MS     10    // Chu kỳ gọi SCH_Update (ms)

typedef struct {
    void (*pTask)(void);    // Con trỏ hàm
    uint32_t Delay;         // Thời gian trễ (tính bằng Ticks)
    uint32_t Period;        // Chu kỳ lặp (tính bằng Ticks)
    uint8_t  RunMe;         // Cờ báo chạy
    uint8_t  NextTaskIndex; // TRỎ ĐẾN TASK TIẾP THEO (Linked List)
} sTask;

// API
void SCH_Init(void);
void SCH_Update(void);    // Gọi trong ngắt SysTick
void SCH_Dispatch(void);  // Gọi trong while(1)
uint8_t SCH_Add_Task(void (*pFunction)(void), uint32_t DELAY_MS, uint32_t PERIOD_MS);
uint8_t SCH_Delete_Task(uint8_t taskIndex);

#endif
