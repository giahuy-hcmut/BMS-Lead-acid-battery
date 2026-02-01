/*
 * App_BMS.h
 *
 *  Created on: Jan 31, 2026
 *      Author: User
 */

#ifndef INC_APP_BMS_H_
#define INC_APP_BMS_H_

#include "BMS_ADC.h"
#include "BMS_CAN.h"

// Struct quản lý trạng thái của cả hệ thống BMS Slave
typedef struct {
    float voltage;       // Điện áp hiện tại
    uint8_t status;      // Trạng thái (0: OK, 1: Error)
    uint32_t lastTick;   // Dùng để canh thời gian gửi
} App_BMS_t;

// Hàm xử lý chính
void App_BMS_Init(void);
void App_BMS_Process(ADC_HandleTypeDef *hadc, CAN_HandleTypeDef *hcan);

#endif /* INC_APP_BMS_H_ */
