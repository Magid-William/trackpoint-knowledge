# Exp06: TrackPoint Serial Reader

## Hypothesis

Before integrating PS/2 motion data into the SPI pipeline, verify that the TrackPoint is alive and communicating with the Pro Mini over PS/2. A minimal sketch using the PS2Trackpoint library reads 3-byte motion packets and prints X/Y/buttons over serial. No SPI, no ZMK, no MOT — PS/2-only validation.

## Plan

1. Create `trackpoint-serial-reader/trackpoint-serial-reader.ino` — a standalone sketch that:
   - Initialises the PS2Trackpoint library (CLK=D7, DAT=D3)
   - Runs `reset()` + `enableStreaming()` after a 2s power-up delay
   - In `loop()`, calls `readPacket(x, y, buttons)` and prints the values over 115200 baud serial

2. Update `.github/workflows/compile.yml` — add a second compile step + artifact upload for the new sketch

3. Push to GitHub, download the artifact, flash via avrdude

4. Read serial output while touching the TrackPoint nub

## Files Changed

| File | Change |
|------|--------|
| `trackpoint-serial-reader/trackpoint-serial-reader.ino` | New file — minimal PS/2 serial reader |
| `.github/workflows/compile.yml` | Add second compile step + artifact |
| `Experiments.md` | Add Exp06 row |
| `experiments/Exp06/Exp06.md` | This file |

## No Changes

| File | Reason |
|------|--------|
| `zmk-pmw3610-driver/` | Not involved in this experiment |
| `zmk-trackpoint-shield/` | Not involved in this experiment |

## Success Criteria

- [x] Pro Mini builds via GH Actions (new artifact produced)
- [x] Flashes successfully via avrdude (2812 bytes, verified)
- [x] Serial 115200 shows "Ready." at boot
- [x] X/Y values update when TrackPoint nub is moved
- [x] Button field changes when physical buttons are pressed
- [x] No CRC errors, no stuck bytes, no garbled output

## Key Findings

1. **No init commands needed** — This IC streams motion packets immediately on power-up and rejects all host commands (0xFF reset, 0xF4 enable, etc.). Calling reset()/enableStreaming() is unnecessary and consumes bytes that may desync the parser.

2. **Wiring confirmed** — D7=CLK, D3=DAT works at 3.3V with AVR internal pull-ups. No external 4.7kΩ resistors required (despite the reference page recommending them).

3. **Packet format** — Standard 3-byte PS/2 motion packets with status byte (bit3=packet, bit0-2=buttons), X byte, Y byte. Overflow handling works (clamps to -128/127).

4. **Buttons** — Left/Right/Middle reported in status byte bits 0-2.

5. **Noise** — First 1-2 packets after power-up may have garbage overflow values (X=±128, Y=±128), after which data stabilizes.

## Conclusion

**Exp06: Success.** The TrackPoint PS/2 communication is fully verified. The road is now clear for Exp07: integrate real TrackPoint data into the SPI burst pipeline (the original goal). The proven architecture:

```
TrackPoint ──PS/2──→ Pro Mini ──SPI──→ NiceNano (ZMK PMW3610 driver)
```

The next experiment will replace `update_rect_step()` with a `ps2_poll()` state machine that reads TrackPoint packets and feeds X/Y deltas into the burst array, combining Exp05's interrupt-driven SPI + Exp06's validated PS/2 reader.
