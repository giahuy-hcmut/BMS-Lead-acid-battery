/*
 * Task_Temperature.c
 *
 *  Created on: Mar 3, 2026
 *      Author: User
 */

#include "ds18b20.h"
#include "Shared_Data.h"
#include "Board_Config.h"   // THRESHOLD_OVER_TEMP
#include "Task_Temperature.h"
#include "Debug_Pins.h"
void Task_Temperature_Run(void)
{
    DBG_SET(DBG_TEMP);          // bat den: task Temperature bat dau chay
    // Đọc kết quả của nhịp đo trước
    float temp = DS18B20_Read_Temperature();

    // Gán vào kho gửi CAN. Doc loi (-127) thi KHONG goi Set: gia tri va co
    // loi cu duoc giu nguyen - dung nhu hanh vi truoc day.
    if (temp != -127.0) {
        uint8_t faults = 0;
        if (temp > THRESHOLD_OVER_TEMP) { faults |= ERROR_OVER_TEMP; }
        BMS_Data_SetTemp(temp, faults);
    }

    // Ra lệnh đo tiếp (STM32 sẽ rảnh tay đi làm việc khác trong 750ms tới)
    DS18B20_Start_Conversion();
    DBG_CLR(DBG_TEMP);          // tat den: task Temperature ket thuc
}
