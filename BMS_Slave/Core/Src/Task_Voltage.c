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
#include "BMS_CAN.h"        // BMS_CAN_GetLastRxTick - dong con tuoi khong
#include "SOC_Kalman.h"     // KF_R0_NOM - MOT nguon cho con so 13.5 mOhm
#include "Debug_Pins.h"
#include "main.h"           // HAL_GetTick

// Dong cu hon nguong nay -> KHONG bu. Cung con so voi CURRENT_TIMEOUT_MS cua
// Task_SOC: master phat 0x100 moi 5 ms nen 100 ms = 20 frame lien tiep bi mat.
#define UVP_CURRENT_TIMEOUT_MS   100U

// Chan do lon phan bu. O chieu xa phan bu LAM TANG v_uvp, nen mot gia tri dong
// RAC co the che mat UVP THAT - do moi la huong nguy hiem. 150 A * 13.5 mOhm
// ~ 2.03 V du cho he 100 A (4 motor x 25 A) ma con thua bien.
#define UVP_MAX_COMP_V           2.03f

void Task_Voltage_Run(void) {
    DBG_SET(DBG_VOLT);          // bat den: task Voltage bat dau chay
    // 1. Gọi Driver để đo (Đã có sẵn lấy mẫu 100 lần và tính hiệu chỉnh)
    float vol = BMS_ADC_GetVoltage();

    /* 2. UVP so voi ap DA BU NOI TRO, khong phai ap tho.
     *
     * THRESHOLD_UNDER_VOLT = 10.50 V la ap LUC NGHI (1.75 V/cell, datasheet).
     * Duoi tai, ap dau cuc sut I*R0 = 1.35 V o 100 A, nen mot binh dang nghi
     * 11.85 V (~17% SOC, con dung duoc) doc ra dung 10.50 V -> NGAT OAN.
     *
     * Chi bu khi dong con TUOI. Dong cu hoac khong co thi bu sai, va bu sai o
     * chieu xa se CHE MAT UVP that; khong bu thi chi trip SOM hon = an toan,
     * nen do la mac dinh khi mat dong.
     *
     * V_RC (phan cuc) KHONG duoc bu vi chi co R0 tuc thoi, nen v_uvp van thap
     * hon OCV that mot chut khi xa keo dai -> tiep tuc nghieng ve an toan.
     *
     * KHONG bu nhiet cho R0: lanh thi R0 that LON hon nen bu it hon thuc te,
     * lai trip som -> an toan, ma code don gian hon. */
    BMS_Snapshot_t snap;
    BMS_Data_GetSnapshot(&snap);

    float v_uvp = vol;          /* mac dinh: ap tho, khong bu */
    if ((HAL_GetTick() - BMS_CAN_GetLastRxTick()) <= UVP_CURRENT_TIMEOUT_MS) {
        float comp = snap.current_a * KF_R0_NOM;     /* I > 0 = xa -> cong lai */
        if (comp >  UVP_MAX_COMP_V) { comp =  UVP_MAX_COMP_V; }
        if (comp < -UVP_MAX_COMP_V) { comp = -UVP_MAX_COMP_V; }
        v_uvp = vol + comp;
    }

    /* `faults` la bien local, bat dau tu 0 moi lan chay, nen khong con can
     * nhanh `else` de xoa bit nhu truoc.
     * OVP dung ap THO: 15.00 V la gioi han cua chinh ap DAU CUC (qua nguong do
     * la hai binh, bat ke sut/nang ap den tu dong nao). */
    uint8_t faults = 0;
    if (v_uvp < THRESHOLD_UNDER_VOLT) { faults |= ERROR_UNDER_VOLT; }
    if (vol   > THRESHOLD_OVER_VOLT)  { faults |= ERROR_OVER_VOLT;  }

    /* 3. Nạp vào kho qua API. Task nay chi so huu 2 bit tren, khong the
     *    cham vao nhiet do / dong dien / SOC cua chu khac.
     *    Bao cao `vol` - ap ĐO THAT - chu khong phai v_uvp: web va master phai
     *    thay so do that, chi RIENG quyet dinh loi moi dung so da bu. */
    BMS_Data_SetVoltage(vol, faults);
    DBG_CLR(DBG_VOLT);          // tat den: task Voltage ket thuc
}
