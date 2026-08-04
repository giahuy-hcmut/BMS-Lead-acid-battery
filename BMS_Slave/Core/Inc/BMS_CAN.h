/*
 * BMS_CAN.h
 *
 *  Created on: Jan 31, 2026
 *      Author: User
 */

#ifndef INC_BMS_CAN_H_
#define INC_BMS_CAN_H_

#include "stm32f1xx_hal.h"


// Nộp handle CAN MỘT LẦN lúc khởi động. Phải gọi trước mọi hàm khác.
void BMS_CAN_Init(CAN_HandleTypeDef *hcan);

// Cấu hình filter + bật ngắt RX. Dùng handle đã nộp ở BMS_CAN_Init.
void BMS_CAN_InitRx(void);

// Gửi 1 frame. Trả 1 nếu vào được mailbox, 0 nếu bận / lỗi / chưa Init.
uint8_t BMS_CAN_Transmit(uint32_t id, uint8_t *data, uint8_t len);

// Mốc HAL_GetTick() của frame CAN_MASTER_ID nhận gần nhất.
// Task_Sleep dùng để đếm timeout. Biến thật là `static` trong BMS_CAN.c.
uint32_t BMS_CAN_GetLastRxTick(void);

#endif /* INC_BMS_CAN_H_ */
