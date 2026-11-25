# Extracting your Plus+ keys (donor device material)

The emulator needs five values from **your own** Pokemon GO Plus +. Four of
them live in the device's external SPI flash chip; the fifth (the BLE MAC
address) is broadcast over the air and is NOT in the flash image — you read it
with a phone app.

This guide is written for the no-purchases, soldering-first path: wire bridges
from your ESP32-DevKitC to the Plus+ board, no programmer hardware. Optional
purchases (CH341A programmer, spring-probe clip) are mentioned at the end for
people who prefer them.

## What you need

| Item | Purpose |
|---|---|
| Pokemon GO Plus + | the donor device (keep it intact — you can keep using it) |
| ESP32-DevKitC V4 | the "programmer"; reuse the same board you will run the firmware on |
| 4 wires + soldering iron | taps onto the flash chip pins |
| 2 × 1N4148/1N400x diodes (or any 2 silicon diodes) | dropper: 3.3 V → ~1.9 V supply for the flash chip (method A only) |
| 3 × 4.7k–10k resistors | series resistors on CS/SCK/MOSI (method A only) |
| Fine-tip iron, flux, solder wick | desoldering the flash chip (method A only) |
| Phone with a BLE scanner app (nRF Connect / LightBlue) | read the BLE MAC |

> **Legal / account note:** the extracted material identifies *your* Plus+ to
> Niantic's app. Use it only on devices you own. Also make a backup of the
> full dump — it is the only copy of your device's identity material.

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
(".SAV") at `0x3000`. The reader script checks both.

## Chip: Macronix MX25U6433F

8-pin SPI NOR flash, **1.8 V** (that voltage matters — see method A). Pinout
of the standard SOP-8/WSON-8 package, pin 1 marked by the dot/triangle:

```
        ┌────∪────┐
  CS → 1│         │8  VCC (1.8 V)
MISO → 2│         │7  /HOLD (tie to VCC)
/WP  → 3│         │6  SCK
GND  → 4│         │5  MOSI
        └─────────┘
```

Pin 7 (/HOLD) is already tied high on the Plus+ board; on a desoldered chip
connect it to VCC.

## Method A (recommended): desolder + wire to ESP32

Deterministic: you read the whole image yourself, verify it twice, done.

### 1. Desolder the chip

The MX25U6433F sits near the main SoC on the Plus+ board. Take a photo of the
board first, mark pin 1, then remove the chip:

- **With hot air:** 320–340 °C, low airflow, flux around the pins, lift the
  chip with tweezers once the solder flows. Keep the time under heat short.
- **With an iron only (SOP-8 style, slower but doable):** flood all pins with
  extra solder, wick across both rows alternately with a clean, well-tinned,
  heavily-fluxed wick until the chip slides free. Do not pry; reheat instead.

Mark pin 1 on the desoldered chip (the dot). If the silkscreen dot is under
solder, pin 1 faces the beveled corner.

> **Alternative:** you can often leave the chip on the board and lift only
> pins 1, 5 and 6 (CS, MOSI, SCK) off their pads by heating each pin and
> nudging the wire under it — the chip stays powered by the board's own 1.8 V
> rail (then skip the diode dropper below and use pin 8 as-is). This avoids
> hot-air work but requires steadier hands. The ESP32 then only drives
> through the series resistors exactly as described.

### 2. Wire it to the ESP32

Power the chip at ~1.9 V made from the ESP32's 3V3 rail and **two silicon
diodes in series** (each drops ~0.65–0.75 V; measure with a multimeter and add
a third diode if you read above 1.95 V). MX25U6433F operates from 1.65–1.95 V.

```
ESP32 3V3 ──[|>──[|>]──┬────────── VCC (pin 8)      two diodes in series
                       │
                      ─┴─ 100 nF capacitor across VCC/GND (if you have one)

ESP32 IO21 ──[4.7k]────────── CS   (pin 1)
ESP32 IO22 ──[4.7k]────────── SCK  (pin 6)
ESP32 IO23 ──[4.7k]────────── MOSI (pin 5)
ESP32 IO25 ────────────────── MISO (pin 2)      direct, no resistor
ESP32 GND  ────────────────── GND  (pin 4)      (+ /WP pin 3 to GND)
```

The series resistors limit the current where a 3.3 V output meets the 1.8 V
chip's inputs. That operating point is technically out of the chip's spec —
bounded to ~0.2 mA, and this is the standard hobbyist compromise; if you want
a strictly-in-spec setup, add a 4-channel BSS138 level shifter module (~$2,
optional purchase) between the ESP32 and the chip instead of the resistors.

MISO needs no shifting: the ESP32's input threshold (~1.4 V typical) reads the
1.8 V logic fine on nearly every unit.

