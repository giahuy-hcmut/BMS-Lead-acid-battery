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
    float   current_a;      // dong dien (A), >0 = XA
    float   temp_c;         // nhiet do (C)
    uint8_t soc_pct;        // SOC (%)      - 0 cho toi GD5
    uint8_t faults;         // OR cua co loi cua moi chu
} BMS_Snapshot_t;

// --- Producers: exactly one caller each ---
void BMS_Data_SetVoltage(float voltage_v, uint8_t fault_bits);   // Task_Voltage
void BMS_Data_SetTemp(float temp_c, uint8_t fault_bits);         // Task_Temperature

// Dong dien tu frame CAN_MASTER_ID, DA doi ve don vi ampe (>0 = xa).
// Nguoi goi tu scale, kho khong tinh toan gi.
// Goi tu ISR CAN RX (BMS_CAN.c).
void BMS_Data_SetCurrent(float current_a);                       // CAN RX ISR

// SOC da tinh xong, 0..100. Goi tu Task_SOC.
void BMS_Data_SetSoc(uint8_t soc_pct);                           // Task_SOC

// --- Consumer ---
void BMS_Data_GetSnapshot(BMS_Snapshot_t *out);

// Tra 1 VA XOA co neu co mau dien ap MOI ke tu lan goi truoc, nguoc lai tra 0.
// Co duoc bat tu dong ben trong BMS_Data_SetVoltage().
//
// Kalman chi duoc phep Update bang ap TUOI: hieu chinh bang mau ap cu tung la
// mot loi thiet ke that (xem PROJECT_BRIEF muc 2). Ap ve moi 50 ms con Predict
// chay moi 5 ms, nen 10 lan Predict moi co 1 lan Update.
//
// Goi tu Task_SOC.
uint8_t BMS_Data_TakeVoltageFresh(void);                         // Task_SOC

#endif /* SHARED_DATA_H */
