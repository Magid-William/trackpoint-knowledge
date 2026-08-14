# Exp31: Error-tolerant I2C polling with exponential backoff

## Hypothesis

When the Pro Mini enters deep sleep, its I2C slave goes non-responsive (`TWCR = 0`). The ZMK driver was polling `i2c_write_read_dt()` every 10ms regardless, wasting ~1-2ms per failed transaction. This starved the keyboard matrix scan and BLE stack on the nRF52, making the right half feel dead until the Pro Mini is physically reset.

Fix this entirely on the ZMK driver side — no Pro Mini firmware changes.

## Changes

### zmk-pmw3610-driver (branch Exp31)

| File | Change |
|------|--------|
| `src/trackpoint-i2c.c` | Added `consecutive_errors` counter and `poll_interval_ms` to data struct. On I2C error: increments counter, applies backoff (3 err → 100ms, 10 err → 1s, 50 err → 5s). On success: resets to 10ms instantly. |

### zmk-config-dabaseV_0-2 (branch Exp31)

| File | Change |
|------|--------|
| `config/west.yml` | Driver pinned to `Exp31` branch |

### promini-trackpoint

No changes.

## Backoff thresholds

| Consecutive errors | Poll interval | Notes |
|---|---|---|
| 0–2 | 10 ms | Normal operation |
| 3–9 | 100 ms | Transient glitch / brief sleep |
| 10–49 | 1 s | Pro Mini likely in deep sleep |
| 50+ | 5 s | Deep sleep, minimum wake-up traffic |

On the first successful `i2c_write_read_dt()` after any error streak, `consecutive_errors` resets to 0 and `poll_interval_ms` drops back to 10ms.

## No MOT changes

The MOT/IRQ pin is not used for gating — this is purely error-driven backoff. The GPIO callback at `trackpoint-i2c.c:123` remains a no-op.

## Trade-off

When the Pro Mini wakes up (TrackPoint generates motion), the ZMK driver may not notice for up to the current backoff interval (max 5s at deepest sleep). This is acceptable because:
- Typing doesn't start with a trackpoint motion
- The delay only applies to the *first* motion after prolonged idle
- After the first successful poll, interval drops to 10ms instantly

## Verification

1. Build right half firmware from dabase config (Exp31)
2. Flash Pro Mini with Exp26 I2C slave (no changes needed)
3. Let Pro Mini idle for 30s → it enters sleep
4. Check ZMK serial log: I2C errors appear, poll interval backs off
5. Touch trackpoint → within 5s, cursor starts moving again
6. Verify keyboard keys on right half remain responsive during sleep

## Key findings

- [x] I2C polls slow to 5s during Pro Mini sleep
- [x] Keyboard matrix scan no longer starved — right half keys work
- [x] Cursor resumes within 5s of touching trackpoint after sleep
- [x] No Pro Mini reset needed to recover

## Conclusion

**Success.** The exponential backoff eliminates the CPU waste from polling a sleeping I2C slave. The right half remains fully responsive during Pro Mini sleep, and the cursor recovers automatically when the trackpoint is touched again. No hardware changes, no Pro Mini firmware changes — pure ZMK driver fix.
