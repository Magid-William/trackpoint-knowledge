# Exp42: ATtiny85 isolation test — SSD1306 OLED X/Y readout (Tiny4kOLED)

## Hypothesis

Before the NiceNano joins, the ATtiny85 can be validated standalone by driving an SSD1306 0.91" OLED over the USI I2C pins in **master** mode (the same PB0/PB2 that will later be the slave bus to the NiceNano). Live X/Y from the PS/2 trackpoint proves: (1) PS/2 bit-bang works at 1 MHz, (2) the USI I2C hardware works, (3) the WDT idle-sleep path — the code that only compiles today, never runs — behaves. Fully standalone: no PC, no serial.

## Plan

1. Vendor Tiny4kOLED 2.3.0 (pinned tag `785469d`) into `libraries/Tiny4kOLED/`
2. New diagnostic sketch `trackpoint-oled-attiny85/` — USI I2C **master**, PS/2 read at 50 Hz, draw X/Y, WDT sleep with OLED `SLEEP` indicator
3. New PlatformIO project `attiny85-trackpoint-oled/` (attiny85 @ 1 MHz, `stk500v1`)
4. Build → flash via Nano ArduinoISP → bench test

## Wiring

| ATtiny85 | → | OLED / TrackPoint |
|---|---|---|
| pin 5 (PB0) | → | OLED SDA (USI I2C master) |
| pin 7 (PB2) | → | OLED SCL |
| pin 8 / pin 4 | → | VCC (3.3V) / GND |
| pin 2 (PB3) | → | TrackPoint CLK |
| pin 3 (PB4) | → | TrackPoint DAT |
| pin 1 (RST) | → | 10k to VCC (ISP programmer during flash) |
| pin 6 (PB1) | — | unused in this build |

10kΩ pull-ups on SDA/SCL. Power from the Nano's 3.3V rail — runs unattended. SSD1306 addr 0x3C (module-dependent).

## Library

`datacute/Tiny4kOLED` v2.3.0 — uses ATTinyCore's Wire (USI) by default, no extra I2C dependency. Frame buffer lives in the SSD1306's own RAM (double-buffered 128x32), so MCU RAM stays small.

## Build

```
pio run -d attiny85-trackpoint-oled
```

## Success criteria

- [x] Vendored Tiny4kOLED 2.3.0 (`libraries/Tiny4kOLED`, 21 files)
- [x] `trackpoint-oled-attiny85.ino` compiles clean (no warnings)
- [x] Flash: 5636 / 8192 bytes (68.8%)
- [x] RAM: 125 / 512 bytes (24.4%)
- [x] Exp41 slave build unaffected by the new library (3318 / 94 — unchanged)

## Bench checklist (hardware)

- [ ] OLED lights and shows `X: 0` / `Y: 0` at boot
- [ ] Moving the nub updates X/Y live (validates PS/2 bit-bang at 1 MHz)
- [ ] No stuck/drift values (visually confirms MAX_DELTA / DEADBAND)
- [ ] After 5 s idle the OLED shows `SLEEP`; touching the nub wakes and resumes
- [ ] I2C master speed at 1 MHz is slowish (~50 kHz) — display updates may look slow; expected

Flash status: **DONE** — 5636 bytes written and verified (see Flashing section).

## Findings

- Tiny4kOLED's `oled.begin()` calls `Wire.begin()` itself and waits for the OLED to ACK (`tiny4koled_check_wire` loop) — if the OLED is missing/wrong-addr the firmware hangs at setup. By design for a diagnostic build.
- `oled.clear()` clears the render frame (SSD1306 upper pages); `oled.switchFrame()` swaps — no MCU framebuffer, no flicker.
- Redraw only on value change keeps I2C traffic minimal at 1 MHz.
- Cleanup: the production-only `was_moving` / `last_ps2_ms` statics were dropped from this build (no burst-holding in a display test).

## Flashing (Exp40 path — Nano ArduinoISP + WSL avrdude)

Success: `5636 bytes of flash written` and **verified**. Two hard-won gotchas:

1. **`-D` flag breaks writes on ATtiny85 via ArduinoISP.** With `-D` (skip chip-erase) avrdude reports "written" but verify fails (`0x00 != 0x21` at byte 0 — page writes silently no-op). Drop `-D` so avrdude does the full chip erase first.
2. **VCC off-by-one → dead chip, `0x000000` signature.** The ATtiny85 wasn't powered (power was one pin off); fuses/locks were all normal. Signature reads came back all-zero. Fix the power pin (pin 8 on 3.3V, pin 4 on GND) and the chip responds.
3. **OLED shares the USI/ISP pins (PB0/PB2).** Must be disconnected (plus its pull-ups) during flashing — it loads the MOSI/SCK lines. Reconnect after.
4. **No 10µF cap on the Nano RST:** each port open resets the Nano (ArduinoISP reboots). Worked around with `stty -F /dev/ttyUSB0 19200 -hupcl` before avrdude to hold DTR high.
5. CH340 detaches when WSL idles — re-run `usbipd attach --wsl --busid 1-3` + `modprobe ch341` as needed.

Flash command used:
```bash
stty -F /dev/ttyUSB0 19200 -hupcl
avrdude -C /etc/avrdude.conf -pt85 -cstk500v1 -P /dev/ttyUSB0 -b 19200 \
  -U flash:w:"/mnt/d/DIY/trackpoint/pmw3610/promini-trackpoint/attiny85-trackpoint-oled/.pio/build/program_via_ArduinoISP/firmware.hex":i
```

## Conclusion

**Compile + flash success.** Both ATtiny85 builds (slave + OLED) are green; the OLED firmware is written and verified on the chip.

## Next steps

1. Flash `attiny85-trackpoint-oled` via Nano ArduinoISP (Exp40 path, `stk500v1` @ 19200)
2. Bench: confirm the checklist above
3. Then Exp43: flash the slave build, move SDA/SCL to the NiceNano, verify 0x42 + MOT wake from the ZMK shell
