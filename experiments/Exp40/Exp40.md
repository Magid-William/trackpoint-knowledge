# Exp40: Flash ATtiny85 blink via Arduino Nano ISP (3.3V, PlatformIO + WSL avrdude)

## Hypothesis

An Arduino Nano running the standard ArduinoISP sketch can program an ATtiny85 over the ISP bus. Because the ATtiny85 runs at 3.3V while the Nano's I/O is 5V, MOSI/SCK/RESET need level shifting (resistor dividers); MISO is safe direct. On Windows, the CH340 driver is unreliable for serial flashing (Win32 error 31), so the flash itself must go through WSL Alpine's Linux driver via usbipd. A simple blink on PB0 proves the whole pipeline.

## Plan

1. Upload the stock ArduinoISP sketch to the Nano (PlatformIO project `arduino-isp`)
2. Wire the ATtiny85 to the Nano (3.3V + dividers)
3. Create a blink project (`attiny85-blink`) compiled for ATtiny85 @ 1 MHz to match factory fuses
4. Attach the CH340 to WSL Alpine via usbipd and flash through Linux avrdude
5. Probe signature first, then flash + verify, confirm LED blinks

## Wiring

Nano 5V ISP -> ATtiny85 3.3V:

| ATtiny85 Pin      | Nano        | Note                                          |
|-------------------|-------------|-----------------------------------------------|
| Pin 1 (PB5/RESET) | D10 (RESET) | via 10k/20k divider                           |
| Pin 4 (GND)       | GND         | common ground                                 |
| Pin 5 (PB0)       | D11 (MOSI)  | via 10k/20k divider (5V→3.3V)                 |
| Pin 6 (PB1)       | D12 (MISO)  | direct (3.3V out, above Nano input threshold) |
| Pin 7 (PB2)       | D13 (SCK)   | via 10k/20k divider                           |
| Pin 8 (VCC)       | 3.3V        | 3.3V rail                                     |

LED + 220R from PB0 (physical pin 5) to GND.

> **10µF capacitor** between Nano RESET and GND — install **after** ArduinoISP is uploaded to the Nano. It prevents the Nano from resetting while avrdude talks to ArduinoISP, but it **blocks bootloader entry** if present during the ArduinoISP upload itself.

## Build

### `arduino-isp/` (Nano programmer)

`platformio.ini`:
```ini
[env:nano]
platform = atmelavr
framework = arduino
board = nanoatmega328new
upload_protocol = arduino
upload_port = COM1
```

`src/main.cpp` = stock ArduinoISP sketch (Randall Bohn). Built on Windows: `pio run` (4354 bytes).

### `attiny85-blink/` (target)

