# Exp28: Migrate trackpoint to dabase config

## Hypothesis

Migrate the working Exp27 trackpoint configuration (ZMK pinned SHA `fa33e35f`, driver `041f096`, split input/listener architecture) from `zmk-trackpoint-shield` into the dabase keyboard config repo (`zmk-config-dabaseV_0-2`) so the trackpoint works in the dabase 2-peripheral + dongle topology.

## Changes

### zmk-config-dabaseV_0-2 (Exp28 branch, based on feature/promini-i2c)

| File | Change |
|------|--------|
| `config/west.yml` | Pin ZMK to `fa33e35f`, driver to `041f096` (combined I2C + extensive logging). Add `zmk-input-processor-report-rate-limit` and `zmk-input-processor-xyz` repos. |
| `config/dabase_v2_right.conf` | Added `CONFIG_I2C=y`, `CONFIG_TRACKPOINT_I2C_LOG_LEVEL_DBG=y`, `CONFIG_LOG_PRINTK=y` |
| `config/dabase_v2_dongle.conf` | Added `CONFIG_LOG_PRINTK=y`, `CONFIG_ZMK_POINTING=y`, `CONFIG_ZMK_USB=y`, `CONFIG_ZMK_USB_INIT_PRIORITY=60`, `CONFIG_BT_SHELL=y`, `CONFIG_ZMK_SPLIT_BLE=y`, `CONFIG_ZMK_BLE_EXPERIMENTAL_CONN=y`, `CONFIG_ZMK_INPUT_LISTENER=y` |
| `config/dabase_v2_left.conf` | Added `CONFIG_SHELL=y`, `CONFIG_KERNEL_SHELL=y`, `CONFIG_LOG=y`, `CONFIG_LOG_PRINTK=y` |
| `boards/shields/dabase_v2/dabase_v2_right.overlay` | Added `irq-gpios = <&gpio0 6 (GPIO_ACTIVE_LOW \| GPIO_PULL_UP)>` to trackball node |
| `boards/shields/dabase_v2/dabase_v2_dongle.overlay` | Added `split_inputs` + `trackball_split` + `trackball_listener` nodes (were missing — dongle had no way to receive split input events) |
| `build.yaml` | Added `snippet: zmk-usb-logging` and `artifact-name` to left build target |

### promini-trackpoint

No changes to source, but the Pro Mini needs to be flashed with `trackpoint-i2c-slave.ino` (Exp26, PS/2 CLK D7 DAT D3). Pre-built hex at `build/Exp27/promini/pro-mini-i2c-slave/trackpoint-i2c-slave.ino.hex`.

### zmk-pmw3610-driver

No changes (already at `041f096` with extensive logging).

## Build targets

```yaml
- dabase_v2_dongle-usb-log          # XIAO BLE central, USB HID to PC
- dabase_v2_dongle_with_studio      # XIAO BLE with studio support
- dabase_v2_left                    # left NiceNano peripheral
- dabase_v2_right-promini-i2c       # right NiceNano peripheral (trackpoint)
- dabase_v2_right-standalone-usb    # right NiceNano standalone (USB direct)
- settings_reset-nice_nano
- settings_reset-xiao_ble
```

## Key bugs found & fixed

1. **Dongle missing listener**: The dongle overlay had no `trackball_listener` or `trackball_split` nodes. On the central side, these are required to receive split input events from the peripheral over BLE. Fixed by adding them to `dabase_v2_dongle.overlay`.

2. **Dongle missing configs**: `CONFIG_ZMK_POINTING`, `CONFIG_ZMK_USB`, `CONFIG_ZMK_INPUT_LISTENER`, and other configs were missing from the dongle conf. Without these, the listener can't process pointing events or send them via USB HID.

3. **irq-gpios missing**: The right overlay's trackball node was missing `irq-gpios`. Added `P0.06` (MOT/IRQ pin).

4. **Settings reset required**: After flashing new firmware on peripherals, BLE bonds from the old firmware prevent pairing. Must flash `settings_reset` first to clear bonds, then re-flash actual firmware.

## Findings

### Sticky keys cause

The right half exhibited sticky/stuttering keys. Root cause: the Pro Mini was in deep sleep (I2C slave disabled, `TWCR = 0` after 30s idle). The ZMK driver polls at 10ms intervals via `i2c_write_read_dt()`. Each poll against a non-responsive slave causes an I2C timeout (~1-2ms), consuming enough CPU to starve the keyboard scan.

**Fix**: Wire `P0.08` (reset-gpios) on the NiceNano to the Pro Mini's RST pin (via 1kΩ). The driver already asserts this pin for 100ms during init, then releases it. This would cold-boot the Pro Mini and ensure the I2C slave is active when ZMK starts polling. See Exp21.

### Pro Mini firmware

The Pro Mini was running the SPI slave firmware (Exp16). The I2C slave firmware (`trackpoint-i2c-slave.ino`, Exp26, PS/2 CLK=D7, DAT=D3) must be flashed via the CH340G programmer for the I2C handshake to work. Pre-built hex available.

## Success

- [x] All 7 builds pass
- [x] Left, right, and dongle firmware built from dabase config
- [x] Dongle receives keyboard events from both peripherals via BLE
- [x] Settings reset clears stale bonds (left re-paired successfully)
- [x] Cursor moves (after physical Pro Mini reset)

## Suggestion for next experiment

1. **Wire P0.08 → Pro Mini RST** (via 1kΩ) so the ZMK driver's init sequence (100ms LOW + 500ms wait) cold-boots the Pro Mini on every power-up. This eliminates the sleep-stale I2C timeout problem and fixes sticky keys reliably.

2. **Flash Pro Mini with I2C firmware** using the CH340G programmer and the pre-built hex at `build/Exp27/promini/pro-mini-i2c-slave/trackpoint-i2c-slave.ino.hex`.

3. **Verify cold-boot flow**: Power cycle the dongle + peripherals together and confirm trackpoint works without needing a physical Pro Mini reset.
