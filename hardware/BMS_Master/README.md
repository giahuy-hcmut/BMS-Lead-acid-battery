BMS_Master — central controller PCB
===================================

Two-layer board carrying the ESP32 central controller: the INA219 across the
external 100 A / 75 mV shunt, the MCP2515 vehicle-CAN controller on SPI, the
transceiver for the internal CAN bus to the five monitors, and the drive for the
CZW200A contactor. One board per vehicle.

Firmware: [`BMS_Master/`](../../BMS_Master). Block diagram, part table and photos
of the finished board: main README
[§3.2](../../README.md#32-central-controller).

### Files

| Path | What it is | Last changed |
|---|---|---|
| `altium/Master_BMS.PrjPcb` | Altium project | 2026-09-11 |
| `altium/Master.SchDoc` | schematic | 2026-09-11 |
| `altium/Master.PcbDoc` | layout, 2 copper layers | 2026-09-03 |
| `altium/Master_BMS.OutJob` | the output job that produced `fabrication/rev-a` | 2026-09-10 |
| `doc/schematic.pdf` | schematic, no Altium needed | 2026-09-10 |
| `fabrication/rev-a/` | Gerbers, drill files, reports | 2026-09-03 |

### rev-a

Exported 2026-09-03 from a `Master.PcbDoc` of the same date, so the fabrication
data and the layout it came from agree. `Master.SchDoc` was edited on 2026-09-11,
after that export; the edit did not reach the board, which is why the Gerbers are
still the right ones for the assembled hardware.

97 holes in total, round holes and slots in separate Excellon files. The drill
report notes `Two holes at same point : 2`, which is the same finding the design
rule check reports below.

### Design rule check

15 rules were checked and 11 pass. The four that do not, with the reason each was
accepted rather than fixed:

| Rule | Violations | Finding |
|---|---|---|
| Silk To Solder Mask, 0.254 mm | 27 | silkscreen printed across a pad opening — cosmetic, no electrical effect |
| Silk to Silk, 0.254 mm | 13 | reference designators overlapping each other — cosmetic |
| Hole Size Constraint, 0.025–2.54 mm | 6 | holes larger than the rule's own ceiling, listed below |
| Hole To Hole Clearance, 0.254 mm | 3 | a pad reported against itself, listed below |

The six oversized holes are four 3.2 mm round holes on pads `MCP2515-17` through
`MCP2515-20`, at (67.297, 28.448), (90.297, 28.448), (67.297, 63.448) and
(90.297, 63.448) mm — a 23.0 × 35.0 mm rectangle, which is the mounting pattern
of the MCP2515 module rather than four signal pins — and two 4.2 mm slots on pads
`But_1-1` and `But_1-2`. Neither is a defect: the rule's 2.54 mm maximum was left
at its default, which is a figure meant for component pins, and a mechanical hole
has no reason to obey it.

The three hole-to-hole collisions are each a pad against itself at one
coordinate: `P1-1` and `P1-1` both at (29.337, 88.519), `P1-2` at (29.337,
93.599), and `P1-3` at (29.337, 98.679) against (29.337, 98.692), 13 µm apart.
Coincident pads in a connector footprint, not two drills fighting for the same
spot. The drill report counts the same thing as two holes at one point.

None of the four blocks manufacture, and the board in the photographs was built
from exactly this data and works.