`platformio.ini` (per the guide's snippet + 1 MHz clock to match factory fuses):
```ini
[env:program_via_ArduinoISP]
platform = atmelavr
framework = arduino
board = attiny85
upload_protocol = stk500v1
; each flag in a new line
upload_flags =
    -P$UPLOAD_PORT
    -b$UPLOAD_SPEED

upload_speed = 19200
upload_port = COM1
board_build.f_cpu = 1000000UL
```

`src/main.cpp`: blink digital pin 0 (PB0), 1s on/1s off. Built on Windows: `pio run` (466 bytes).

## Flashing (WSL — the working path)

The Windows CH340 driver was broken for flashing (`can't set com-state`, Win32 error 31, even after reboot). The Linux driver in WSL Alpine works flawlessly.

```powershell
# 1. Ensure Alpine is running, then attach the CH340 (busid 1-3)
wsl -d Alpine -u root -- bash -c "echo ready"
usbipd attach --wsl --busid 1-3
```

```bash
# 2. In Alpine: load driver, probe the Nano (expect 0x1e950f = m328p)
modprobe ch341
avrdude -C /etc/avrdude.conf -p atmega328p -c arduino -P /dev/ttyUSB0 -b 57600

# 3. Flash ArduinoISP to the Nano (built hex from PlatformIO)
avrdude -C /etc/avrdude.conf -v -patmega328p -carduino -P /dev/ttyUSB0 -b 57600 -D \
  -U flash:w:"/mnt/d/DIY/trackpoint/pmw3610/arduino-isp/.pio/build/nano/firmware.hex":i

# 4. Probe the ATtiny85 via ArduinoISP (expect 0x1e930b = t85), 19200 baud
avrdude -C /etc/avrdude.conf -v -pt85 -cstk500v1 -P /dev/ttyUSB0 -b 19200

# 5. Flash blink, write + verify
avrdude -C /etc/avrdude.conf -v -pt85 -cstk500v1 -P /dev/ttyUSB0 -b 19200 -D \
  -U flash:w:"/mnt/d/DIY/trackpoint/pmw3610/attiny85-blink/.pio/build/program_via_ArduinoISP/firmware.hex":i
```

## Success criteria

- [x] `arduino-isp` builds (4354 bytes) and flashes to the Nano (signature 0x1e950f, verified)
- [x] `attiny85-blink` builds for ATtiny85 @ 1 MHz (466 bytes)
- [x] ATtiny85 signature read via Nano ArduinoISP = 0x1e930b
- [x] Blink written and verified by avrdude (466 bytes)
- [x] LED on PB0 blinks 1s/1s

## Troubleshooting

### Windows CH340: "A device attached to the system is not functioning" / avrdude `can't set com-state`
- Symptom: every open of COM1 fails with Win32 error 31. Even `GetCommState` succeeds but `SetCommState` fails — avrdude says `ser_open(): can't set com-state`.
- Persists across device restart, usbipd unbind/rebind, root-hub restart, **and a full reboot**. The device even drops out of `Win32_SerialPort` while PnP reports OK.
- **Workaround: flash via WSL.** Attach the CH340 to Alpine (`usbipd attach --wsl --busid 1-3`), where the Linux `ch341` driver works cleanly.
- Sometimes only certain bauds "work" (e.g. 19200 via direct DCB, 57600 fails) — inconsistent, don't rely on it.

### usbipd states: `Shared` vs `Attached`
- `Shared` = bound for WSL sharing but not currently attached (usable by Windows).
- `Attached` = claimed by WSL; Windows cannot use it.
- If attach fails with "There is no WSL 2 distribution running", start Alpine first (`wsl -d Alpine -u root -- bash -c "echo ready"`).
- The CH340 on busid 1-3 is the same device used for the Pro Mini programmer — confirm the cable is on the right board.

### 10µF cap blocks Nano bootloader entry
- With the cap installed, uploading ArduinoISP to the Nano fails (`stk500_getsync()` never syncs, response bytes like `0x1c`/`0xe0`).
- Remove the cap → upload ArduinoISP → reinstall the cap → wire the ATtiny85.

### avrdude part name typos
- `atttiny85` (typo) → "AVR Part not found". Use `t85` or `attiny85`.

### Garbage sync bytes (`0x1c`, `0xe0`, `0x00`)
- A running sketch (e.g. Pro Mini serial-reader at 9600) echoing on the same CH340 produces garbage when probed at 57600. Verify the physical cable is on the Nano, not the CH340G programmer.

### 5V Nano vs 3.3V ATtiny85
- Must level-shift the Nano's output lines (MOSI/SCK/RESET) with 10k/20k dividers. MISO is 3.3V from the target and reads fine directly. Without dividers the 3.3V ATtiny85 is driven out of spec.

### Clock: factory fuses = 1 MHz
- A fresh ATtiny85 runs at 1 MHz. Compile with `board_build.f_cpu = 1000000UL` so `delay()` is accurate and matches the factory fuses (no fuse writes, no bricking risk).

## Conclusion

**Success.** The full pipeline works: PlatformIO builds both sides, the Nano runs ArduinoISP, and the ATtiny85 blinks on PB0. The one real gotcha is the broken Windows CH340 driver — flashing through WSL's Linux driver sidesteps it entirely. This is now the documented flash path for any future ATtiny85 work.
