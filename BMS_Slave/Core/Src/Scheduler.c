/*
 * Scheduler.c
 *
 *  Created on: Feb 10, 2026
 *      Author: User
 */


/* Scheduler.c */
#include "Scheduler.h"

/* `volatile`: SCH_Update() runs in the SysTick ISR and writes Delay/RunMe
 * while SCH_Dispatch() reads them from the main loop. Without volatile the
 * compiler may keep RunMe in a register across the check, and Dispatch then
 * never sees the ISR's update - the scheduler simply stops. At -O0 this is
 * masked; a release build (-Os) is free to break it.
 * `static`: nothing outside this file touches the table. */
static volatile sTask SCH_tasks_G[SCH_MAX_TASKS];
static volatile uint8_t Head_Index = NO_TASK_ID; // Đầu danh sách

/* Number of times a task was already pending again when it got dispatched,
 * i.e. a deadline was missed. The surplus RunMe cannot be recovered (the
 * re-armed instance starts at RunMe = 0), so at least count the event.
 * Watch SCH_GetOverrunCount() in Live Expressions: it must stay 0. */
static volatile uint16_t s_overrun_count = 0;

uint16_t SCH_GetOverrunCount(void) {
    return s_overrun_count;
}

// Xóa trắng danh sách khi khởi động
void SCH_Init(void) {
    uint8_t i;
    for (i = 0; i < SCH_MAX_TASKS; i++) {
        SCH_Delete_Task(i);
    }
    Head_Index = NO_TASK_ID;
}

// Hàm này chạy trong ngắt: CỰC NHANH (O(1))
void SCH_Update(void) {
    if (Head_Index != NO_TASK_ID) {
        // Chỉ trừ Task đầu tiên
        if (SCH_tasks_G[Head_Index].Delay > 0) {
            SCH_tasks_G[Head_Index].Delay--;
        }

        // Nếu Task đầu đếm về 0 -> Báo chạy
        if (SCH_tasks_G[Head_Index].Delay == 0) {
            SCH_tasks_G[Head_Index].RunMe += 1;

            // Nếu đây là Task chạy 1 lần (Delay=0, Period=0),
            // nó vẫn nằm ở Head chờ Dispatch xử lý.
        }
    }
}

// Hàm thêm Task: Sắp xếp chèn (Insertion Sort) vào danh sách Delta
uint8_t SCH_Add_Task(void (*pFunction)(void), uint32_t DELAY_MS, uint32_t PERIOD_MS) {
    uint8_t New_Index = 0;
    uint32_t primask;

    /* Both arguments are converted by integer division below, so anything
     * that is not a whole number of ticks gets silently truncated:
     * PERIOD_MS = 5 with SCH_TICK_MS = 10 yields Period = 0, which Dispatch
     * treats as a one-shot and deletes. Refuse it loudly instead. */
    if (((PERIOD_MS % SCH_TICK_MS) != 0U) || ((DELAY_MS % SCH_TICK_MS) != 0U)) {
        return NO_TASK_ID;
    }

    /* 1. Tìm chỗ trống trong mảng.
     * Bound check FIRST: the old order dereferenced SCH_tasks_G[New_Index]
     * before testing New_Index < SCH_MAX_TASKS, so a full table read one
     * element past the end of the array. */
    while ((New_Index < SCH_MAX_TASKS) && (SCH_tasks_G[New_Index].pTask != 0)) {
        New_Index++;
    }
    if (New_Index == SCH_MAX_TASKS) return NO_TASK_ID; // Hết chỗ

    // 2. Tính toán Ticks
    // Lưu ý: Cộng thêm 1 để tránh lỗi làm tròn nếu gọi ngay sát mép ngắt
    uint32_t delay_ticks = DELAY_MS / SCH_TICK_MS;

    SCH_tasks_G[New_Index].pTask = pFunction;
    SCH_tasks_G[New_Index].Period = PERIOD_MS / SCH_TICK_MS;
    SCH_tasks_G[New_Index].RunMe = 0;

    // 3. THUẬT TOÁN CHÈN (CRITICAL SECTION BẮT ĐẦU)
    // Phải khóa ngắt để tránh SCH_Update làm sai lệch Delay khi đang tính toán
    // Save/restore PRIMASK instead of __enable_irq(): this function is called
    // from SCH_Dispatch(), so re-enabling unconditionally would break any
    // critical section the caller might be holding.
    primask = __get_PRIMASK();
    __disable_irq();

    // Trường hợp 1: Danh sách rỗng
    if (Head_Index == NO_TASK_ID) {
        Head_Index = New_Index;
        SCH_tasks_G[New_Index].Delay = delay_ticks;
        SCH_tasks_G[New_Index].NextTaskIndex = NO_TASK_ID;
    }
    // Trường hợp 2: Chèn vào ĐẦU danh sách (Delay mới < Head Delay)
    else if (delay_ticks < SCH_tasks_G[Head_Index].Delay) {
        SCH_tasks_G[Head_Index].Delay -= delay_ticks; // Trừ bù cho Task cũ

        SCH_tasks_G[New_Index].NextTaskIndex = Head_Index;
        SCH_tasks_G[New_Index].Delay = delay_ticks;
        Head_Index = New_Index;
    }
    // Trường hợp 3: Chèn vào GIỮA hoặc CUỐI
    else {
        uint8_t current = Head_Index;
        uint8_t prev = NO_TASK_ID;

        // Duyệt tìm vị trí (Trừ dần delay_ticks)
        while(current != NO_TASK_ID) {
            if (delay_ticks < SCH_tasks_G[current].Delay) {
                break; // Tìm thấy chỗ chèn trước 'current'
            }
            delay_ticks -= SCH_tasks_G[current].Delay;
            prev = current;
            current = SCH_tasks_G[current].NextTaskIndex;
        }

        // Chèn vào sau 'prev'
        SCH_tasks_G[New_Index].Delay = delay_ticks;
        SCH_tasks_G[New_Index].NextTaskIndex = current;
        SCH_tasks_G[prev].NextTaskIndex = New_Index;

        // Nếu không phải chèn cuối, update lại delay của thằng phía sau
        if (current != NO_TASK_ID) {
            SCH_tasks_G[current].Delay -= delay_ticks;
        }
    }

    __set_PRIMASK(primask); // Mở lại ngắt (CRITICAL SECTION KẾT THÚC)

    return New_Index;
}

