# Exp55: ATtiny85 synthetic I2C → ZMK shell X/Y read (right = central, no trackpoint)

## Hypothesis

The existing `trackpoint-i2c-synth-attiny85` sketch (Exp44-era synthetic
rectangle generator, I2C slave @0x42 serving `[acc_x, acc_y]` destructively,
**no trackpoint, no PS2Trackpoint**) flashed onto the ATtiny85 via the Leonardo
(Exp53/54 path), then physically moved to the NiceNano's I2C bus, is read by the
stock `promini,trackpoint-i2c` driver. The X/Y stream is visible in the ZMK USB
shell — no mouse movement required, this experiment is purely a read test.

**Zero ZMK code changes.** Reusing the `dabase_v2_right-standalone-usb` build
(right half = central, zmk-usb-logging snippet, I2C shell, 10ms plain polling
since the overlay has no `irq-gpios`).

## Plan

1. Rebuild `attiny85-trackpoint-synth` (no source change) → hex.
2. Flash Leonardo as ArduinoISP (`pio run -e leonardo -t upload`).
3. Attach Leonardo to WSL, probe signature, flash synth hex (no `-D`).
4. **User physically moves the ATtiny85 from the Leonardo to the NiceNano.**
5. Create + push `Exp55` branch in `zmk-config-dabaseV_0-2` → GH Actions builds.
6. Wait, download `dabase_v2_right-standalone-usb.uf2`.
7. Flash NiceNano via `flash-nicenano.ps1`.
8. Read X/Y from the ZMK shell (`i2c scan` for 0x42, watch driver log stream).

## Wiring (ATtiny85 → NiceNano)

| ATtiny85 | Function | NiceNano |
|----------|----------|----------|
| Pin 5 (PB0) | SDA | P0.17 |
| Pin 7 (PB2) | SCL | P0.20 |
| Pin 8 | VCC | 3.3V |
| Pin 4 | GND | GND |

4.7kΩ pull-ups on SDA/SCL → 3.3V (per AGENTS.md). No MOT wire — driver plain
polls (no `irq-gpios` in this overlay).

## Success criteria

- [x] ATtiny85 sig `0x1e930b`, synth hex flashed + verified via Leonardo
- [x] After physical move + ZMK flash: `i2c read 0x42 0x00` succeeds (register returns data, no -EIO)
- [x] Shell shows continuous `raw sign-extended: x=.. y=..` stream (no buttons)
- [x] No trackpoint, no Pro Mini involved

## Findings

- **Handshake works**: stock `promini,trackpoint-i2c` driver reads the ATtiny85 synth
  continuously (~11ms poll), logs `raw sign-extended: x=.. y=..`, and emits
  `SEND ... ev=REL type=REL_X/REL_Y` events. Zero ZMK code changes.
- **The breadboard was the fault, not firmware or the sketch.** SCL was stuck low
  (~0.21V) / phantom scan addresses (`0x04/0x05/0x23/0x35`) even with pull-up
  removed → intermittent 0.9V on both lines. Moving the ATtiny85 to a prototype
  PCB (both SDA/SCL idling at 3.3V) fixed it instantly. USI slave + nRF TWIM were
  never the problem — the bus was electrically broken all along.
- Debug log level (`CONFIG_TRACKPOINT_I2C_LOG_LEVEL_DBG=y`) floods USB logging:
  `shell_uart: RX ring buffer full` + heavy `messages dropped`. Info-level
  (`raw sign-extended` + `SEND`) is the right balance for live streaming.

## Conclusion

Exp55 succeeds: ATtiny85 synthetic I2C slave @0x42 is readable by the stock
`promini,trackpoint-i2c` driver, and X/Y + REL events flow to the host over USB.
The USI-slave / combined-transaction concern (Exp44) does not block reading —
a stable bus is sufficient. The synth stream drives the cursor as a rectangle.
Next step is the real Pro Mini as the I2C slave serving live trackpoint X/Y.
