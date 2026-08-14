# Exp29: Layer toggle + DT configurable axis swap/invert

## Hypothesis

1. Layer-toggle cannot work from the driver on a split peripheral (ZMK excludes `keymap.c` from peripheral builds). Implement it on the **dongle** via ZMK's built-in `zmk,input-processor-temp-layer` — activates `tp_layer` on motion, deactivates after idle timeout.

2. `swap-xy`, `invert-x`, `invert-y` should be devicetree boolean properties instead of hardcoded `#define`s so they can be changed per-build without touching the driver code.

## Changes

### zmk-pmw3610-driver (branch Exp29, SHA `41321ce`)

| File | Change |
|------|--------|
| `dts/bindings/promini,trackpoint-i2c.yml` | Added optional `swap-xy`, `invert-x`, `invert-y` boolean properties |
| `src/trackpoint-i2c.c` | Removed `#define SWAP_XY 1`, `INVERT_X 1`, `INVERT_Y 1`. Added `bool swap_xy/invert_x/invert_y` to config struct. Poll function reads from `cfg->` fields. DEFINE macro reads DT booleans via `DT_INST_PROP`. |

### zmk-config-dabaseV_0-2 (branch Exp29)

| File | Change |
|------|--------|
| `boards/shields/dabase_v2/dabase_v2_dongle.overlay` | Added `tp_temp_layer` node (`zmk,input-processor-temp-layer`, `#input-processor-cells = <2>`). Added `input-processors = <&tp_temp_layer 1 2000>;` to `trackball_listener` |
| `boards/shields/dabase_v2/dabase_v2_right.overlay` | Added `swap-xy; invert-x; invert-y;` to trackball node |
| `config/west.yml` | Driver bumped to SHA `41321ce` |

### promini-trackpoint

No changes.

## DT property usage

In the trackball node, properties default to `false` when omitted:

```dts
trackball: trackball@42 {
    compatible = "promini,trackpoint-i2c";
    reg = <0x42>;
    irq-gpios = <&gpio0 6 (GPIO_ACTIVE_LOW | GPIO_PULL_UP)>;
    reset-gpios = <&gpio0 8 (GPIO_ACTIVE_LOW | GPIO_PULL_UP)>;
    swap-xy;     // swap X and Y axes
    invert-x;    // negate X
    invert-y;    // negate Y
};
```

Remove any line to disable that transform.

## Build targets (dabase config)

```
- dabase_v2_dongle-usb-log
- dabase_v2_dongle_with_studio
- dabase_v2_left
- dabase_v2_right-promini-i2c
- dabase_v2_right-standalone-usb
- settings_reset (nice_nano + xiao_ble)
```

## Key findings

1. **keymap.c excluded from split peripherals** — `app/CMakeLists.txt:282` only compiles it when `!CONFIG_ZMK_SPLIT || CONFIG_ZMK_SPLIT_ROLE_CENTRAL`. Driver-side layer-toggle can't link on peripherals.

2. **ZMK already has `zmk,input-processor-temp-layer`** — guarded by the same condition, but the dongle (central) meets it. Needs `#input-processor-cells = <2>` (param1=layer, param2=timeout_ms).

3. **Layer toggle lives on the dongle** — the `temp_layer` processor activates `tp_layer` (layer 1) when REL_X/REL_Y arrive via BLE split, deactivates after 2s idle.

4. **Axis config moved to DT** — `swap-xy`, `invert-x`, `invert-y` are now per-node booleans. No more recompiling the driver to flip an axis.

## Success

- [x] All 8 builds pass (dabase config)
- [x] Dongle activates `tp_layer` on trackpoint motion, deactivates after 2s idle
- [x] `swap-xy`, `invert-x`, `invert-y` configurable via devicetree, default false
- [x] Cursor moves, layer toggle works
