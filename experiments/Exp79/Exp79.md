# Exp79: Battery life - deep-sleep timeout, tp_layer timing, logging off the battery halves

> **Numbering note.** This work lives on the branch named `Exp78` in
> `zmk-config-dabaseV_0-2` (Oct 12-14 2026), which collides with
> [Exp78](../Exp78/Exp78.md) - the uart-ps2 @19200 attempt (Sep 4
> 2026). The commits on that branch are even prefixed `Exp78:`. Memory numbers
> stay unique, so per the Exp76 precedent the real experiment here is **Exp79**
> and the config-repo branch keeps its (misleading) name. Read `Exp78 branch`
> in that repo as Exp79.

## Hypothesis

The direct-PS/2 dongle stack spends too much battery. Three suspects, in
order of expected cost:

1. **Deep sleep too late / never** - the halves sat at 15 min, and Exp76
   ("right half never sleeps") left the sleep path unproven in the dongle
   topology.
2. **`tp_layer` auto-release timing** - `temp_layer`'s idle timeout governs how
   long the trackpoint layer is held; wrong values either strand the layer or
   thrash it (a keymap change per movement event).
3. **Log framework + shell on the battery halves** - `LOG`, `SHELL`,
   `KERNEL_SHELL`, `LOG_PROCESS_THREAD_STARTUP_DELAY_MS` and a USB CDC console
   keep clocks and buffers alive for no benefit on a half that is never
   plugged in.

Also worth testing: **manual on-demand sleep** (soft-off combos) instead of
waiting for the timeout, so the keyboard can be put away immediately.

## Plan

| Repo | Change |
|------|--------|
| `zmk-config-dabaseV_0-2` (`Exp78` branch) | Sleep strategy in `build.yaml` / `dabase_v2.dtsi` / `dabase_v2_dongle.overlay` / keymap; `tp_layer` timeout; logging + shell stripped from the two battery halves. |
| `zmk-trackpoint-driver`, `trackpoint-knowledge` | None (config-only experiment). |

Procedure: config change -> GH Actions build -> flash -> on-device check of
sleep/wake and pointer feel.

## Build

Eight commits, in the order they were tried:

| Commit | Date | What |
|---|---|---|
| `87cedc9` | Oct 12 | **Always-on halves + manual sleep.** Drop `CONFIG_ZMK_SLEEP`/`IDLE_SLEEP_TIMEOUT` so the halves never auto-sleep; add `CONFIG_ZMK_PM_SOFT_OFF=y`, a `soft_off_wakers` node re-arming `kscan0` during soft off, a `sleep_halves` behavior and combos `sleep_left` Q+P, `sleep_right` L+;. |
| `6efc04b` | Oct 12 | **Fix: node/label `sleep_halves` (12) truncated to `sleep_ha` on the wire** - the BLE split run-behavior payload is `ZMK_SPLIT_RUN_BEHAVIOR_DEV_LEN` = 9 bytes (8 chars + NUL), so the halves never resolved the behavior. Renamed to `halfoff`, matching ZMK's own `z_so_off` / `extpower` / `sysreset` / `rgb_ug`. |
| `75c4172` | Oct 12 | **Fix: sleep on key RELEASE.** The combo fired on press, and split-peripheral-off-on-press killed a half while the combo keys were still held -> soft off with a live matrix -> latched kscan wake line -> only the RST button revived it. `hold-time-ms = 0` on the halves, `60000` on the dongle so the central relays instead of sleeping. |
| `9896a05` | Oct 12 | **Revert the combos.** In the dongle topology the central keeps the invoking half's slot, so that half could not reconnect after waking. Keymap/dtsi/conf/dongle changes rolled back to `d34768e`; only the timeout kept - halves auto deep-sleep after **2 h** (was 5/15 min) and wake on any key. |
| `72678f9` | Oct 12 | `tp_layer` idle `400 -> 10 ms` (`temp_layer` treats 0 as "no auto-release", so 10 is the closest real value to the requested 0); deep sleep `2 h -> 1.5 h` (`5400000`). |
| `492eb52` | Oct 12 | `10 -> 100 ms`. |
| `32224f1` | Oct 12 | `100 -> 50 ms` - 10 ms thrashed the layer (deactivate/re-activate per movement event) and stalled the pointer; 400 lingered. 50 sits just above the typical inter-event gap. |
| `e7e6ec1` | Oct 14 | **Logging + shell off both battery halves.** The `zmk-usb-logging` snippet is what actually selects `LOG`/`CONSOLE`/`USB_UART_CONSOLE` (ZMK select chain), so it is removed from the `dabase_v2_left` and `dabase_v2_right` (ps2-direct) `build.yaml` entries; `CONFIG_SHELL`/`KERNEL_SHELL`/`GPIO_SHELL`/`LOG`/`LOG_BACKEND_UART`/`LOG_PROCESS_THREAD_STARTUP_DELAY_MS` removed from both `.conf` files (Zephyr defaults are `LOG=n` / `SHELL=n`). **Dongle (USB) and `dabase_v2_right-standalone-usb` keep full logging + shell as the debug path.** |

