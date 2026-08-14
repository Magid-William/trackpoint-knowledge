# Exp23: Pin ZMK revision to rule out main branch regression

## Hypothesis

The cursor stopped moving because ZMK `main` (pulled via `west.yml`) changed between the working build (July 26) and now. The overlay, `.conf`, and driver source are identical — only the ZMK/Zephyr environment differs. Pinning ZMK to the last known-working commit from July 26 will restore cursor movement.

If it does, the root cause is confirmed as a ZMK main regression. Future experiments should pin ZMK to a specific tag (e.g., `v24.11.0`) instead of `main`.

If the cursor still doesn't move, the root cause is in our code (driver, overlay, or conf) and deeper debugging is needed.

## Changes

| File | Change |
|------|--------|
| `config/west.yml` | `revision: main` → `revision: fa33e35f11d2b15311973cda9fb89dcd2376888c` |

## Test procedure

1. Push `Exp23` branch → GitHub Action builds
2. Download `corne_trackpoint_right-nice_nano-standalone.uf2`
3. Flash NiceNano via `flash-nicenano.ps1`
4. Check USB log for:
   - `trackpoint-i2c INIT OK`
   - `I2C INIT OK: PID=0x...`
   - `POLL: rawx=... rawy=...`
   - `REPORT: dx=... dy=...`
   - `kernel threads input` → check state
5. Move trackpoint nub — cursor should move on screen

## Expected outcome

- **Cursor moves**: confirmed ZMK main regression. Merge Exp23, pin ZMK permanently.
- **Cursor doesn't move:** root cause is in our code. Add `printk` to trace poll → `input_report()` path.

## Risk

`fa33e35f` is a docs-only dependabot commit. It pins the ZMK tree + `app/west.yml` (which controls Zephyr version) to what was active on July 26. Even though no ZMK code changed at this commit, the Zephyr pinning should match the working build's environment.

## Results

(To be filled after testing)
