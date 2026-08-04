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
#define SLAVE_INDEX             0U      // 0..4, one slave per battery

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
// NOTE: carried over UNCHANGED from the old Shared_Data.h so that this
// refactoring step cannot alter behaviour. Revisited in step 9 against
// the CSB EVX12200 datasheet:
//   - datasheet cutoff is 10.5 V, so 9.00 V discharges too deep
//   - 12.7 V will trip while charging; the real charge voltage must be
//     read from the datasheet, not guessed
#define THRESHOLD_OVER_VOLT     12.7f   // Qua ap (V)        - TODO step 9
#define THRESHOLD_UNDER_VOLT    9.00f   // Sut ap (V)        - TODO step 9
#define THRESHOLD_OVER_TEMP     60.0f   // Qua nhiet do (C)

#endif /* BOARD_CONFIG_H */
