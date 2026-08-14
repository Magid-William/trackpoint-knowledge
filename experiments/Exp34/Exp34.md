# Exp34: Diagnose permanent I2C lockup — cap consecutive_errors + Pro Mini wake logging

## Hypothesis

After 5–15 minutes of Pro Mini deep sleep, the trackpoint never recovers (requires reset). The root cause is theorized to be:

1. **ZMK driver side**: `uint8_t consecutive_errors` wraps at 255 (~17.7 min at 5s polling). After the wrap, `poll_interval_ms` drops to 10ms, flooding I2C with rapid failed transactions. This burst triggers nRF52840 TWIM Errata 78 (TWI stuck in BUSY state after repeated NACKs), permanently locking the I2C peripheral — every subsequent read fails, the Pro Mini stays in its sleep loop, and the cursor never recovers.

2. **Pro Mini side**: The MCU is likely still cycling its sleep/wake loop every 60ms, but the ZMK can no longer reach it. Need to confirm with heartbeat logging.

## Changes

### promini-trackpoint (branch Exp34)

| Change | Detail |
|--------|--------|
| `static uint16_t wake_count` | Monotonic counter incremented each sleep-wake cycle |
| Register `0x01` | New read-only register returns `wake_count` as 2 bytes (big-endian) so ZMK can probe liveness |
| LED heartbeat | Toggle D13 each wake cycle — visible proof of life |
| Serial log | `SLP:<count>` every 100 wake cycles (low-bandwidth, confirms cycling indefinitely) |

### zmk-pmw3610-driver (branch Exp34)

| File | Change |
|------|--------|
| `src/trackpoint-i2c.c` | Cap `consecutive_errors` at 200 to prevent uint8_t wrap. After 200 errors, the counter stops incrementing, `poll_interval_ms` stays at 5000ms permanently — no 10ms flood burst ever occurs. |

### zmk-config-dabaseV_0-2 (branch Exp34)

| File | Change |
|------|--------|
| `config/west.yml` | `zmk-pmw3610-driver` pinned to `Exp34` |

## Diagnostic register map

| Address | Access | Returns |
|---------|--------|---------|
| `0x01` | read | `wake_count` (uint16_t big-endian) — number of sleep-wake cycles since boot |
| `0x11` | write | Set speed scale (unchanged) |
| `0x12` | burst read | X, Y motion deltas (unchanged) |

## Verification

1. Flash Pro Mini with Exp34, then observe D13 — should blink at ~8 Hz during sleep (toggle every 60ms cycle = blink at ~8.3 Hz)
2. Connect serial (115200 baud) and watch for `SLP:100`, `SLP:200`, etc. every ~6 seconds during sleep
3. From ZMK shell, probe `i2c write_read <bus> 0x42 0x01 2` to read wake_count while sleeping — should return a non-zero, incrementing value
4. Leave idle for 20+ minutes. Check if D13 still blinks (Pro Mini alive), ZMK can still read wake_count via I2C, and cursor recovers within 5s of touching the trackpoint
5. If lockup still occurs despite the cap, the root cause is elsewhere (nRF52 TWIM erratum triggered by errors alone, not by burst rate)

## Key findings

TBD after testing.

## Conclusion

TBD after testing.
