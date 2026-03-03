/*
 * ds18b20.h
 *
 *  Created on: Mar 3, 2026
 *      Author: User
 */

#ifndef DS18B20_H_
#define DS18B20_H_

#include "stm32f1xx_hal.h" // Sửa thành f4 hoặc f0 tùy dòng chip của bạn

// ==========================================================
// CẤU HÌNH CHÂN GPIO TẠI ĐÂY (Cho đồ án Murata)
// ==========================================================
#define DS18B20_PORT GPIOA
#define DS18B20_PIN  GPIO_PIN_1

// Timer dùng để delay micro-giây (Đã cấu hình ở Bước 2.1)
extern TIM_HandleTypeDef htim4;
#define DS18B20_TIM  &htim4
// ==========================================================

// Các hàm giao tiếp chính cho Task_Temperature
void DS18B20_Init(void);
void DS18B20_Start_Conversion(void);
float DS18B20_Read_Temperature(void);

#endif /* DS18B20_H_ */
