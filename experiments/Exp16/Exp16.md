# Exp16: BLE Smoothness Test — Synthetic Rectangle Over BLE

## Hypothesis

The Exp05 synthetic rectangle pattern produces perfectly smooth cursor motion over USB (100 Hz interrupt-driven SPI). The same pipeline over direct BLE (right NiceNano → PC) should also be smooth — BLE HID pointing reports at 100 Hz are well within the bandwidth of a standard BLE connection. If choppiness or stuttering appears, the bottleneck is likely the BLE connection interval or HID report servicing, not the SPI pipeline.

## Iterations

### Exp16a (50 Hz MOT)
Doubled deltas, 20ms period. Halved MOT rate from Exp05's 100 Hz to match expected BLE throughput.

**Result:** Smoother than 100 Hz — every BLE report carries fewer but larger deltas. Good baseline.

### Exp16b (25 Hz MOT)
Quadrupled deltas, 40ms period. Further reduction to align with BLE connection interval.

**Result:** Worse — rectangle visibly wider, no improvement. Larger per-step deltas make jumps more visible.

### Exp16c (50 Hz + BLE Kconfig attempt)
Reverted to 50 Hz. Added `CONFIG_BT_PERIPHERAL_PREF_CONN_INTERVAL` and `CONFIG_ZMK_HID_REPORT_QUEUE_SIZE` to shield config.

**Result:** Build failed — neither Kconfig symbol exists in Zephyr/ZMK. BLE connection interval is not configurable via Kconfig.

### Exp16d (Running average filter, SMOOTH_FACTOR=4)
Replaced raw deltas with 4-tap moving average on Pro Mini. Blends adjacent velocity targets.

**Result:** Zigzag drift — the 4-step window blends X and Y simultaneously at corners, creating cumulative diagonal error (trace drifts to top-left).

### Exp16e (Per-axis rate limiter, MAX_CHANGE=2)
Replaced running average with independent per-axis clamping. Each axis changes by at most 2 per step, preventing cross-axis blend.

**Result:** "98% there" — smooth but not matching `&mmv MOVE_UP` emulation quality.

### Exp16f (Per-axis rate limiter, MAX_CHANGE=1)
Reduced max change to 1 to match `&mmv`'s ±1-per-event approach.

**Result:** No noticeable change from MAX_CHANGE=2. The bottleneck is elsewhere.

### Exp16g (Driver-side acceleration work)
Reverted Pro Mini to raw deltas at 50 Hz. Added a delayable work in `pmw3610.c` that generates ramped ±1 events, mimicking `&mmv`'s acceleration curve.

**Result:** Tiny rectangle (16×30 px instead of 200×200). Bug: `accel_start_ms` reset on every MOT pulse, so acceleration never built up.

### Exp16h (badjeff's input processors)
Reverted driver to stock Exp15. Added badjeff's `zmk-input-processor-report-rate-limit` and `zmk-input-processor-xyz` modules via west.yml. Configured `&zip_report_rate_limit 4` + `&zip_xyz` as input processors.

**Result:** Mouse didn't move. `&zip_xyz` compresses X/Y into a single Z event for split keyboard transport — our standalone right half has no central side to decompress, so the PC received Z events instead of X/Y mouse movement.

### Exp16h fix (BLE-only rate limit)
Removed `&zip_xyz`, switched to `&zip_ble_report_rate_limit 4` (rate limit on BLE only). Stock driver + rate limit only.

**Result:** Untested — experiment called off.

## Key Technical Insights

1. **BLE connection interval is not configurable via Kconfig** in ZMK — there's no `CONFIG_BT_PERIPHERAL_PREF_CONN_INTERVAL` or mouse report queue size. The connection interval is negotiated by the BLE controller with the host, and ZMK has no application-level control.

2. **`zip_xyz` is for split keyboards only** — it compresses X/Y into Z for the split transport between halves. On a standalone right half, using it destroys X/Y mouse data.

3. **`zip_ble_report_rate_limit` 4ms** from badjeff's modules is the closest to a ready-made solution. It rate-limits BLE reports without affecting USB.

4. **`&mmv MOVE_UP` is genuinely smoother** because it sends ±1 at varying frequency, while our sensor pipeline sends fixed-frequency deltas. The fundamental issue is the event pattern, not just the magnitude.

5. **The Pro Mini SPI pipeline is not the bottleneck** — 50 Hz with per-byte transactions works flawlessly over USB (Exp05 proven). The bottleneck is entirely on the BLE HID delivery side.

## What's Left Untested

The final Exp16h build has:
- Stock PMW3610 driver (reverted)
- `&zip_ble_report_rate_limit 4` input processor
- `CONFIG_ZMK_BLE_EXPERIMENTAL_CONN=y`
- Pro Mini: raw deltas at 50 Hz

This configuration was not tested because the experiment was called off. It's the most promising approach — using badjeff's battle-tested modules without any custom driver code.

## Conclusion

Exp16 demonstrated that achieving smooth cursor motion over BLE is fundamentally a BLE transport problem, not a SPI/input pipeline problem. Multiple smoothing approaches on the Pro Mini and driver side improved the feel incrementally but never matched the smoothness of ZMK's built-in `&mmv` behavior. The underlying issue — BLE connection interval controlled by the PC's BT controller — cannot be tuned from ZMK's Kconfig.

## Next Experiment (Exp17)

A sanity check to verify the hardware setup produces usable motion before further optimization. Consider testing with the full PS/2 trackpoint pipeline (real input, not synthetic rectangle) paired with `&zip_ble_report_rate_limit` and stock driver to see if real-world usage is acceptable despite the BLE smoothness gap. Alternatively, investigate using `zmk-input-split` architecture where the right half acts as a peripheral to a central — giving access to badjeff's full toolchain.
