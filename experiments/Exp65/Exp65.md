# Exp65: ESB split transport (badjeff/zmk-feature-split-esb)

## Context

Exp64 works over BLE split transport: Dongle (XIAO nRF52840, central, USB HID)
←BLE→ Left (NiceNano, peripheral) and Right (NiceNano, peripheral with the
trackpoint). The trackpoint-lag problem (Exp11/16) is fundamentally limited by
BLE split latency. The wishlist still has "works in BLE", "works over dongle
topology" open.

`badjeff/zmk-feature-split-esb` replaces the ZMK split transport with Nordic
**Enhanced ShockBurst (ESB)** — a raw 2.4 GHz radio protocol with much lower
packet overhead than BLE. The relevant topology for us is:

> **USB-only Dongle with ONLY ESB** — Dongle = ESB PRX (receiver) + USB HID;
> peripherals = ESB PTX. Min latency 1 ms.

## Hypothesis

Swapping the dongle↔peripherals link from BLE split to ESB lowers split latency
and, critically, removes the BLE stack from the trackpoint data path. The
trackpoint / Pro-Mini / `zmk-pmw3610-driver` code is **untouched** — only the
split link changes. Cursor movement should be smoother/faster than Exp64 BLE.

## Plan

1. Add `zmk-feature-split-esb` + its NCS deps (`sdk-nrf` badjeff fork,
   `nrfxlib`) to `config/west.yml`. Keep ZMK pinned at `fa33e35` (all ESB
   headers verified present at that pin).
2. Enable `CONFIG_ZMK_SPLIT_ESB=y` on all three shields; disable
   `CONFIG_ZMK_BLE`/`ZMK_SPLIT_BLE`/`ZMK_SPLIT_WIRED`.
   - Dongle = central, `ZMK_SPLIT_ESB_PERIPHERAL_COUNT=2`.
   - Left = peripheral ID 1, Right = peripheral ID 2.
3. Add the `esb_split` address node to all three overlays.
4. Shared ESB stack/buffer tunables in `dabase_v2.conf`.
5. Drop `dabase_v2_right-standalone-usb` build (right shield is ESB-only now);
   drop `CONFIG_ZMK_SLEEP` cmake-args for first pass (ESB idle-disable needs
   timeslot/BLE — revisit after motion is proven).

## Implementation

- `config/west.yml`: +`nrfconnect` remote; +`sdk-nrf` (`v3.1-branch+zmk-fixes`,
  path `nrf`), +`nrfxlib` (`v3.1-branch`, repo-path `sdk-nrfxlib`), +
  `zmk-feature-split-esb` (`main`).
- `boards/shields/dabase_v2/Kconfig.defconfig`: dongle sets
  `ZMK_SPLIT_ROLE_CENTRAL default y` (was legacy `ZMK_SPLIT_BLE_ROLE_CENTRAL`,
  which no longer selects the role under `ZMK_SPLIT_BLE=n`).
- `dabase_v2_dongle.conf`: ESB central, `PERIPHERAL_COUNT=2`,
  `AUTO_HEAL_KEY_POS_MAX=48`, `MPSL_TIMESLOT_SESSION_COUNT=1`; removed
  `CONFIG_BT_SHELL` / `CONFIG_ZMK_BLE_EXPERIMENTAL_CONN` (BLE-only).
- `dabase_v2_left.conf`: ESB peripheral ID 1.
- `dabase_v2_right.conf`: ESB peripheral ID 2, `RETRY_INPUT_EVENT=0`
  (lossy-but-sharp cursor).
- `dabase_v2.conf`: removed `CONFIG_BT_BAS` (BLE-only); added
  `SYSTEM_WORKQUEUE_STACK_SIZE=4096`, `INPUT_THREAD_STACK_SIZE=4096`,
  `INPUT_QUEUE_MAX_MSGS=256`, `ESB_MAX_PAYLOAD_LENGTH=48`,
  `ESB_TX_FIFO_SIZE=1`, `MAIN_STACK_SIZE=4096`, `IDLE_STACK_SIZE=512`.
- `build.yaml`: removed standalone-usb entry; left/right cmake-args now only
  `-DCONFIG_ZMK_SPLIT_ROLE_CENTRAL=n`.
- Overlays: `esb_split` node (identical arbitrary addresses) in dongle/left/right.

## Wiring

No wiring change (ESB uses the on-board radio). Trackpoint stays on the right
half I2C (ATtiny85 @ 0x42, MOT P0.06).

## Build & flash

- GH Actions run `31790839334` — **success** (all 6 builds). First build slow (~4 min) due to
  NCS `sdk-nrf`/`nrfxlib` checkout.
- Artifacts:
  - `dabase_v2_right-promini-i2c.uf2` (281,088 B) — ESB peripheral ID 2
  - `dabase_v2_left.uf2` (252,416 B) — ESB peripheral ID 1
  - `dabase_v2_dongle-usb-log.uf2` (348,672 B) — ESB central
  - `dabase_v2_dongle_with_studio.uf2` (427,008 B)
  - `settings_reset` ×2

Config verified from build logs:
- Dongle: `ZMK_SPLIT_ROLE_CENTRAL=y`, `ZMK_SPLIT_ESB_PERIPHERAL_COUNT=2`,
  `AUTO_HEAL_KEY_POS_MAX=48`; ESB module CMake patched `central.h`
  ("ESB peripheral count is > 0").
- Left: `ZMK_SPLIT_ESB_PERIPHERAL_ID=1`, `PERIPHERAL_COUNT=0` (peripheral).
- Right: `ZMK_SPLIT_ESB_PERIPHERAL_ID=2`, `RETRY_INPUT_EVENT=0`,
  `PERIPHERAL_COUNT=0` (peripheral).

## Result

**FAILED — declared by user (2026-08-14).**

What worked:
- ESB split compiled at pinned ZMK `fa33e35`, all 6 builds flashed.
- Keystrokes from both halves reached the dongle and produced HID output
  (typing confirmed working over ESB).
- Keystroke logging works via `log enable dbg zmk` at the shell (runtime only;
  `CONFIG_LOG_OVERRIDE_LEVEL=4` + `CONFIG_LOG_CMDS=y` in the dongle conf did NOT
  stick at boot — `log status` showed runtime filter `none` until the shell
  command raised it).

What failed:
- The original "chatter / repeating characters" symptom was never captured. Logs
  show clean keystroke streams, but when the chatter occurs the log output
  freezes/stops updating — no scroll-flood evidence reached the capture.
- One session showed a suspected scroll-layer (layer 6) flood
  (`Got peripheral event` → `Remapped` → `scale_val` → `Mouse scroll set`), but
  that is working behavior per the user and was not the reported issue.

Verdict: ESB is not official and is full of bugs. The user does not see using it
for now. **Exp65 is a dead end** — do not build on the `zmk-feature-split-esb`
transport.

## Known-good

- zmk-config-dabaseV_0-2 `44b2f4d` (Exp65, pre-debug-log)
- zmk-config-dabaseV_0-2 `b802e2c` (last Exp65 commit — debug logging for capture)

## Next steps

- Return to the BLE split dongle topology (Exp64) as the working baseline.
- If the chatter is ever pursued again, capture it on the BLE build (not ESB).
