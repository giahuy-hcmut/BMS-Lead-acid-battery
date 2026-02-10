/*
 * Task_Voltage.c
 *
 *  Created on: Feb 10, 2026
 *      Author: User
 */


#include "Task_Voltage.h"
#include "Shared_Data.h"
#include "BMS_ADC.h"
#include "main.h" // Để lấy biến hadc1

extern ADC_HandleTypeDef hadc1; // Lấy từ main.c sang

void Task_Voltage_Run(void) {
    // 1. Gọi Driver cũ để đo
    float vol = BMS_ADC_GetVoltage(&hadc1);

    // 2. Cập nhật vào kho chung
    myBMS.voltage = vol;

    // 3. Logic Bảo vệ (Protection Logic)
    if (vol < 10.5) {
        myBMS.status |= ERROR_UNDER_VOLT;
        // Code ngắt Relay ở đây (nếu có)
    }
    else if (vol > 15.0) {
        myBMS.status |= ERROR_OVER_VOLT;
    }
    else {
        myBMS.status = ERROR_NONE; // Clear lỗi nếu bình thường
    }
}
