/**
 * @file    main.cpp
 * @brief   Bench rig for the INA219 + external shunt pack-current sensor.
 *
 * Runs on a SPARE ESP32, not the master. Purpose is to answer four questions
 * before this code is allowed near a 60 V pack:
 *
 *   1. Is the INA219 alive?          -> I2C scan must find the address
 *   2. Is the measuring chain right? -> a KNOWN current must read the
 *                                       PREDICTED millivolts
 *   3. What is the zero offset?      -> long average with no current flowing
 *   4. Which way is positive?        -> discharge must come out POSITIVE
 *
 * Answers 3 and 4 become CURRENT_ZERO_MV and CURRENT_SIGN in Config.h. The
 * conversion maths and the I2C health check below are the same code that moves
 * into Task_Current, so integration is a copy rather than a rewrite.
 *
 * WHY "reads non-zero" is NOT a success criterion: getShuntVoltage_mV() returns
 * 0.0 both when the current is genuinely zero AND when the I2C bus is dead. The
 * bus probe is what tells those two apart, so every line printed below carries
 * an explicit I2C verdict.
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA219.h>

/* ============================ WIRING ============================ */
#define PIN_INA_SDA             21
#define PIN_INA_SCL             22
#define INA219_I2C_ADDR         0x40    /* default; A0/A1 pads unbridged */

/* ====================== SHUNT + CONVERSION ======================
 * Keep these identical to Config.h section 6 when this moves into the master.
 *
 * External shunt 100 A / 75 mV = 0.75 mOhm. The module's own R100 (0.1 Ohm)
 * must be DESOLDERED before the external shunt is wired in - left in place it
 * sits in parallel with the sense leads and drags 0.75 A through wires meant
 * for microvolts.
 *
 * PGA stays at the library default /8 (+/-320 mV, set by begin() via
 * setCalibration_32V_2A). That is 427 A full scale on this shunt, so the ~100 A
 * peak and the 150 A trip threshold both fit with room to spare. /2 would clip
 * at 107 A - below the trip point - and /4 needs a raw register write. The
 * offset error does NOT scale with PGA, so the wide range costs only
 * quantisation (107 mA), which is far smaller than the +/-133 mA offset.
 */
#define SHUNT_FULL_A            100.0f
#define SHUNT_FULL_MV           75.0f

/* Measured in the zero run (send 'z', leave it idle, read back "mean"). */
#define CURRENT_ZERO_MV         0.000f

/* Flip to -1.0f if a real discharge reads negative. Fix the SIGN HERE, never by
 * swapping the sense wires - the wires are the thing you already verified. */
#define CURRENT_SIGN            (+1.0f)

/* Consecutive I2C failures before the sensor is declared lost. At
 * SAMPLE_PERIOD_MS this is ~60 ms of silence, long enough to ride out a single
 * glitch and short enough to beat any plausible over-current event. */
#define CURRENT_FAULT_LIMIT     20

#define SAMPLE_PERIOD_MS        3
#define PRINT_PERIOD_MS         250

/* Software moving average. 8 samples x 3 ms = 24 ms window.
 * The master currently uses 50 x 10 ms = a 500 ms window, which lags the true
 * current by ~250 ms - the slaves' Kalman Predict runs at 200 Hz and is being
 * fed data a quarter of a second stale. This window is the replacement. */
#define AVG_SAMPLES             8

/* ============================= STATE ============================= */
static Adafruit_INA219 ina(INA219_I2C_ADDR);

static bool     inaReady        = false;
static uint16_t i2cFailRun      = 0;        /* CONSECUTIVE failures */
static uint32_t i2cFailTotal    = 0;
static bool     sensorFault     = false;

static float    avgBuf[AVG_SAMPLES];
static uint8_t  avgIdx          = 0;
static float    avgSum          = 0.0f;
static bool     avgPrimed       = false;

/* Welford accumulator - numerically stable mean/stddev over an unbounded run,
 * which is what the zero calibration needs (minutes of samples). */
