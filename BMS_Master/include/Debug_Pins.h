/*
 * Debug_Pins.h
 *
 *  Timing-debug pins for logic-analyzer verification of FreeRTOS scheduling
 *  on the master (ESP32). Each PERIODIC task drives one GPIO HIGH while its
 *  body runs; the analyzer then reads period, execution time, preemption
 *  nesting and deadline overrun straight off the waveform.
 *
 *  Unlike the slave (cooperative scheduler, pulses can NEVER nest), the master
 *  is PREEMPTIVE: a high-priority pulse WILL appear inside a low-priority one.
 *  That nesting is the point - it is how preemption becomes visible - but it
 *  also means a pulse width is "execution + time stolen", not execution alone.
 *
 *  DBG_WEB is the odd one out: Task_WebServer lives on core 0, so its pulse
 *  overlaps the core-1 pulses WITHOUT that being preemption. It also uses
 *  vTaskDelay (not vTaskDelayUntil), so its period is "500 ms + run time" and
 *  drifts by design; an overrun there shows as a LONGER gap, never as two
 *  pulses back to back.
 *
 *  Pin choice - everything else is taken or unusable:
 *      21/22 I2C . 23 relay . 25/26/27/33 SPI . 32/35 TWAI . 14 MCP2515 INT
 *      1/3   UART0 (Serial Monitor - never touch)
 *      0/2/5/12/15 strapping . 6-11 flash . 34/36/39 input-only
 *  -> 4, 13, 18, 19 are the only four that are always safe. 18/19 are the old
 *     VSPI pins, freed when the PCB pinout changed (commit 4989d34).
 *     16/17 would give two more, but they are wired to PSRAM on WROVER
 *     modules; check ESP.getPsramSize() == 0 before considering them.
 *
 *  Logic-analyzer channel map:
 *      D0  GPIO 4    Task_Current      3 ms    prio 4   core 1
 *      D1  GPIO 13   Task_WebServer  500 ms    prio 2   core 0
 *      D2  GPIO 18   Task_Logic       10 ms    prio 4   core 1
 *      D3  GPIO 19   Task_VehicleCAN 100 ms    prio 3   core 1
 *      GND           ESP32 GND                 MANDATORY
 *
 *  Comment out DBG_TIMING for a production build: every macro compiles to
 *  nothing and the four pins are released.
 */
#ifndef DEBUG_PINS_H
#define DEBUG_PINS_H

#include <Arduino.h>
#include "Config.h"     /* FAULT_INJECT_TEST - it claims GPIO 4 */

/* ---- master switch: comment out for a production build ---- */
#define DBG_TIMING

#ifdef DBG_TIMING

#include "soc/gpio_reg.h"   /* GPIO_OUT_W1TS_REG / GPIO_OUT_W1TC_REG, REG_WRITE */

/* GPIO 4 doubles as the fault-injection INPUT in a decision-latency build, so
 * it must not be driven as an output then. Task_Current timing is not part of
 * that measurement, so this one channel simply disappears; DBG_PIN_OFF makes
 * every DBG_SET/DBG_CLR on it compile to nothing. */
#define DBG_PIN_OFF  0xFFU
#ifdef FAULT_INJECT_TEST
  #define DBG_CURRENT  DBG_PIN_OFF
#else
  #define DBG_CURRENT    4U   /* D0 - Task_Current       3 ms */
#endif
#define DBG_WEB       13U   /* D1 - Task_WebServer   500 ms, core 0 */
#define DBG_LOGIC     18U   /* D2 - Task_Logic        10 ms */
#define DBG_VCAN      19U   /* D3 - Task_VehicleCAN  100 ms */

/* Direct register write, ~10 ns. digitalWrite() costs ~1-2 us because it goes
 * through the pin-mux table - that is 4% error on a task body running 50 us,
 * and the overhead lands INSIDE the pulse being measured.
 * REG_WRITE (not the GPIO struct) because the struct field changed shape
 * between ESP-IDF 4.x and 5.x; the register macro is stable across both.
 * All four pins are below 32, so the single OUT_W1TS/W1TC register covers them
 * and no GPIO_OUT1_* variant is needed. */
#define DBG_SET(p)  do { if ((p) != DBG_PIN_OFF) { REG_WRITE(GPIO_OUT_W1TS_REG, (1UL << (p))); } } while (0)
#define DBG_CLR(p)  do { if ((p) != DBG_PIN_OFF) { REG_WRITE(GPIO_OUT_W1TC_REG, (1UL << (p))); } } while (0)

/* Call once from setup(), BEFORE any task is created. */
static inline void DBG_Pins_Init(void)
{
    const uint8_t pins[4] = { DBG_CURRENT, DBG_WEB, DBG_LOGIC, DBG_VCAN };
    for (uint8_t i = 0U; i < 4U; i++) {
        if (pins[i] == DBG_PIN_OFF) { continue; }
        pinMode(pins[i], OUTPUT);
        digitalWrite(pins[i], LOW);
    }
}

#else  /* !DBG_TIMING - compile everything away */

#define DBG_SET(p)       ((void)0)
#define DBG_CLR(p)       ((void)0)
#define DBG_Pins_Init()  ((void)0)

#endif /* DBG_TIMING */

#endif /* DEBUG_PINS_H */
