# Exp68: badjeff's PS/2 driver, never-send-commands mode, trackpoint DIRECT to nice_nano

## Hypothesis

The weird generic USB trackpoint streams standard 3-byte PS/2 packets on power-up and
rejects all host commands (Exp06). badjeff's `kb_zmk_ps2_mouse_trackpoint_driver`
(UART-trick `ps2_uart` @ 9600 on nRF52) is proven working on a nice_nano via justinmklam's
dabao keyboard. If we strip **every** host→device command from that driver, the trackpoint
still streams, the nRF decodes it directly, and the cursor moves with **no AVR coprocessor**.

## Result: SUCCESS — cursor moves smoothly, 4 directions, zero host commands, no coprocessor.

## Plan + journey

1. **Fork** `badjeff/kb_zmk_ps2_mouse_trackpoint_driver` @ `2df4d6d` (exactly what
   justinmklam's verified combo pins) → `Magid-William/zmk-ps2-trackpoint-driver`,
   branch `Exp68`.
2. **`ZMK_INPUT_MOUSE_PS2_NO_HOST_COMMANDS`** (default n): init thread skips self-test/
   reset/detect/config handshake (waits 1.5 s for the power-up stream, enables callback
   without the 0xF4 write); resend (0xFE) and reporting (0xF4/0xF5) become no-ops;
   `ps2_write()` itself refuses everything (-ENOTSUP).
3. **Config repo** `Magid-William/zmk-config-ps2-test`, branch `Exp68`, ZMK pinned
   `ac7f75b8`, shield `ps2test_right` (bench 4x6 matrix = corne_trackpoint right wiring).
4. **Backend conclusion**: the `uart-ps2` trick NEVER worked cleanly on THIS trackpoint
   (see findings) — the final build uses the **`gpio-ps2` backend** (CLK-falling-edge
   interrupt sampling = the same mechanism as the AVR decoder this project was built on).

## Final state

- TrackPoint **DAT → P0.08 (pro_micro 0)**, **CLK → P0.06 (pro_micro 1)**,
  **RST float**, VCC 3.3 V rail via EXT_POWER (P0.13). Internal pull-ups on both lines
  (patched in `ps2_gpio.c` — it zeroes DT flags and reconfigures pins itself).
  NOTE: this is REVERSED vs stock dabao; the bench wiring proved the physical truth.
- Config: `gpio-ps2` backend, `MY_NO_HOST_COMMANDS=y`, X/Y decoded as **int8** (this TP's
  status byte was garbled over UART; with gpio-ps2 the decode is clean anyway),
  `SPEED_DIVISOR=5` (remainder-accumulated, user's ~x5-of-1% ask), clicks disabled,
  error mitigation OFF, ZMK_EXT_POWER=y, shell + USB logging (AGENTS.md rule).
- Filters (median-5/slew/EMA) were added for the UART decode and are **OFF** in the
  final config (they only add lag on the clean GPIO decode) — code stays Kconfig-gated.

## Findings (the long road)

1. **Wiring**: only RX-on-P0.08 ever received frames; P0.06 never does. First no-data
   builds were due to missing `CONFIG_ZMK_EXT_POWER` (P0.13 rail off) + no CLK pull-up.
   Verify by P-number, not silkscreen.
2. **`uart-ps2` @ 9600**: phase-walk alphabet {8C, CC, C4, 84, 80, BE, BD, BF, FA, F6...},
   every byte ≥ 0x80 → "up/left only" (status sign bits garbage).
3. **`uart-ps2` @ 14400** (9600×1.5 = measured cell rate): real 4-directional decode but
   ~50/50 erratic — one byte per packet randomly reads full-scale (±127/-128/-64) in
   bursts of 2-3 packets. Root cause: **this trackpoint's clock jitters** (cheap RC
   oscillator); fixed-rate UART sampling straddles bit boundaries. Filters (median-3,
   EMA-8, median-5+slew) improved but never fixed it.
4. **`gpio-ps2`**: samples DAT on real CLK falling edges → jitter-immune → **smooth**.
   This is the backend to use for jittery-clock trackpoints; the UART trick's README
   caveat ("needs a compatible clock") is exactly what it means.
5. USB-serial log output stays silent on this build (known Exp47/56 quirk); `printk` diag
   worked for bring-up and was removed (flood + console interference).
6. Build-ops: west.yml `revision` must be the repo's own name (schema rejects
   `repo-name`); board target is plain `nice_nano` (not `//zmk`) at ac7f75b8; DTS
   comments must use `//`, not `#` (preprocessor directive!); a PowerShell prefix-replace
   concatenated a SHA once — set full SHAs.

## Known-good revisions

- ZMK: `ac7f75b8`
- Driver fork (Exp68): `5ef6699` (final; all filter options Kconfig-gated, off)
- Config (Exp68): `fb9e917` — build run `33206064201`, speed divisor 1 (raw)
- Firmware flashed on bench nice_nano (COM8): `ps2test_right-nice_nano.uf2`
- User-confirmed: **smooth, 4 directions, speed "Perfect"** (x5 of the
  middle build = raw stream, as close to their x6 ask as integers allow)

## Files changed (fork delta vs upstream JSON)

- `src/drivers/input/Kconfig`: NO_HOST_COMMANDS, SPEED_DIVISOR, MOVEMENT_EMA_N, MEDIAN_WINDOW, SLEW_MAX
- `src/drivers/input/input_mouse_ps2.c`: gated init thread, write-free reporting/resend,
  int8 X/Y decode, divisor/EMA/median/slew plumbing
- `src/drivers/ps2/ps2_uart.c`: write → -ENOTSUP; SCL pull-up during receive
- `src/drivers/ps2/ps2_gpio.c`: SCL+SDA pull-ups in pin config

## Next experiments (suggestions)

- Port to dabase_v2 right (production): TP direct to those freed P0.06/P0.08 on the
  keyboard PCB; power-gating the TP (P0.13) for sleep; layer-toggle via `temp_layer`.
- Optional: upstream `.dtsi`-level pull-up flags so no fork patch is needed.
- Worth documenting in AGENTS.md: the gpio-ps2-vs-uart-ps2 decision for this TP.