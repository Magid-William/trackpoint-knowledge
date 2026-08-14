# Exp32 — Hold J to Scroll (Trackpoint scroll mode)

## Hypothesis

ZMK's input-processor layer-specific overrides can make the trackpoint act as a scroll wheel when a key is held. Using `&zip_xy_to_scroll_mapper` (maps REL_X→REL_HWHEEL, REL_Y→REL_WHEEL) in a `scroll_override` child node on the listener, activated when layer 6 is active, gives the user scroll-on-hold without any driver changes.

## Plan

1. Add `scroll_layer` (layer 6) to `dabase_v2.keymap`, keyed by `&mo 6` on `tp_layer`'s J position
2. Add `#include <input/processors.dtsi>` and a `scroll_override` child on `trackball_listener` (central mode) and `trackball_split` (peripheral mode) in `dabase_v2_right.overlay`
3. Same on `dabase_v2_dongle.overlay` — the dongle is the central in split topology, so its listener processes the trackpoint data
4. Tune speed via `&zip_scroll_scaler 1 5` (0.2×)
5. Tune axis inversion via `&zip_scroll_transform`

## Changes

### `dabase_v2.keymap`
- `tp_layer`: `&kp J` → `&mo 6`
- Added `scroll_layer` (layer 6): same bindings as `tp_layer`, J = `&trans`, thumbs keep MB1/MB2/MB3 for click-while-scrolling
- Replaces the old `extra1` (reserved) slot

### `dabase_v2_right.overlay`
- `#include <input/processors.dtsi>` and `#include <dt-bindings/zmk/input_transform.h>`
- `trackball_listener` (central/standalone): `scroll_override { layers = <6>; input-processors = <&zip_xy_to_scroll_mapper>, <&zip_scroll_transform INPUT_TRANSFORM_Y_INVERT>, <&zip_scroll_scaler 1 5>; }`
- `trackball_split` (peripheral/split): same override

### `dabase_v2_dongle.overlay`
- Same includes and `scroll_override` on `trackball_listener`, with `process-next` to keep `&tp_temp_layer` active alongside the scroll mapper

## Commits

| Commit | Change |
|--------|--------|
| `748e424` | Initial scroll layer + overrides |
| `756f261` | Add scroll_override to dongle |
| `bcc3050` | Scale 0.2× via `&zip_scroll_scaler 1 5` |
| `d625351` | Add axis inversion |
| `84aa6d0` | Y-only inversion (X was correct) |

## Findings

- `zmk,input-listener` supports `layers` property on child nodes for layer-specific input processor overrides (per ZMK docs)
- `&zip_xy_to_scroll_mapper` maps REL_X→REL_HWHEEL, REL_Y→REL_WHEEL — no custom code needed
- `&zip_scroll_scaler` multiplies/divides scroll values after mapping
- `&zip_scroll_transform` inverts scroll axes independently via `INPUT_TRANSFORM_X_INVERT` / `INPUT_TRANSFORM_Y_INVERT`
- The `process-next` flag chains the override with base processors (needed on the dongle where `&tp_temp_layer` must keep running)
- In split mode, the dongle's listener processes trackpoint data; its `scroll_override` is the one that matters for the dongle topology

## Result

**Success.** Trackpoint scrolls when J is held on `tp_layer`. Speed 0.2× of raw movement. Y-axis inverted for natural scroll direction. No PMW3610 driver changes — all done in shield config and overlays.

## Next

- Consider adding scroll acceleration or variable speed
- Could expose `scroll-scale`, `scroll-invert-x`, `scroll-invert-y` as DT properties on the trackball node for easier tuning without touching overlays
