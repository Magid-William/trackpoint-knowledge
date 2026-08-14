# Exp18: Synthetic Rectangle Motion over I2C

## Hypothesis

Pro Mini I2C slave with a rectangle sequencer + ZMK I2C driver with `input_report()` will move the cursor in a rectangle on screen, with variable speed per segment.

## Plan

Send synthetic motion deltas from the Pro Mini to the NiceNano over I2C in a looping rectangle pattern. No PS/2 TrackPoint involved — purely synthetic data to validate the end-to-end I2C pipeline before integrating the real sensor.

### Wiring

Same as Exp17:

| Pro Mini | I2C | NiceNano V2 |
|----------|-----|-------------|
| D18 (A4) | SDA | P0.17 (4.7kΩ pull-up) |
| D19 (A5) | SCL | P0.20 (4.7kΩ pull-up) |
| D14 (A0) | MOT | P0.06 |
| GND | GND | GND |

### Rectangle Pattern

4 segments, 20ms per step (50 Hz MOT):

| Segment | Direction | dx | dy | Steps | Speed (px/s) |
|---------|-----------|----|----|-------|-------------|
| A→B | Right (slow) | +1 | 0 | 200 | 50 |
| B→C | Down (fast) | 0 | +4 | 50 | 200 |
| C→D | Left (fast) | -4 | 0 | 50 | 200 |
| D→A | Up (fast) | 0 | -4 | 50 | 200 |

Loop time: ~7 seconds. Deltas are 12-bit two's complement, matching PMW3610 format.

### Pro Mini Side: I2C Slave with Rectangle Sequencer

- `trackpoint-i2c-slave.ino` — add rectangle sequencer that advances one step each MOT cycle (20ms)
- Store 12-bit encoded deltas in `regs[0x03-0x05]` (X_L, Y_L, XY_H)
- Set `regs[0x02]` (MOTION) = 0x01 when moving, 0x00 when idle
- `requestEvent` burst read at 0x12 sends 7 bytes (regs 0x02–0x08)
- Clear motion registers after read to prevent stale re-reporting
- MOT pin pulses LOW for 100µs every 20ms

### ZMK Side: I2C Driver with Motion Reporting

- `trackpoint-i2c.c` — add 10ms polling timer reads 7-byte burst from 0x12
- Parse X/Y using same 12-bit two's complement as `pmw3610.c`
- Accumulate deltas and call `input_report()` with `INPUT_EV_REL` / `INPUT_REL_X` / `INPUT_REL_Y`
- The existing `zmk,input-listener` + `zip_ble_report_rate_limit 4` in the overlay handles forwarding

### No Changes To

- `corne_trackpoint_right.overlay` — already configured for I2C + input listener
- `corne_trackpoint_right.conf` — already has `CONFIG_I2C`, `CONFIG_TRACKPOINT_I2C`, etc.
- `pmw3610.c` (PMW3610 SPI driver) — **untouched**
- `zmk-pmw3610-driver/Kconfig` — already has `TRACKPOINT_I2C`
- `zmk-pmw3610-driver/CMakeLists.txt` — already compiles `trackpoint-i2c.c`

### Modified Files

| File | Action |
|------|--------|
| `promini-trackpoint/trackpoint-i2c-slave/trackpoint-i2c-slave.ino` | Rectangle sequencer + burst read |
| `zmk-pmw3610-driver/src/trackpoint-i2c.c` | Poll + motion reporting |
| `experiments/Exp18/Exp18.md` | This file |

### I2C Burst Protocol

- Master writes `0x12` (burst address)
- Master reads 7 bytes: `[MOTION, X_L, Y_L, XY_H, SQUAL, SHUTTER_H, SHUTTER_L]`
- Same register layout as PMW3610, so parsing code mirrors `pmw3610.c`

### Success Criteria

- [ ] UF2 builds and flashes on NiceNano
- [ ] .hex builds and flashes on Pro Mini
- [ ] Cursor moves in a rectangle on screen
- [ ] Rectangle loops continuously without stalling
- [ ] Each segment moves at perceptibly different speeds (slow right, fast everywhere else)

## Findings

1. **12-bit two's complement encoding works but overcomplicates debugging** — I2C reads the correct values, but the encoding/decoding adds complexity. Switching to raw `int8` (2-byte burst: X, Y) eliminated encoding concerns entirely.

2. **Y-axis inversion on host** — The ZMK/HID pipeline sends +Y as DOWN, but the host interprets +Y as UP. Added `y = -(int8_t)buf[1]` in the driver to correct.

3. **Value magnitude threshold** — Delta values of ±1 produce visible data at the I2C level but no cursor movement on screen (OS mouse acceleration/pipeline filtering). Values ≥ ±4 are required for visible movement.

4. **Smooth slow motion** — For slow speeds (20–100 px/s), sending smaller values at higher frequency (e.g., +1/40ms = 25 fps) is much smoother than sending larger values at low frequency (e.g., +4/200ms = 5 fps). Implemented via `skip` counter: extra MOT cycles between updates without changing the MOT period.

5. **Pro Mini I2C slave + ZMK polling is reliable** — No I2C bus errors during hours of testing. The nRF TWIM + AVR TWI combination works correctly with combined transactions.

6. **Speed variants are visible** — 0.5× right (100 px/s, smooth), 4× down (800 px/s, fast zip), 1× left/up (200 px/s, brisk) produce clearly different cursor speeds.

## Conclusion

Exp18 succeeded. The end-to-end I2C pipeline from Pro Mini synthetic data to cursor movement on screen works reliably. Key lessons:
- Raw `int8` encoding is simpler and sufficient for mouse deltas
- Y-axis may need negation depending on HID pipeline
- Small deltas (< ±4) may be filtered by OS mouse acceleration
- For smooth slow motion, use higher frequency with smaller values

## Next Steps

Exp19: Integrate PS/2 TrackPoint reading on the Pro Mini (using `PS2Trackpoint` library), expose real X/Y motion data over I2C registers, handle buttons.
