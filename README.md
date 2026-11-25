# pgpp-esp32

Emulation of the **Pokemon GO Plus +** accessory for the **ESP32**, built on
the kiyoukan/Jackv protocol work (AGPLv3). Connects to Pokemon GO as a real
Plus+ (pairing handshake, service-D auth, reconnect state) and supports
auto-catch, auto-spin, manual button presses — with **multiple phones/accounts
at once** on one ESP32.

> **Status: experimental.** The multi-phone path (up to 4 simultaneous
> connections) is new engineering on top of the upstream single-session
> protocol code and is not yet field-tested against current app versions.
> Niantic updates have repeatedly broken third-party auto-catch devices in
> the past (e.g. app 0.415.x in 2026); expect protocol maintenance.

## Features

- Full PGPP pairing + reconnect protocol (auth challenge/response, Service-D
  typed writes, reconnect compact lane, CCCD seeding, sleep-export data)
- Auto-catch and auto-spin with randomized press delays (normal / fast modes)
- Manual "press" via the DevKitC BOOT button (catch + spin like a real button)
- Up to **4 concurrent phones** — each connection gets its own pairing state,
  record binding, catch/spin settings and auth FSM (experimental)
- Pairing persistence in NVS keyed by phone address; exactly-one-record
  legacy binding; last-session reconnect on boot
