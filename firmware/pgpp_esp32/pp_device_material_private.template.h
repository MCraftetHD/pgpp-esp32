#pragma once

#include <stdint.h>

// Copy this file to pp_device_material_private.h and fill every value from
// your own Plus+ device, or use tools/read_flash_data.py which generates it
// (and the console 'k' upload lines) from a flash dump.
#define PP_DEVICE_MATERIAL_CONFIGURED 0

// Enter the public BLE address in little-endian byte order.
// Example: AA:BB:CC:DD:EE:FF -> { 0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa }
#define PP_DEVICE_PUBLIC_ADDR_LE_BYTES \
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }

// selector 2 = the 256-byte cert block at flash offset 0x2006
#define PP_DEVICE_CERT_BLOCK_BYTES \
  { 0 }

// selector 3 = the 16-byte key lane at flash offset 0x2106
#define PP_DEVICE_SELECTOR3_BYTES \
  { 0 }

// selector 4 = the 16 bytes at flash offset 0x306C. The upstream protocol
// model ships a fixed default; leave the placeholder unless you know your
// dump differs (the generated header from tools/read_flash_data.py fills it).
#define PP_DEVICE_AUTH_SELECTOR4_BYTES \
  { 0x85, 0x6a, 0x07, 0x37, 0xb1, 0x30, 0x47, 0x6e, 0x6c, 0x82, 0xf1, 0x00, 0xd3, 0x06, 0x7a, 0xe1 }

// selector 6 = the 10 bytes at flash offset 0x2116 (factory) / 0x307D (SAV)
#define PP_DEVICE_SELECTOR6_BYTES \
  { 0 }
