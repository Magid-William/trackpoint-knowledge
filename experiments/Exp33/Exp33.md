# Exp33: Dead zone for trackpoint sensitivity

## Hypothesis

The trackpoint is too sensitive — even the lightest touch produces cursor drift. A per-axis dead zone in the Pro Mini firmware zeroes `burst_x`/`burst_y` when the scaled delta is within ±DEADBAND (default 3), preventing tiny motions from reaching the ZMK driver.

## Changes

### promini-trackpoint (`trackpoint-i2c-slave.ino`)

| Line | Change |
|------|--------|
| 132 | `burst_x = (abs(sx) <= DEADBAND) ? 0 : sx;` |
| 133 | `burst_y = (abs(sy) <= DEADBAND) ? 0 : sy;` |
| 134 | MOT pulse check changed from `abs(burst_x) > DEADBAND \|\| ...` to `burst_x \|\| burst_y` |
| 207–208 | Same per-axis zeroing in sleep wake path |
| 209 | `continue` back to sleep if both axes zero out after deadband |

No ZMK driver changes.

## Divider logic

```c
// After speed_scale division:
burst_x = (abs(sx) <= DEADBAND) ? 0 : sx;
burst_y = (abs(sy) <= DEADBAND) ? 0 : sy;
```

Each axis is independently zeroed. MOT is only pulsed when at least one axis exceeds the dead zone.

## Tuning

`DEADBAND 3` at line 21 — increase to widen the dead zone, decrease to narrow it.

## Result: FAILED

The dead zone approach was too blunt — it either blocks legitimate small movements or lets drift through if DEADBAND is too small. A different approach is needed (e.g. drift compensation via automatic centering rather than hard cutoff). All code changes reverted.
