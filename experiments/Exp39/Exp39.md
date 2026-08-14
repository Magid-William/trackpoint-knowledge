# Exp39: Flash ATtiny85 via Arduino Leonardo ISP

## Hypothesis

An Arduino Leonardo (native USB, ATmega32u4) running the ArduinoISP sketch can program an ATtiny85 over the ISP bus. A simple blink on PB3 (physical pin 2) proves the whole pipeline: Leonardo-as-programmer -> avrdude -> ATtiny85 flash.

## Plan

1. Write a pure AVR C blink sketch for the ATtiny85, LED on PB3
2. Compile with Alpine WSL `avr-gcc` (`-mmcu=attiny85`, 1 MHz to match default fuses)
3. Attach the Leonardo to WSL via `usbipd attach --wsl --busid 3-1`
4. Probe the ATtiny85 signature first (fresh check that ArduinoISP actually works)
5. Flash the .hex and verify
6. Confirm the LED blinks

## Wiring

Leonardo 5V ISP -> ATtiny85:

| Leonardo | ATtiny85 Pin | Function |
|----------|--------------|----------|
| ICSP Pin 2 (5V) | Pin 8 | VCC |
| ICSP Pin 6 (GND) | Pin 4 | GND |
| Digital Pin 10 | Pin 1 | RESET (PB5) |
| ICSP Pin 4 (MOSI) | Pin 5 | PB0 / SDA |
| ICSP Pin 1 (MISO) | Pin 6 | PB1 |
| ICSP Pin 3 (SCK) | Pin 7 | PB2 / SCL |

LED + 220R from PB3 (physical pin 2) to GND.

## Build

```bash
avr-gcc -mmcu=attiny85 -DF_CPU=1000000UL -Os -o blink-pb3.elf blink-pb3.c
avr-objcopy -O ihex -R .eeprom blink-pb3.elf blink-pb3.hex
```

## Flashing

```bash
usbipd attach --wsl --busid 3-1        # Leonardo (COM9) -> WSL Alpine
avrdude -p attiny85 -c avrisp -P /dev/ttyACM0 -b 19200   # probe, expect 0x1E930B
avrdude -p attiny85 -c avrisp -P /dev/ttyACM0 -b 19200 -U flash:w:blink-pb3.hex:i
```

## Success criteria

- [ ] Compiles cleanly to .hex with Alpine avr-gcc
- [ ] ATtiny85 signature read (0x1E930B) via Leonardo ArduinoISP
- [ ] Flash written and verified by avrdude
- [ ] LED on physical pin 2 blinks

## Findings

- Compile of `blink-pb3.c` and `nano-isp.c` via Alpine avr-gcc: clean, `.hex` produced (build/).
- ISP pipeline via Leonardo ArduinoISP: worked (Nano ISP in Exp40 later replaced this route).
- Bench test of the trackpoint slave: **mouse only moved on one axis** — the other axis produced no motion. Cause uninvestigated before the experiment was closed.

## Conclusion

**Failed — mouse only moved on one axis.** The one-axis-only motion points at a wiring/pin issue on the dead axis (PS/2 CLK vs DAT, or USI-TWI PB0/PB2) rather than the ISP flash pipeline, which itself worked.

## Next experiment suggestion

Isolate the dead axis:
1. Use the Exp42 OLED I2C-master test rig to display both axes raw — if one axis reads zero there, the fault is upstream (PS/2 wiring/init), not the slave firmware.
2. Recheck the ATtiny85 USI-TWI pin mapping (PB0=USI-DI, PB2=USI-DO/SDA) and the TrackPoint DAT/CLK swap from Exp26.
3. If axis reads are fine on the OLED, diff the slave firmware axis handling (X vs Y paths) between the Pro Mini and ATtiny85 builds.
