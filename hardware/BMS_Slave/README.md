BMS_Slave — battery monitor PCB
===============================

Two-layer board carrying one STM32F103C8T6 battery monitor: the voltage divider
and DS18B20 on the floating side, the ADuM1201 isolator and the CAN transceiver
on the bus side, and an LM2596HV stepping the battery it measures down to the
logic rail. Five identical boards, one per battery.

The five are the same layout and the same bill of materials. Nothing on the board
distinguishes them — the CAN identifier and the two-point voltage calibration are
compile-time constants in the firmware, selected by `SLAVE_INDEX`, which is why
the five firmware images are not interchangeable even though the boards are.

Firmware: [`BMS_Slave/`](../../BMS_Slave). Block diagram, part table and photos:
main README [§3.1](../../README.md#31-battery-monitor-5).

### Files

| Path | What it is | Last changed |
|---|---|---|
| `altium/Slave_BMS.PrjPcb` | Altium project | 2026-09-11 |
| `altium/Salve.SchDoc` | schematic | 2026-09-11 |
| `altium/Slave.PcbDoc` | layout, 2 copper layers | 2026-08-29 |
| `altium/Slave_BMS.OutJob` | the output job that produced `fabrication/rev-a` | 2026-09-10 |
| `doc/schematic.pdf` | schematic, no Altium needed | 2026-09-10 |
| `fabrication/rev-a/` | Gerbers, drill files, reports | 2026-08-15 |

The schematic file is named `Salve.SchDoc`. The spelling is wrong, and it is left
wrong on purpose: the name is referenced from `Slave_BMS.PrjPcb`, so correcting it
means renaming inside Altium, not renaming the file.

### rev-a does not match the current layout

The Gerbers were exported on 2026-08-15. `Slave.PcbDoc` was then edited on
2026-08-29, fourteen days later. **The fabrication data in `rev-a` is therefore
not an export of the layout now sitting in `altium/`,** and which of the two
describes the five physical boards is not recorded anywhere.

The two possibilities differ in what has to happen next:

- the boards were made from the 2026-08-15 data and the later edit never reached
  copper, in which case `rev-a` is correct as a frozen record and the current
  `Slave.PcbDoc` is an unfabricated rev-b that should be exported under that name;
- or the boards were made after the edit, in which case `rev-a` is stale, has to
  be re-exported from the current layout, and must not be sent to a fab house as
  it stands.

Until that is settled, treat `rev-a` as a historical artefact and re-export before
ordering anything. The Master board does not have this problem: its Gerbers and
its layout carry the same date.

90 holes in total. The drill report notes `Two holes at same point : 2`, the same
finding the design rule check reports below.

### Design rule check

15 rules were checked and 11 pass. The four that do not:

| Rule | Violations | Finding |
|---|---|---|
| Silk To Solder Mask, 0.254 mm | 32 | silkscreen printed across a pad opening — cosmetic, no electrical effect |
| Silk to Silk, 0.254 mm | 12 | reference designators overlapping each other — cosmetic |
| Hole Size Constraint, 0.025–2.54 mm | 2 | 4.2 mm slots on pads `But_1-1` and `But_1-2`, at (122.2, 141.07) and (122.2, 146.15) mm — a mechanical slot has no reason to obey a 2.54 mm ceiling meant for component pins |
| Hole To Hole Clearance, 0.254 mm | 3 | `P2-1`, `P2-2` and `P2-3` each reported against themselves at one coordinate; `P2-3` sits at (92.955, 147.625) against (92.968, 147.625), 13 µm apart — coincident pads in a connector footprint, not two drills at one point |

This check was run on 2026-08-15, against the layout of that date, so it shares
the mismatch described above.
