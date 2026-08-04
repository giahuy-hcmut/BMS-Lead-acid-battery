/*
 * Shared_Data.h
 *
 *  Created on: Feb 10, 2026
 *      Author: User
 *
 *  NOTE: despite the historical file name, this is NOT a public global
 *  store any more. The measured state lives in Shared_Data.c as `static`
 *  variables that no other file can name.
 *      write it with the BMS_Data_Set*() functions (one owner each)
 *      read  it with BMS_Data_GetSnapshot()
 *
 *  Why no mutex: the slave runs a COOPERATIVE scheduler
 *  (run-to-completion), so two tasks can never overlap and task-vs-task
 *  races cannot exist. A mutex would in fact deadlock here - there is no
 *  context switch to let the holder finish. The only real hazard is
 *  task-vs-ISR, which GetSnapshot() closes with a short PRIMASK section.
 */


#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <stdint.h>
#include "Board_Config.h"   // MIGRATION SHIM: SLAVE_INDEX / CAN ids /
                            // THRESHOLD_* moved to Board_Config.h (step 1)

// Định nghĩa các cờ lỗi (Bitmask)
#define ERROR_NONE          0x00
#define ERROR_OVER_VOLT     0x01
#define ERROR_UNDER_VOLT    0x02
#define ERROR_OVER_TEMP     0x04

// ==========================================
// KHO DU LIEU (DATA STORE) - giao dien moi
// ==========================================

// Snapshot returned BY VALUE: the caller gets a private copy and never
// holds a pointer into the real store. It also guarantees that every
// field comes from the same instant.
typedef struct
{
    float   voltage_v;      // dien ap binh (V)
    float   current_a;      // dong dien (A) - 0.0 cho toi GD5
    float   temp_c;         // nhiet do (C)
    uint8_t soc_pct;        // SOC (%)      - 0 cho toi GD5
    uint8_t faults;         // OR cua co loi cua moi chu
} BMS_Snapshot_t;

// --- Producers: exactly one caller each ---
void BMS_Data_SetVoltage(float voltage_v, uint8_t fault_bits);   // Task_Voltage
void BMS_Data_SetTemp(float temp_c, uint8_t fault_bits);         // Task_Temperature

// --- Consumer ---
void BMS_Data_GetSnapshot(BMS_Snapshot_t *out);

// ==========================================
// LEGACY - duoc thay the dan o Buoc 5, xoa o Buoc 7
// ==========================================
typedef struct {
    float voltage;      // Điện áp (V)
    float current;      // Dòng điện (A) - Nếu có
    float temp;         // Nhiệt độ (doC)
    uint8_t soc;        // Dung lượng (%)
    uint8_t status;     // Trạng thái lỗi
} BMS_State_t;

// Khai báo biến extern để các file khác dùng chung
extern BMS_State_t myBMS;
extern volatile uint32_t lastHeartbeatTick;



#endif /* SHARED_DATA_H */
