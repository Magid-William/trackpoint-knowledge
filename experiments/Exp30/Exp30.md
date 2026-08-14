# Exp30: Move speed divider from ZMK driver to Pro Mini firmware

## Hypothesis

The DT-configurable `divider` in the ZMK driver (`trackpoint-i2c.c`) causes choppy cursor movement because the accumulator + divide + remainder logic runs at the ZMK poll rate (100 Hz), which doesn't align with the PS/2 sample rate (50 Hz). Reports oscillate between 0 and 2 instead of smooth 1s.

Move the divider to the Pro Mini firmware where per-sample division happens naturally at each PS/2 read (50 Hz). Remainder tracking preserves sub-count precision across samples.

## Changes

### zmk-pmw3610-driver — reverted

| File | Change |
|------|--------|
| `dts/bindings/promini,trackpoint-i2c.yml` | Removed `divider` property entirely |
| `src/trackpoint-i2c.c` | Removed `divider` from config struct. Removed divider/remainder logic from poll function. Simplified to direct dx/dy report and clear. |

No DT property, no division, no remainder tracking in the driver. The driver now reports accumulated motion directly as received from the Pro Mini.

### promini-trackpoint

| File | Change |
|------|--------|
| `trackpoint-i2c-slave.ino` | Added `#define SPEED_DIVIDER 2`. Added `rem_x`/`rem_y` remainder accumulators. Both main loop and sleep wake paths apply `x / SPEED_DIVIDER` with remainder tracking before assigning to `burst_x`/`burst_y`. |

### zmk-trackpoint-shield

No changes — the overlay never had `divider` set.

## Divider logic

```c
int32_t tx = x + rem_x;
int32_t ty = y + rem_y;
int8_t dx = tx / SPEED_DIVIDER;
int8_t dy = ty / SPEED_DIVIDER;
rem_x = tx - dx * SPEED_DIVIDER;
rem_y = ty - dy * SPEED_DIVIDER;
burst_x = dx;
burst_y = dy;
```

Division happens once per PS/2 sample (50 Hz), and remainders carry forward to the next sample. This preserves sub-count precision without the ZMK-side accumulator oscillation.

## Success

- [x] `SPEED_DIVIDER 1` = unchanged behavior (full speed)
- [ ] `SPEED_DIVIDER 2` = smooth half-speed cursor, no choppiness — **failed: jitter**
- [ ] Remainder tracking prevents motion loss over time
- [x] ZMK driver has zero divider code — pure revert

## Findings

With `SPEED_DIVIDER 2` on the Pro Mini the cursor jitters instead of moving at a smooth half speed — reports oscillate rather than delivering steady deltas. The 50 Hz per-sample division with remainder carry did not reproduce the smoothness the ZMK-side accumulator approach was supposed to fix; the perceived speed change introduces visible oscillation. The divide-down approach itself (at either side) needs rethinking.

## Conclusion

**Failed — the speed change caused jitter.** Moving the divider to the Pro Mini traded ZMK-side choppiness for worse AVR-side jitter. Driver-side divider stays removed (clean revert).

## Next experiment suggestion

Don't divide raw counts at fixed ratio at a single point. Try one of:
1. Re-add the divider to the ZMK driver but with a **smoothed ramp** (like Exp08's power curve) instead of raw divide — ramp at poll rate, not per-sample.
2. Reduce the PS/2 sensitivity on the TrackPoint side (Sensitivity register / no-decel) instead of dividing after the fact.
3. Scale in ZMK with a `zip_xy_scaler` style input-processor (existing, already proven in Exp32 scroll work) rather than touching the driver at all.