Final state = `origin/Exp78` (`e7e6ec1`) = `origin/Exp77` + these 8 commits;
`Exp77` was fast-forwarded to `32224f1` on Oct 20 2026 (the 7 commits up to the
`tp_layer` retune; only `e7e6ec1` stayed unique to the branch).

## Result

**Success - for the parts that are config-decided; the sleep timing itself was
never bench-timed.**

What the branch establishes:

- The **manual soft-off combo** approach is dead for this topology - not a bug
  to fix but a structural limit: the central holds the half's connection slot,
  so a half that soft-offs cannot be reconnected through it. Two real bugs were
  found and fixed on the way (`sleep_ha` truncation; soft off with a live
  matrix latching the wake line), and then the whole approach was dropped.
- The **reliable sleep path** is the plain ZMK one: `CONFIG_ZMK_SLEEP=y` +
  `IDLE_SLEEP_TIMEOUT=5400000` (1.5 h) on both battery halves, wake on any key.
  Same mechanism Exp49/Exp50 proved; only the timeout moved.
- `tp_layer` needs a **non-zero but small** timeout: `0` = never release
  (`temp_layer` semantics), `10` = layer thrash that stalls the pointer,
  `400` = noticeable linger. **`50 ms`** is the value that sits just above the
  inter-event gap while moving.
- The **`zmk-usb-logging` snippet is the master switch** for log + shell on
  these builds - dropping the snippet (not the `CONFIG_LOG*` lines) is what
  actually turns the framework off, because the snippet's select chain
  re-enables it.

Known trade-off, and it contradicts the house rule in `AGENTS.md` ("every
build must have USB logging"): the two battery halves now have **no shell and
no log framework**, so headless GPREGRET bootloader entry over serial is
**not possible on the `dabase_v2_right-ps2-direct` build**. Recovery is the
`-standalone-usb` build (which keeps everything) or the reset button.

Not evidenced anywhere in the record, so treat as unverified: that the halves
actually reach 1.5 h idle and wake cleanly from it, and that dropping the log
framework measurably extended battery life. Nobody measured current before or
after.

## Conclusion

The battery story on this stack is one config knob and one discipline, not a
feature: keep the halves on ZMK's own deep sleep with a long timeout, keep
`tp_layer` at 50 ms, and build the battery halves without the logging snippet
while keeping a debug build for bring-up. Manual sleep combos and any
"peripheral powers itself off" scheme are off the table in the dongle topology.

## Next

- **Measure** it: current draw per half with and without the logging snippet,
  and a timed 1.5 h idle -> key wake test. Right now "success" is a config
  argument, not a number.
- Decide the logging rule: either accept per-build logging exceptions in
  `AGENTS.md`, or keep a tiny debug shell on the halves and pay for it.
- If battery still drains fast (open wishlist item), the remaining suspects are
  the PS/2 clock line and the `gpio-ps2` interrupt stream keeping the core
  awake - that is a real experiment (Exp80 territory), not a config tweak.
