# Exp72: layer-toggle on the direct-PS2 dongle topology (temp_layer)

## Hypothesis

Now that the direct-PS/2 trackpoint moves the cursor naturally on the dongle
topology (Exp68→71), re-introduce the **layer-toggle**: touching the nub
activates a `tp_layer` (mouse buttons MB1/MB2/MB3 on the right thumb cluster),
which drops again after a short idle timeout. This mirrors the layer-toggle
that `origin/main` of `zmk-config-dabaseV_0-2` has (the pre-direct-PS2
I2C/ATtiny era, Exp29+): a `zmk,input-processor-temp-layer` node on the dongle
`trackball_listener` + a `tp_layer` keymap layer.

Plan is config-only, on top of the known-good Exp71 stack:

- Keymap: add `tp_layer` (all-transparent except MB1/MB2/MB3 on the right thumb
  cluster, row 4 positions 6-8 of the right half).
- Dongle overlay: define `tp_temp_layer` (`#input-processor-cells = <2>`) and
  attach `input-processors = <&tp_temp_layer 5 500>;` to `trackball_listener`.
- Layer index **5** (not 6 as in origin/main) — the dongle keymap here has no
  `scroll_layer`/`volume_layer` layers, so `tp_layer` lands right after
  `gaming`.
- Timeout **500 ms** (matching origin/main's `<&tp_temp_layer 6 500>`).
- No driver, west.yml, right-half overlay, or .conf changes.

## Plan

- Repo: `zmk-config-dabaseV_0-2`, branch `Exp72` off Exp71 `091ee80`.
- Changes:
  - `config/dabase_v2.keymap` — insert `tp_layer` node after `gaming`.
  - `boards/shields/dabase_v2/dabase_v2_dongle.overlay` — add `tp_temp_layer`
    node + `input-processors` on the listener.
- Build via GH Actions (run `33260481003`), flash the **dongle**
  (`dabase_v2_dongle-usb-log.uf2`, COM21/22). Right half stays on Exp71
  firmware (transform is applied right-half-side and split-forwarded).
- Verify: touch nub → right-thumb MB1/MB2/MB3 light up on the tp_layer;
  release → layer drops after 500 ms idle; cursor still moves the same.

## Result: SUCCESS — user-confirmed

- Build `33260481003` all 8 jobs pass. Dongle flashed headless:
  `dabase_v2_dongle-usb-log.uf2` (754176 B) via COM21 (probed `uart:~$`;
  COM22 empty, COM7 = right half) + `XIAO-SENSE` drive.
- User-confirmed via question tool: **touch nub → right-thumb MB1/MB2/MB3
  click; they stop shortly after you stop touching the nub; cursor still moves
  normally.** Layer-toggle works, 500 ms idle timeout, zero regressions.
- Right half **not** re-flashed — it stayed on Exp71 firmware (transform is
  applied right-half-side and split-forwarded; the toggle lives wholly on the
  dongle listener). Data flow: right half `tpoint0` → `trackball_split`
  (swap) → BLE split-forward → dongle listener → `tp_temp_layer` 5 500 → HID.

## Known-good

- Config `Magid-William/zmk-config-dabaseV_0-2` branch `Exp72` @ `7f885ac`.
- GH run `33260481003`, all builds green.
- ZMK `ac7f75b8`, driver `5fbc21f` (Exp70), everything else as Exp71.
- Dongle: `dabase_v2_dongle-usb-log.uf2` (754176 B) on COM21, flashed
  `2026-08-29`.

## Inclusions / notes

- The dongle keymap is the unified `dabase_v2.keymap`; all layer references
  (`&mo 1`, `&mo 2`, `&lt 3`, `&tog 4`) stay valid since `tp_layer` only
  shifts the reserved `extra2`/`extra3` indices (5→6, 6→7).
- `temp_layer` processor (`zmk,input-processor-temp-layer`) exists in pinned
  ZMK `ac7f75b8` (`app/dts/bindings/input_processors/…`, impl in
  `app/src/pointing/input_processor_temp_layer.c`). Auto-picked-up when a
  node references the compatible.
- DTS gotcha (Exp36): the `tp_temp_layer` node must live inside the `/ {}`
  root block of the overlay, not at bare top level.

## Conclusion

**Status: Success (Exp72).** The layer-toggle is back on the direct-PS2 dongle
topology. Touching the nub activates `tp_layer` (keymap index 5) via the
`zmk,input-processor-temp-layer` input processor on the dongle listener, giving
MB1/MB2/MB3 on the right thumb cluster; the layer drops 500 ms after the last
nub event. Config-only change on the known-good Exp71 stack — no driver,
west.yml, right-half, or .conf modifications. User-confirmed, cursor behavior
unchanged.

### What worked

- `temp_layer` input processor is core ZMK (pinned `ac7f75b8`) — no module or
  driver work needed; the node gets picked up automatically when referenced.
- The dongle listener is the right home for the toggle: the right half only
  forwards (with the Exp71 swap), so the layer state change happens centrally
  where the keymap is evaluated.
- Layer index 5 (not 6 like origin/main): the direct-PS2 keymap has no
  scroll/volume layers, so `tp_layer` slots right after `gaming`.

### Pitfalls / notes

1. **Layer index must match the keymap position** — `&tp_temp_layer 5 500`
   refers to the `tp_layer` node in `config/dabase_v2.keymap` (after
   `gaming`); origin/main used index 6 only because its keymap had
   `scroll_layer`/`volume_layer` occupying 5 and 7.
2. **DTS placement** (Exp36 repeat): `tp_temp_layer` must be inside the `/{}`
   root block, not a bare top-level overlay node.
3. **Dongle-only flash** — the right half was left on Exp71 firmware. The
   keymap (`tp_layer`) is baked into every build, but the toggle logic only
   lives on the dongle listener; the standalone-central listener path is still
   unverified (broken in Exp69).

### Next steps

- Retune the idle timeout (500 ms → 1000–2000 ms) if the MB layer drops while
  still aiming for a click.
- Port scroll (hold J) and volume (hold K) onto the same dongle listener to
  reach full origin/main parity on the direct-PS2 stack.

## Next experiments (suggestions)

- If 500 ms feels too short (layer drops while still aiming), retune the
  timeout cell — try 1000–2000 ms (the Exp15/20/29-era documented behavior
  was 2 s idle).
- Port scroll (hold J → `zip_xy_to_scroll_mapper`) and volume (hold K →
  keybind) onto this same dongle listener, restoring full origin/main parity.