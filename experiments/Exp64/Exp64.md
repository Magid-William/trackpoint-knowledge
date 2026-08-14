# Exp64: MOT data-ready + power-gate cleanup

## Context

The ATtiny85 I2C slave has been running with the ZMK driver **polling every
10ms** (Exp62 fixed-rate re-poll). The PS/2 packet period (~8.7-10ms) and the
10ms poll are rate-mismatched, so Exp63 added accumulator math to patch dropped
packets. The MOT pin (PB1, physical pin 6) existed but was held HIGH and unused;
the driver's `irq-gpios` support was still the old **sleep gate** (HIGH=stop).

The NPN/PNP power gates (Exp48) are no longer used — the ATtiny85 is a
standalone 4-wire config, always powered from the NiceNano's EXT_VCC.

## Hypothesis

Repurposing MOT as an **active-low data-ready level** (LOW = motion pending,
HIGH = drained) lets the ZMK driver read on the falling edge instead of a fixed
10ms poll. This eliminates the rate-mismatch at the source and drops idle I2C
traffic to ~1 heartbeat/sec. A 1s heartbeat keeps dead-link recovery (Exp62) and
param re-apply (Exp60) intact. Simultaneously, strip the unused NPN/PNP gates
from EXT_POWER (freeing P0.06 for MOT) and remove the dead poweroff build.

## Implementation

### ATtiny (`trackpoint-i2c-slave-attiny85.ino`)

- `set_mot(bool pending)` — direct PORTB write, MOT LOW while `acc_x||acc_y`,
  HIGH when drained. Called from:
  - `requestEvent` after the serve/subtract (ISR)
  - `loop` after accumulate
  - `loop` stale-clear

### Driver (`zmk-pmw3610-driver/src/trackpoint-i2c.c`)

- MOT semantics flipped from sleep-gate to data-ready (logical 1 = active =
  data pending, `GPIO_ACTIVE_LOW`).
- `trackpoint_i2c_poll` → `trackpoint_i2c_read`, single-shot: skip while idle
  (unless forced), drain at 1ms while MOT stays active, retry at 10ms on NACK.
- New `trackpoint_i2c_heartbeat` (1s): forces an unconditional read — re-syncs a
  dead link, re-applies params on link-restore, covers MOT-low-across-wake.
- `init`: probe + param write **unconditionally** (MOT HIGH at boot = idle, not
  "sleeping").
- Per-read `LOG_INF` flood demoted to `LOG_DBG` (edge events stay INF/WRN).

### Config (`zmk-config-dabaseV_0-2`)

- `dabase_v2_right.overlay`: `irq-gpios = <&gpio0 6 (GPIO_ACTIVE_LOW |
  GPIO_PULL_UP)>` on `trackball@42`; `EXT_POWER` reduced to P0.13 (EXT_VCC) only.
- `build.yaml`: removed the `poweroff` build entry.
- `config/west.yml`: pin driver to `84920415`.

## Wiring

ATtiny85 physical pin 6 (PB1) → NiceNano **P0.06** (freed from the NPN gate).

## Build & flash

- ATtiny: `pio run` → 5956 B flash (72.7%), 235 B RAM (45.9%). Flashed + verified
  via Leonardo ISP (sig `0x1e930b`). Note: `-D` (no-erase) failed verification;
  dropping `-D` (auto-erase) fixed it.
- ZMK: GH Actions run `31701898644` success. Flashed
  `dabase_v2_right-promini-i2c.uf2` (547840 B) via `flash-nicenano.ps1` COM7.

## Result

**Working great** (user-confirmed). MOT-driven reads move the cursor smoothly.

- Targeted `i2c read i2c@40003000 0x42 0x12 2` returns clean `00 00` — ATtiny
  at 0x42, link healthy.
- Transient oddity: `i2c scan` reported ~53 devices once (ghost ACKs while the
  driver was mid-read during the scan). Non-issue — the targeted read is clean;
  user confirmed no problem.

## Known-good

- attiny85-trackpoint `519e6d8` (Exp64)
- zmk-pmw3610-driver `84920415cf04e7850ec0908482d466f2a25c6553`
- zmk-config-dabaseV_0-2 `623cddb`

## Next steps

- Watch for wake-from-deep-sleep recovery (heartbeat should cover it).
- Optional: parity validation in `readByte` + atomic accumulator (deferred from
  the earlier "even better" discussion).