static uint32_t statN           = 0;
static float    statMean        = 0.0f;
static float    statM2          = 0.0f;
static float    statMin         = 0.0f;
static float    statMax         = 0.0f;

/* ========================= HELPERS ============================== */

/**
 * @brief  Ask the INA219 to acknowledge its address.
 * @return true if the device ACKed.
 *
 * Pure Wire API - no register knowledge needed. This is the only way to tell
 * "0 A" from "bus dead", because the library reports no error of its own.
 */
static bool ina_probe(void)
{
    Wire.beginTransmission(INA219_I2C_ADDR);
    return (Wire.endTransmission() == 0);
}

/**
 * @brief  Read one 16-bit INA219 register.
 *
 * DIAGNOSTIC ONLY, and only in this throwaway bench sketch. Task_Current in the
 * master configures nothing and reads nothing by register - it calls begin() and
 * getShuntVoltage_mV(), that is all. This exists because the full-scale range is
 * a safety-critical number (it decides at how many amps the reading clips) and
 * asking the chip is definitive, where inferring it from a zero reading is not.
 */
static bool ina_read_reg(uint8_t reg, uint16_t *out)
{
    Wire.beginTransmission(INA219_I2C_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission() != 0)                                 { return false; }
    if (Wire.requestFrom((uint8_t)INA219_I2C_ADDR, (uint8_t)2) != 2)  { return false; }

    uint8_t hi = (uint8_t)Wire.read();
    uint8_t lo = (uint8_t)Wire.read();
    *out = (uint16_t)(((uint16_t)hi << 8) | (uint16_t)lo);
    return true;
}

/**
 * @brief  Print what the chip is actually configured to do, in plain words.
 */
static void ina_report_config(void)
{
    /* PGA (config bits 12-11) -> full-scale shunt range. */
    static const char * const PGA_NAME[4]  = { "/1", "/2", "/4", "/8" };
    static const float        PGA_FS_MV[4] = { 40.0f, 80.0f, 160.0f, 320.0f };

    /* SADC (config bits 6-3) -> shunt conversion time. Codes 0..3 are plain
     * resolutions; 9..15 are 12-bit with hardware averaging. */
    static const char * const SADC_NAME[16] = {
        "9-bit 84us",    "10-bit 148us",  "11-bit 276us",  "12-bit 532us",
        "12-bit 532us",  "12-bit 532us",  "12-bit 532us",  "12-bit 532us",
        "12-bit 532us",  "2 avg 1.06ms",  "4 avg 2.13ms",  "8 avg 4.26ms",
        "16 avg 8.51ms", "32 avg 17.0ms", "64 avg 34.1ms", "128 avg 68.1ms"
    };

    static const char * const MODE_NAME[8] = {
        "power-down", "shunt triggered",  "bus triggered",  "shunt+bus triggered",
        "ADC off",    "SHUNT continuous", "bus continuous", "shunt+bus continuous"
    };

    uint16_t cfg = 0;

    if (!ina_read_reg(0x00, &cfg))
    {
        Serial.println("config read : FAILED (I2C)");
        return;
    }

    uint8_t pga  = (uint8_t)((cfg >> 11) & 0x03U);
    uint8_t sadc = (uint8_t)((cfg >>  3) & 0x0FU);
    uint8_t mode = (uint8_t)( cfg        & 0x07U);

    float fsMv = PGA_FS_MV[pga];
    float fsA  = fsMv * (SHUNT_FULL_A / SHUNT_FULL_MV);

    Serial.println();
    Serial.println("--- WHAT THE CHIP IS ACTUALLY SET TO ---");
    Serial.printf("  PGA         : %s  -> full scale +/-%.0f mV\n",
                  PGA_NAME[pga], fsMv);
    Serial.printf("  MAX CURRENT : +/-%.0f A   (shunt %.3f mOhm)\n",
                  fsA, SHUNT_FULL_MV / SHUNT_FULL_A);
    Serial.printf("  Shunt ADC   : %s\n", SADC_NAME[sadc]);
    Serial.printf("  Mode        : %s\n", MODE_NAME[mode]);

    /* The whole reason this function exists. 150 A is MAX_DISCHARGE_CURRENT in
     * the master's Config.h - a range that cannot reach it means the over-current
     * protection can never trip, no matter what the firmware does. */
    if (fsA < 150.0f)
    {
        Serial.println();
        Serial.println("  *** PROBLEM: range is BELOW the 150 A trip threshold.");
        Serial.println("  *** Over-current protection could never fire. Widen the");
        Serial.println("  *** PGA before this goes anywhere near the pack.");
    }
    else
    {
        Serial.printf("  VERDICT     : OK - covers the 150 A trip point with"
                      " %.0f A to spare\n", fsA - 150.0f);
    }
    Serial.println();
}

