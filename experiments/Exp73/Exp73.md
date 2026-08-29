# Exp73: Scroll-layer-toggle (hold J) + Volume-layer-toggle (hold K)

## Hypothesis

The layer-toggle (`temp_layer` touch-toggle, Exp72) proves the dongle listener
can drive layer-specific input-processor overrides with the direct-PS2
topology. Reusing the proven Exp32/Exp51 patterns — **hold J** → `scroll_layer`
(6) with `&zip_xy_to_scroll_mapper`, **hold K** → `volume_layer` (7) with the
te9no keybind processor — should give scroll-on-hold and volume-on-hold
without any driver changes (config-only in the dabase shield/config repo).

## Plan

Config-only, on the Exp72 stack (`zmk-config-dabaseV_0-2`, branch `Exp73` off
`7f885ac`):

1. `west.yml` — add te9no remote + `zmk-input-processor-keybind` (`main`).
2. `dabase_v2.keymap` — `&kp J` → `&lt 6 J`, `&kp K` → `&lt 7 K`; replace the
   reserved `extra2`/`extra3` with `scroll_layer` (6) and `volume_layer` (7),
   transparent except thumb-cluster MB1/2/3.
3. `dabase_v2_dongle.overlay` — includes (`input/processors.dtsi`,
   `input_transform.h`), `scroll_override` (map→Y-invert→scaler `1 2`,
   `process-next`), `volume_override` (y-invert→keybind, `process-next`) +
   `volume_move_zero` (scaler `0 1`, no process-next), inline `keybind_volume`
   node (module dtsi pulls a missing header).
4. Layer indices: tp_layer 5 (unchanged `&tp_temp_layer 5 500`), scroll 6,
   volume 7.
5. Dongle-only flash (`dabase_v2_dongle-usb-log.uf2`); right half stays on
   Exp71 firmware. Verify: hold J → scroll, hold K → volume (slow, cursor
   stays put), tap J/K still type, touch-toggle + cursor unchanged.

## Findings

(to be filled in as we go — build run, flash, user verification, tuning.)

## Conclusion

(to be written after user verification.)

## Next steps

- If scroll feels hot (direct-PS2 raw deltas > old PowerCurve'd stream),
  lower `zip_scroll_scaler` (`1 2` → `1 4`/`1 6`/`1 10`).
- If volume direction/speed is off, flip the Y-invert in `volume_override` or
  raise `tick`.
- Optionally mirror the overrides on the right-half standalone listener
  (standalone USB path; currently unresolved per Exp69, non-blocking).