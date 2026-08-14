# Exp51: Hold K → trackpoint tilt becomes volume (very slow, slower than scroll)

## Hypothesis

The scroll layer (5, hold J) already proves layer-specific `input-processor` overrides can remap the trackpoint (Exp32). Reusing the same pattern with the `te9no/zmk-input-processor-keybind` module, a hold of **K** should turn trackpoint tilt into volume key presses — and a deliberately high `tick` (32) should make it **very slow**, slower than scroll, so it's usable for fine volume control.

Two differences vs. scroll:

1. **Keybind processor** instead of the scroll mapper: accumulates REL_X/REL_Y deltas per sync event and converts movement into key press/release (directional via `bindings` array: RIGHT, LEFT, DOWN, UP).
2. **Override swallowing**: scroll only remaps and lets the base (cursor) processor run via `process-next`. The keybind processor returns `STOP` on all REL events, so the base cursor chain is suppressed — meaning cursor would NOT move on layer 7 — but ZMK overrides only honor that if the override lacks `process-next` (ZMK `fa33e35f`: `if (!override->process_next) return 0;`). Since we need cursor suppression, we must ensure nothing re-enables it.

## ZMK override semantics (verified in `fa33e35f` source)

- `filter_with_input_config()` walks active overrides for the current layer in order.
- An override WITH `process-next` that returns `STOP` does **not** halt processing — the loop continues to the next override, and if none returns a stop and the last has `process-next`, it falls through to the base processors.
- Therefore the keybind override alone CANNOT suppress the cursor: the base processor would still run.
- Fix: a **second override** for the same layer whose processors scale the event to zero — `volume_move_zero` with `&zip_xy_scaler 0 1` (0× X, 0× Y). Both overrides run; keybind converts to volume keys (STOP), the zero-scaler erases any leftover movement, and the base never moves the cursor.
- `zip_xy_scaler` / `zip_xy_transform` ship in ZMK's own `input/processors.dtsi` — no new dependency.

## Keybind module notes

- Module: `te9no/zmk-input-processor-keybind` (west `revision: main`).
- **Do NOT** `#include` the module's `dts/input/processors/zmk-input-processor-keybind.dtsi` — it pulls `<dt-bindings/zmk/input-processor-keybind.h>`, which is missing at the pinned ZMK `fa33e35f`. Define the `keybind_volume` instance inline instead (compatible `"zmk,input-processor-keybind"`).
- Bindings order is `[RIGHT, LEFT, DOWN, UP]` per `press_work_cb` → `&none`, `&none`, `&kp C_VOLUME_DOWN`, `&kp C_VOLUME_UP`.
- Tuning for "very slow": `tick = <32>`, `wait-ms = <10>`, `tap-ms = <12>`, `threshold = <1>`, `max_threshold = <512>`, `max_pending_activations = <2>`, `track_remainders`. `tick 32` = 32 input units per key repeat step (scroll layer uses scaler 1/10 on the same 50Hz source — so volume ~3× slower than scroll).
- `mode = <0>` (raw) accumulates X and Y independently; X bindings are `&none` so only Y produces volume.
- Volume mapping mirrors scroll's physical direction: Y-invert (`INPUT_TRANSFORM_Y_INVERT`) so **up = louder** (matches Exp32 scroll's "push away = up").

## Changes by repo

| Repo | Branch | Change |
|------|--------|--------|
| promini-trackpoint | — | **NO changes** |
| zmk-pmw3610-driver | — | **NO changes** |
| zmk-config-dabaseV_0-2 | Exp51 | 1) `west.yml`: add remote `te9no` + project `zmk-input-processor-keybind` (`main`) 2) `dabase_v2.keymap`: `SOFT_OFF_KEY` `&kp K` → `&lt 7 K`; `extra1` (reserved) → `volume_layer` at index 7, transparent except MB1-3 on thumb cluster (mirrors scroll_layer) 3) `dabase_v2_right.overlay` + `dabase_v2_dongle.overlay`: add includes (keys.h, behaviors.dtsi), `keybind_volume` node, `volume_override` (Y-invert + keybind, `process-next`) and `volume_move_zero` (scaler 0 1) inside `tp_listener` |

## Verification plan

1. Build both `dabase_v2_right-promini-i2c` and `dabase_v2_dongle-usb-log` via GH Actions.
2. Flash XIAO dongle (label `XIAO-SENSE`) + NiceNano right half.
3. Without holding anything: cursor moves normally (base chain untouched).
4. Tap K: types K (layer-tap tap behavior).
5. Hold K + tilt trackpoint up/down: volume changes very slowly (≈ 3× slower than scroll), cursor stays put.
6. Hold J (scroll layer): scroll unchanged.
7. Tilt trackpoint while holding K but on a layer where keybind shouldn't fire: no volume.

## Conclusion

**Success.** Hold K turns trackpoint tilt into very slow volume control, verified by the user:

- Hold K + tilt up/down → volume steps very slowly (≈ 3× slower than scroll), cursor stays put.
- Tap K still types K (layer-tap tap behavior).
- Hold J scroll layer unchanged.
- Cursor moves normally when not holding K (base chain untouched).

Implementation lessons:

- `RC(row,col)` (matrix transform) vs `RC(keycode)` (modifiers.h via keys.h) macro collision: the dongle overlay expands its transform map AFTER the `keys.h` include, so the 1-arg `RC` wins → `RC(0,5)` errors. Fix: include `keys.h`/`behaviors.dtsi` **before** `matrix_transform.h` so the 2-arg `RC` is defined last. Right half unaffected (transform map inside `dabase_v2.dtsi` expands before `keys.h`).
- The keybind processor returns `STOP` on all REL events, but ZMK `fa33e35f` only honors that when the override lacks `process-next`. The `volume_move_zero` companion override (`zip_xy_scaler 0 1`, no process-next) is what actually keeps the cursor put — both overrides run per sync event.
- Volume direction: Y-invert matches scroll's physical-up = louder mapping.

## Known-good

- shield/config repo branch `Exp51` @ `be1b088`
- ZMK `fa33e35f`, Zephyr `9df4b12`, te9no keybind module `main`
- firmware: `dabase_v2_right-promini-i2c.uf2` (548864 B) + `dabase_v2_dongle-usb-log.uf2` (763904 B) → `build/Exp51/firmware/`, CI run `31174392742`

## Next steps

1. If volume granularity needs tuning, `tick` in `keybind_volume` is the single knob (lower = faster).
2. Wishlist item "[ ] It works perfectly on usb" — Exp51's volume path is another input-layer case to include in USB-mode regression checks.