static void stats_reset(void)
{
    statN    = 0;
    statMean = 0.0f;
    statM2   = 0.0f;
    statMin  = 0.0f;
    statMax  = 0.0f;
}

static void stats_push(float x)
{
    float delta;

    if (statN == 0)
    {
        statMin = x;
        statMax = x;
    }
    else
    {
        if (x < statMin) { statMin = x; }
        if (x > statMax) { statMax = x; }
    }

    statN++;
    delta     = x - statMean;
    statMean += delta / (float)statN;
    statM2   += delta * (x - statMean);
}

static float stats_sd(void)
{
    if (statN < 2U) { return 0.0f; }
    return sqrtf(statM2 / (float)(statN - 1U));
}

/**
 * @brief  Convert shunt millivolts to amperes.
 *
 * The ONE line that has to be right. Scaling by (A/mV) is a multiply, not a
 * divide - the same reason the slave's CAN decode uses *0.01f instead of /100.
 */
static float mv_to_amps(float mv)
{
    return (mv - CURRENT_ZERO_MV) * CURRENT_SIGN * (SHUNT_FULL_A / SHUNT_FULL_MV);
}

static void i2c_scan(void)
{
    uint8_t found = 0;

    Serial.println("I2C scan:");
    for (uint8_t addr = 0x08; addr < 0x78; addr++)
    {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0)
        {
            Serial.printf("   0x%02X  <-- device%s\n", addr,
                          (addr == INA219_I2C_ADDR) ? "  [THIS IS THE INA219]" : "");
            found++;
        }
    }

    if (found == 0)
    {
        Serial.println("   NOTHING FOUND.");
        Serial.println("   -> check VCC=3.3V, GND, SDA=21, SCL=22, and that the");
        Serial.println("      module's own pull-ups are present.");
    }
}

/* =========================== SETUP ============================== */
void setup(void)
{
    Serial.begin(115200);
    delay(300);

    Serial.println();
    Serial.println("=============================================");
    Serial.println(" INA219 + EXTERNAL SHUNT - BENCH RIG");
    Serial.println("=============================================");

    Wire.begin(PIN_INA_SDA, PIN_INA_SCL);
    i2c_scan();

    inaReady = ina.begin(&Wire);
    Serial.printf("\nina.begin()      : %s\n", inaReady ? "OK" : "FAILED");

    if (!inaReady)
    {
        Serial.println("\nSTOP. Fix the wiring before going further.");
        Serial.println("Reading 0.00 mV with a dead bus looks exactly like 0 A.");
    }

    Serial.printf("Shunt            : %.1f A / %.1f mV = %.3f mOhm\n",
                  SHUNT_FULL_A, SHUNT_FULL_MV,
                  (SHUNT_FULL_MV / SHUNT_FULL_A));

    /* Read the range back instead of assuming it. begin() is DOCUMENTED to set
     * PGA /8, but a range that silently turned out narrower would clip the
     * current during hard acceleration and quietly disable over-current
     * protection - too important to take on documentation alone. */
    ina_report_config();
    Serial.printf("Zero offset      : %.3f mV\n", CURRENT_ZERO_MV);
    Serial.printf("Sign             : %+.0f\n", CURRENT_SIGN);

    Serial.println();
    Serial.println("--- KNOWN-CURRENT TEST (do this BEFORE desoldering R100) ---");
    Serial.println("  3.3V --[ R ]-- IN+ --(R100 0.1ohm)-- IN- -- GND");
    Serial.println("  expected reading in mV = 330 / R(ohm)");
    Serial.println("    R = 100 ohm 1/4W  ->  33 mA  ->  expect 3.30 mV");
    Serial.println("    R =  47 ohm 1/2W  ->  70 mA  ->  expect 7.02 mV");
    Serial.println();
    Serial.println("  NEVER put 3.3 V straight across IN+/IN-: that is 3.3 V");
    Serial.println("  across 0.1 ohm = a short circuit, and 10x beyond the");
    Serial.println("  +/-320 mV measuring range anyway.");
    Serial.println();
    Serial.println("--- SERIAL COMMANDS ---");
    Serial.println("  z = reset the mean/stddev accumulator");
    Serial.println("      (zero run: no current, send 'z', wait 60 s, read mean)");
    Serial.println("  c = re-print the chip configuration above");
    Serial.println();

    for (uint8_t i = 0; i < AVG_SAMPLES; i++) { avgBuf[i] = 0.0f; }
    stats_reset();
}

