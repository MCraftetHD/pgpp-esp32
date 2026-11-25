#!/usr/bin/env python3
"""Host-side syntax check for the pgpp_esp32 firmware.

Compiles every firmware source with g++ -fsyntax-only against the stub
headers in this directory. This catches type errors in the multi-session
refactor without needing an ESP32 toolchain. It is NOT a real build: only
syntax and type checking, no linking, no Arduino core, no NimBLE.
"""
import subprocess
import sys
from pathlib import Path

FIRMWARE = Path(__file__).resolve().parent.parent.parent / "firmware" / "pgpp_esp32"
STUBS = Path(__file__).resolve().parent

SOURCES = sorted(FIRMWARE.glob("pp_*.cpp"))
# The sketch compiled as C++ (the .ino has explicit forward declarations, so
# it is a valid translation unit on its own).
SOURCES.append(FIRMWARE / "pgpp_esp32.ino")

INCLUDES = ["-I", str(STUBS), "-I", str(FIRMWARE)]
FLAGS = [
    "-fsyntax-only", "-std=gnu++17", "-Wall",
    "-Wno-unused-variable", "-Wno-unused-function", "-Wno-unused-parameter",
    "-Wno-sign-compare", "-Wno-missing-field-initializers",
    "-DPP_HOST_CHECK=1",
    "-DMYNEWT_VAL_BLE_SVC_GAP_PPCP_MIN_CONN_INTERVAL=6",
    "-DMYNEWT_VAL_BLE_SVC_GAP_PPCP_MAX_CONN_INTERVAL=24",
    "-DMYNEWT_VAL_BLE_SVC_GAP_PPCP_SLAVE_LATENCY=0",
    "-DMYNEWT_VAL_BLE_SVC_GAP_PPCP_SUPERVISION_TMO=500",
    "-DCONFIG_BT_NIMBLE_MAX_CONNECTIONS=4",
]

fails = []
for src in SOURCES:
    cmd = ["g++", *FLAGS, *INCLUDES, str(src)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    status = "OK  " if r.returncode == 0 else "FAIL"
    print(f"{status} {src.name}")
    if r.returncode != 0:
        fails.append(src.name)
        # show the first errors
        lines = r.stderr.splitlines()
        shown = 0
        for line in lines:
            if "error:" in line:
                print("   ", line.strip())
                shown += 1
                if shown >= 12:
                    print("    ... (more errors suppressed)")
                    break

print()
if fails:
    print(f"{len(fails)} file(s) failed: {', '.join(fails)}")
    sys.exit(1)
print(f"All {len(SOURCES)} sources passed the syntax check.")
