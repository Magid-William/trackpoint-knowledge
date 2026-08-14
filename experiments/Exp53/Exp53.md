# Exp53: ATtiny85 serial read through the Arduino Leonardo (no CH340G)

## Hypothesis

The Leonardo (ATmega32u4, native USB CDC) can replace the CH340G entirely as the serial read path for the ATtiny85. Because the Leonardo's `Serial` is its USB port, a small SoftwareSerial-forwarder sketch can bridge the ATtiny85's soft-serial TX (PB0, pin 5) to the PC over USB — **zero rewiring** from the Exp39 flash layout, because the ATtiny85's pin 5 is already wired to the Leonardo's ICSP MOSI (= digital D16).

## Plan

1. Add `[env:leonardo]` to `arduino-isp/` (board=leonardo, avr109) — Exp39 proved this board route.
2. Flash ArduinoISP to the Leonardo over native USB (`pio run -e leonardo -t upload`).
3. Wire ATtiny85 to Leonardo (Exp39 5V direct, verbatim).
4. Attach Leonardo to WSL (`usbipd attach --wsl --busid 3-1`), probe signature, flash `attiny85-serial-test` hex.
5. Reflash the Leonardo with a SoftwareSerial forwarder (RX on D16) — read the ATtiny85 serial over the Leonardo's USB CDC on Windows, no CH340G, no WSL.

## Wiring

### Flash (Leonardo 5V ISP -> ATtiny85, Exp39 verbatim)

| Leonardo | ATtiny85 Pin | Function |
|----------|--------------|----------|
| ICSP Pin 2 (5V) | Pin 8 | VCC |
| ICSP Pin 6 (GND) | Pin 4 | GND |
| Digital Pin 10 | Pin 1 | RESET (PB5) |
| ICSP Pin 4 (MOSI) | Pin 5 | PB0 |
| ICSP Pin 1 (MISO) | Pin 6 | PB1 |
| ICSP Pin 3 (SCK) | Pin 7 | PB2 |

### Serial read (SAME wiring — no rewire needed)

ATtiny85 pin 5 (PB0, soft-serial TX) is already on the Leonardo's ICSP MOSI = digital **D16**. The reader sketch samples D16 with SoftwareSerial and forwards to USB CDC.

## Commands

```powershell
# ArduinoISP for the Leonardo
pio run -e leonardo -t upload          # arduino-isp/, avr109 over native USB

# Attach Leonardo to WSL (busid 3-1 = Leonardo COM9)
usbipd attach --wsl --busid 3-1
# then in Alpine:
# modprobe cdc_acm; ls /dev/ttyACM0
# avrdude -p attiny85 -c avrisp -P /dev/ttyACM0 -b 19200          # probe: 0x1e930b
# avrdude -p attiny85 -c avrisp -P /dev/ttyACM0 -b 19200 -U flash:w:...firmware.hex:i

# Reader sketch (leonardo-serial-reader/), reflash Leonardo
pio run -t upload

# Read on Windows (COM9, USB CDC — baud ignored)
# open COM9 @115200 -> expect millis() heartbeat ~every 500ms
```

## Findings

- **Leonardo as ISP programmer for ATtiny85: worked.** Signature `0x1e930b` (t85), flash + verify clean (1240 B), no level shifting needed (all 5V).
- **Leonardo as serial bridge: worked with zero rewiring.** The `leonardo-serial-reader` sketch (SoftwareSerial RX=D16, forward to USB CDC) reads the ATtiny85's 9600 soft-serial cleanly on Windows COM9 — values increment by ~514ms matching `delay(500)`.
- **usbipd gotchas:** Leonardo is busid **3-1** (not AGENTS.md's stale `1-3`). After a physical re-plug the device drops out of WSL — re-run `usbipd attach --wsl --busid 3-1` and `modprobe cdc_acm`. First probe after attach may sync-fail — drain with `timeout 2 cat /dev/ttyACM0`.
- **First flash attempt got 0xffffff** — user had missed the RESET (pin 1 -> Digital 10) jumper; fixing it restored the signature.

## Conclusion

**Success.** The Leonardo replaces both the Nano ArduinoISP *and* the CH340G: it flashes the ATtiny85 over ISP, then (after a 6 KB reader reflash) serves as the USB serial monitor for the ATtiny85 — all over one USB cable, with no rewiring between flash and read.

**Key trade-off:** the Leonardo can't be ArduinoISP and serial-bridge at the same time. Reflashing it back to ArduinoISP is required to flash the ATtiny85 again (~7s each way).

## Next steps

- Use the same reader pattern to log ATtiny85 trackpoint builds (`SERIAL_LOG` on PB0) — Exp52's next-steps item #2, now with a cleaner read path.
- If USB-based debug is needed on a budget: a Micro (32u4) or clone Leonardo gives the same native-CDC bridge.

## Hardware

- Arduino Leonardo (native USB, ATmega32u4, 5V) — COM9
- ATtiny85-20PU DIP-8 (1 MHz factory fuses)
- CH340G — NOT needed for this read path

## References

- Exp39 — Leonardo ISP -> ATtiny85 wiring/probe/flash (5V direct)
- Exp52 — ATtiny85 serial-test sketch (PB0 soft-serial TX @9600), flash + CH340G read
- Exp40 — Nano ArduinoISP route (superseded here by the Leonardo)
