# Exp07: Real TrackPoint PS/2 Data via SPI Pipeline

## Hypothesis

The interrupt-driven SPI pipeline (Exp05, 100Hz MOT) can be fed real TrackPoint motion data by replacing the synthetic rectangle pattern with the PS2Trackpoint library (Exp06-proven). The PS/2 data rate (~100-120Hz) matches the MOT rate, so each tick gets fresh X/Y deltas and the cursor follows the nub.

## Architecture

```
TrackPoint ──PS/2 (D7=CLK, D3=DAT)──→ Pro Mini ──SPI (per-byte, CS deasserted)──→ NiceNano ZMK driver
                                              ↑
                                    ISR(SPI_STC_vect) handles SPI bytes
                                    Main loop: ps2.readPacket() + MOT timing
```

## Changes by file

### `promini-trackpoint/trackpoint-spi-slave/trackpoint-spi-slave.ino`

Replace Exp05's `update_rect_step()` (synthetic rectangle pattern) with real PS/2 data:

1. **Add PS2Trackpoint** — `#include <PS2Trackpoint.h>`, instantiate with D7=CLK, D3=DAT
2. **Remove rectangle** — Delete `rect_data[]` PROGMEM table, `update_rect_step()`, `segment`, `step_in_seg`
3. **Add `update_from_ps2(x, y)`** — Converts `int8_t` x,y to 12-bit two's complement PMW3610 format, atomically updates burst array with `cli()`/`sei()`
4. **Main loop** — Call `ps2.readPacket(x, y, buttons)` each iteration; if valid, call `update_from_ps2()`; delete stale values after 500ms of no PS/2 data (clear burst to zero)
5. **All else unchanged** — SPI ISR, MOT pulsing (100Hz), serial debug — identical to Exp05c

### `experiments/Exp07/Exp07.md`

This file.

### `Experiments.md`

Add Exp07 row.

## No changes

| File | Reason |
|------|--------|
| `zmk-pmw3610-driver/` | Zero driver changes per AGENTS.md |
| `zmk-trackpoint-shield/` | No config changes needed |
| `PS2Trackpoint/` | Used as-is |
| `.github/workflows/compile.yml` | Already compiles SPI slave with `./libraries` PS2Trackpoint |

## Key decisions

1. **Use `ps2.readPacket()` as-is** — The 40µs timeout per byte is the biggest concern (30ms idle blocking), but during active tracking PS/2 bytes arrive at ~100µs each, so `readPacket` returns in <2ms. 30ms idle timeout is acceptable — MOT just fires less frequently, and the ZMK driver sees zero motion.

2. **Y-axis sign** — TrackPoint positive Y = down (pull toward you). PMW3610 positive Y convention is sensor-dependent. Burst data passes through natural TrackPoint values; shield can set `invert_y` if needed.

3. **Buttons deferred** — PMW3610 burst has no button field. Mapping buttons to ZMK keycodes can be added as a separate mechanism later.

4. **No init commands** — Per Exp06 finding: this IC streams on power-up and rejects host commands. Just `ps2.begin()` + 2s delay.

## Stale data handling

If no valid PS/2 packet arrives for 500ms (nub idle), clear burst motion to zero. This prevents the ZMK driver from re-reading stale deltas.

## Success criteria

- [x] Pro Mini builds via GH Actions (spi-slave artifact with PS2Trackpoint)
- [x] NiceNano boots, `trackball@0` READY — cursor moves with nub
- [x] Host cursor moves in the direction of nub push (after X axis reversal)
- [x] Smooth tracking — speed scaling (x/4, y/4) gives fine control
- [x] No stale repeats — burst cleared after MOT pulse ends (fixes triple-move on finger lift)
- [x] No choppy X/Y alternating — resolved by burst clear fix
- [ ] Buttons (left/right/middle) — not yet mapped
- [ ] Runs 10+ minutes — not formally tested but stable across multiple sessions

## Timing budget

| Scenario | readPacket duration | MOT interval |
|----------|-------------------|--------------|
| Active tracking (~100Hz PS/2) | ~1-2ms | Every 10ms |
| Idle (no nub touch) | ~30ms (timeout) | Every ~30ms during idle |

Well within AVR Pro Mini limits.

## Findings

### 1. X axis reversed
The TrackPoint nub's X direction was opposite the screen cursor. Fixed by negating X in `update_from_ps2()`.

### 2. Sensitivity too high (1px input → ~10px cursor)
Raw PS/2 deltas (1-3 per packet) were encoded into the 12-bit PMW3610 format and reported directly. ZMK accumulated and submitted them with no scaling. Fixed by `x /= 4; y /= 4;` before 12-bit encoding, giving precise cursor control.

### 3. Stale motion repeated 3x after finger lift (worst bug)
When the user stopped touching the nub, the last motion packet stayed in the burst array. The ZMK driver reads burst on every MOT interrupt (10ms). Without new PS/2 data, the same stale values were read multiple times, causing 2-3 extra cursor jumps.

**Root cause:** The MOT pulse signals "data ready" to ZMK, but the burst was only updated when a new PS/2 packet arrived. Between packets, the burst held stale data.

**Fix:** Clear `burst[0..3] = 0x00` atomically right after `digitalWrite(MOT_PIN, HIGH)` (end of MOT pulse). Each MOT tick reads fresh data exactly once; subsequent reads see zero until next PS/2 packet.

### 4. Choppy X-then-Y alternating movement
Resolved as a side effect of fix #3. Stale data caused partial burst reads where X and Y were consumed on different MOT ticks. Clearing burst after MOT ensures both X and Y update atomically and get read in the same burst.

## Conclusion

**Exp07: Success!** The complete pipeline is proven end-to-end:

```
TrackPoint ──PS/2──→ Pro Mini ──SPI (per-byte)──→ NiceNano (ZMK PMW3610 driver) ──USB HID──→ PC cursor
```

Key achievements:
1. **No ZMK driver changes** — per-byte transactions (Exp04) and interrupt-driven SPI (Exp05) were reused as-is
2. **Non-blocking PS/2 integration** — `ps2.readPacket()` called each main loop iteration works despite its 30ms idle timeout
3. **Cursor follows nub** — smooth, responsive, correctly oriented
4. **No stale repeats** — burst clear after MOT eliminates the triple-jump issue
5. **Usable sensitivity** — x/4, y/4 gives fine-grained cursor control

The AVR Pro Mini successfully serves as a PMW3610 SPI emulator bridging PS/2 TrackPoint data to ZMK.

## Next experiment

Exp08: Add ZMK-side button reporting. The PMW3610 burst has no button field, so buttons need a separate mechanism — either a second sensor channel via GPIO, or mapping the TrackPoint's PS/2 buttons to ZMK keycodes via a direct GPIO connection to the NiceNano.
