# Exp04: Per-Byte SPI Transactions

## Hypothesis

Splitting the burst read into individual 1-byte `spi_transceive_dt` calls (with CS deasserted between each) gives the AVR Pro Mini enough inter-byte gap (~tens of µs) to read SPDR and write the next response. The 1-byte-shift race is eliminated because the AVR processes each byte at leisure between transactions. At 1MHz SPI with ~10-20µs transaction overhead, a 7-byte burst takes ~130-200µs — still fast enough for a pointing device.

## Changes by file

### `zmk-pmw3610-driver/src/pmw3610.c`

`pmw3610_read()` split into per-byte transactions:
- Transaction 1: address byte (CS↓, 1 byte, CS↑)
- Transactions 2..N+1: one per data byte (CS↓, 1 byte, CS↑)

`pmw3610_write_reg()` stays as 2-byte single transaction — writes are safe because:
- The ~8µs window (at 1MHz) between byte 0 and byte 1 is enough for the AVR polling loop to catch the address byte before the data byte overwrites SPDR.
- Master ignores MISO during writes.

`pmw3610_read_reg()` calls `pmw3610_read()` so it's automatically handled.

### `promini-trackpoint/trackpoint-spi-slave/trackpoint-spi-slave.ino`

New SPI state machine:

| State | On SPIF (byte complete) |
|-------|------------------------|
| `S_IDLE` | This byte is an **address**. Store it. If addr == 0x12 → `S_BURST`, else `S_ADDR_RCVD`. Pre-load SPDR with response. |
| `S_ADDR_RCVD` | Single-register data. Value was pre-loaded after address. Return to `S_IDLE`. |
| `S_BURST` | Burst data. `burst_idx++`. Pre-load next `burst[burst_idx]`. When `burst_idx >= 7`, return to `S_IDLE`. |

Burst array expanded to 7 bytes matching PMW3610 register layout:
- `burst[0] = 0x01` (MOTION — motion detected)
- `burst[1] = X_L` (DELTA_X_L)
- `burst[2] = Y_L` (DELTA_Y_L)
- `burst[3] = XY_H` (DELTA_XY_H)
- `burst[4-6] = 0x00` (SQUAL, SHUTTER_H, SHUTTER_L)

Circle pattern stays for testing — no PS/2 integration yet.

### `Experiments.md`

Add Exp04 row.

## Build

1. Push `promini-trackpoint` Exp04 → GH Actions compiles AVR firmware
2. Push `zmk-pmw3610-driver` Exp04 → any push
3. Push `zmk-trackpoint-shield` Exp04 (west.yml → driver Exp04) → GH Actions builds ZMK UF2

## Flash

1. Pro Mini: `avrdude` via Alpine WSL
2. NiceNano: `flash-nicenano.ps1`

## Verification

1. Serial shell responds on NiceNano (`uart:~$`)
2. `kernel log_level pmw3610 4` → burst hex dump shows correct X_L/Y_L/XY_H at buf[1-3]
3. Host cursor moves in smooth circle (MOT pulses every 500ms)

## Timing

| Step | Duration |
|------|----------|
| Per-byte transaction (CS↓ + 1 byte @ 1MHz + CS↑) | ~8µs SPI + ~10-20µs overhead ≈ 18-28µs |
| 7-byte burst read total | ~130-200µs |
| AVR SPIF poll latency | ~0.3-0.5µs |

## Why this works and Exp03 didn't

Exp03 tried compensating the 1-byte shift within a single continuous transaction (zero gap). The AVR's SPDR is single-buffered for RX — the received byte overwrites TX data at the byte boundary with no time to recover.

Exp04 splits into separate transactions. CS deassertion creates a guaranteed gap of tens of µs. The AVR processes each byte (read SPDR, write next response) at leisure. When CS↓ arrives for the next byte, SPDR is already loaded — the SPI hardware shifts it out automatically.

## Results

All success criteria met!

### Builds
- [x] GH Actions builds succeed (all 3 repos) — Arduino: 29s, ZMK: 4m17s
- [x] Pro Mini flashes and responds (2978 bytes written and verified via avrdude)
- [x] `trackball@0` READY in `device list`

### Burst hex dump (debug log)
```
01 03 01 00 00 00 00  → X=3,  Y=1  ✓
01 02 02 00 00 00 00  → X=2,  Y=2  ✓
01 01 03 00 00 00 00  → X=1,  Y=3  ✓
01 00 03 00 00 00 00  → X=0,  Y=3  ✓
01 ff 03 f0 00 00 00  → X=-1, Y=3  ✓
01 fe 02 f0 00 00 00  → X=-2, Y=2  ✓
```

`buf[1]` = X_L, `buf[2]` = Y_L, `buf[3]` = XY_H — perfectly aligned, no 1-byte shift.
- [x] Burst hex dump shows correct values (no shift)
- [x] Cursor moves in smooth circle (values cycle through circle pattern)

## Conclusion

**Exp04: Success.** The per-byte SPI transaction approach eliminates the AVR single-buffered SPDR race that made Exp02 and Exp03 fail. By splitting the burst read into individual 1-byte transactions with CS deassertion between each, the AVR Pro Mini has guaranteed time (~tens of µs) to read SPDR and write the next response before the next byte begins.

Key changes:
1. `pmw3610_read()` in the ZMK driver modified to issue 1-byte `spi_transceive_dt` calls
2. AVR state machine handles per-byte transactions (S_IDLE → S_ADDR_RCVD or S_BURST)
3. Writes remain as 2-byte continuous transactions (safe at 1MHz, 8µs window between bytes)

## Next Experiment (Exp05)

Integrate the PS2Trackpoint library to read real trackpoint motion and pipe it through the SPI burst read. The AVR state machine is proven — the burst array just needs to be populated from PS/2 data instead of the synthetic circle pattern.
