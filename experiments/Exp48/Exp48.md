# Exp48: NPN+PNP power switching from NiceNano (hardware cut)

## Hypothesis

P0.06 (NPN, low-side GND switch) + P0.08 (PNP, high-side VCC switch), each via 10k base resistor, let the NiceNano physically cut/allow power to the Pro Mini + trackpoint. Two firmware builds prove it with zero runtime interaction: **poweron** (ext-power asserts the pins → Pro Mini powered) and **poweroff** (`CONFIG_ZMK_EXT_POWER=n` → pins float → both transistors off → Pro Mini dead). Then a DOT-key `&ext_power EP_TOG` binding gives live on/off control.

## Changes by repo

| Repo | Branch | Commit | Change |
|------|--------|--------|--------|
| promini-trackpoint | — | — | **NO changes — never flashed** (`0a8af67` stays) |
| zmk-pmw3610-driver | Exp48 | `6f84b62` | Edge-triggered link logs: single `LOG_WRN` on NACK edge, `LOG_INF` on recovery — replaces per-poll `LOG_ERR` flood. **Stated exception** to the zero-driver-change rule (user-approved) |
| zmk-trackpoint-shield | Exp48 | `88cb051`→`8db0daa`→`70ba6d9`→`93cb179`→`c7e78be`→`86d1dd9` | ext-power integration, fixes, DOT toggle key |

Shield commit history:
- `88cb051`: ext-power node (P0.06/P0.08), `CONFIG_TRACKPOINT_I2C_LOG_LEVEL_INF`, poweron/poweroff builds, driver pinned `6f84b62`
- `8db0daa`: label collision — ZMK behavior already owns `ext_power`
- `70ba6d9`: log-level Kconfig is the choice-style bool (`_INF=y`, not string `INF`)
- `93cb179`: attempted `&{/EXT_POWER}` path ref → **DT parse error** (unsupported)
- `c7e78be`: **THE FIX** — node-name merge: re-declare `EXT_POWER` inside `/ {}` (Zephyr overlay merge)
- `86d1dd9`: DOT key → `&ext_power EP_TOG` (base + tp layers)

## Key findings

1. **`zmk,ext-power-generic` supports only `DT_INST(0)`.** The driver (`app/src/ext_power_generic.c` at ZMK `fa33e35f`) hardcodes instance 0 for the device, config, and settings handler. Any second ext-power node compiles but is **never initialized** — pins stay at reset default.
2. **nice_nano v2 already owns instance 0.** `app/boards/nicekeyboards/nice_nano/nice_nano_nrf52840_zmk_2_0_0.overlay` defines `EXT_POWER { compatible = "zmk,ext-power-generic"; control-gpios = <&gpio0 13 GPIO_ACTIVE_HIGH>; }` — the VCC header rail load switch. Symptom of the conflict: PIN_CNF[13]=0x3 (output), PIN_CNF[6]/[8]=0x2 (reset default), `i2c scan` = 0 devices despite `CONFIG_ZMK_EXT_POWER=y` and the driver compiling.
3. **Fix**: re-declare `EXT_POWER` in the shield overlay inside a root `/ {}` block → overlay merge replaces `control-gpios` in place. All three pins in one node: P0.08 (PNP, `GPIO_ACTIVE_LOW`), P0.06 (NPN, `GPIO_ACTIVE_HIGH`), P0.13 (rail switch, `GPIO_ACTIVE_HIGH`). Settings key/name stability preserved.
4. **Hardware**: emitter/collector swapped on a transistor → no current despite correct base drive (2N3904/2N3906 E/C are not symmetric!). Correct: PNP emitter→3.3V, collector→Pro Mini VCC; NPN emitter→GND, collector→Pro Mini GND. Measured at base: P0.06 ≈ 2.92V (NPN on), P0.08 ≈ 0.01V (PNP on).
5. **Multimeter gotcha**: "VCC connected" measured at the NiceNano header is the P0.13 rail — NOT the Pro Mini's VCC pin through the PNP. Measure at the Pro Mini VCC pin for the truth.
6. **Flash script**: the bootloader disconnects the NICENANO drive immediately after accepting the UF2 — "drive vanished after copy" = success, not an error. `flash-nicenano.ps1` updated: copy retry loop + accept-on-disconnect detection + size verify.

## Final wiring (I2C + power gates)

| NiceNano | Part | Pro Mini |
|---|---|---|
| P0.06 | 10k → NPN base; emitter→GND; collector→Pro Mini **GND** | GND cut |
| P0.08 | 10k → PNP base; emitter→3.3V; collector→Pro Mini **VCC** | VCC cut |
| P0.13 | nice_nano VCC rail switch (in ext-power control list, kept on) | — |
| P0.17 | SDA (+4.7k to 3.3V) | A4/D18 |
| P0.20 | SCL (+4.7k to 3.3V) | A5/D19 |
| GND | GND | GND |

DOT key (base + tp layers) = `&ext_power EP_TOG`.

## Verification

- **poweron build**: `i2c scan i2c0` → `0x42` found (1 device); burst read 0x12 → `00 00` idle; mouse moves (after E/C swap fix).
- **poweroff build**: `i2c scan` → 0 devices; PIN_CNF[6]/[8] = `0x2` (reset default, floating); Pro Mini VCC ≈ 0V, LED off (user confirmed).
- **Recovery**: reflash poweron → `0x42` back, mouse moves.
- **DOT toggle**: DOT → 0 devices; DOT → `0x42` back. Live power cut/restore proven.

## Caveats

- **Settings persistence**: ext-power state is saved to settings (60s debounce, `CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE`). A toggled-OFF state survives reboots and reflashes — if the trackpoint seems dead after a flash, press DOT once. `settings_reset` clears it.
- EP_OFF (DOT or poweroff build) also drops P0.13 → the whole nice_nano VCC rail goes off. Fine standalone; watch in split/dongle topologies where the rail might feed the other half.

## Conclusion

**Success.** The NPN+PNP gate pair cleanly cuts and restores Pro Mini power from the NiceNano, proven two ways: firmware builds (poweron/poweroff) and a live DOT-key toggle. The wishlist item "cut trackpoint power after inactivity" now has a working hardware mechanism — the remaining step is automation (cut on keyboard sleep / idle).

## Known-good pins

- promini `0a8af67` (Exp47, untouched)
- driver `6f84b62` (Exp48)
- shield `86d1dd9` (Exp48)
- ZMK `fa33e35f`

## Next steps

1. Automate the cut: `CONFIG_ZMK_SLEEP` + PM suspend already disables ext-power (`ext_power_generic_pm_action`) → trackpoint power dies with keyboard sleep automatically. Test idle→sleep→wake power behavior (Pro Mini reboot delay on wake ≈ 1-2s is the tradeoff vs Exp36 MOT gating).
2. Investigate silent USB logging (Exp47 carryover) if log-based verification is needed.
3. If battery still drains: dongle topology / third peripheral with own battery.
