#pragma once

// Multi-phone port: GATT topology + identity composition. The BLE objects
// themselves are device-global (one GATT table shared by all connections);
// only the *values* mirrored into them are per-session, which the read
// callbacks re-sync per slot (see the sketch's read_authorize_callback).

#include "bluefruit.h"

#include "pp_types.h"

// Device-global GATT objects live in the sketch translation unit; these
// builders only need the pointers, so they are declared extern here and
// defined in the sketch.
extern DebugBLEService serviceA;
extern DebugBLEService batteryService;
extern DebugBLEService serviceB;
extern DebugBLEService serviceC;
extern DebugBLEService serviceD;
extern DebugBLEService serviceE;
extern DebugBLEService authService;

extern DebugBLECharacteristic batteryLevelChar;
extern BLECharacteristic serviceAChar1;
extern BLECharacteristic serviceAChar2;
extern BLECharacteristic serviceAChar3;
extern BLECharacteristic serviceAChar4;
extern BLECharacteristic serviceAChar5;
extern BLECharacteristic serviceBChar1;
extern BLECharacteristic serviceBChar2;
extern BLECharacteristic serviceBChar3;
extern DebugBLECharacteristic serviceCChar1;
extern DebugBLECharacteristic serviceCChar2;
extern DebugBLECharacteristic serviceDChar1;
extern DebugBLECharacteristic serviceDChar2;
extern DebugBLECharacteristic serviceDChar3;
extern DebugBLECharacteristic ledWriteChar;
extern DebugBLECharacteristic buttonNotifyChar;
extern DebugBLECharacteristic serviceEChar3;
extern DebugBLECharacteristic serviceEChar4;
extern DebugBLECharacteristic versionChar;
extern DebugBLECharacteristic configChar;
extern DebugBLECharacteristic authControl;
extern DebugBLECharacteristic authBlob;

// Device-global advertising material (defined in the sketch).
extern uint8_t plusplusManufacturerData[16];
static constexpr uint16_t SERVICE_C_NOTIFY_LEN = 74;
static constexpr uint16_t SHORT_VALUE_LEN = 32;
static constexpr uint16_t MEDIUM_VALUE_LEN = 128;
static constexpr uint16_t LONG_VALUE_LEN = 378;
static constexpr uint8_t BATTERY_LEVEL_DEFAULT = 100;
static constexpr char VERSION_STRING[] = "3.900.1.115r1";

static BleSetupCallbacks makeBleSetupCallbacks()
{
  BleSetupCallbacks callbacks = {
    cccd_write_callback,
    read_authorize_callback,
    service_write_callback
  };
  return callbacks;
}

static BleIdentityConfig makeBleIdentityConfig()
{
  BleIdentityConfig config = {
    DEVICE_NAME,
    REAL_PLUSPLUS_ADDR_LE,
    static_cast<uint16_t>(sizeof(REAL_PLUSPLUS_ADDR_LE)),
    BLE_APPEARANCE_UNKNOWN
  };
  return config;
}

static BleGapPreferenceConfig makeBleGapPreferenceConfig()
{
  BleGapPreferenceConfig config = {
    REAL_PLUSPLUS_CONN_INTERVAL_MIN_UNITS,
    REAL_PLUSPLUS_CONN_INTERVAL_MAX_UNITS,
    REAL_PLUSPLUS_CONN_TIMEOUT_UNITS
  };
  return config;
}

static BleAdvertisingConfig makeBleAdvertisingConfig()
{
  BleAdvertisingConfig config = {
    plusplusManufacturerData,
    static_cast<uint16_t>(sizeof(plusplusManufacturerData)),
    PLUSPLUS_SERVICE_DATA_32,
    static_cast<uint16_t>(sizeof(PLUSPLUS_SERVICE_DATA_32)),
    REAL_PLUSPLUS_ADV_INTERVAL_UNITS
  };
  return config;
}

// The topology binds value buffers from slot 0's session as the boot-time
// initial content. Reads are re-synced per requesting session at read time.
static BleTopologyRefs makeBleTopologyRefs()
{
  PPSession& s = gSessions[0];
  static constexpr uint16_t serviceDReadBlobInitialLen = 20;
  BleTopologyRefs refs = {
    &serviceA,
    &batteryService,
    &serviceB,
    &serviceC,
    &serviceD,
    &serviceE,
    &authService,
    &batteryLevelChar,
    &serviceAChar1,
    &serviceAChar2,
    &serviceAChar3,
    &serviceAChar4,
    &serviceAChar5,
    &serviceBChar1,
    &serviceBChar2,
    &serviceBChar3,
    &serviceCChar1,
    &serviceCChar2,
    &serviceDChar1,
    &serviceDChar2,
    &serviceDChar3,
    &ledWriteChar,
    &buttonNotifyChar,
    &serviceEChar3,
    &serviceEChar4,
    &versionChar,
    &configChar,
    &authControl,
    &authBlob,
    s.serviceARead3,
    sizeof(s.serviceARead3),
    s.serviceARead4,
    sizeof(s.serviceARead4),
    s.serviceAStatus,
    sizeof(s.serviceAStatus),
    &BATTERY_LEVEL_DEFAULT,
    sizeof(BATTERY_LEVEL_DEFAULT),
    s.serviceBStatus1,
    sizeof(s.serviceBStatus1),
    s.serviceBStatus2,
    sizeof(s.serviceBStatus2),
    s.serviceBRead3,
    sizeof(s.serviceBRead3),
    s.serviceCState,
    sizeof(s.serviceCState),
    s.serviceDStatus,
    sizeof(s.serviceDStatus),
    s.serviceDReadBlob,
    serviceDReadBlobInitialLen,
    sizeof(s.serviceDReadBlob),
    s.buttonState,
    sizeof(s.buttonState),
    reinterpret_cast<const uint8_t*>(VERSION_STRING),
    static_cast<uint16_t>(sizeof(VERSION_STRING) - 1),
    s.configState,
    sizeof(s.configState),
    PLUSPLUS_AUTH_CTRL_REPLY_25,
    sizeof(PLUSPLUS_AUTH_CTRL_REPLY_25),
    s.authBlobVisibleValue,
    sizeof(s.authBlobVisibleValue),
    LONG_VALUE_LEN,
    SHORT_VALUE_LEN,
    MEDIUM_VALUE_LEN
  };
  return refs;
}
