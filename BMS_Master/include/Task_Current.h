#ifndef TASK_CURRENT_H
#define TASK_CURRENT_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include "Config.h"
#include "System_Data.h"

// SOC gio do TUNG SLAVE tinh bang Kalman roi gui ve trong byte 4 cua frame CAN.
// Class nay chi con MOT viec: do dong dien pack. Bo dem Coulomb cu da bi go -
// hai nguon SOC ghi vao cung mot cho se danh nhau.
//
// Loc TRUNG BINH CAT BIEN, khong phai trung binh thuong. Ly do do duoc tren bench
// (Code/BENCH_INA219): luc rut day SDA, mot lan doc I2C bi hong tra ve -168.21 mV
// = -224 A, va no DI QUA DUOC vi probe() ngay truoc do van OK - probe va read la
// HAI giao dich I2C rieng biet, day bi rut giua hai giao dich do.
//
// Range check khong bat duoc loai rac nay: thanh ghi shunt la 16-bit nen gia tri
// hong nam trong dai +/-327 mV, khong the phan biet bang do lon. Cat bien loai mau
// le ma KHONG can biet vi sao no xau - dung chung cho rac I2C va gai EMI tu 4 bo
// dieu khien BLDC bam 100 A.
class CurrentSensor_Manager {
private:
    Adafruit_INA219 ina;

    static const uint8_t AVG_SAMPLES = 8;   // 8 x TASK_CURRENT_PERIOD_MS = 24 ms
    float    samples[AVG_SAMPLES];
    uint8_t  sampleIndex;
    bool     primed;                        // da du AVG_SAMPLES mau TUOI chua
    uint16_t i2cFailRun;                    // so lan that bai LIEN TIEP
    bool     ready;

    bool  probe(void);                      // chip co ACK dia chi khong
    float trimmedMean(void) const;

public:
    CurrentSensor_Manager();
    void init();
    void loop();
};

void Task_Current_Run(void *pvParameters);

#endif
