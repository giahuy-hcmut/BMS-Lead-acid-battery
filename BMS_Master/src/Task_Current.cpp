/**
 * @file      Task_Current.cpp
 * @author    Do Dang Gia Huy
 * @brief     INA219 sampling, trimmed-mean filtering and sensor-loss handling.
 * @date      2026-02-10
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 */

/**********************
 *      INCLUDES
 **********************/

#include "Task_Current.h"

/**********************
 *      DEFINES
 **********************/

/** Ceiling for the consecutive-failure counter, so it cannot wrap back to a
 * value below kCurrentFaultLimit and silently clear the fault. */
constexpr uint16_t kFailRunMax = 0xFFFFU;

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief  Bind the driver to the configured address and clear the window.
 */
CurrentSensorManager::CurrentSensorManager()
    : ina_(kIna219I2cAddr),
      samples_{},
      sampleIndex_(0U),
      primed_(false),
      i2cFailRun_(0U),
      ready_(false)
{
}

/**
 * @brief  Start the I2C bus and the INA219.
 * @return None.
 */
void CurrentSensorManager::init()
{
    Wire.begin(kPinInaSda, kPinInaScl);

    /* begin() leaves the PGA at /8, which is +/-320 mV. Across the
     * 0.75 mOhm shunt that is +/-427 A, so the kMaxDischargeCurrentA trip
     * at 150 A has plenty of headroom and 277 A to spare.
     *
     * This was MEASURED on the bench - 3.27 V differential into IN+/IN- read
     * back as exactly 320.000 mV with zero standard deviation - rather than
     * taken from the library documentation. A narrower range (PGA /2 gives
     * 107 A) would make the over-current protection unable to trip at all,
     * and it would fail silently. */
    ready_             = ina_.begin(&Wire);
    currentSensorFault = !ready_;

    if (!ready_)
    {
        Serial.println("[CURRENT] INA219 not found - check SDA 21 / SCL 22 / 3V3");
        return;
    }

    Serial.println("[CURRENT] INA219 ready - shunt 0.75 mOhm, range +/-427 A");
}

/**
 * @brief  Sample, filter and publish the pack current forever.
 * @return None.
 */
void CurrentSensorManager::loop()
{
    /* One job: measure the pack current and publish it.
     *
     * What used to be here and is now gone:
     *   - analogRead() on GPIO 32 against a Hall sensor (ACS758) - the
     *     advisor requires a shunt, so the Hall path is deleted, not
     *     commented out
     *   - a 50-sample x 10 ms moving average = a 500 ms window, which lagged
     *     the true current by about 250 ms while the slaves' Kalman Predict
     *     runs at 200 Hz
     *   - a blocking wait for the first CAN voltage, only there to seed SOC
     *   - an OCV interpolation for the initial SOC
     *   - Coulomb integration
     *   - an unsynchronised globalPacks[0].soc write, outside the mutex that
     *     System_Get_Snapshot() takes to read the very same field
     *
     * Each slave now runs its own Kalman filter and reports SOC in byte 4. */
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (1)
    {
        if (!probe())
        {
            if (i2cFailRun_ < kFailRunMax)
            {
                i2cFailRun_++;
            }

            /* Losing the current sensor means losing the over-current
             * protection with it. The INA219 has no ALE pin (the INA226
             * does), so this software path is the ONLY layer.
             *
             * Publishing 0.0 A instead of raising a fault would make all
             * five slaves integrate zero current AND make the test
             * (0 > 150 A) never true: two failures, both silent. */
            if (i2cFailRun_ >= kCurrentFaultLimit)
            {
                currentSensorFault = true;

                /* Drop the window: after the bus recovers, do not mix stale
                 * samples with fresh ones. */
                primed_      = false;
                sampleIndex_ = 0U;
            }
        }
        else
        {
            if (i2cFailRun_ >= kCurrentFaultLimit)
            {
                /* The bus recovered from a REAL loss. Call begin() again
                 * rather than trusting the chip to have kept its
                 * configuration - it may have been power cycled. The INA219
                 * reset default happens to be PGA /8 as well, but relying on
                 * that would be an unstated assumption. */
                ina_.begin(&Wire);
            }

            i2cFailRun_        = 0U;
            currentSensorFault = false;

            samples_[sampleIndex_] = ina_.getShuntVoltage_mV();
            sampleIndex_ = (uint8_t)((sampleIndex_ + 1U) % kAvgSamples);

            if (sampleIndex_ == 0U)
            {
                primed_ = true;
            }

            /* Publish only once kAvgSamples fresh samples exist: trimming a
             * window still holding initialisation zeros yields a wrong
             * number. */
            if (primed_)
            {
                float mv = trimmedMean();

                /* Multiply by (A/mV) rather than divide, for the same reason
                 * Task_CAN decodes with *0.01f instead of /100. */
                float currentA = (mv - kCurrentZeroMv) * kCurrentSign *
                                 (kShuntFullA / kShuntFullMv);

                if (fabsf(currentA) < kCurrentIdleBandA)
                {
                    currentA = 0.0f;
                }

                System_Update_Current(currentA);
            }
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(kTaskCurrentPeriodMs));
    }
}

/**
 * @brief  FreeRTOS entry point for the current sensor.
 * @param[in] pvParameters  Unused.
 * @return None.
 */
void Task_Current_Run(void *pvParameters)
{
    CurrentSensorManager mySensor;

    (void)pvParameters;

    mySensor.init();
    mySensor.loop();
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * @brief  Does the chip still acknowledge its address?
 * @return true when the address was acknowledged.
 */
bool CurrentSensorManager::probe(void)
{
    Wire.beginTransmission(kIna219I2cAddr);
    return (Wire.endTransmission() == 0);
}

/**
 * @brief  Mean of the window with the lowest and highest sample discarded.
 * @return Filtered shunt voltage, mV.
 */
float CurrentSensorManager::trimmedMean(void) const
{
    float sum = 0.0f;
    float lo  = samples_[0];
    float hi  = samples_[0];

    for (uint8_t i = 0U; i < kAvgSamples; i++)
    {
        sum += samples_[i];

        if (samples_[i] < lo)
        {
            lo = samples_[i];
        }

        if (samples_[i] > hi)
        {
            hi = samples_[i];
        }
    }

    return (sum - lo - hi) / (float)(kAvgSamples - 2U);
}
