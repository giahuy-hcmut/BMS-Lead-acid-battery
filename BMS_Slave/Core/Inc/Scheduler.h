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
// Chu kỳ gọi SCH_Update (ms). Hạ 10 -> 5 để chứa được Task_SOC 5 ms
// (Kalman Predict theo nhịp dòng điện của master).
// MỌI DELAY_MS/PERIOD_MS truyền vào SCH_Add_Task phải là bội số của hằng này,
// nếu không task sẽ bị từ chối (trả NO_TASK_ID).
#define SCH_TICK_MS     5

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
// DELAY_MS and PERIOD_MS must both be whole multiples of SCH_TICK_MS,
// otherwise the task is rejected and NO_TASK_ID is returned.
uint8_t SCH_Add_Task(void (*pFunction)(void), uint32_t DELAY_MS, uint32_t PERIOD_MS);
uint8_t SCH_Delete_Task(uint8_t taskIndex);

// Số lần một task đã tới hạn lại trước khi được dispatch (trượt deadline).
// Phải luôn bằng 0 trên lịch trình lành.
uint16_t SCH_GetOverrunCount(void);

#endif
