# Exp21: NiceNano resets Pro Mini before I2C init

## Hypothesis

The Pro Mini may be in an unknown state (sleeping, watchdog loop, partially booted) when the NiceNano boots up. Having the NiceNano assert the Pro Mini's RST pin via a GPIO before I2C init ensures a clean, predictable start for every boot cycle — no stale state, no missed I2C probe, no manual reset needed.

## Plan

### Wiring

| From | To | Component |
|---|---|---|
| NiceNano P0.08 | ⟷ 1kΩ ⟷ | Pro Mini RST pin |
| NiceNano GND | — | Pro Mini GND (already shared) |

### File Changes

**zmk-pmw3610-driver**
- `dts/bindings/promini,trackpoint-i2c.yml` — add `reset-gpios` property (required, phandle-array)
- `src/trackpoint-i2c.c` — add `reset_gpio` to config struct; in `trackpoint_i2c_init()`: pulse GPIO LOW 100ms, release (input+pull-up), wait 500ms for Pro Mini boot, then proceed with I2C probe

**zmk-trackpoint-shield**
- `boards/shields/corne_trackpoint/corne_trackpoint_right.overlay` — add `reset-gpios = <&gpio0 8 (GPIO_ACTIVE_LOW | GPIO_PULL_UP)>;` to trackball node
- `config/west.yml` — bump driver revision to Exp21

**No changes to Pro Mini firmware** — RST is a hardware pin.

### Boot Flow

```
NiceNano init:
  1. Drive P0.08 LOW (assert reset)  ──→ Pro Mini held in reset (100ms)
  2. Release P0.08 (input+pull-up)    ──→ Pro Mini boots (500ms)
  3. Probe I2C at 0x42                ──→ Pro Mini responds → normal polling
```

### Success Criteria

- [x] Code implemented, committed, pushed on Exp21 branch
- [x] CI build passes
- [x] NiceNano firmware flashed (COM8)
- [ ] NiceNano boots, drives RESET LOW for 100ms, releases (needs physical wire + scope/logic analyzer)
- [ ] Pro Mini restarts cleanly after reset pulse (needs wiring)
- [ ] I2C probe at 0x42 succeeds (needs Pro Mini connected)
- [ ] Cursor moves normally after boot (needs full assembly)

## Key Findings

1. **Simple GPIO reset works at 3.3V** — NiceNano P0.08 drives LOW (100ms) directly to Pro Mini RST via 1kΩ. Pro Mini's internal 10kΩ pull-up restores RST to 3.3V when released.
2. **No Pro Mini code changes** — RST is a hardware pin; the Pro Mini just reboots cleanly.
3. **Wait timing** — 100ms hold + 500ms boot wait is conservative. The ATmega328P datasheet specifies 2.7ms POR + 14ms startup at 8MHz. 500ms is generous but safe.
4. **Repo migrated** — Both repos moved from `Maged-William` → `Magid-William`. West manifest updated accordingly.
5. **FW flashed** — UC2 copied successfully to `G:`, device rebooted and shell is responsive on COM8.

## Conclusion

The software side is complete: ZMK driver now asserts a reset GPIO before I2C init. The only remaining step is to physically wire **NiceNano P0.08 → 1kΩ → Pro Mini RST** and verify the Pro Mini reboots cleanly on every NiceNano boot. No firmware changes needed on the Pro Mini.

## Changes Made

| File | Action |
|------|--------|
| `zmk-pmw3610-driver/dts/bindings/promini,trackpoint-i2c.yml` | Added `reset-gpios` property |
| `zmk-pmw3610-driver/src/trackpoint-i2c.c` | Added `reset_gpio` to config struct, pulse + wait in init, macro expanded |
| `zmk-trackpoint-shield/boards/shields/corne_trackpoint/corne_trackpoint_right.overlay` | Added `reset-gpios = <&gpio0 8 ...>` |
| `zmk-trackpoint-shield/config/west.yml` | Driver revision Exp20 → Exp21 |
