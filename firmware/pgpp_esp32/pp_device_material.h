#pragma once

#include <stdint.h>

// Device material = the identity + keys of YOUR OWN Pokemon GO Plus+.
//
// Two ways to provide it:
//   1. Compiled in: copy pp_device_material_private.template.h to
//      pp_device_material_private.h (gitignored) and fill it in - or let
//      tools/read_flash_data.py generate it from your flash dump.
//   2. Uploaded at runtime over the serial console with the 'k' command
//      (stored in NVS, survives reboots, no recompile needed).
//
// If neither is present the firmware boots but stays unpaired-able: it will
// not advertise, and the console keeps telling you to upload material.

// Runtime storage (loaded at boot; the protocol modules read these arrays).
extern uint8_t REAL_PLUSPLUS_ADDR_LE[6];          // public BLE address, little-endian
extern uint8_t PLUSPLUS_FACTORY_CERT_BLOCK[256];  // selector 2 (flash 0x2006)
extern uint8_t PLUSPLUS_FACTORY_SELECTOR3[16];    // selector 3 key lane (flash 0x2106)
extern uint8_t PLUSPLUS_FACTORY_SELECTOR4[16];    // auth selector 4 (flash 0x306C or model default)
extern uint8_t PLUSPLUS_FACTORY_SELECTOR6[10];    // selector 6 (flash 0x2116 / 0x307D)

bool ppDeviceMaterialLoad();        // call once from setup() before BLE init
bool ppDeviceMaterialConfigured();
bool ppDeviceMaterialCompiledIn();
bool ppDeviceMaterialNvsLoaded();
const uint8_t* ppDeviceMaterialAddrLe();
