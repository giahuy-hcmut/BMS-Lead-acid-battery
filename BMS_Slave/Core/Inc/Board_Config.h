/*
 * Board_Config.h
 *
 *  Created on: Aug 4, 2026
 *      Author: User
 *
 *  Per-board identity and protection limits for ONE slave node.
 *  This is the only header that differs between the five slaves.
 */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

// ==========================================
// NHAN DIEN SLAVE (SLAVE IDENTITY)
// ==========================================
// The ONLY line to edit when flashing a different board.
#define SLAVE_INDEX             1U      // 0..4, one slave per battery

#if (SLAVE_INDEX > 4U)
    #error "SLAVE_INDEX must be 0..4"
#endif

// ==========================================
// DIA CHI CAN (derived from SLAVE_INDEX - do not edit)
// ==========================================
#define CAN_BASE_ID             0x103U                          // id of slave 0
#define CAN_SLAVE_ID            (CAN_BASE_ID + SLAVE_INDEX)     // this node's TX id
#define CAN_MASTER_ID           0x100U                          // master -> slaves

// ==========================================
// NGUONG BAO VE (PROTECTION LIMITS)
// ==========================================
// Generic 12 V lead-acid AGM limits, deliberately NOT tied to one part number
// since the pack is expected to change. They match the common market band, and
// also the CSB EVX12200 datasheet currently on the bench (rev RA240531):
//
//   cycle charge voltage   14.4 - 14.8 V typical  (CSB: 14.4 - 15.0 V @25C)
//   final discharge (F.V)  10.5 V = 1.75 V/cell   (the 20 Ah / 20 hr rating point)
//   operating temperature  up to 50 C on discharge
//
// Over-voltage trips ABOVE the charge band: the old 12.7 V sat below even the
// minimum charge voltage, so it raised a fault for the whole charge cycle.
#define THRESHOLD_OVER_VOLT     15.00f  // Qua ap (V)

// The old 9.00 V allowed a much deeper discharge than any AGM datasheet does.
//
// KNOWN LIMITATION (step 9b, needs the master's current frame): this compares
// the TERMINAL voltage, while 10.5 V is a resting figure. At 100 A the I*R drop
// is 100 * 13.5 mOhm = 1.35 V, so a battery resting at 11.85 V reads 10.5 V
// under load and trips at ~17% SOC. The fix is to compare the IR-compensated
// value, V + I*R0, which needs the current now arriving over CAN.
#define THRESHOLD_UNDER_VOLT    10.50f  // Sut ap (V)

// Datasheet discharge limit is 50 C. Note the DS18B20 reads the CASE, and the
// core runs hotter under load, so this is not a conservative figure.
#define THRESHOLD_OVER_TEMP     50.0f   // Qua nhiet do (C)

#endif /* BOARD_CONFIG_H */
