# Exp52: ATtiny85 serial print loop — flash + CH340G serial read (fresh start)

## Hypothesis

Fresh revisit of the ATtiny85 (ignore all prior ATtiny85 experiments). The goal is narrow: prove that a simple serial-print-in-a-loop sketch can be flashed to the ATtiny85 and its serial output read back, using the same toolchain/approach as Exp40 (PlatformIO build + Nano ArduinoISP flash via WSL avrdude), and the CH340G for both flashing (as the USB bridge to the Nano) and serial read. No trackpoint, no ZMK — flashing + reading only.

## Plan

1. New PlatformIO project `attiny85-serial-test/` (mirror `attiny85-blink`): board=attiny85, `board_build.f_cpu = 1000000UL` (factory fuses, no fuse writes), `stk500v1`, `upload_speed = 19200`
2. Sketch = bare `Serial.println(millis()); delay(500);` @9600 (mirror `serial-test.ino` from Exp46). Uses the tiny core's built-in `TinySoftwareSerial` `Serial` — no library, no UART needed.
3. Build on Windows: `pio run`
4. Flash via Nano ArduinoISP (Exp40 wiring): probe signature → flash → verify
5. Rewire CH340G directly to ATtiny85 TX, read @9600 (Exp46 read path)

## Wiring

### Flash (Nano ArduinoISP — Exp40, verbatim)

| Nano | ATtiny85 Pin | Note |
|---|---|---|
| D10 (RESET) | Pin 1 (PB5/RESET) | via 10k/20k divider |
| GND | Pin 4 (GND) | common ground |
| D11 (MOSI) | Pin 5 (PB0) | via 10k/20k divider |
| D12 (MISO) | Pin 6 (PB1) | direct |
| D13 (SCK) | Pin 7 (PB2) | via 10k/20k divider |
| 3.3V | Pin 8 (VCC) | 3.3V rail |

10µF cap Nano RESET→GND (after ArduinoISP is on the Nano). Nano itself flashed via CH340G + WSL avrdude (`-carduino` @57600).

### Serial read (CH340G direct to ATtiny85)

| CH340G | ATtiny85 Pin | Note |
|---|---|---|
| RXD | Pin 5 (PB0) | soft-serial TX |
| GND | Pin 4 | common ground |
| 3.3V | Pin 8 | VCC |
| — | Pin 1 (RST) | 10k to VCC (pull-up) |

## Result

- Build: **1240 bytes flash / 79 bytes RAM** (15.1% / 15.4%), green `pio run`
- Nano: ArduinoISP re-flashed + verified (4354 bytes, sig 0x1e950f)
- ATtiny85 signature probe via ArduinoISP: **0x1e930b** (t85)
- Flash: **1240 bytes written + verified** (`-cstk500v1` @19200, no `-D` — full erase first)
- Serial read @9600: **clean `millis()` prints every ~500ms** (51126 → 51638 → 52148 → ...)

## Findings

- `main.cpp` needs `#include <Arduino.h>` (unlike `.ino`). First build failed without it — `Serial`/`millis`/`delay` undeclared.
- The tiny core's built-in `Serial` is `TinySoftwareSerial` on the Analog Comparator pins: TX = AIN0 = PB0 (pin 5), RX = AIN1 = PB1 (pin 6). At 1 MHz + 9600 baud the delay divisor `((1000000/9600)-39)/12 = 5` is valid → reliable TX.
- **First probe after flashing ArduinoISP sync-fails** (`resp=0x15`, protocol errors) — a cold-start hiccup. `timeout 2 cat /dev/ttyUSB0` to drain the port, then re-run avrdude → succeeds. Use this drain before every avrdude call.
- CH340G is currently **busid 3-1 (COM31)** — AGENTS.md's `1-3` is stale (now the nRF52). Re-attach before reading: `usbipd attach --wsl --busid 3-1` + `modprobe ch341`.
- `stty -F /dev/ttyUSB0 raw 9600 cs8 -cstopb -parenb -hupcl` + `timeout N cat` reads the ATtiny85 soft-serial cleanly (Exp46 path, no reset pulse).

## Addendum: Flash + read with Nano AND CH340G connected simultaneously

Follow-up experiment: keep both the Nano (ArduinoISP) and the CH340G wired to the ATtiny85 at the same time, so we can flash and then read **without rewiring** — using a **raw splice** on the CH340G RXD node (Nano TXO + ATtiny85 pin 5 tied together, no isolation resistor).

### Findings

- **Read works perfectly with the raw splice.** With both the Nano and CH340G connected and the ATtiny85 running, the 9600 soft-serial stream is clean (`167014 → 167526 → 168040 → ...`, ~514ms apart, zero corruption). The Nano's TXO sitting idle-high does NOT corrupt the read.
- **Flash does NOT work while the ATtiny85 is running.** During stk500 sync, ArduinoISP only pulls ATtiny85 RESET low *after* sync, so the running sketch's PB0 soft-serial TX keeps firing 9600 garbage onto the shared RXD node → `not in sync: resp=0x1e/0x38/0x78...` (garbage, not 0x00). Flash must happen with the target silenced.
- **The workaround for simultaneous wiring:** ground ATtiny85 pin 1 (RESET) during the flash → PB0 goes high-Z → Nano's TXO drives RXD alone → flash + verify clean (1240 B). Then release the ground jumper and read — still no rewiring needed.
  - Note: with pin 1 grounded I saw one probe run fail with `resp=0x00` (line held low) before the successful run — likely the ground jumper momentarily touching the RXD rail. Verify the jumper is cleanly on pin 1 only.
- **Two CH340s can be present at once** (busid 3-1 COM31 and 3-4 COM28) — the read stream was on **ttyUSB1** (=3-1) this round. Probe both ports if the stream is missing.
- CH340G keeps dropping out of WSL after any physical re-plug — always re-run `usbipd attach --wsl --busid 3-1` (+ `3-4` if present) before reading.

### Conclusion (addendum)

The raw splice idea is **half-validated**: reading with both devices connected is clean and needs no isolation resistor. Only the flash needs the ATtiny85 silenced, which the pin-1 ground jumper achieves without a full rewire. If we want fully hands-free flash+read in the future, the remaining option is the 10kΩ on the Nano TXO leg — but it's not required for the read path as originally feared.

## Next steps

1. Confirm the soft-serial pins work for debugging future ATtiny85 builds (PB0 = pin 5 currently = USI SDA in the slave build — pin conflict must be managed when both are used).
2. Apply `SERIAL_LOG` to an ATtiny85 trackpoint build and log X/Y over this path.
