# Exp03: 1-Byte Shift Compensation

## Hypothesis

The nRF52 SPIM inter-byte gap is near-zero, causing a **deterministic** 1-byte shift in the AVR's SPI responses. Since the shift is consistent (`master buf[1] = AVR burst[0]`, `buf[2] = burst[1]`, `buf[3] = burst[2]`), the AVR can compensate entirely in firmware by pre-shifting the burst array layout. At 1MHz SPI (8µs/byte vs ~3µs ISR), the `SPDR=burst[burst_idx++]` write reliably completes within the current byte, pre-loading the write buffer for byte N+2. Zero driver changes, zero hardware changes.

## Plan

1. Fix `PCINT0_vect` to only act on CS **falling** edge, eliminating a race with `SPI_STC_vect` on CS rising
2. Verify burst array layout is already correct (`burst[0]=X_L`, `burst[1]=Y_L`, `burst[2]=XY_H`)
3. Build via GitHub Actions, flash Pro Mini and Nicenano
4. Verify cursor moves in smooth circle via USB logging

## Timing Trace (1MHz SPI, 8MHz AVR — 64 cycles/byte)

```
CS↓ PCINT:  SPDR=0x00 → write_buffer=0x00

Byte 0 (addr 0x12): AVR sends 0x00. Ends. Byte 1 starts (~0 gap), loads wb=0x00
  → master discards (addr echo)

Byte 1 (data 0): AVR sends 0x00 → master buf[0]=0x00 (motion, ignored)
  ISR runs during byte1 (3µs/64µs): SPDR=burst[0] → wb=burst[0]

Byte 2 (data 1): loads wb=burst[0] → master buf[1]=burst[0] → X_L ✓
  ISR: SPDR=burst[1] → wb=burst[1]

Byte 3 (data 2): loads wb=burst[1] → master buf[2]=burst[1] → Y_L ✓
  ISR: SPDR=burst[2] → wb=burst[2]

Byte 4 (data 3): loads wb=burst[2] → master buf[3]=burst[2] → XY_H ✓
  ISR: SPDR=burst[3] → ...continues
```

## Files Changed

| File | Change |
|------|--------|
| `promini-trackpoint/trackpoint-spi-slave/trackpoint-spi-slave.ino` | PCINT0_vect: only act on CS falling edge |
| `experiments/Exp03/Exp03.md` | This file |
| `Experiments.md` | Add Exp03 row |

## No Changes

| File | Reason |
|------|--------|
| ZMK driver (`pmw3610.c`, `pmw3610.h`) | Zero driver changes per AGENTS.md |
| Shield overlay/conf | 1MHz SPI stays, no changes needed |
| PS/2 library | Not yet integrated, still synthetic circle |

## Success Criteria

- [ ] Pro Mini builds via GitHub Actions and flashes successfully
- [ ] Nicenano boots, serial shell responds, `trackball@0` READY
- [ ] USB logging shows `x/y:` values matching circle pattern (3/0, 3/1, 2/2, -1/3, ...)
- [ ] Host cursor moves in smooth circle (no zig-zags, no 500px random jumps)

## Findings

### Phase 1 — PCINT reset + SPI byte counter (failed)

The 1-byte shift compensation approach was tried in multiple iterations. The Nicenano's PMW3610 debug logs (enabled via `kernel log_level pmw3610 4`) show the burst hex dump, which revealed the true data:

```
buf    11 ff ff 33 ff ff ff    diagnostic values {0x11,0x22,0x33,…}
```

burst[0]=0x11 lands at buf[0] (not buf[1] — ISR is fast enough during the EasyDMA list-transition gap between addr byte and first data byte).  

buf[1] and buf[2] are **always 0xFF** (the master's dummy byte received by the AVR). The intended burst[1]=0x22 and burst[2]=0x33 are **lost**.

### Root cause: AVR SPDR is single-buffered for receive

The ATmega328P's SPI Data Register serves dual purpose:

| Operation | Effect on SPDR |
|-----------|---------------|
| Write SPDR | Sets TX data for next byte |
| Read SPDR | Returns received byte from previous byte |
| **Auto at byte end** | **Received byte overwrites SPDR** |

When a byte finishes:
1. The received byte is transferred from the shift register → **SPDR, overwriting any TX data that was written** during that byte.
2. The next byte starts **immediately** (nRF52 SPIM EasyDMA has zero inter-byte gap within the same DMA entry).
3. The shift register loads from SPDR — which now contains the **received byte** (0xFF master dummy), not the intended burst data.

The TX write (`SPDR = burst[burst_idx]`) happens at ~10 AVR cycles into the current byte. But the received data write happens at the byte boundary — and since there's zero gap, the next byte's shift register load also happens at the boundary. The result: the received byte (0xFF) is transmitted instead of the burst data.

A per-byte approach (CS deasserted between bytes) would fix this because the CS assertion-deassertion gap gives the AVR time to read SPDR (consuming the received byte) and write back the TX data before the next byte starts. But per-byte transactions require modifying the ZMK driver — prohibited per AGENTS.md.

## Conclusion

**Status: Failed** — the ATmega328P cannot serve as an SPI slave to the nRF52840 SPIM when bytes within a DMA entry are back-to-back (zero gap). The single-buffered receive overwrites the TX data before the next byte can use it.

Three remaining paths:

1. **RP2040 (Pico/XIAO) — recommended.** PIO handles SPI slave with cycle-accurate, zero-CPU-overhead response. The TX and RX are handled by FIFOs, eliminating the register-overwrite race entirely. Cost: ~$5. Requires rewriting firmware for PIO + PS/2.

2. **External shift registers — (74HC165 + 74HC595).** Decouple the SPI timing: shift registers handle the nRF52's back-to-back clock, AVR reads/writes in parallel at leisure. Requires ~12 extra wires and 2 chips.

3. **Per-byte driver modification — (not allowed per AGENTS.md).** Split `pmw3610_read()` into CS-per-byte transactions. The CS deassertion gives the AVR ample time to read SPDR and reload TX data. This is the cleanest software fix but violates the "zero driver changes" constraint.
