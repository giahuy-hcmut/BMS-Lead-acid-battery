#include "Task_Current.h"

CurrentSensor_Manager::CurrentSensor_Manager() : ina(INA219_I2C_ADDR) {
    for (uint8_t i = 0; i < AVG_SAMPLES; i++) { samples[i] = 0.0f; }
    sampleIndex = 0;
    primed      = false;
    i2cFailRun  = 0;
    ready       = false;
}

void CurrentSensor_Manager::init() {
    Wire.begin(PIN_INA_SDA, PIN_INA_SCL);

    /* begin() de PGA o /8 (+/-320 mV). Tren shunt 0.75 mOhm do la +/-427 A, nen
     * nguong trip MAX_DISCHARGE_CURRENT (150 A) voi tay tro duoc va con du 277 A.
     *
     * Dieu nay DA DO tren bench - cap 3.27 V vi sai vao IN+/IN- va so doc kep o
     * dung 320.000 mV voi sd = 0 - chu KHONG lay tu tai lieu thu vien. Mot thang
     * do hep hon (PGA /2 -> 107 A) se lam bao ve qua dong khong bao gio trip duoc,
     * va no that bai am tham. */
    ready = ina.begin(&Wire);
    currentSensorFault = !ready;

    if (!ready) {
        Serial.println("[CURRENT] INA219 NOT FOUND - kiem SDA 21 / SCL 22 / 3V3");
        return;
    }
    Serial.println("[CURRENT] INA219 ready - shunt 0.75 mOhm, range +/-427 A");
}

bool CurrentSensor_Manager::probe(void) {
    /* Thuan Wire API, khong can biet thanh ghi nao. getShuntVoltage_mV() tra ve 0.0
     * CA khi dong that bang 0 LAN khi bus chet, va thu vien khong bao loi gi cua
     * rieng no - day la thu duy nhat phan biet duoc hai truong hop do. */
    Wire.beginTransmission(INA219_I2C_ADDR);
    return (Wire.endTransmission() == 0);
}

float CurrentSensor_Manager::trimmedMean(void) const {
    float sum = 0.0f;
    float lo  = samples[0];
    float hi  = samples[0];

    for (uint8_t i = 0; i < AVG_SAMPLES; i++) {
        sum += samples[i];
        if (samples[i] < lo) { lo = samples[i]; }
        if (samples[i] > hi) { hi = samples[i]; }
    }

    /* Bo 1 min + 1 max -> mot mau rac don le KHONG dich duoc ket qua chut nao.
     * Con lai 6 mau. Trung binh thuong thi mot mau -168 mV trong 8 mau se keo ket
     * qua ve -21 mV = -28 A. */
    return (sum - lo - hi) / (float)(AVG_SAMPLES - 2U);
}

void CurrentSensor_Manager::loop() {
    /* One job: measure the pack current and publish it.
     *
     * What used to be here and is now gone:
     *   - analogRead() on GPIO 32 against a Hall sensor (ACS758) - the advisor
     *     requires a shunt, so the Hall path is deleted, not commented out
     *   - a 50-sample x 10 ms moving average = a 500 ms window, which lagged the
     *     true current by ~250 ms while the slaves' Kalman Predict runs at 200 Hz
     *   - a blocking wait for the first CAN voltage, only there to seed SOC
     *   - an OCV interpolation for the initial SOC
     *   - Coulomb integration
     *   - an unsynchronised globalPacks[0].soc write, outside the mutex that
     *     System_Get_Snapshot() takes to read the very same field
     *
     * Each slave now runs its own Kalman filter and reports SOC in byte 4. */
    while (1) {
        if (!probe()) {
            if (i2cFailRun < 0xFFFFU) { i2cFailRun++; }

            /* Mat cam bien dong = mat luon bao ve qua dong. INA219 khong co chan
             * ALE (INA226 thi co) nen duong mem nay la lop DUY NHAT.
             *
             * Cong bo 0.0 A thay vi bao loi se de Kalman cua ca 5 slave tich dong
             * bang 0 VA de phep so (0 > 150 A) khong bao gio dung: hai that bai,
             * ca hai deu im lang. */
            if (i2cFailRun >= CURRENT_FAULT_LIMIT) {
                currentSensorFault = true;

                /* Vut bo dem: sau khi bus hoi, khong tron mau cu voi mau moi. */
                primed      = false;
                sampleIndex = 0;
            }
        } else {
            if (i2cFailRun >= CURRENT_FAULT_LIMIT) {
                /* Bus hoi sau mot lan mat THAT. Goi lai begin() thay vi tin rang
                 * chip con giu cau hinh - no co the da bi cup nguon. (Gia tri reset
                 * cua INA219 tinh co cung la PGA /8, nhung dua vao do la mot gia
                 * dinh ngam.) */
                ina.begin(&Wire);
            }
            i2cFailRun         = 0;
            currentSensorFault = false;

            samples[sampleIndex] = ina.getShuntVoltage_mV();
            sampleIndex = (uint8_t)((sampleIndex + 1U) % AVG_SAMPLES);
            if (sampleIndex == 0U) { primed = true; }

            /* Cho du AVG_SAMPLES mau moi cong bo: cat bien tren mot bo dem con so 0
             * khoi tao se ra mot con so sai. */
            if (primed) {
                float mv = trimmedMean();

                /* Nhan (A/mV), khong chia - cung ly do Task_CAN giai ma bang *0.01f
                 * thay vi /100. */
                float currentA = (mv - CURRENT_ZERO_MV) * CURRENT_SIGN
                                 * (SHUNT_FULL_A / SHUNT_FULL_MV);

                if (fabsf(currentA) < CURRENT_IDLE_BAND) { currentA = 0.0f; }

                System_Update_Current(currentA);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(TASK_CURRENT_PERIOD_MS));
    }
}

// --- FREE RTOS WRAPPER ---
void Task_Current_Run(void *pvParameters) {
    CurrentSensor_Manager mySensor;
    mySensor.init();
    mySensor.loop();
}
