#include <Arduino.h>
#include <Preferences.h>
#include <string.h>

#include "pp_device_material.h"

// Compiled-in material (optional, gitignored).
#if __has_include("pp_device_material_private.h")
#include "pp_device_material_private.h"
#ifndef PP_DEVICE_MATERIAL_CONFIGURED
#define PP_DEVICE_MATERIAL_CONFIGURED 1
#endif
#else
#define PP_DEVICE_MATERIAL_CONFIGURED 0
#endif

static const uint8_t kAuthSelector4ModelDefault[16] = {
  0x85, 0x6a, 0x07, 0x37, 0xb1, 0x30, 0x47, 0x6e,
  0x6c, 0x82, 0xf1, 0x00, 0xd3, 0x06, 0x7a, 0xe1
};

uint8_t REAL_PLUSPLUS_ADDR_LE[6] = { 0 };
uint8_t PLUSPLUS_FACTORY_CERT_BLOCK[256] = { 0 };
uint8_t PLUSPLUS_FACTORY_SELECTOR3[16] = { 0 };
uint8_t PLUSPLUS_FACTORY_SELECTOR4[16] = { 0 };
uint8_t PLUSPLUS_FACTORY_SELECTOR6[10] = { 0 };

static bool gConfigured = false;
static bool gCompiledIn = PP_DEVICE_MATERIAL_CONFIGURED != 0;
static bool gNvsLoaded = false;

static void printHexLine(const char* label, const uint8_t* data, int len)
{
  Serial.print(label);
  Serial.print(" (");
  Serial.print(len);
  Serial.print(" bytes): ");
  for (int i = 0; i < len; ++i) {
    if (data[i] < 0x10) Serial.print('0');
    Serial.print(data[i], HEX);
  }
  Serial.println();
}

bool ppDeviceMaterialLoad()
{
  bool have = false;

#if PP_DEVICE_MATERIAL_CONFIGURED
  memcpy(REAL_PLUSPLUS_ADDR_LE, PP_DEVICE_PUBLIC_ADDR_LE_BYTES, 6);
  memcpy(PLUSPLUS_FACTORY_CERT_BLOCK, PP_DEVICE_CERT_BLOCK_BYTES, 256);
  memcpy(PLUSPLUS_FACTORY_SELECTOR3, PP_DEVICE_SELECTOR3_BYTES, 16);
  memcpy(PLUSPLUS_FACTORY_SELECTOR4, PP_DEVICE_AUTH_SELECTOR4_BYTES, 16);
  memcpy(PLUSPLUS_FACTORY_SELECTOR6, PP_DEVICE_SELECTOR6_BYTES, 10);
  gCompiledIn = true;
  have = true;
  Serial.println("Device material: using compiled-in values");
#else
  gCompiledIn = false;
  Preferences prefs;
  if (prefs.begin("ppmat", true)) {
    size_t nAddr = prefs.getBytesLength("addr");
    size_t nCert = prefs.getBytesLength("cert");
    size_t nSel3 = prefs.getBytesLength("sel3");
    size_t nSel6 = prefs.getBytesLength("sel6");
    if (nAddr == 6 && nCert == 256 && nSel3 == 16 && nSel6 == 10) {
      prefs.getBytes("addr", REAL_PLUSPLUS_ADDR_LE, 6);
      prefs.getBytes("cert", PLUSPLUS_FACTORY_CERT_BLOCK, 256);
      prefs.getBytes("sel3", PLUSPLUS_FACTORY_SELECTOR3, 16);
      prefs.getBytes("sel6", PLUSPLUS_FACTORY_SELECTOR6, 10);
      size_t nSel4 = prefs.getBytesLength("sel4");
      if (nSel4 == 16) {
        prefs.getBytes("sel4", PLUSPLUS_FACTORY_SELECTOR4, 16);
      } else {
        memcpy(PLUSPLUS_FACTORY_SELECTOR4, kAuthSelector4ModelDefault, 16);
      }
      gNvsLoaded = true;
      have = true;
      Serial.println("Device material: loaded from NVS (uploaded via console)");
    }
    prefs.end();
  }
  if (!have) {
    // Zeroed material: firmware runs, but refuses to advertise/pair.
    memcpy(PLUSPLUS_FACTORY_SELECTOR4, kAuthSelector4ModelDefault, 16);
    Serial.println("Device material: NOT configured (use console 'k' to upload)");
  }
#endif

  gConfigured = have;
  if (have) {
    printHexLine("  addr(LE)", REAL_PLUSPLUS_ADDR_LE, 6);
  }
  return have;
}

bool ppDeviceMaterialConfigured() { return gConfigured; }
bool ppDeviceMaterialCompiledIn() { return gCompiledIn; }
bool ppDeviceMaterialNvsLoaded() { return gNvsLoaded; }
const uint8_t* ppDeviceMaterialAddrLe() { return REAL_PLUSPLUS_ADDR_LE; }
