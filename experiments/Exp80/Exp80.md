# Exp80: PS/2 drop hold — resync the 3-byte assembler after a lost byte

## Hypothesis

The random cursor jumps on the direct-PS/2 dabase right half are not a bad
packet being decoded; they are a **partial packet being completed by the next
packet after a byte was lost**.

The chain in the code:

1. `ps2_gpio_read_abort()` drops the byte that failed parity/stop/timing.
2. `CONFIG_PS2_GPIO_NO_RESEND=y` suppresses the `0xFE` **and** the resend
   callback (`ps2_gpio.c`, `ps2_gpio_send_cmd_resend_worker()`). The input
   driver is never told that a byte vanished.
3. `zmk_mouse_ps2_activity_callback()` still has the partial 3-byte packet in
   `packet_buffer`. The next packet's bytes fill the hole. Depending on which
   byte was lost, the result is a plausible-looking bogus packet → jump.
4. Secondary: the missed-interrupt abort had no `return`, so the same edge was
   reprocessed as a start bit; and the single `callback_byte` slot could
   silently coalesce bytes when the callback work queue was delayed.

The obvious switch, `CONFIG_PS2_GPIO_ENABLE_PS2_RESEND_CALLBACK`, cannot be
used on this stack: the pinned Zephyr `v4.1.0+zmk-fixes` still exposes the
one-argument `ps2_config()`. Enabling it fails to compile.

Fix: report lost bytes to the input driver **in stream order**, drop the partial
packet, and hold movement until the 3-byte phase is proven again.

## Plan

| Repo | Change |
|------|--------|
| `zmk-ps2-trackpoint-driver` (`Exp80` branch, from `75d4d71`) | Ordered byte/error event queue in `ps2_gpio` (replaces the single callback slot). Module-private `ps2_gpio_set_read_error_callback()` (`include/zmk/ps2_gpio.h`). `CONFIG_ZMK_INPUT_MOUSE_PS2_HOLD_ON_DROP` state machine: after a drop, scan for a status byte, assemble a candidate packet, confirm two more starts 3 bytes apart, then resume; nothing from the damaged packet is reported as movement. Fix the missed-interrupt `return`. Per-drop WRN (`pos`, `reason`, `t=`) + counters. |
| `zmk-config-dabaseV_0-2` (`Exp80` branch, from `origin/Exp78`) | Restore `zmk-usb-logging` + `LOG`/`LOG_BACKEND_UART`/shell/`LOG_CMDS`/4K log buffer on the **right-half ps2-direct build only** (left stays logging-free per Exp79). `CONFIG_ZMK_INPUT_MOUSE_PS2_HOLD_ON_DROP=y`, keep telemetry. Pin driver `3ac4d1d`. |
| `trackpoint-knowledge` | This file + `Experiments.md` row. |

Procedure: build via GH Actions → flash `dabase_v2_right-ps2-direct` → open
the right half's USB CDC log at 115200 → reproduce long enough to catch a drop.

## Build

- Driver `Exp80` = `3ac4d1d` (`Exp80: hold pointer on PS/2 byte loss (ordered
  event queue + resync)`), pushed to
  `Magid-William/zmk-ps2-trackpoint-driver`.
- Config `Exp80` = `d412c18` (`Exp80: right-half USB logging, drop hold,
  driver 3ac4d1d`), pushed to
  `Magid-William/zmk-config-dabaseV_0-2`.
- GH Actions run: pending.

## What to capture

Per second (both lines are `LOG_INF`):

```
ps2_gpio: int=… abort=…(miss=… start=… par=… stop=… to=…) err=… resend_supp=… deliv=… cb_lost=…
ps2_mouse: bytes=… pkts=… align_abort=… buf_to=… drops=… held=… resync=…
```

Per event (`LOG_WRN`):

```
PS/2 read drop: pos=… reason=… resend-suppressed t=…
PS/2 byte lost at pkt_idx=…; holding cursor until resync t=…
PS/2 resync complete: aligned on a packet boundary t=…
```

How to read it:

- **Drop + resync but no jump** = fixed. The cursor should hold briefly (~2-3
  packets), then continue cleanly.
- **Jump with no `PS/2 read drop` line** = a silent bit error that still passed
  parity/stop, or a downstream problem. `ps2_mouse: pkts` still rising while
  the cursor freezes points at the split/BLE/HID path, not PS/2.
- **`deliv` > `bytes` / `cb_lost` > 0** = bytes lost in the callback handoff.
  The event queue should make `cb_lost` stay 0; if it does not, the consumer is
  stalling for >~100 ms and the next step is a bigger queue / higher callback
  thread priority.
- `par`/`stop` dominating = signal integrity (pull-ups, wire length, noise).
  `miss`/`to` dominating = clock jitter / CPU latency.

## Findings

Pending flash + user capture.

## Conclusion

Pending.

## Next

- If drops still move the cursor: inspect the `deliv`/`bytes`/`cb_lost` split
  and consider making the PS/2 callback work queue cooperative/higher
  priority.
- If the hold feels too long: pass the abort reason/position to the input
  driver and use the exact remaining-byte count for parity/stop aborts (clean
  re-framing), keeping the full scan only for missed-clock/timeout aborts.
- If `par`/`stop` dominates: hardware-side look at the PS/2 lines before more
  driver filtering.
