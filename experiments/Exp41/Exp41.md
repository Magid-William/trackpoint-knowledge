# Exp41: ATtiny85 trackpoint I2C slave — compile only

## Hypothesis

The Pro Mini firmware (`trackpoint-i2c-slave.ino`, Exp38) can be ported to an ATtiny85 and compile cleanly with only four mechanical changes. The ATtiny85's USI-TWI slave (`Wire` in ATTinyCore) uses fixed pins PB0=SDA / PB2=SCL and supports the ZMK driver's combined `i2c_write_read_dt()` (repeated START) — so the same register protocol (0x42, burst 0x12, speed 0x11, wake_count 0x01) works unchanged. No driver or PS2Trackpoint library changes required.

## Plan

1. Fork `trackpoint-i2c-slave.ino` → `trackpoint-i2c-slave-attiny85.ino`
2. Apply the 4 ATtiny85 adaptations (below)
3. Build via PlatformIO (`board = attiny85`, F_CPU 1 MHz to match factory fuses)
4. Success = green `pio run`, `firmware.hex` < 8192 bytes

## Pin map (ATtiny85, USI hardware I2C)

| ATtiny85 | GPIO | Arduino | Function | Notes |
|---|---|---|---|---|
| 5 | PB0 | 0 | SDA | fixed by USI TWI |
| 7 | PB2 | 2 | SCL | fixed by USI TWI |
| 6 | PB1 | 1 | MOT | |
| 2 | PB3 | 3 | PS2 CLK | |
| 3 | PB4 | 4 | PS2 DAT | |
| 1 | PB5 | — | RST | 10kΩ pull-up to VCC; optional NiceNano reset-on-init hookup |

## The 4 ATtiny85 adaptations

| # | Pro Mini (Exp38) | ATtiny85 | Reason |
|---|---|---|---|
| 1 | `MOT_PIN 14`, `PS2_CLK 7`, `PS2_DAT 3` | `MOT_PIN 1`, `PS2_CLK 3`, `PS2_DAT 4` | tinyX5 pin numbering |
| 2 | `SERIAL_LOG 1` | `SERIAL_LOG 0` | no UART; all 6 GPIOs consumed |
| 3 | `WDTCSR` (×2) | `WDTCR` | ATtiny85 register name differs from ATmega328 |
| 4 | `TWCR = 0` | `Wire.end()` | `TWCR` doesn't exist; `Wire.end()` = `USI_TWI_Slave_Disable()` |

Wake path already re-calls `Wire.begin(I2C_ADDR)` (unchanged).

## Build

```
pio run -d attiny85-trackpoint-i2c-slave
```

`platformio.ini`:
```ini
[platformio]
src_dir = ../trackpoint-i2c-slave-attiny85

[env:program_via_ArduinoISP]
platform = atmelavr
framework = arduino
board = attiny85
board_build.f_cpu = 1000000UL
lib_extra_dirs = ../libraries
upload_protocol = stk500v1
upload_speed = 19200
```

- `src_dir` reuses the .ino in place; `lib_extra_dirs` reuses `PS2Trackpoint` — no duplication.
- `board_build.f_cpu = 1000000UL` matches factory fuses (no fuse changes, no bricking risk, per Exp40).

## Success criteria

- [x] `trackpoint-i2c-slave-attiny85.ino` compiles clean via `pio run`
- [x] Flash: 3318 / 8192 bytes (40.5%)
- [x] RAM: 94 / 512 bytes (18.4%)
- [x] `firmware.hex` produced

## Components (confirmed by user)

| Component | Value | Note |
|---|---|---|
| SDA/SCL pull-ups | 10kΩ | fine at 100kHz on a short bus; no real idle-power benefit (lines sit HIGH when idle) |
| RST pull-up | 10kΩ to VCC | standard for ATtiny85; disconnectable header during ISP flashing |
| RST → NiceNano | GPIO (Exp21 reset-on-init) | optional, keep disconnectable |
| VCC/GND decoupling | 0.1µF ceramic | ideal value |

## Findings

- The ATtiny85 `Wire` library is a USI-TWI slave (not the AVR TWI the Pro Mini uses). It is registered-mode friendly: on a repeated START the USI start-condition ISR fires `onReceive` (sets `cur_addr`), then the read phase fires `onRequest` — exactly the semantics the ZMK `i2c_write_read_dt()` needs. So no driver changes.
- `PS2Trackpoint` compiled unchanged — ATTinyCore's `tiny` core defines the classic `digitalPinToPort`/`portInputRegister`/etc. macros.
- The `tiny` core's `TinySoftwareSerial Serial` is only pulled in if referenced; `SERIAL_LOG 0` keeps it fully out of the binary.
- WDT register on ATtiny85 is `WDTCR` (iotnx5.h) — a silent trap if you just copy ATmega code.
- Compiled first try; no warnings.

## Conclusion

**Success.** The 4-line adaptation compiles cleanly and fits comfortably (40.5% flash). The compile-focused goal of the experiment is met.

## Next steps (Exp42+)

1. Flash the ATtiny85 via Nano ArduinoISP (Exp40 path) — `attiny85-trackpoint-i2c-slave` env is already `stk500v1` @ 19200
2. Bench test: I2C bus scan for 0x42 from the NiceNano, MOT wake, real PS/2 motion
3. Consider 8 MHz (fuse change) if I2C/PS2 timing at 1 MHz proves marginal
4. Power: switch `SLEEP_MODE_IDLE` → `SLEEP_MODE_PWR_DOWN` for real battery gains
