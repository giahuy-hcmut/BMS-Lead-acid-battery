/*
 * Task_Temperature.c
 *
 *  Created on: Mar 3, 2026
 *      Author: User
 */

#include "ds18b20.h"
#include "Shared_Data.h"
#include "Task_Temperature.h"
void Task_Temperature_Run(void)
{
    // Đọc kết quả của nhịp đo trước
    float temp = DS18B20_Read_Temperature();

    // Gán vào biến chung gửi CAN
    if (temp != -127.0) {
        myBMS.temp = temp;
        if (temp > THRESHOLD_OVER_TEMP) {
            myBMS.status |= ERROR_OVER_TEMP;
        } else {
            myBMS.status &= ~ERROR_OVER_TEMP;
        }
    }

    // Ra lệnh đo tiếp (STM32 sẽ rảnh tay đi làm việc khác trong 750ms tới)
    DS18B20_Start_Conversion();
}
