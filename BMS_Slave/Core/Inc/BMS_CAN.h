/*
 * BMS_CAN.h
 *
 *  Created on: Jan 31, 2026
 *      Author: User
 */

#ifndef INC_BMS_CAN_H_
#define INC_BMS_CAN_H_

#include "stm32f1xx_hal.h"


// Khai báo hàm
void BMS_CAN_Init(CAN_HandleTypeDef *hcan);
void BMS_CAN_InitRx(CAN_HandleTypeDef *hcan);
uint8_t BMS_CAN_Transmit(CAN_HandleTypeDef *hcan, uint32_t id, uint8_t *data, uint8_t len);

#endif /* INC_BMS_CAN_H_ */
