/*
 * Shared_Data.c
 *
 *  Created on: Feb 10, 2026
 *      Author: User
 */


#include "Shared_Data.h"

// Khởi tạo biến thực sự tại đây
BMS_State_t myBMS = {0};
volatile uint32_t lastHeartbeatTick = 0;
