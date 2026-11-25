#!/usr/bin/env python3
"""Capture a hex dump from the ESP32 'dump_plusplus_flash' sketch into a file.

Connect the ESP32 running tools/dump_plusplus_flash (mode A) over USB, send
'd' (or 'D <start> <end>'), and this script records the BEGIN/END hex block
into a raw binary image you can feed to tools/read_flash_data.py.

Usage:
    pip install pyserial       # once
    python3 tools/capture_dump.py            # uses the default serial port
    python3 tools/capture_dump.py --port COM5 --baud 115200

The output file lands in tools/input/flash.bin (the reader's default input
location).
"""

import argparse
import sys
import time
from pathlib import Path

try:
    import serial  # pyserial
except ImportError:
    print("pyserial is required:  pip install pyserial")
    sys.exit(1)

FLASH_SIZE = 8 * 1024 * 1024
SCRIPT_DIR = Path(__file__).resolve().parent
OUTPUT_FILE = SCRIPT_DIR / "input" / "flash.bin"


def parse_args():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port", default="/dev/ttyUSB0",
                    help="serial port (default /dev/ttyUSB0, e.g. COM5 on Windows)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--out", default=str(OUTPUT_FILE))
    return ap.parse_args()


def find_port():
    """Best-effort auto-detection of an ESP32 serial port."""
    from serial.tools import list_ports
    ports = list(list_ports.comports())
    prefs = ("CP210", "CH340", "CH341", "FT232", "usbserial")
    for p in ports:
        if any(k.lower() in (p.description or "").lower() for k in prefs):
            return p.device
    return ports[0].device if ports else None


def main():
    args = parse_args()
    port = args.port
    if port == "/dev/ttyUSB0":
        auto = find_port()
        if auto and auto != port:
            print(f"No --port given, using detected port {auto}")
            port = auto

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)

    with serial.Serial(port, args.baud, timeout=2) as ser:
        time.sleep(0.5)
        # Wake any console state: help banner proves the sketch is listening.
        ser.reset_input_buffer()
        ser.write(b"\n")
        time.sleep(0.3)
        banner = ser.read(4096).decode(errors="replace")
        if "PGPP flash dump tool" not in banner:
            print("WARNING: sketch banner not seen; pressing ESP32 reset may help.")
            print(banner[:400])

        print("Sending 'd' (dump default region 0x0000-0x10000)...")
        ser.write(b"d\n")

        image = bytearray(FLASH_SIZE)
        captured = 0
        started = None
        last_data = time.time()
        while True:
            raw = ser.readline()
            if not raw:
                if started is not None and time.time() - last_data > 20:
                    print("\nTimeout waiting for data.")
                    break
                continue
            line = raw.decode(errors="replace").strip()
            if line.startswith("BEGIN"):
                _, start_s, end_s = line.split()
                started = int(start_s, 16)
                print(f"Capturing 0x{started:x}..0x{int(end_s, 16):x}")
                captured = started
                continue
            if line.startswith("END"):
                print(f"\n{line} -- capture complete.")
                break
            if line.startswith("MISMATCH"):
                print(f"\n{line} -- re-run 'd' to retry, check wiring.")
                break
            if line.startswith("VERIFY-OK"):
                print(line)
                continue
            if ":" in line and started is not None:
                addr_s, hexdata = line.split(":", 1)
                try:
                    addr = int(addr_s, 16)
                    data = bytes.fromhex(hexdata)
                except ValueError:
                    continue
                if addr != captured:
                    print(f"\nGAP: expected 0x{captured:x}, got 0x{addr:x} (resend 'd')")
                image[addr:addr + len(data)] = data
                captured = addr + len(data)
                print(f"\r{captured:#x} bytes", end="", flush=True)
                last_data = time.time()

    if captured > 0:
        size = captured
        out.write_bytes(image[:size])
        print(f"Wrote {size} bytes (0x{size:x}) to {out}")
        print("Next: python3 tools/read_flash_data.py")
        return 0
    print("Nothing captured.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
