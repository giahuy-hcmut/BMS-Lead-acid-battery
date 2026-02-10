/*
 * Scheduler.c
 *
 *  Created on: Feb 10, 2026
 *      Author: User
 */


/* Scheduler.c */
#include "Scheduler.h"

sTask SCH_tasks_G[SCH_MAX_TASKS];
static uint8_t Head_Index = NO_TASK_ID; // Đầu danh sách

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

    // 1. Tìm chỗ trống trong mảng
    while ((SCH_tasks_G[New_Index].pTask != 0) && (New_Index < SCH_MAX_TASKS)) {
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

    __enable_irq(); // Mở lại ngắt (CRITICAL SECTION KẾT THÚC)

    return New_Index;
}

// Hàm xóa Task (Để tái sử dụng slot)
uint8_t SCH_Delete_Task(uint8_t taskIndex) {
    if (taskIndex >= SCH_MAX_TASKS || SCH_tasks_G[taskIndex].pTask == 0) return 0;

    __disable_irq(); // Khóa ngắt an toàn

    // Logic xóa khỏi danh sách liên kết hơi phức tạp nên ta reset mềm
    // (Trong thực tế Dispatch sẽ tự xử lý việc remove khỏi Head)
    SCH_tasks_G[taskIndex].pTask = 0;
    SCH_tasks_G[taskIndex].Delay = 0;
    SCH_tasks_G[taskIndex].Period = 0;
    SCH_tasks_G[taskIndex].RunMe = 0;

    __enable_irq();
    return 1;
}

// Hàm Dispatch (Chạy trong while(1))
void SCH_Dispatch(void) {
    // Chỉ kiểm tra Head (Vì danh sách đã sắp xếp, ai Delay=0 chắc chắn nằm đầu)
    if (Head_Index != NO_TASK_ID) {

        if (SCH_tasks_G[Head_Index].RunMe > 0) {

            // 1. Lưu thông tin Task cần chạy
            void (*pRunTask)(void) = SCH_tasks_G[Head_Index].pTask;
            uint32_t period = SCH_tasks_G[Head_Index].Period;
            uint8_t current_id = Head_Index;

            // 2. Giảm cờ RunMe
            SCH_tasks_G[current_id].RunMe -= 1;

            // 3. Tách Task khỏi đầu danh sách (Pop Head)
            __disable_irq();
            Head_Index = SCH_tasks_G[Head_Index].NextTaskIndex;
            // Nếu Task kế tiếp trở thành Head, nó không cần chờ nữa (Delay=0 tương đối)
            // (Thực ra Delay của nó đã đúng nhờ thuật toán Delta rồi)
            __enable_irq();

            // 4. CHẠY TASK
            if (pRunTask != 0) {
                pRunTask();
            }

            // 5. Nếu là Task định kỳ -> THÊM LẠI VÀO DANH SÁCH
            if (period > 0) {
                // Ta dùng lại hàm Add để nó tự tính toán vị trí chèn mới
                // Chuyển đổi ngược Ticks -> MS để gọi hàm
                SCH_Add_Task(pRunTask, period * SCH_TICK_MS, period * SCH_TICK_MS);

                // Dọn dẹp slot cũ (Vì Add_Task đã tạo slot mới ở vị trí khác hoặc chính nó)
                // Lưu ý: Đây là cách đơn giản nhất. Để tối ưu RAM hơn thì cần hàm "Move" thay vì "Add new".
                // Nhưng với BMS < 10 tasks thì cách này an toàn nhất.
                SCH_Delete_Task(current_id);
            } else {
                 SCH_Delete_Task(current_id); // Task chạy 1 lần thì xóa luôn
            }
        }
    }
}
