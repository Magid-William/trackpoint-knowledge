# Exp13: Standalone CH340G Programmer for Pro Mini

## Hypothesis

A standalone CH340G USB TTL programmer (not the Arduino Nano passthrough) can flash the Pro Mini 3.3V directly. This simplifies the setup by removing the Nano as a flashing intermediary and frees up the Nano for other use.

## Plan

1. Replace old AGENTS.md flashing section (Arduino Nano passthrough) with the direct CH340G method
2. Create a minimal blink-test sketch with a distinctive pattern (2 pulses, gap, 2 pulses) to verify the workflow
3. Compile using Alpine WSL's avr-gcc toolchain and flash via avrdude
4. Document the new flashing workflow in AGENTS.md

## Changes

### AGENTS.md — Flashing section rewritten

Old wiring (Nano passthrough):

| Pro mini 3.3V | Arduino Nano | Note |
|--------------|--------------|------|
| GND | GND | |
| VCC | V3.3 | |
| TXO | TX | |
| RXI | RX | |
| CH340(DTR) | RST | 100nF |

New wiring (CH340G direct):

| CH340G | Pro Mini 3.3V | Note |
|--------|--------------|------|
| TXD | RXI | |
| RXD | TXO | |
| GND | GND | |
| 3.3V | VCC | |
| DTR | RST | 100nF |

### New files

- `promini-trackpoint/blink-test/blink-test.c` — Pure AVR C sketch:
  - `_delay_ms(100)` on/off × 2, then `_delay_ms(1000)` gap, repeat
  - LED on PB5 (D13), direct port access, no Arduino core needed
  - 234 bytes flash, 8 bytes RAM

- `promini-trackpoint/build/blink-test/blink-test.hex` — Compiled binary

## Build method

Discovered arduino-cli's bundled AVR GCC toolchain (glibc) doesn't run on Alpine (musl libc) — `__strdup` symbol missing. Installed `gcompat` but still insufficient.

Solution: Use Alpine's own `gcc-avr` + `avr-libc` from edge/testing repo:

```bash
apk add --no-cache -X http://dl-cdn.alpinelinux.org/alpine/edge/testing gcc-avr avr-libc
avr-gcc -mmcu=atmega328p -DF_CPU=8000000UL -Os -o sketch.elf sketch.c
avr-objcopy -O ihex -R .eeprom sketch.elf sketch.hex
```

## Flashing

Proved the full pipeline:

1. `usbipd attach --wsl --busid 1-3` (CH340G VID:PID `1a86:7523`)
2. `modprobe ch341` → device at `/dev/ttyUSB0`
3. `avrdude -patmega328p -carduino -P /dev/ttyUSB0 -b 57600 -D -U flash:w:blink-test.hex:i`

AVR device initialized, signature 0x1e950f (m328p), 234 bytes written and verified.

## Success criteria

- [x] Blink-test sketch compiles and produces .hex
- [x] CH340G detected on COM1 (busid 1-3)
- [x] Device attaches to WSL Alpine
- [x] avrdude flashes successfully (234 bytes written + verified)
- [x] AGENTS.md updated with new wiring and commands

## Wiring verification

Confirmed from `usbipd list`:

```
BUSID  VID:PID    DEVICE                        STATE
1-3    1a86:7523  USB-SERIAL CH340 (COM1)       Shared
```

## Findings

1. **Alpine glibc gap**: Arduino CLI's AVR toolchain (prebuilt glibc) won't run on musl-based Alpine even with `gcompat`. Use Alpine's own `gcc-avr` for simple sketches, GitHub Actions for Arduino sketches needing the Arduino core.
2. **CH340G DTR auto-reset works**: The "Automatic Programmer" DTR signal replaces the previous 100nF + Nano passthrough — same behavior, fewer components.
3. **COM port can be COM1**: CH340G may grab COM1 (normally reserved for legacy serial), don't assume COM3/COM4.

## Conclusion

The standalone CH340G programmer is ready. AGENTS.md updated with the new flashing workflow. The next trackpoint-spi-slave build can be flashed directly without the Arduino Nano.

## Next steps

Flash a real trackpoint-spi-slave build (via GitHub Actions artifact) using the new CH340G method. Or continue Exp12 (dongle stall fix) with the freed-up Nano.