/* ============================ LOOP ============================== */
void loop(void)
{
    static uint32_t lastSample = 0;
    static uint32_t lastPrint  = 0;
    static float    lastMv     = 0.0f;

    uint32_t now = millis();

    if (Serial.available() > 0)
    {
        int c = Serial.read();
        if ((c == 'z') || (c == 'Z'))
        {
            stats_reset();
            Serial.println(">>> stats reset");
        }
        else if ((c == 'c') || (c == 'C'))
        {
            ina_report_config();
        }
    }

    /* ---------------- sample ---------------- */
    if ((now - lastSample) >= SAMPLE_PERIOD_MS)
    {
        lastSample = now;

        if (!ina_probe())
        {
            i2cFailTotal++;
            if (i2cFailRun < 0xFFFFU) { i2cFailRun++; }

            /* Latch the fault. In the master this is what raises systemLocked:
             * losing the current sensor means losing over-current protection
             * entirely (no ALE pin on the INA219), so the vehicle must not keep
             * running on a reading of 0 A. */
            if (i2cFailRun >= CURRENT_FAULT_LIMIT) { sensorFault = true; }
        }
        else
        {
            i2cFailRun  = 0;
            sensorFault = false;

            lastMv = ina.getShuntVoltage_mV();

            avgSum -= avgBuf[avgIdx];
            avgBuf[avgIdx] = lastMv;
            avgSum += lastMv;
            avgIdx = (uint8_t)((avgIdx + 1U) % AVG_SAMPLES);
            if (avgIdx == 0U) { avgPrimed = true; }

            stats_push(lastMv);
        }
    }

    /* ---------------- report ---------------- */
    if ((now - lastPrint) >= PRINT_PERIOD_MS)
    {
        lastPrint = now;

        float avgMv = avgSum / (float)AVG_SAMPLES;

        Serial.printf("[%6.1fs] I2C:%-5s", (float)now / 1000.0f,
                      sensorFault ? "LOST" : (i2cFailRun ? "glit" : "ok"));

        if (sensorFault)
        {
            /* Deliberately NOT printing a current here. A number next to a
             * dead bus is the exact failure this rig exists to make visible. */
            Serial.printf("  *** SENSOR LOST - no reading is trustworthy ***"
                          "  (fails:%lu)\n", (unsigned long)i2cFailTotal);
        }
        else
        {
            Serial.printf("  Vsh:%8.3f mV  I:%9.3f A  |  avg:%8.3f mV"
                          "  mean:%8.4f  sd:%6.4f  min:%7.3f  max:%7.3f  n:%lu%s\n",
                          lastMv, mv_to_amps(avgMv), avgMv,
                          statMean, stats_sd(), statMin, statMax,
                          (unsigned long)statN,
                          avgPrimed ? "" : "  (avg filling)");
        }
    }
}
