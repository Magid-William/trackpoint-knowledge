# Exp12: Fix BLE Dongle Stall — Buffer Pools + Report Throttle

## Hypothesis

The BLE dongle stall (keyboard + mouse both freezing ~1-2s after motion starts) is caused by buffer pool exhaustion at the controller level (`CONFIG_BT_CTLR_TX_BUFFERS` default = 3), combined with the missing report throttle (`CONFIG_PMW3610_ALT_REPORT_INTERVAL_MIN` = 0, meaning every MOT tick fires `input_report`). The peripheral saturates the shared BT buffer chain and both keyboard + mouse freeze together waiting for the same pool of 3 controller TX buffers to drain.

Three independent changes (config-only, no code edits):

1. **Built-in report throttle**: `CONFIG_PMW3610_ALT_REPORT_INTERVAL_MIN=20` batches 100 Hz MOT → 50 Hz input reports, halving the notify demand at source
2. **Buffer pool expansion**: Bump three pool sizes from tiny defaults so the burst from motion start doesn't exhaust all available buffers
3. **Diagnostic Kconfigs**: Add thread-monitoring to the peripheral for `kernel threads` in case the stall persists

## Architecture

Same as Exp11 — dongle split topology:

```
PC ──USB HID──→ XIAO BLE (central/dongle) ──BLE split──→ NiceNano (peripheral) ──SPI──→ Pro Mini ──PS/2──→ TrackPoint
```

## Changes by file

### Modified

| File | Change |
|------|--------|
| `corne_trackpoint_right.conf` | Add `CONFIG_PMW3610_ALT_REPORT_INTERVAL_MIN=20`, `CONFIG_BT_L2CAP_TX_BUF_COUNT=10`, thread-monitoring Kconfigs. Note: `BT_CTLR_TX_BUFFERS`/`BT_CTLR_TX_PDU_CNT` don't exist (`BT_CTLR` not enabled on ZMK) and `BT_BUF_ACL_TX_COUNT` doesn't exist in Zephyr 4.1 — only `BT_L2CAP_TX_BUF_COUNT` is valid. |
| `Experiments.md` | Add Exp12 row |

### No changes

| File | Reason |
|------|--------|
| `zmk-pmw3610-driver/` | Zero driver changes per AGENTS.md — `REPORT_INTERVAL_MIN` is already guarded in pmw3610.c |
| `PS2Trackpoint/` | Used as-is |
| `promini-trackpoint/` | Pro Mini firmware unchanged |
| `trackpoint-spi-slave.ino` | Still at PERIOD_MS=10 (100 Hz) as verified in current Exp11 state |
| `trackpoint_dongle/` | Already has diagnostic Kconfigs, not the bottleneck side |
| `build.yaml` | Same 4 targets: peripheral, dongle, 2× settings_reset |

## Build targets

```yaml
- board: nice_nano//zmk
  shield: corne_trackpoint_right
- board: xiao_ble//zmk
  shield: trackpoint_dongle
- board: nice_nano//zmk
  shield: settings_reset
- board: xiao_ble//zmk
  shield: settings_reset
```

## Success criteria

- [x] All 4 build targets pass via GitHub Actions
- [ ] Cursor moves continuously via dongle USB HID >30s without stall
- [ ] If stall persists: `kernel threads` output captured on both boards showing blocked-thread picture

## Per-experts diagnosis plan (if stall persists)

1. On peripheral USB shell during stall: run `kernel threads`
2. Look for `sysworkq` state, notify work queue thread state, and any thread stuck on buffer/fifo wait
3. Confirm the notify work is on its own named thread, not sharing `service_work_q`
4. Also check `kernel stacks` for stack overflow warnings

## Safety notes

- ZMK issue in 2022 reported `BT_BUF_ACL_TX_COUNT=32` causing boot crash on NiceNano. Starting at 10 conservatively.
- If boot fails after buffer bump, bisect down: first remove ACL/L2CAP lines, retry CTLR alone at 6.

## Build result

All 4 targets build successfully.

Artifacts available from GitHub Actions Exp12 run:
- `corne_trackpoint_right-nice_nano__zmk-zmk.uf2` (peripheral)
- `trackpoint_dongle-xiao_ble__zmk-zmk.uf2` (dongle)
- `settings_reset-nice_nano__zmk-zmk.uf2`
- `settings_reset-xiao_ble__zmk-zmk.uf2`

## Flashing (user-manual)

1. Flash **settings_reset** to both boards (clear bonds)
2. Flash **corne_trackpoint_right** to NiceNano (peripheral)
3. Flash **trackpoint_dongle** to XIAO BLE (dongle)
4. Power both — they auto-pair
5. Connect XIAO USB to PC and test cursor motion
6. If stall persists: connect NiceNano USB, run `kernel threads` in shell during stall

## Rollback

Revert `.conf` changes in `zmk-trackpoint-shield`, go back to Exp10 architecture (right half as central, USB direct).

## Next steps

If this resolves the stall: dial `CONFIG_PMW3610_ALT_REPORT_INTERVAL_MIN` lower toward 10 to improve responsiveness while maintaining stability.

If it doesn't: JTAG/SWD debugging is the only remaining path to find the root cause.
