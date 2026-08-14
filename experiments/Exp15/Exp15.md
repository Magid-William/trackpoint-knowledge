# Exp15: Auto Layer-Toggle on TrackPoint Motion

## Hypothesis

By adding layer-toggle logic directly to the PMW3610 ZMK driver, the keyboard automatically switches to a mouse-button layer when the trackpoint is in use. While motion is flowing, the target layer stays active. After motion stops, the layer deactivates after a 2-second timeout.

This is modeled after the `layer-toggle` feature in [infused-kim's PS/2 mouse driver](https://github.com/infused-kim/kb_zmk_ps2_mouse_trackpoint_driver).

## Why in-driver instead of a separate input-listener

The infused-kim approach uses a separate `zmk,input-listener-ps2` DT compatible that hooks into the input event stream. Our PMW3610 driver already has the motion data at the point of `pmw3610_report_data()` — integrating layer-toggle there is simpler, avoids a second DT node, and keeps the change self-contained in one module.

## Changes by file

### `zmk-pmw3610-driver/`

| File | Change |
|------|--------|
| `dts/bindings/pixart,pmw3610-alt.yml` | Add `layer-toggle` (int, default -1) and `layer-toggle-timeout-ms` (int, default 2000) DT properties |
| `src/pixart.h` | Add layer-toggle fields to `pixart_data` and `pixart_config` structs |
| `src/pmw3610.c` | Add layer-toggle logic: activate on non-zero motion, deactivate after timeout via delayed work |
| `Kconfig` | Add `PMW3610_ALT_LAYER_TOGGLE` bool |

### `zmk-trackpoint-shield/`

| File | Change |
|------|--------|
| `corne_trackpoint_right.overlay` | Add `layer-toggle = <2>;` and `layer-toggle-timeout-ms = <2000>;` to trackball node |
| `corne_trackpoint_right.keymap` | Add mouse-button layer at index 2 |
| `corne_trackpoint_right.conf` | Add `CONFIG_PMW3610_ALT_LAYER_TOGGLE=y` |

## No changes

| File | Reason |
|------|--------|
| `promini-trackpoint/` | No AVR-side changes needed |
| `PS2Trackpoint/` | Used as-is |

## DT api

```dts
trackball: trackball@0 {
    compatible = "pixart,pmw3610-alt";
    ...
    layer-toggle = <2>;              // layer to activate on motion (-1 = disabled)
    layer-toggle-timeout-ms = <2000>; // ms after last motion before deactivation
};
```

## Layer-toggle flow

```
Motion starts → report_data() sees non-zero x/y → activate layer 2 → reschedule deactivation work
                                                                       │
                                                             2s timeout fires (no new motion)
                                                                       │
                                                                       └── deactivate layer 2
```

- Activation is **instant** on first non-zero motion after idle
- Each new motion reschedules the 2s deactivation timer
- The deactivation work fires only after `timeout_ms` of no motion (no stale-event guard needed since the work is rescheduled on every motion)

## Build iterations

### Iteration 1: Kconfig not visible
`depends on ZMK_KEYMAP` blocked `CONFIG_PMW3610_ALT_LAYER_TOGGLE` from being visible. The `.conf` setting was silently ignored — layer-toggle code never compiled.

**Fix:** Remove `depends on ZMK_KEYMAP`.

### Iteration 2: Wrong API signature
ZMK main (Zephry 4.1) requires two arguments for both `zmk_keymap_layer_activate(layer, false)` and `zmk_keymap_layer_deactivate(layer, false)`.

**Fix:** Add `, false` second argument to both calls.

### Iteration 3: Stale-event guard blocked deactivation
The guard `elapsed > timeout/10` was copied from infusion-kim's activation handler. In the deactivation handler, elapsed ≈ `timeout_ms` when the work fires, making the condition always true — deactivation never executed.

**Fix:** Remove the stale-event guard from deactivation (work is rescheduled on each motion; it can only fire after genuine inactivity).

## Success criteria

- [x] Shield builds via GH Actions with layer-toggle enabled
- [x] Shell shows layer activation/deactivation log messages
- [x] TrackPoint motion activates layer 2 (mouse keys become available)
- [x] 2s after motion stops, layer 2 deactivates, keyboard returns to default
- [x] Repeated motion/idle cycles stable
- [x] `layer-toggle = <-1>` (disabled) behaves as before — no layer changes

## Conclusion

**Status: Success** — layer-toggle works correctly. TrackPoint motion instantly activates layer 2 (MOUSE), and the layer deactivates cleanly 2 seconds after the last motion.

### What worked
- All three driver-level changes (no separate input-listener needed)
- DT properties `layer-toggle` and `layer-toggle-timeout-ms` on the sensor node
- Delayed work for automatic deactivation timeout
- Mouse-button layer (LCLK/RCLK on thumbs) accessible only during trackpoint use

### Pitfalls encountered
1. **Kconfig dependency** — `depends on ZMK_KEYMAP` silently hid the option. Zephyr warns but doesn't error.
2. **ZMK API change** — `zmk_keymap_layer_activate/deactivate` now take `(layer, oneshot)` in Zephyr 4.1.
3. **Wrong guard placement** — stale-event guard (`elapsed > timeout/10`) is correct for *activation* (prevent stale fire long after motion stopped) but wrong for *deactivation* (where elapsed ≈ timeout when the work fires).

### Next steps
- Tune `layer-toggle-timeout-ms` if 2s feels too long or too short
- Add activation delay (`layer-toggle-delay-ms`) if brief jitter should not trigger the layer
- Customize the mouse-layer keymap with scroll or other pointing behaviors
