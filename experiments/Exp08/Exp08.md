# Exp08: Power Curve Smoothing — "Start Slow, Then Normal"

## Hypothesis

The current flat `x/=4; y/=4` scaling treats every PS/2 delta equally regardless of motion history. Replacing it with an activity ramp (that builds during sustained motion and resets instantly on idle) will give the TrackPoint a "starts slow, accelerates to normal" feel — more precise for small adjustments, responsive for large cursor movements.

## Architecture

No architecture change — only the scaling logic inside `update_from_ps2()` on the Pro Mini.

```
Before: raw_ps2 → x = -x → x /= 4 → 12-bit encode → burst[]
After:  raw_ps2 → x = -x → x * ramp/20 → 12-bit encode → burst[]
                            ↑
                      ramp: 0..20, builds with sustained motion,
                      resets to 0 when PS/2 goes idle
```

The `ramp` variable is a `uint8_t` that:
- **Increases** by 2 per active PS/2 packet (with motion), capped at 20
- **Resets to 0** instantly when `readPacket()` returns false (PS/2 idle)
- Takes ~100ms (10 active ticks at 100Hz) to reach full ramp

## Algorithm

### Ramp update

```
ramp += 2 per active tick
cap at 20
reset to 0 on idle
```

### Power curve

```
output = input * ramp / 20
```

| ramp | time (mag=1) | x=1 out | x=2 out | x=4 out | x=7 out |
|------|-------------|---------|---------|---------|---------|
| 0    | 0ms         | 0       | 0       | 0       | 0       |
| 2    | 10ms        | 0       | 0       | 0       | 0       |
| 4    | 20ms        | 0       | 0       | 0       | 1       |
| 6    | 30ms        | 0       | 0       | 1       | 2       |
| 8    | 40ms        | 0       | 0       | 1       | 2       |
| 10   | 50ms        | 0       | 1       | 2       | 3       |
| 14   | 70ms        | 0       | 1       | 2       | 4       |
| 18   | 90ms        | 0       | 1       | 3       | 6       |
| 20   | 100ms       | 1       | 2       | 4       | 7       |

At ramp=20 (full), output = raw PS/2 value — about 4× faster than the old `x/4` scaling for typical deltas. If the cursor is too fast, ZMK's `zip_xy_scaler` (divisor) can be added in the shield keymap.

### Instant decay

When `readPacket()` returns false (no PS/2 data in ~30ms timeout), `ramp = 0` immediately. Every new finger touch begins from zero.

## Changes by file

### `promini-trackpoint/trackpoint-spi-slave/trackpoint-spi-slave.ino`

1. Add `static uint8_t ramp = 0;` (file-scope, before `update_from_ps2`)
2. Replace `update_from_ps2()` body: remove `x /= 4; y /= 4;`, add ramp-based scaling
3. In `loop()`: add `ramp = 0;` in the `else` branch when `readPacket()` returns false
4. All else unchanged — SPI ISR, MOT pulsing, Serial debug

### `experiments/Exp08/Exp08.md`

This file.

### `Experiments.md`

Add Exp08 row.

## No changes

| File | Reason |
|------|--------|
| `zmk-pmw3610-driver/` | Zero driver changes per AGENTS.md |
| `zmk-trackpoint-shield/` | No ZMK changes needed |
| `PS2Trackpoint/` | Used as-is |

## Key decisions

1. **No `/4` baseline** — removed the flat division entirely. The ramp alone controls output level. At full ramp, raw PS/2 values pass through. The user can tune overall sensitivity via ZMK's `zip_xy_scaler` if needed.

2. **Ramp increment = 2 per tick** — reaches full ramp in ~100ms with continuous light motion. Harder presses (larger mag) don't ramp faster. This keeps the "starts slow" duration consistent regardless of press intensity. Can be tuned later.

3. **Ramp cap = 20** — gives 20 discrete levels, fine enough for smooth acceleration. `x * ramp / 20` avoids 16-bit overflow for any int8_t input.

4. **Instant decay on any idle** — even a single missed PS/2 packet resets ramp. This means quick lift-and-retap always starts from zero. No residual speed carried across taps.

5. **Clamp to int8_t** — defensive clamping on the multiplied output in case of edge cases. Not strictly needed for typical PS/2 values (0..±7) but prevents corruption from unexpected large inputs.

## Success criteria

- [ ] Light/fresh nub tap produces almost no cursor movement for ~30ms, then accelerates
- [ ] Sustained push reaches full cursor speed within ~100ms
- [ ] Lifting finger and retapping restarts from slow (no residual speed)
- [ ] Quick short flicks feel controlled (not jerky)
- [ ] No drift or unwanted motion when idle
- [ ] MOT timing, burst clear, Serial debug all unchanged
- [ ] All builds pass (Pro Mini GH Actions)

## Timing budget

| Scenario | PS/2 read | Ramp calc | Total (per tick) |
|----------|-----------|-----------|------------------|
| Active tracking | ~1-2ms | ~10µs | ~2ms |
| Idle (timeout) | ~30ms | ~1µs | ~30ms |

No change from Exp07 — ramp math is negligible overhead.

## Conclusion

**Exp08: Success!** The power curve smoothing is achieved through a two-layer approach:

### Pro Mini side — Activity ramp
- Formula: `output = x * ramp * 3 / 40`
- `ramp` increases by `+4` per active PS/2 tick, caps at 20, resets instantly on idle
- At ramp=0: output=0 (one tick dead zone, imperceptible)
- At ramp=20: output ≈ 1.5× raw value (6× original /4 scaling)
- This provides the "start slow" feel: each touch begins from zero and ramps up over ~50ms

### ZMK side — Static scaler
- `zip_xy_scaler 1 4` with built-in `track-remainders`
- Divides all HID reports by 4 with fractional remainder tracking
- Provides the 1px minimum movement the user needed

### Combined effect
| | Low ramp (light touch) | Full ramp (firm push) |
|---|---|---|
| Pro Mini burst | 0–1 | ~6 |
| HID (after /4) | ~1 per 4 ticks (3px/40ms) | ~1.5/tick (18px/10ms) |
| Feel | Precise, controlled | Fast, responsive |

### Key iterations
1. **Ramp only** — `x*ramp/20` (no /4) → too fast, not precise
2. **Ramp + /4** — `x*ramp/80` (±1 clamp tested then removed) → better but peak way too slow
3. **Driver slow zone** — `/2` on first burst units after idle → 500px jumps (reverted)
4. **Ramp + /4 + zip_xy_scaler** — `x*ramp*3/40` + `/4` scaler → 1px at low end, 6× at peak ✅

The final stack: `TrackPoint → PS/2 → Pro Mini (ramp: x*ramp*3/40) → SPI per-byte → ZMK driver → zip_xy_scaler 1/4 → HID → USB → cursor`

## Next experiment

Exp09: Map PS/2 TrackPoint buttons (left/right/middle) to ZMK keycodes via direct GPIO or input split.