- LED-only indication (blink patterns, see below) — no display needed
- Serial console for status, settings, key upload and debugging
- Device material (your device's cert/keys) from compile-time header **or**
  serial upload to NVS — no recompile needed
- Idle deep-sleep after 5 minutes with no phone (wake on BOOT button)

## Hardware

Target: **ESP32-DevKitC V4** (classic ESP32, WROOM module). Everything else
is onboard — no screen, no external parts required.

| Onboard element | Pin | Behaviour |
|---|---|---|
| BOOT button (GPIO0) | 0 | click = manual catch/spin press · hold 5 s = wipe all pairing records |
| D2 button (GPIO2) | 2 | click = cycle catch mode (off → all → new → spin-only) · hold 3 s = toggle auto spin |
| Onboard blue LED | 2 | indication patterns below |
| EN | — | hardware reset |

> GPIO2 is both the LED and the D2 button on this board; the LED patterns are
> what you watch, button presses are what you click. If your variant lacks the
> D2 button, jumper GPIO2 to GND briefly (the pin is active-high input with a
> pulldown) — any spare tactile button to GND works too.

### LED codes (onboard blue LED)

| Pattern | Meaning |
|---|---|
| 1 blink | Pokémon nearby (attention) |
| 2 blinks | new Pokémon |
| 3 blinks | PokéStop nearby |
| 6 fast blinks | caught |
| 1 long blink | fled |
| 3 short blinks | items received (spin) |
| 2 long blinks | warning (bag/box full, no balls) |
| (per-pair state) | see serial console for the exact notes |

## Build & flash

Requirements: Arduino IDE 2.x or arduino-cli, **esp32 Arduino core ≥ 3.0**
(`esp32dev` board), **NimBLE-Arduino 2.5.2** library.

1. Install the ESP32 Arduino core
   (Boards Manager URL `https://espressif.github.io/arduino-esp32/package_esp32_index.json`).
2. Install the `NimBLE-Arduino` library, version **2.5.2** (Library Manager).
3. Open `firmware/pgpp_esp32/pgpp_esp32.ino` (the sketch folder must be named
   `pgpp_esp32`).
4. Board: `ESP32 Dev Module` (esp32dev), 115200 baud, default partition
   scheme.
5. **Required build flags** (Preferences → Additional compiler flags, or
   `--build-property` with arduino-cli):

   ```
   -DMYNEWT_VAL_BLE_MAX_CONNECTIONS=4
   -DPP_MAX_CONNECTIONS=4
   -DMYNEWT_VAL_BLE_SVC_GAP_PPCP_MIN_CONN_INTERVAL=6
   -DMYNEWT_VAL_BLE_SVC_GAP_PPCP_MAX_CONN_INTERVAL=24
   -DMYNEWT_VAL_BLE_SVC_GAP_PPCP_SLAVE_LATENCY=0
   -DMYNEWT_VAL_BLE_SVC_GAP_PPCP_SUPERVISION_TMO=500
   ```

   `MYNEWT_VAL_BLE_MAX_CONNECTIONS=4` is essential — the NimBLE default (3)
   would silently cap you at 3 phones.

arduino-cli equivalent:

```sh
arduino-cli lib install "NimBLE-Arduino@2.5.2"
arduino-cli compile --fqbn esp32:esp32:esp32dev \
  --build-property "compiler.cpp.extra_flags=-DMYNEWT_VAL_BLE_MAX_CONNECTIONS=4 -DPP_MAX_CONNECTIONS=4 -DMYNEWT_VAL_BLE_SVC_GAP_PPCP_MIN_CONN_INTERVAL=6 -DMYNEWT_VAL_BLE_SVC_GAP_PPCP_MAX_CONN_INTERVAL=24 -DMYNEWT_VAL_BLE_SVC_GAP_PPCP_SLAVE_LATENCY=0 -DMYNEWT_VAL_BLE_SVC_GAP_PPCP_SUPERVISION_TMO=500" \
  firmware/pgpp_esp32
arduino-cli upload --fqbn esp32:esp32:esp32dev -p /dev/ttyUSB0 firmware/pgpp_esp32
```

A host-side **syntax check** (no ESP32 toolchain needed) covers all 35
firmware sources against stub headers:

```sh
python3 tools/host_check/run.py
```

## Device keys (one-time setup)

The emulator must impersonate *your* physical Plus+: it needs your device's
cert/key material from the MX25U6433F external flash and your device's BLE
MAC. **`docs/keys_extraction.md`** walks through the whole process with a
solder-only method (wire bridges from the ESP32, no programmer needed) and
optional CH341A instructions.

Two ways to feed the keys in:

- **Serial upload (no recompile):** open the Serial Monitor, send `k`, paste
  the `addr=/cert=/sel3=/sel4=/sel6=` lines printed by
  `tools/read_flash_data.py`, send `save`. Stored in NVS, survives reflashing.
- **Compile-time header:** copy
  `firmware/pgpp_esp32/pp_device_material_private.template.h` to
  `pp_device_material_private.h` (or use the generated
  `pp_device_material.generated.h`), fill the values, set
  `PP_DEVICE_MATERIAL_CONFIGURED` to 1, recompile.

Without keys the firmware runs but refuses to advertise or pair.

## Serial console (115200 baud)

| Key | Action |
|---|---|
| `?` / `h` | help |
| `s` | status (sessions, counters, settings) |
| `a` | cycle catch mode (off / all / new / spin-only) |
| `p` | toggle auto spin |
| `f` | toggle press speed (normal / fast) |
| `A` | toggle advertising enabled |
| `w` | wipe all pairing records (re-pair phones) |
| `k` | device-material upload mode |
| `i` | device material info |
| `m` | per-session info |
| `L` | LED blink test |
| `S` | save settings now |
| `r` | restart |

## Multi-phone notes (experimental)

- One pairing record per phone address, up to 4 records in NVS
  (`pprecords` namespace). A phone keeps its own reconnect state; any
  single phone reconnects without re-pairing.
- The advertising payload's "paired" byte becomes `0x02` only while **at
  least one** phone is paired, matching real-device behaviour the app
  checks during reconnect.
- Shared characteristics (Service C/D, auth) re-sync their values to the
  currently-served connection; notify fan-out is per-slot with per-slot
  CCCD bitmaps.
- If two phones pair simultaneously the second one takes the next free
  slot; when all 4 slots are taken, new connections are refused.
- To start over: hold BOOT 5 s or console `w` (wipes all records), then
  pair again.

## Repo layout

```
firmware/pgpp_esp32/   the sketch + protocol modules (compile this)
tools/read_flash_data.py     flash dump -> keys report + console lines
tools/capture_dump.py        serial capture of the dump sketch output
tools/dump_plusplus_flash/   ESP32 dump/sniffer sketch for key extraction
tools/host_check/            host-side syntax check stubs
docs/keys_extraction.md      donor key extraction guide (solder-first)
_reference/                  upstream sources (not part of the build)
```

## Credits & license

Protocol reverse engineering and original nRF52 firmware: **kiyoukan**
(nRF52-go-plus-plus-backup-). ESP32-S3/NimBLE port and board layer: **Jackv**
(ESP32-PoGoPP-Emu). This repository is a multi-phone, classic-ESP32 (DevKitC)
adaptation. Both upstream projects are AGPLv3; this repo carries the AGPLv3
license (see `LICENSE`).

You will need your own device's material — none is included.
