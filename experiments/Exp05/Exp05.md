# Exp05: Rectangle Speed & Smoothness Test

## Hypothesis

Replacing the synthetic circle pattern with a rectangle trace (4 segments at distinct speeds) lets us visually evaluate the per-byte SPI pipeline: smoothness, jitter, and whether speed differences are distinguishable. The MOT rate determines the "frame rate" of cursor updates — too low and movement looks choppy regardless of SPI correctness.

## Evolution

The experiment went through 3 iterations:

### Exp05a (50ms MOT, polling SPI — 20 Hz)

Initial rectangle at the same 500ms → 50ms MOT rate as the circle.

| Segment | Delta | Steps | Speed | Duration |
|---------|-------|-------|-------|----------|
| Fast right | +10, 0 | 20 | 200 px/s | ~1.0s |
| Slow down | 0, +1 | 100 | 20 px/s | ~5.0s |
| Normal left | −5, 0 | 40 | 100 px/s | ~2.0s |
| Extra fast up | 0, −10 | 10 | 200 px/s | ~0.5s |

**Result:** Movement visible and correct, but all segments looked "20 fps". The 50ms MOT rate meant only 20 updates/second — each step was a discrete jump, especially the fast segments with 10px deltas.

### Exp05b (10ms MOT, polling SPI — 100 Hz)

Reduced MOT period to 10ms and scaled deltas down to keep the same screen speeds.

| Segment | Delta | Steps | Speed | Duration |
|---------|-------|-------|-------|----------|
| Fast right | +2, 0 | 100 | 200 px/s | ~1.0s |
| Slow down | 0, +1 | 200 | 100 px/s | ~2.0s |
| Extra fast left | −4, 0 | 50 | 400 px/s | ~0.5s |
| Normal up | 0, −2 | 100 | 200 px/s | ~1.0s |

**Result:** Movement was perfectly smooth — no more "20 fps" stepping. But after ~30s the mouse would stop, then resume on its own. Root cause: `Serial.print()` at 9600 baud blocks the main loop for ~20ms, and the polling-based SPIF handler misses entire 7-byte bursts during that window. The SPI state machine gets corrupted, then recovers on the next CS transition.

### Exp05c (10ms MOT, interrupt-driven SPI — final)

Moved the SPI byte handling into `ISR(SPI_STC_vect)` so every byte is processed immediately, even during `Serial.print()`. Baud rate bumped to 115200 for faster serial output.

**Result:** Rock solid. Tested for 30+ minutes with no stutters or stops.

## Files Changed

| File | Change |
|------|--------|
| `promini-trackpoint/trackpoint-spi-slave/trackpoint-spi-slave.ino` | Circle → rectangle pattern, `PERIOD_MS` 500→50→10, polling SPIF → `SPI_STC_vect` ISR, baud 9600→115200 |
| `experiments/Exp05/Exp05.md` | This file |
| `Experiments.md` | Add Exp05 row |

## No Changes

| File | Reason |
|------|--------|
| `zmk-pmw3610-driver/` | Per-byte transactions from Exp04 unchanged |
| `zmk-trackpoint-shield/` | No config changes needed |

## Key Technical Insight

The ATmega328P's SPIF polling is vulnerable to blocking operations: `Serial.print()` at 9600 baud takes ~20ms per line, during which a complete 7-byte burst (taking ~200µs) arrives and goes unhandled. The state machine misinterprets data bytes as addresses and vice versa.

Switching to interrupt-driven SPI (`SPI_STC_vect`) is essential for reliable operation at high MOT rates. The ISR handles each byte in ~3µs, making blocking delays irrelevant.

## Success Criteria

- [x] Pro Mini builds via GH Actions and flashes successfully (3024 bytes, verified)
- [x] Nicenano boots, `trackball@0` READY
- [x] USB logging shows correct X/Y deltas in burst hex dump
- [x] Cursor traces a rectangle: fast right, slow down, extra fast left, normal up
- [x] Movement is **smooth** — no sudden jumps, jitter, or zig-zags at any speed
- [x] Speed differences are visually distinguishable
- [x] Runs continuously for 30+ minutes without stopping

## Conclusion

**Exp05: Double Success.** The per-byte SPI pipeline (Exp04) delivers smooth, glitch-free motion at 100 Hz. The rectangle test confirms:
1. The AVR Pro Mini can serve as a stable PMW3610 SPI emulator at 100 Hz update rate
2. Interrupt-driven SPI is required for reliability
3. The ZMK driver correctly processes the burst data and moves the cursor

## Next Experiment (Exp06)

Integrate the PS2Trackpoint library to read real trackpoint motion and pipe it through the SPI burst read. The rectangle pattern is proven — just replace `update_rect_step()` with a PS/2 data source.