/* Hàm xóa Task (Để tái sử dụng slot)
 *
 * WARNING: this does NOT unlink the slot from the delta list - the previous
 * node's NextTaskIndex is left pointing at it. Only call it for a node that
 * has already been popped off Head (which is what SCH_Dispatch does).
 * Calling it on a node in the middle of the list corrupts the chain. */
uint8_t SCH_Delete_Task(uint8_t taskIndex) {
    uint32_t primask;

    if (taskIndex >= SCH_MAX_TASKS || SCH_tasks_G[taskIndex].pTask == 0) return 0;

    primask = __get_PRIMASK(); // Khóa ngắt an toàn
    __disable_irq();

    SCH_tasks_G[taskIndex].pTask = 0;
    SCH_tasks_G[taskIndex].Delay = 0;
    SCH_tasks_G[taskIndex].Period = 0;
    SCH_tasks_G[taskIndex].RunMe = 0;
    SCH_tasks_G[taskIndex].NextTaskIndex = NO_TASK_ID;  /* leave no stale link */

    __set_PRIMASK(primask);
    return 1;
}

// Hàm Dispatch (Chạy trong while(1))
void SCH_Dispatch(void) {
    // Chỉ kiểm tra Head (Vì danh sách đã sắp xếp, ai Delay=0 chắc chắn nằm đầu)
    if (Head_Index != NO_TASK_ID) {

        if (SCH_tasks_G[Head_Index].RunMe > 0) {

            // 1. Lưu thông tin Task cần chạy
            void (*pRunTask)(void) = SCH_tasks_G[Head_Index].pTask;
            uint32_t period    = SCH_tasks_G[Head_Index].Period;
            uint8_t current_id = Head_Index;
            uint32_t primask;

            /* 2. Giảm RunMe VÀ tách khỏi Head trong CÙNG một critical section.
             *    `RunMe -= 1` is a read-modify-write (LDRB / SUB / STRB); if
             *    SCH_Update() lands between those instructions its own
             *    `RunMe += 1` is overwritten and a deadline vanishes. */
            primask = __get_PRIMASK();
            __disable_irq();
            if (SCH_tasks_G[current_id].RunMe > 1U) {
                s_overrun_count++;      /* deadline missed - surplus is lost */
            }
            SCH_tasks_G[current_id].RunMe -= 1;
            // Nếu Task kế tiếp trở thành Head, Delay của nó đã đúng nhờ thuật toán Delta
            Head_Index = SCH_tasks_G[current_id].NextTaskIndex;
            __set_PRIMASK(primask);

            /* 3. Nạp lại lịch TRƯỚC khi chạy thân task.
             *    Previously SCH_Add_Task ran after pRunTask(), so the period
             *    was measured from when the task FINISHED. A task overrunning
             *    one tick slipped, and the slip became the new baseline, so
             *    the error accumulated in SCH_TICK_MS steps.
             *    Delete first so Add_Task may reuse this very slot; pRunTask
             *    was already copied out above, so clearing pTask is safe. */
            SCH_Delete_Task(current_id);
            if (period > 0) {
                SCH_Add_Task(pRunTask, period * SCH_TICK_MS, period * SCH_TICK_MS);
            }

            // 4. CHẠY TASK
            if (pRunTask != 0) {
                pRunTask();
            }
        }
    }
}
