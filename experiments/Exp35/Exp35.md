# Exp35 — Code cleanup + optional deep sleep

## Hypothesis

The Pro Mini I2C slave firmware and PS2Trackpoint library accumulated dead code across multiple experiments. Removing unused functionality reduces binary size, improves maintainability, and makes the codebase easier to work with for future experiments. Deep sleep should be optional and default-off for debugging.

## Plan

1. Remove NPN/PMOS gate control pins — previously used for trackpoint power switching (buggy, never worked reliably)
2. Remove unused `LED_PIN` define
3. Remove all calibration code — `calib_end_ms` was never assigned, so calibration always produced 0 offset. Dead code since introduction.
4. Remove unused library methods: `sendByte()`, `reset()`, `enableStreaming()`, `lastError()`, Error enum, `_lastError`
5. Make `readByte()` private — it's an internal implementation detail
6. Remove blocking (`timeout=0`) branch from `readByte()` — only used by removed `reset()`
7. Remove `#include <LowPower.h>` — sleep uses `avr/sleep.h` directly
8. Add `SLEEP_ENABLED` flag (default 0) guarded by `#if` to conditionally compile sleep code

## Changes

### `promini-trackpoint/trackpoint-i2c-slave/trackpoint-i2c-slave.ino`
- Removed `NPN_PIN`, `PMOS_PIN`, `LED_PIN` defines + setup code (lines 10–12, 72–75)
- Removed `#include <LowPower.h>`
- Removed 7 calibration variables + all calibration logic from main loop and sleep loop
- Removed calibration offset subtraction — raw x/y used directly (was always 0 offset)
- Added `#define SLEEP_ENABLED 0` with `#if SLEEP_ENABLED` guard around sleep includes and sleep block

### `promini-trackpoint/libraries/PS2Trackpoint/PS2Trackpoint.h`
- Removed `sendByte()`, `reset()`, `enableStreaming()` declarations
- Removed `lastError()` accessor and `_lastError` member
- Removed `Error` enum
- Made `readByte()` private (default timeout 40000)

### `promini-trackpoint/libraries/PS2Trackpoint/PS2Trackpoint.cpp`
- Removed `sendByte()`, `reset()`, `enableStreaming()` methods
- Removed error tracking from `readByte()` (`_lastError` assignments → simplified returns)
- Removed blocking `timeout==0` branch (only used by `reset()`)

## Commits

| Commit | Change |
|--------|--------|
| `b525c60` | Exp35: cleanup — remove NPN/PMOS gates, calibration, LED, dead library code |
| `593f42c` | Exp35: make deep sleep optional via SLEEP_ENABLED flag (default off) |

## Findings

- Calibration was guaranteed-0: `calib_end_ms` defaulted to 0, so `millis() >= 0` was always true on first check
- Removing calibration saved ~316 bytes of flash (5972 → 5656 bytes)
- Disabling sleep saved another ~890 bytes (5656 → 4766 bytes)
- Total savings from Exp27 baseline: 5972 → 4766 bytes (1206 bytes, 20%)
- `sendByte()` was only called by `reset()` and `enableStreaming()` — neither called anywhere in any sketch
- Error tracking in `readByte()` was never checked by any caller
- No behavioral change: calibration was always 0, sleep guard is compile-time switchable

## Result

**Success.** Codebase is 20% leaner, sleep is optional and off by default. No behavioral regressions — trackpoint reads and I2C communication are identical to Exp27/Exp34.

## Next

- Re-enable sleep (`SLEEP_ENABLED 1`) once debugging is complete
- Consider deeper cleanup: the sleep loop duplicates the main loop logic — could be refactored into a shared function
