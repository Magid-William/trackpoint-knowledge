# Exp67: Back on Pro Mini — 4-wire I2C, no MOT wire

## Context

Done with the ATtiny85. Switched the bridge back to the Pro Mini. Wiring
(matches Exp45 proven pins):

```
trackpoint [GND, CLK, DAT, VCC] => promini [GND, D7, D3, VCC]
promini     [GND, A5, A4, VCC]  => nicenano [GND, P0.20, P0.17, VCC]
```

No MOT (P0.06), no RST (P0.08), no power gates — pure 4-wire I2C like Exp47.
The Pro Mini is powered directly off the nice_nano VCC rail (P0.13, kept on via
EXT_POWER).

## Hypothesis

Re-flash the Pro Mini with the known-good Exp60 firmware (CLK=D7, DAT=D3,
PowerCurve, `SLEEP_ENABLED 0`) and make the current ZMK config poll (drop the
ATtiny-era MOT `irq-gpios`) → cursor moves again on the Pro Mini.

## Plan

1. Flash Pro Mini via CH340G `stk500v1` with `build/Exp60/promini/pro-mini-i2c-slave/trackpoint-i2c-slave.ino.hex`
   (from `8e345cb`, the last known-good Pro Mini build).
2. ZMK config (`zmk-config-dabaseV_0-2` branch Exp67): remove `irq-gpios` from
   `dabase_v2_right.overlay` → driver switches to plain 10ms polling
   ("no irq-gpios in DT — MOT disabled, plain polling"). Keep EXT_POWER P0.13 on,
   keep ZMK deep-sleep + param re-apply (Exp60 model).
3. Build via GH Actions, flash NiceNano, verify `i2c scan` finds 0x42 and the
   nub moves the cursor.

## Findings

- Pro Mini flashed + verified via CH340G (busid 3-4, COM28) → sig `0x1e950f`,
  6500 B written + verified. Hands-free (DTR auto-reset, `stk500v1` sync). No
  need to press reset.
  - Gotcha: WSL had stopped; `usbipd attach` fails with "no WSL 2 distribution
    running" — keep a WSL session alive first. Also, the Alpine default user is
    uid 1000 → `modprobe ch341` fails EPERM — run WSL commands as root
    (`wsl -d Alpine -u root`).
- Config `c10c20e` (Exp67 branch) pushed; GH run `32251403482` **success**.
  All 7 uf2 built. Right-half (trackpoint) build:
  `build/Exp67/firmware/dabase_v2_right-promini-i2c.uf2` (547,840 B).
- NiceNano USB was unplugged for the rewiring — right-half uf2 staged, flash
  pending user reconnecting the keyboard.

## Conclusion

Pro Mini side done: flashed + verified (sig `0x1e950f`, 6500 B). ZMK config
(Exp67) built clean with MOT irq-gpios removed → driver polls at 10 ms.

**Pending:** NiceNano was unplugged for the rewiring ("main keyboard not
connected"). Once reconnected:
1. `.\flash-nicenano.ps1 -UF2Path "build\Exp67\firmware\dabase_v2_right-promini-i2c.uf2" -ComPort COM<X>`
   (probe all COMs for `uart:~$` first).
2. Shell: `i2c scan i2c@40003000` → expect `0x42`.
3. `.\live-trackpoint.ps1 -Port COM<X> -Burst` or just touch the nub → cursor moves.