### 3. Flash the dump sketch and read

1. Open `tools/dump_plusplus_flash/dump_plusplus_flash.ino` in the Arduino IDE
   (same ESP32 board setup as the firmware: esp32dev, 115200 baud).
2. Flash it, open the Serial Monitor, and send `j`.
   Expected reply: `JEDEC OK: C2 25 37`. If you get `UNEXPECTED`, re-check
   wiring/power first — a wrong JEDEC ID with good wiring means the wrong chip.
3. Send `d`. The sketch dumps the first 64 KiB (covers every region you need)
   as hex text between `BEGIN`/`END` markers, then reads it twice and reports
   `VERIFY-OK`.
4. Capture to a file, either by hand (copy the serial text into
   `flash.txt` → trim everything outside `BEGIN`/`END`) or automatically:

   ```
   pip install pyserial
   python3 tools/capture_dump.py --port /dev/ttyUSB0
   ```

   The capture script writes `tools/input/flash.bin`.
5. Optional, for a full backup: send `D 0 800000` (8 MiB, a few minutes).

### 4. Extract the values

```
python3 tools/read_flash_data.py
```

Outputs:
- `tools/device_material_info.txt` — human-readable report **with
  ready-to-paste console `k` upload lines** (recommended path), and
- `tools/pp_device_material.generated.h` — a header you can drop into
  `firmware/pgpp_esp32/` as `pp_device_material_private.h`.

**The BLE MAC is not in the dump.** Scan for the Plus+ (it advertises as
`Pokemon GO Plus +` … actually as a generic device until connected; use nRF
Connect, find the MAC, note it). Fill the address into either
- the `addr=` line before `save` in the console upload, or
- `PP_DEVICE_PUBLIC_ADDR_LE_BYTES` in the generated header (AA:BB:CC:DD:EE:FF
  becomes `0xff,0xee,0xdd,0xcc,0xbb,0xaa`).

### 5. Resolder or not?

You can put the chip back on the Plus+ (reball/tack the 8 pads, heat until it
settles). **Back up the full 8 MiB dump first** — and note that once the
emulator is paired as this device, the original Plus+ and the ESP32 will fight
over the same identity; use one at a time.

## Method B (no desoldering): passive boot sniffer

Experimental. The Plus+ main SoC reads the flash at every boot; the sketch's
`m` → `s` mode listens on the bus while it happens.

1. **Keep the chip on the board.** Solder four taps (thin enameled wire or
   solder-through jumper) onto the flash pins 1 (CS), 2 (MISO), 5 (MOSI),
   6 (SCK) — pin 8 stays powered by the board itself. Connect to ESP32
   IO21/IO25/IO23/IO22. No power connection, no resistors; all four ESP32
   pins are inputs, so nothing on the Plus+ is disturbed.
2. Flash the same dump sketch, open the Serial Monitor, send `m` (switch to
   mode 1) then `s`.
3. Power-cycle the Plus+ (hold its button, or re-seat the battery). The
   sniffer prints every byte it catches.
4. Send `x` for the coverage report. You need every 256-byte page covering
   `0x2000–0x21A2` (FBCT) and `0x3000–0x3288` (SAV). Missing pages? Boot again
   — different boots read different regions — or accept that the onboard SoC
   may clock the flash faster than the ESP32 can reliably sample (that is why
   this method is best-effort).

Method B cannot give you regions the SoC never reads. If the coverage map
stays incomplete after several boots, fall back to method A.

## Optional purchases (only if you want them)

- **CH341A USB programmer + 1.8 V adapter** (~$10): plugs into a breadboard
  chip or a SOIC-8 spring-probe clip; no soldering at all. Make sure it is a
  1.8 V-capable setup — a bare 3.3 V CH341A on a 1.8 V chip is out of spec.
- **SOIC-8 spring-probe clip**: in-circuit reads without desoldering.
- **4-channel BSS138 level shifter**: makes method A fully in-spec.

## Troubleshooting

| Symptom | Fix |
|---|---|
| `JEDEC UNEXPECTED` | Check CS/SCK/MOSI/MISO mapping, 1.9 V on VCC, /WP to GND, /HOLD to VCC. |
| Verify mismatch / garbage bytes | Shorten wires (<15 cm), add 100 nF across VCC/GND, lower the dump speed (extend the `spiShiftByte` delays). |
| Capture gaps | Serial dropped bytes — re-run `d`; the capture script reports any gap. |
| Reader: `FBCT magic MISMATCH` | Dump start/end wrong — the regions must be at the absolute offsets listed above. |
| Reader: image too small | You captured less than 0x3289 bytes; send `D 0 4000` at minimum, or `d` for the 64 KiB default. |
