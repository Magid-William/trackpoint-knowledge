# Exp09: Wiring Update per AGENTS.md

## Hypothesis

Update the Pro Mini firmware pin assignments to match the current AGENTS.md wiring, which differs significantly from the old pinout. Add NPN transistor power control for the TrackPoint and wire the TTP223 touch sensor (unused in logic for now). The SPI-to-NiceNano link and PS/2 trackpoint reading should continue to work.

## Wiring changes (oldAGENTS.md → AGENTS.md)

| Signal | Old Pin | New Pin | Reason |
|--------|---------|---------|--------|
| MOT (IRQ to NiceNano) | D2 | D14/A0 | Free D2 for PS/2, MOT on analog-capable pin |
| PS/2 SDA (data) | D3 | D2 | Centralize PS/2 on adjacent pins D2/D3 |
| PS/2 SCL (clock) | D7 | D3 | Centralize PS/2 on adjacent pins D2/D3 |
| TrackPoint power | *(none)* | D4 | NPN transistor low-side switch for TrackPoint GND |
| Touch sensor | *(none)* | D19/A5 | TTP223 capacitive touch on the nub |

### NPN transistor

D4 → 1kΩ → NPN Base, Collector → TrackPoint GND, Emitter → GND. D4 HIGH powers the TrackPoint. Set HIGH at boot.

### TTP223 touch sensor

Wired to D19 (input). Initialized but not used in logic — deferred to future experiment.

## Changes by file

### `promini-trackpoint/trackpoint-spi-slave/trackpoint-spi-slave.ino`

- `MOT_PIN` 2 → 14 (A0)
- `PS2_CLK` 7 → 3 (D3)
- `PS2_DAT` 3 → 2 (D2)
- Added `NPN_PIN 4` — set HIGH in `setup()` to power TrackPoint
- Added `TOUCH_PIN 19` — set INPUT in `setup()`, unused in logic
- Updated header comment and boot Serial message

### `promini-trackpoint/trackpoint-diag/trackpoint-diag.ino`

- `PS2_CLK` 7 → 3, `PS2_DAT` 3 → 2

### `promini-trackpoint/trackpoint-serial-reader/trackpoint-serial-reader.ino`

- `CLK_PIN` 7 → 3, `DAT_PIN` 3 → 2, updated comment

## No changes

| File | Reason |
|------|--------|
| `zmk-pmw3610-driver/` | Zero driver changes per AGENTS.md rule |
| `zmk-trackpoint-shield/` | NiceNano pin assignments unchanged |
| `PS2Trackpoint/` | Library used as-is; pins passed at construction |
| `flash-nicenano.ps1` | Already correct |

## Verification

- [x] Pro Mini builds via GH Actions (all 3 sketches)
- [x] Pro Mini flashes via WSL avrdude (3970 bytes written/verified)
- [x] SPI handshake: 118 → 134 → 182 bytes flowing (NiceNano reading)
- [x] NiceNano flashes headless via GPREGRET method
- [x] Serial boot message confirms new pins: `MOT=D14  PS/2 CLK=D3  DAT=D2  NPN=D4  TOUCH=D19`
- [x] Cursor moves on screen (PS/2 data flowing through pipeline)

## Conclusion

**Exp09: Success!** The wiring update per AGENTS.md is complete and verified end-to-end:

```
TrackPoint ──PS/2 (D3=CLK, D2=DAT)──→ Pro Mini ──SPI (per-byte, CS deasserted)──→ NiceNano ──USB HID──→ cursor
                                        ↑
                                 NPN (D4) powers TrackPoint
                                 TTP223 (D19) wired, unused
```

All 3 Pro Mini sketches compiled and flashed. SPI handshake confirmed between Pro Mini and NiceNano. Cursor movement confirmed via the full pipeline.

Physical rewiring is required to match the new pin assignments (see table above). The old wiring (D7=CLK, D3=DAT, D2=MOT) is no longer valid.

## Next experiment

Exp10: Integrate TTP223 touch sensor logic — gate MOT pulses on touch to save SPI traffic/power, or use it for faster ramp reset.
