# Extracting your Plus+ keys — the easy way

The emulator needs five values from **your own** Pokemon GO Plus + ("donor
material") plus its BLE MAC address. Four of them live in the Plus+'s external
flash chip (a small 8-pin Macronix MX25U6433F); the MAC is broadcast over the
air — a phone app reads it.

**The whole flow, top to bottom:**

1. **Flash the emulator firmware to the ESP32 once** (USB-C cable, PC).
2. **Solder 4 wires** from the flash chip on the Plus+ to the ESP32.
3. **Send `d` in the serial console** → ESP32 reads the chip and prints a dump.
4. **Run `tools/read_flash_data.py`** on the dump → it prints your key lines.
5. **Send `k`, paste the key lines, send `save`** → keys stored, reboot, done.

You never swap firmware, never buy a programmer, and never open the emulator
code again. The dump reader is **built into the emulator firmware**.

> **"Why do I need Arduino for anything?"** You don't — there is no Arduino
> board anywhere in this project. The *Arduino IDE* (or `arduino-cli`) is just
> free PC software used to compile and push the code to your ESP32 over its
> USB-C port, the same way you'd use any flashing tool. Your ESP32-DevKitC is
> the only hardware: it flashes over USB-C, it reads the flash chip, and it
> runs the emulator.

## What you need

| Item | Purpose |
|---|---|
| ESP32-DevKitC V4 | the one board that does everything |
| USB-C data cable | flash + serial console |
| Soldering iron + 4–5 thin wires | taps onto the Plus+ flash chip |
| 2 × silicon diodes (1N4148 or similar) + 3 × 4.7k–10k resistors | only for master mode (`d`) — makes a ~1.9 V supply and protects the lines |
| Phone with a BLE scanner app (nRF Connect / LightBlue) | read the BLE MAC |

## The five values and where they live

| Value | Flash offset | Length | How obtained |
|---|---|---|---|
| selector 2 — cert block | `0x2006` | 256 B | flash dump |
| selector 3 — key lane | `0x2106` | 16 B | flash dump |
| selector 6 — factory | `0x2116` | 10 B | flash dump |
| selector 4 — persisted | `0x306C` | 16 B | flash dump |
| selector 5 — persisted | `0x307C` | 1 B | flash dump (persisted sel 6 at `0x307D`, 10 B) |
| public BLE address | — | 6 B | BLE scanner app while the Plus+ advertises |

Sanity markers in the dump: `46 42 43 54` ("FBCT") at `0x2000`, `18 53 41 56`
(".SAV") at `0x3000`. `read_flash_data.py` checks both for you.

## Step 1 — flash the firmware (once)

Follow the **Build & flash** section of the README (ESP32 board support +
NimBLE-Arduino library in the Arduino IDE, open `firmware/pgpp_esp32.ino`,
build with the flags from the README, upload over USB-C). Then open the
Serial Monitor at **115200 baud** and type `?` — you should see the console
help, including the flash-reader commands `j d D x X`.

## Step 2 — solder the taps

Chip pinout (pin 1 is the dot / beveled corner):

```
        ┌────∪────┐
  CS → 1│         │8  VCC (1.8 V)
MISO → 2│         │7  /HOLD
/WP  → 3│         │6  SCK
GND  → 4│         │5  MOSI
        └─────────┘
```

The chip sits near the main SoC on the Plus+ board (take a photo first, mark
pin 1, verify with the pinout above — check continuity from a pin to GND with
a multimeter: pin 4 goes to GND).

**Wires (chip stays soldered on the board):**

| Flash pin | ESP32 pin | Notes |
|---|---|---|
| 1 CS | IO21 | through a 4.7k resistor (master mode) |
| 2 MISO | IO25 | direct |
| 5 MOSI | IO23 | through a 4.7k resistor (master mode) |
| 6 SCK | IO22 | through a 4.7k resistor (master mode) |
| 4 GND | GND | common ground (master mode) |

### Mode A — master dump (`d`): recommended, full deterministic read

The ESP32 talks to the chip directly and reads everything. The catch: the
chip normally gets its 1.8 V from the Plus+ board, so with the Plus+ switched
**OFF** you feed the chip's VCC pin yourself:

```
ESP32 3V3 ──[|>──[|>]──┬──── VCC (pin 8)   two diodes in series = ~1.9 V
                       └── GND             (+ a 100 nF cap if you have one)
```

- Switch the Plus+ **off** (hold its button until it shuts down) and keep it
  off the charger. Its SoC must not talk on the bus while the ESP32 reads.
- Solder the GND wire (common ground is required).
- Send `j` → expect `JEDEC OK: C2 25 37`.
- Send `d` → dumps 0x0000–0x10000 (covers every region you need), then reads
  it twice and reports `VERIFY-OK`.
- Optional: `D 0 800000` = full 8 MiB backup (a few minutes; recommended once,
  it's the only copy of your device's identity).

> If `d` gives mismatches or a wrong JEDEC ID even though wiring looks right,
> the powered-off Plus+ SoC is leaking onto the bus from the back-fed VCC.
> Use mode B for those regions instead — or desolder the chip (end of this
> document).

### Mode B — passive sniffer (`x`/`X`): zero electrical risk

The Plus+ stays fully powered and **the ESP32 drives nothing** — all four
wires are inputs, so you can't damage anything. At every boot the Plus+'s own
SoC reads the flash, and the ESP32 listens on the bus:

- Solder just the four bus taps (CS/MISO/MOSI/SCK) — **no GND, no VCC, no
  resistors needed** (resistors are fine too, they don't hurt).
- Send `x`, then power-cycle the Plus+ (hold button to off, then on).
- The sniffer prints everything it catches for 60 s, then stops by itself
  (`X` stops early and prints the coverage report).
- The coverage report lists which 256-byte pages were captured. You need the
  pages covering `0x2000–0x21A2` (FBCT) and `0x3000–0x3288` (SAV). Missing
  pages? Boot the Plus+ again while the sniffer runs — different boots read
  different regions.

Mode B is best-effort: the Plus+ SoC clocks the flash faster than the ESP32
can reliably sample, so some bytes may be lost. Retry a few boots; if pages
stay missing, fall back to mode A (or desoldering).

## Step 3 — capture the dump to a file

Either copy the serial text by hand into a file, or (easier):

```
pip install pyserial
python3 tools/capture_dump.py --port COM5      # Windows
python3 tools/capture_dump.py                  # auto-detects the port on Linux
```

It writes `tools/input/flash.bin`.

## Step 4 — turn the dump into keys

```
python3 tools/read_flash_data.py
```

Two outputs:
- `tools/device_material_info.txt` — the report, with **ready-to-paste
  console lines** at the bottom (starts with `k`), and
- `tools/pp_device_material.generated.h` — optional, for baking the keys into
  the firmware header instead.

## Step 5 — upload the keys

In the serial console:

```
k
addr=AA:BB:CC:DD:EE:FF      <- your Plus+'s BLE MAC from a scanner app, in the
cert=<512 hex chars>           order the app shows (the reader script prints
sel3=<32 hex chars>            the lines fully filled; only the addr= line
sel4=<32 hex chars>            needs the MAC typed in)
sel6=<20 hex chars>
save
```

The firmware stores everything in NVS and reboots. Verify with `i` (device
material info). Now scan with your phone, pair once, and the emulator acts as
your Plus+.

**The BLE MAC is not in the flash.** Scan with nRF Connect / LightBlue while
the real Plus+ advertises and note the address — that's the one value the
reader cannot extract.

## Step 6 — keep it connected (reconnect behaviour)

- After the first pair, the phone is remembered in NVS (keyed by its BLE
  address, up to 4 phones). On every later connection the emulator answers
  from the stored record — **no re-pairing**, the phone just reconnects.
- The emulator advertises as "paired" the moment any record exists, and it
  restarts advertising immediately whenever a phone disconnects, so the app
  finds it again in the background.
- **Keep the real Plus+ powered off.** It carries the same BLE MAC; if both
  are advertising, the phone connects to whichever it sees first and the
  pairing state will desync.
- Stray scanner connections (other phones' background BLE) are dropped after
  90 s so they can't occupy all four slots and block a real reconnect.
- If things ever get confused: console `w` (or hold BOOT 5 s) wipes the
  records and you re-pair from scratch.
- One honest caveat: the game itself has broken third-party auto-catchers
  with updates before (e.g. v0.415.x in 2026). If the app stops reconnecting
  after an app update, that's a protocol restriction, not a pairing-state
  problem — check the project README/issues first.

## Alternative: desolder the chip (only if you want it)

Fully deterministic, no bus contention at all: hot-air the MX25U6433F off
(320–340 °C, flux, short heat), power it from the same 2-diode dropper, add
/HOLD (pin 7) → VCC and /WP (pin 3) → GND, and use the same `j`/`d` commands.
Put the chip back afterwards (tack the 8 pads). **Back up the full dump
first.** For most people mode A/B above is enough — this is the fallback.

## Optional purchases (never required)

- **CH341A programmer + 1.8 V adapter** (~$10) — replaces the ESP32 as
  reader; no soldering on the bus, plug onto a breadboard chip or use a
  SOIC-8 clip. Must be 1.8 V-capable.
- **SOIC-8 spring-probe clip** — in-circuit reads without solder.
- **BSS138 4-channel level shifter** — makes master mode fully in-spec.

## Troubleshooting

| Symptom | Fix |
|---|---|
| `JEDEC UNEXPECTED` (mode A) | Check pin map (1↔IO21, 2↔IO25, 5↔IO23, 6↔IO22), ~1.9 V on pin 8, GND common, Plus+ OFF. |
| MISMATCH / garbage (mode A) | Plus+ SoC is fighting the bus — make sure it's off; shorten wires (<15 cm); retry; or use mode B. |
| Sniffer captures nothing | Wires on the wrong pins — verify pin 1 dot and the pinout; check continuity pin 4 → GND. |
| Sniffer misses pages | Retry more boots; reduce BLE activity; fall back to mode A. |
| Capture gaps | Serial dropped bytes — re-run `d`; the script reports gaps. |
| Reader: `FBCT magic MISMATCH` | Dump didn't start at 0x0000 — use `d` or `D 0 10000`. |
| Reader: image too small | Capture needs ≥ 0x3289 bytes (`d` gives 0x10000). |
| Firmware won't pair after upload | `i` shows the stored material; check the `addr=` MAC against the scanner app (byte order!). |
