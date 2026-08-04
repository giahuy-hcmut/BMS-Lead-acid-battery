/*
 * Task_Voltage.c
 *
 *  Created on: Feb 10, 2026
 *      Author: User
 */


/* Task_Voltage.c */
#include "Task_Voltage.h"
#include "Shared_Data.h"
#include "Board_Config.h"   // THRESHOLD_UNDER_VOLT / THRESHOLD_OVER_VOLT
#include "BMS_ADC.h"

void Task_Voltage_Run(void) {
    // 1. Gọi Driver để đo (Đã có sẵn lấy mẫu 100 lần và tính hiệu chỉnh)
    float vol = BMS_ADC_GetVoltage();

    // 2. Logic Bảo vệ. `faults` la bien local, bat dau tu 0 moi lan chay,
    //    nen khong con can nhanh `else` de xoa bit nhu truoc.
    uint8_t faults = 0;
    if (vol < THRESHOLD_UNDER_VOLT) { faults |= ERROR_UNDER_VOLT; }
    if (vol > THRESHOLD_OVER_VOLT)  { faults |= ERROR_OVER_VOLT;  }

    // 3. Nạp vào kho qua API. Task nay chi so huu 2 bit tren, khong the
    //    cham vao nhiet do / dong dien / SOC cua chu khac.
    BMS_Data_SetVoltage(vol, faults);
}
