# Exp66 — Smooth scrolling + scroll scaler retune

## Hypothesis

`CONFIG_ZMK_POINTING_SMOOTH_SCROLLING=y` enables the HID Resolution Multiplier so
compatible hosts (recent Windows/macOS/Linux browsers) interpret wheel counts as
**fractions of a detent** (~1/16 per count) instead of whole detents (~50 px each).
This drops the quantization unit from ~50 px to sub-pixel, giving 1 px precision at
low speed while the scaler controls top speed.

Current `zip_scroll_scaler 1 100` gives ~50 px per step (each count = 1 detent =
~50 px). With smooth scrolling the same count is ~3 px, so the scaler must be
sped up to compensate. Starting at `1 10` (~10 × faster, net ~1.6 × slower than
current — iterate by feel).

## Plan

1. Enable `CONFIG_ZMK_POINTING_SMOOTH_SCROLLING=y`:
   - `config/dabase_v2_dongle.conf` — dongle (central) build
   - `build.yaml` cmake-args on `dabase_v2_right-standalone-usb` only
     (standalone central; shared .conf would poison the peripheral build)
2. Change `&zip_scroll_scaler 1 100` → `1 10` in all 3 overlay occurrences:
   - `dabase_v2_right.overlay` ×2 (listener line 91, split-child line 137)
   - `dabase_v2_dongle.overlay` ×1 (listener line 73)
3. Build via GH Actions, flash dongle (+ standalone right half if used),
   iterate scaler by feel.

## Why smooth scrolling is needed

`zip_scroll_scaler` is integer-only (`input_processor_scaler.c`). It banks sub-1
deltas and emits whole wheel ticks (value ≥ 1). Without the Resolution Multiplier
each whole tick = 1 detent = ~50 px. You cannot get sub-detent precision from
whole-tick wheel reporting. Enabling smooth scrolling collapses the quantization
unit from ~50 px to a fraction of a pixel — that's what makes 1 px AND controlled
speed possible at the same time.

### What `1 1` showed

At `1 1` the scaler passes raw deltas straight through. A gentle nudge = 1 px
(host mapped count 1 → 1 px). But sustained motion = brutal (raw deltas 50–100+
at 50 Hz → 50–100 px per report). The problem is purely the uncapped top end,
not the low end. Smooth scrolling + a moderate scaler solves both.

### Kconfig guard

`ZMK_POINTING_SMOOTH_SCROLLING` is behind `if !ZMK_SPLIT || ZMK_SPLIT_ROLE_CENTRAL`.
Only the dongle (central) and standalone right-half (central) builds can take it.
The `dabase_v2_right-promini-i2c` peripheral build ignores it.

## Tuning notes

With smooth scrolling on, each wheel count ≈ 1/16 detent ≈ 3 px (host-dependent).
For the same perceived scroll speed as `1 100` without smooth scrolling:

- `1 100` old: 100 units → 1 count → 50 px. Rate = 0.5 px/unit.
- `1 10` new: 10 units → 1 count → 3 px. Rate = 0.3 px/unit.
  (Net ~60% of current speed — conservative starting point.)

If too slow: move toward `1 5` (0.6 px/unit). If too fast: move toward `1 20`
(0.15 px/unit). Iterate until comfortable.

## Files changed

| File | Change |
|---|---|
| `zmk-config-dabaseV_0-2/config/dabase_v2_dongle.conf` | +`CONFIG_ZMK_POINTING_SMOOTH_SCROLLING=y` |
| `zmk-config-dabaseV_0-2/build.yaml` | +cmake-arg on standalone entry |
| `zmk-config-dabaseV_0-2/.../dabase_v2_right.overlay` | `1 100` → `1 10` (×2) |
| `zmk-config-dabaseV_0-2/.../dabase_v2_dongle.overlay` | `1 100` → `1 10` (×1) |

No driver or AVR changes.

## Conclusion

**Success.** Smooth scrolling + `1 10` gives 1 px precision at low speed with a
comfortable, controllable top end. The HID Resolution Multiplier drops the
quantization unit from ~50 px (whole detent) to ~3 px (1/16 detent), and the
scaler at `1 10` compensates for the finer host granularity. The `1 10` ratio
is a good starting point — further tuning (1/5, 1/20) is straightforward.

Known-good: config `75397a0` (Exp66 branch).
