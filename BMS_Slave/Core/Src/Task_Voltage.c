/*
 * Task_Voltage.c
 *
 *  Created on: Feb 10, 2026
 *      Author: User
 */


/* Task_Voltage.c */
#include "Task_Voltage.h"
#include "Shared_Data.h"
#include "BMS_ADC.h"
#include "main.h"

extern ADC_HandleTypeDef hadc1;

void Task_Voltage_Run(void) {
    // 1. Gọi Driver để đo (Đã có sẵn lấy mẫu 100 lần và tính hiệu chỉnh)
    float vol = BMS_ADC_GetVoltage(&hadc1);

    // 2. Cập nhật vào kho chung
    myBMS.voltage = vol;

    // 3. Logic Bảo vệ (Protection Logic) - SỬ DỤNG MACRO TỪ SHARED_DATA
    if (vol < THRESHOLD_UNDER_VOLT) {
        myBMS.status |= ERROR_UNDER_VOLT;
        // Code ngắt Relay bảo vệ xả cạn (nếu có)
    }
    else if (vol > THRESHOLD_OVER_VOLT) {
        myBMS.status |= ERROR_OVER_VOLT;
        // Code ngắt Relay bảo vệ sạc nhồi (nếu có)
    }
    else {
        myBMS.status = ERROR_NONE;
    }
}
