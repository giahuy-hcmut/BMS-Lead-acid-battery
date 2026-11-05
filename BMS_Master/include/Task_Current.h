/**
 * @file      Task_Current.h
 * @author    Do Dang Gia Huy
 * @brief     Pack current measurement through an INA219 and a 0.75 mOhm shunt.
 * @date      2026-02-10
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 *
 * SOC is now measured PER SLAVE by a Kalman filter and arrives in byte 4 of
 * each CAN frame, so this class has exactly one job left: measure the pack
 * current. The old Coulomb counter was removed rather than disabled - two
 * sources writing the same SOC field would have fought each other.
 *
 * The filter is a TRIMMED mean, not a plain average, and the reason was
 * measured on the bench (Code/BENCH_INA219): while the SDA wire was pulled,
 * one I2C read returned -168.21 mV = -224 A, and it got THROUGH because the
 * probe() immediately before it still succeeded - probe and read are two
 * separate I2C transactions and the wire came out between them.
 *
 * A range check cannot catch that kind of garbage: the shunt register is
 * 16-bit, so a corrupt value still lands inside +/-327 mV and is
 * indistinguishable by magnitude. Trimming discards the outlier WITHOUT
 * having to know why it is bad, which covers both I2C corruption and EMI
 * spikes from four BLDC controllers switching 100 A.
 */

#ifndef TASK_CURRENT_H_
#define TASK_CURRENT_H_

/**********************
 *      INCLUDES
 *********************/

#include <Arduino.h>
#include <Wire.h>

#include <Adafruit_INA219.h>

#include "Config.h"
#include "System_Data.h"

#include <array>

/**********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/** Owns the INA219, its filter window and its loss detection. */
class CurrentSensorManager
{
public:
    /**
     * @brief  Bind the driver to the configured I2C address and clear the
     *         filter window; touches no hardware.
     */
    CurrentSensorManager();

    /**
     * @brief  Start the I2C bus and the INA219.
     * @note   Clears currentSensorFault on success. The flag starts TRUE, so
     *         a sensor that never answers keeps the system in its fail-safe
     *         state.
     * @return None.
     */
    void init();

    /**
     * @brief  Sample, filter and publish the pack current forever.
     * @note   Never returns; runs on a kTaskCurrentPeriodMs cadence.
     * @return None.
     */
    void loop();

private:
    /** Samples averaged per published value; 8 x 3 ms = a 24 ms window. */
    static constexpr uint8_t kAvgSamples = 8U;

    /**
     * @brief  Does the chip still acknowledge its address?
     * @note   Pure Wire API, so it needs to know no register. The library's
     *         getShuntVoltage_mV() returns 0.0 BOTH when the current really
     *         is zero and when the bus is dead, and reports no error of its
     *         own - this is the only thing that tells those apart.
     * @return true when the address was acknowledged.
     */
    bool probe(void);

    /**
     * @brief  Mean of the window with the single lowest and highest sample
     *         discarded.
     * @note   Dropping one min and one max means a lone corrupt sample
     *         cannot move the result at all. A plain average of 8 samples
     *         including one at -168 mV would have pulled the result to
     *         -21 mV = -28 A.
     * @return Filtered shunt voltage, mV.
     */
    float trimmedMean(void) const;

    Adafruit_INA219 ina_;
    std::array<float, kAvgSamples> samples_;
    uint8_t  sampleIndex_;
    bool     primed_;           /**< window holds kAvgSamples fresh samples */
    uint16_t i2cFailRun_;       /**< CONSECUTIVE failed transactions        */
    bool     ready_;
};

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * @brief  FreeRTOS entry point for the current sensor.
 * @param[in] pvParameters  Unused, required by the FreeRTOS signature.
 * @return None.
 */
void Task_Current_Run(void *pvParameters);

/**********************
 *      MACROS
 **********************/

#endif /*TASK_CURRENT_H_*/
