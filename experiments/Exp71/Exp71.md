# Exp71: fix trackpoint cursor direction (nub up→left, left→up)

## Hypothesis

The TP's sensor axes are rotated vs the keyboard mount: with raw reporting,
pushing the nub **up** moves the cursor **left** and pushing it **left** moves
the cursor **up**. The TP rejects all host commands (`NO_HOST_COMMANDS`), so its
register invert/swap config bits are no-ops. Applying the equivalent transform
in software on the right half — via ZMK core's `zmk,input-processor-transform`
(`zip_xy_transform`, present at pinned ZMK `ac7f75b8`) — should make the cursor
follow the nub naturally, **config-only** (no driver/ATtiny changes).

## Plan

- Repo: `zmk-config-dabaseV_0-2`, branch `Exp71` off Exp70 `7e9d97b`.
- `dabase_v2_right.overlay`:
  - include `<input/processors.dtsi>` + `<dt-bindings/zmk/input_transform.h>`
  - attach `input-processors = <&zip_xy_transform (FLAGS)>` to
    `trackball_split` (peripheral/ps2-direct build — runs the chain BEFORE the
    BLE split-forward, verified in `input_split.c`) and to `trackball_listener`
    (standalone-central build, `input_listener.c`). No dongle change (already
    corrected events arrive there).
- Build via GH Actions, flash right half (`dabase_v2_right-ps2-direct.uf2`),
  user tests all four nub directions.

## Result: SUCCESS — user-confirmed, natural in all four directions

- it1 (`2fa4157`): `XY_SWAP | X_INVERT | Y_INVERT` (=7, "swap+invert both").
  Over-corrected: **up→down, left→right** — both axes mirrored, swap correct.
- it2 (`091ee80`): **swap-only** (`INPUT_TRANSFORM_XY_SWAP` =1).
  **Natural: up→up, left→left, down→down, right→right.** ✓

Lesson: the naive reflection `M(x,y)=(-y,-x)` was the FIRST guess, but this
TP's raw/screen sign convention already matches — the sign flips double-inverted
it. Only the axis interchange was wrong. (When in doubt: ship the swap first.)

## Why swap-only (derived from it1 data)

With raw reporting, up-push reports as left ⇒ raw(up)=(a,0) maps to cursor-left.
After =7: (0,-a) → cursor-*down*, and (0,b)→cursor-*right* ⇒ the needed numeric
relation is up-value=a, left-value=b. Requiring up→(0,up) and left→(left,0)
gives T(x,y)=(y,x): **swap only**.

## Files changed

Only `boards/shields/dabase_v2/dabase_v2_right.overlay` (includes + 2
`input-processors` lines). No driver, west.yml, keymap, or .conf changes.

## Known-good (it2)

- Config `Magid-William/zmk-config-dabaseV_0-2` branch `Exp71` @ `091ee80`.
- GH run `33259401310` — all builds pass. `dabase_v2_right-ps2-direct.uf2`
  (615424 B) flashed on COM7.
- ZMK `ac7f75b8`, driver `5fbc21f` (Exp70), no other changes.
- User-confirmed natural in all four directions.

## Notes / gotchas

- COM7 was held by the user's Tabby serial app → "Access to the path 'COM7' is
  denied" on Open. Close the app, then probe.
- Per-packet `P ... x=%d y=%d` printk shows RAW packet values — the transform
  runs later in the split handler, so those lines are not evidence of the fix.

## Next experiments (suggestions)

- Reputation check: flash the standalone-central build (listener path) — it was
  broken in Exp69; the transform there remains unverified until that path works.
- Re-introduce temp_layer / scroll / volume (Exp51/66-era) on the now-proven
  direct-PS/2 stack.