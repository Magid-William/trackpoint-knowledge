# Exp02: SPI Handshake + Synthetic Motion

## Hypothesis

A minimal Pro Mini SPI slave returning hardcoded register values will pass the PMW3610 driver's init sequence. With the Product ID moved from `0x00` to `0x3F`, the self-test and identity checks succeed. Synthetic motion triggered by toggling the MOT pin will move the cursor on the host — proving the full SPI link works end-to-end with zero PS/2 code.

## Attempted Fixes

| # | Approach | Result |
|---|----------|--------|
| 1 | Diagonal motion (X=1,Y=1) at 2MHz SPI, function calls in ISR | Zig-zag cursor |
| 2 | Pre-computed RAM lookup table, no function calls in ISR | Still corrupt |
| 3 | SPI speed reduced to 1MHz | More byte time but gap unchanged |
| 4 | `SPDR = 0x00` written as first ISR instruction | SCK starts before ISR enters |
| 5 | Driver skips observation + product ID init checks | Init passes but data still shifted |

## What Did Work

- ZMK driver with Product ID at `0x3F` builds and flashes
- Pro Mini SPI slave handles writes and produces correct circle step values (confirmed via CH340 serial monitor)
- MOT/IRQ toggling triggers correct burst read sequence on the master
- `trackball@0` registers READY in the device tree
- Pro Mini Serial debug output is clean and predictable

## Root Cause

The nRF52840 SPIM with EasyDMA transfers SPI bytes **back-to-back with near-zero inter-byte gap** (~2-5 AVR cycles at 8MHz). The AVR SPI slave ISR needs at minimum `4 (hardware vector dispatch) + 1 (OUT SPDR)` = **5 cycles** just to write the next response byte. The next byte's SCK begins before the ISR can write SPDR, so the slave sends the **previous SPDR value** (the received address byte) instead of the intended response byte. Every byte shifts by one position.

The result: the master decodes X/Y from wrong nibble combinations, producing 500px random jumps instead of a clean circle. Lowering the SPI frequency does not help because the bottleneck is **inter-byte gap**, not byte duration.

## Conclusion

**Status: Failed** — the fix must be on the master (ZMK) side, not the slave. The AVR literally cannot satisfy the timing requirement of the nRF52840 SPIM.

## Next Experiment Direction

Three viable approaches for Exp03:

1. **Per-byte SPI transactions** — Modify `pmw3610_read()` in the driver to issue one byte per SPI transaction (CS asserted per-byte). The CS deassertion-reassertion time gives the AVR ~50µs+ to prepare the next byte.

2. **External SPI buffer** — Use a 74HC595/74HC165 shift register pair between the nRF52 and AVR to decouple the timing.

3. **Use a different MCU** — An RP2040 or Teensy LC has faster SPI slave response or can use PIO for cycle-accurate SPI.
