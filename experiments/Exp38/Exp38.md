# Exp38: Minimal serial logging — wake/sleep transitions only

## Hypothesis

The Pro Mini's serial logging is too noisy for normal use: motion coordinates spam during movement, an `SLP:` heartbeat spams during sleep, and a redundant `AWAKE` follows every `Woke!`. Reduce output to only the two meaningful transitions (sleep entry, wake exit), gated by a single compile-time config so the firmware can be built fully silent.

## Changes

`promini-trackpoint/trackpoint-i2c-slave/trackpoint-i2c-slave.ino` (branch Exp38, commit `aee50c2`):

### Removed
| Output | Reason |
|---|---|
| Boot banner `--- Exp36 ---` | Not a transition; boot is inferred from `Sleeping...` ~5s after power-on |
| `x,y` motion lines (main loop) | Spam during movement |
| `0` burst-clear line | Noise |
| `SLP:<count>` heartbeat | "How long it's sleep" — spam every ~8s |
| `W:x,y` wake-path motion line | Spam |
| `AWAKE` (after sleep loop) | Redundant — always pairs with `Woke!` |

### Kept
| Output | When |
|---|---|
| `Sleeping...` | sleep entry (before `TWCR = 0`, flushed) |
| `Woke!` | wake transition (motion detected) |

### Config
- `#define SERIAL_LOG 1` — master switch, now **compile-time** (`#if SERIAL_LOG`) instead of runtime `if`:
  - `SERIAL_LOG 1` → only `Sleeping...` / `Woke!`
  - `SERIAL_LOG 0` → `Serial.begin()` skipped entirely, zero serial code in the binary, UART off
- `wake_count` still increments each wake cycle and is served via reg `0x01` (ZMK liveness probe) — only the print was removed

## Verification

- Binary shrank **5724 → 5202 bytes** (522 bytes of serial code stripped)
- Serial output during a full cycle is exactly:
  ```
  Sleeping...
  Woke!
  ```
  No banner, no heartbeat, no motion coordinates
- reg `0x01` still readable via `i2c read i2c0 0x42 0x01 2` while awake → returned `0x013C` (316 wake cycles) — diagnostic counter intact
- Wake → I2C burst still works (unchanged wake path), cursor moves

## Conclusion

**Success.** Serial is now transition-only and fully configurable. When `SERIAL_LOG 0`, the firmware builds completely silent with no UART activity — ready for a future low-power build where serial isn't wanted.

## Notes for next experiments

- Sleep is still `SLEEP_MODE_IDLE` — true `SLEEP_MODE_POWER_DOWN` deep sleep remains the next Pro Mini goal.
- With `SERIAL_LOG 0` the UART is fully off, which will also help a future POWER_DOWN build (no UART interrupts waking the MCU).
