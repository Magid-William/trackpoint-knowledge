# Exp67 — 2× cursor speed: Windows-15 feel at Windows speed 10, from the keyboard

## Hypothesis

The cursor felt "heavy" (sluggish) at Windows pointer speed **10**. The user's
compensation was raising Windows speed to **15**, which felt best — but that's an
OS-level hack. Windows multiplier table (enhance-precision off): **10 → 1.0×,
15 → 2.25×**. So the keyboard must output ~2.25× more X/Y counts and Windows can
stay at 1.0× (10).

The transfer function (PowerCurve on the ATtiny85, Exp60/61) is:

```
gain(v) = (sens/256) · (start/256 + (rate/256)·v)      v = |(x,y)| per PS/2 packet
```

Current overlay values: `speed-scale 128` (sens 0.5×), `curve-rate 18`, `curve-exponent 256` (1.0), `curve-start 77`.
At v=10 the deltas are halved — that's the "heavy".

Sens is Q8.8 with 255 = 1.0. Doubling `speed-scale 128 → 255` doubles every gain
across the whole curve (×1.992) while keeping the exact same curve shape. That
lands at ≈2× of the 2.25× target; if still heavy, nudge `curve-start` 77→86 +
`curve-rate` 18→20 (~+13%) for the remaining 1.13×.

## Plan

1. `zmk-config-dabaseV_0-2`: new branch `Exp67` (off Exp66).
2. `boards/shields/dabase_v2/dabase_v2_right.overlay`: `speed-scale = <128>` → `<255>` + comment.
3. Push → GH Actions (auto-builds all 6 firmware entries).
4. Download `firmware` artifact, flash `dabase_v2_right-promini-i2c.uf2` to the right half.
5. Verify by feel at **Windows speed 10** vs the known-good Windows-15 baseline.

No driver, no ATtiny firmware changes — the curve already lived on the ATtiny and
the driver already programs it from DT props at init + every link-restore.
(Alternative considered: `zip_xy_scaler` input-processor — rejected: in split
topology the peripheral streams raw over `zmk,input-split` and processing happens
on the central, so the scaler would need replicating in right + left-central +
dongle overlays. The AVR-side curve is topology-independent.)

## Findings

- GH run `33073689572` (2m34s, success) → artifact `dabase_v2_right-promini-i2c.uf2` (547840 B).
- Flashed; **user confirms cursor feel is great at Windows speed 10** — no more OS-side speed hack.
- Shell probing note: COM3/COM4 are Bluetooth SPP ports (BTHENUM) and **hang on
  `SerialPort.Open()`** — skip them. The NiceNano enumerates as ZMK's default
  `1D50:615E` CDC ACM with two interfaces (COM21 `MI_00`, COM22 `MI_03`); the
  shell answers on COM22 (`uart:~$`). (The currently-flashed pre-Exp67 firmware
  had no `i2c` shell command, so identity was confirmed by feel after flash.)

## Files changed

| File | Change |
|---|---|
| `zmk-config-dabaseV_0-2/.../dabase_v2_right.overlay` | `speed-scale = <128>` → `<255>` (+ comment) |

No driver or AVR changes.

## Conclusion

**Success.** Doubling `speed-scale` (128 → 255, ≈×2.0 on every speed bin) moved
the Windows-15 compensation into the keyboard: same curve shape (start 77, rate 18,
exp 1.0), just loud. Windows stays at pointer speed 10. If the remaining ~12% to
2.25× ever matters, `curve-start 86` / `curve-rate 20` is the next nudge — but
the user reports no need.

Known-good: config `1dd3c95` (Exp67 branch).