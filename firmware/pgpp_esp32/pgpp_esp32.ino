// pgpp_esp32 - Pokemon GO Plus+ (Plus) emulator for the classic ESP32.
//
// Multi-phone port of Jackv-makes/ESP32-PoGoPP-Emu, which is itself a port of
// kiyoukan/nRF52-go-plus-plus-backup- (the Plus+ pairing/reconnect protocol)
// with the catch/spin engine from pgpemu (yohanes/spezifisch/paper183/
// shortcuts). AGPL v3, same as the upstream firmware. See README.md.
//
// Every BLE connection gets its own session (pp_session.h) with a full copy
// of the volatile protocol state, so several phones can pair and play
// against one ESP32. Persisted pairing records (pp_record_store.h) are keyed
// per phone. Board support (buttons/LED/serial console) is in pp_board.cpp.

#include <NimBLEDevice.h>
#undef BLEService
#undef BLECharacteristic
#undef BLEServer
#undef BLEAdvertising
#undef BLEServerCallbacks
#undef BLECharacteristicCallbacks
#undef BLEAdvertisementData
#undef BLEDescriptor
#undef BLEConnInfo
#undef BLEUUID
#undef BLEAddress
#undef BLEDevice

#include <Arduino.h>
#include <esp_random.h>
#include <string.h>

#include "pp_types.h"
#include "pp_protocol_data.h"
#include "pp_device_material.h"

#include "pp_debug_ble.h"
#include "pp_debug_utils.h"
#include "pp_log.h"

#include "pp_ble_topology.h"
#include "pp_ble_dispatch.h"
#include "pp_ble_services.h"

#include "pp_app_lifecycle.h"
#include "pp_auth_blob.h"
#include "pp_auth_bootstrap.h"
#include "pp_auth_material.h"
#include "pp_auth_protocol.h"
#include "pp_button_runtime.h"
#include "pp_cccd_seed.h"
#include "pp_connection_coordinator.h"
#include "pp_first_pair_flow.h"
#include "pp_pair_first_time.h"
#include "pp_pair_helpers.h"
#include "pp_pair_reconnect.h"
#include "pp_pair_runtime_support.h"
#include "pp_pair_state.h"
#include "pp_reconnect_flow.h"
#include "pp_service_d_local.h"
#include "pp_service_d_protocol.h"
#include "pp_sleep_export.h"
#include "pp_sleep_policy.h"

#include "pp_catch_engine.h"
#include "pp_board.h"
#include "pp_session.h"
#include "pp_record_store.h"

// ---------------- Device-global state ----------------

static const char DEVICE_NAME[] = "Pokemon GO Plus +";
static const char BUILD_TAG[] = "pgpp-esp32-multi-v1";

bool gServicesReady = false;
bool gPersistentStoreReady = false;
// Device-level mirror: non-invalid while any session holds a connection.
// The idle-sleep lifecycle binds this so it only fires with zero phones.
uint16_t gAnyConnHandleMirror = BLE_CONN_HANDLE_INVALID;
bool gDisconnectedSleepArmed = false;
uint32_t gDisconnectedSleepAt = 0;
bool gPhysicalResetButtonActive = false;
bool gPhysicalResetButtonTriggered = false;
uint32_t gPhysicalResetButtonPressedAt = 0;

// Pairing-eligible advertising state (device-global; byte 15 is rewritten by
// ppSetManufacturerState with the aggregate across sessions).
uint8_t plusplusManufacturerData[16] = {
  0x53, 0x05,
  0x01, 0xae, 0xde, 0x00, 0xf0, 0xbe, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// ---------------- GATT topology (device-global, shared by all sessions) ----

DebugBLEService serviceA("addc3e26-4aa5-4c1a-8a6a-735db4e01c6c");
DebugBLEService batteryService(0x180F);
DebugBLEService serviceB("2bbe7f7c-7304-4466-8407-8eaf89f8ce45");
DebugBLEService serviceC("6b7f9c98-8588-4624-8a94-38c9bce32685");
DebugBLEService serviceD("bbe87709-5b89-4433-ab7f-8b8eef0d8e37");
DebugBLEService serviceE("21c50462-67cb-63a3-5c4c-82b5b9939aeb");
DebugBLEService authService("138c35b6-23a0-459c-9869-acb9228be64f");

DebugBLECharacteristic batteryLevelChar(0x2A19, "batteryLevelChar");

BLECharacteristic serviceAChar1("addc3e26-4aa5-4c1a-8a6a-735db4e01c6d");
BLECharacteristic serviceAChar2("addc3e26-4aa5-4c1a-8a6a-735db4e01c6e");
BLECharacteristic serviceAChar3("addc3e26-4aa5-4c1a-8a6a-735db4e01c6f");
BLECharacteristic serviceAChar4("addc3e26-4aa5-4c1a-8a6a-735db4e01c70");
BLECharacteristic serviceAChar5("addc3e26-4aa5-4c1a-8a6a-735db4e01c71");

BLECharacteristic serviceBChar1("2bbe7f7c-7304-4466-8407-8eaf89f8ce46");
BLECharacteristic serviceBChar2("2bbe7f7c-7304-4466-8407-8eaf89f8ce47");
BLECharacteristic serviceBChar3("2bbe7f7c-7304-4466-8407-8eaf89f8ce49");

DebugBLECharacteristic serviceCChar1("68282de6-e643-4caa-a6b4-e96f46ed988a", "serviceCChar1");
DebugBLECharacteristic serviceCChar2("290278ac-5eb9-47df-899d-e904ab62b4b3", "serviceCChar2");

DebugBLECharacteristic serviceDChar1("bbe87709-5b89-4433-ab7f-8b8eef0d8e38", "serviceDChar1");
DebugBLECharacteristic serviceDChar2("bbe87709-5b89-4433-ab7f-8b8eef0d8e39", "serviceDChar2");
DebugBLECharacteristic serviceDChar3("bbe87709-5b89-4433-ab7f-8b8eef0d8e3a", "serviceDChar3");

DebugBLECharacteristic ledWriteChar("21c50462-67cb-63a3-5c4c-82b5b9939aec", "ledWriteChar");
DebugBLECharacteristic buttonNotifyChar("21c50462-67cb-63a3-5c4c-82b5b9939aed", "buttonNotifyChar");
DebugBLECharacteristic serviceEChar3("21c50462-67cb-63a3-5c4c-82b5b9939aee", "serviceEChar3");
DebugBLECharacteristic serviceEChar4("21c50462-67cb-63a3-5c4c-82b5b9939aef", "serviceEChar4");
DebugBLECharacteristic versionChar("21c50462-67cb-63a3-5c4c-82b5b9939af0", "versionChar");
DebugBLECharacteristic configChar("1e9d6f80-9b71-4bbf-9ba3-1d4d97212b49", "configChar");

DebugBLECharacteristic authControl("7e11e71f-97f6-4578-aa23-2f0b2f8a946e", "authControl");
DebugBLECharacteristic authBlob("125266b9-bcfd-4904-8be0-530ef0b93372", "authBlob");

#include "pp_wiring.h"

// ---------------- Board / catch engine wiring ----------------

static void resetPairingFromPhysicalButtonAll(const char* reason);

static BoardRefs makeBoardRefs()
{
  BoardRefs refs = {
    &gSessions[0].connHandle,
    &gSessions[0].pairingStage,
    &gSessions[0].persistentReconnectReady,
    BUILD_TAG,
    REAL_PLUSPLUS_ADDR_LE,
    manual_button_press,
    start_advertising,
    stop_advertising,
    resetPairingFromPhysicalButtonAll,
    pairingStageLabel
  };
  return refs;
}

static CatchEngineRefs makeCatchEngineRefs(PPSession& s)
{
  CatchEngineRefs refs = {
    &s.pairingStage,
    &s.connHandle,
    &s.catchState,
    catch_send_button,
    stopPostAuthRuntimeScaffolding,
    ppBoardAutoSpin,
    ppBoardCatchMode,
    ppBoardPressSpeed,
    catch_on_event
  };
  return refs;
}

static void board_init()
{
  ppBoardInit();
}

static void board_tick()
{
  ppBoardTick(makeBoardRefs());
}

static void catch_tick()
{
  for (int i = 0; i < PP_MAX_CONNECTIONS; ++i) {
    if (!gSessions[i].used) continue;
    ppSetCurrentSession(&gSessions[i]);
    ppCatchTick(makeCatchEngineRefs(gSessions[i]));
  }
  ppSetCurrentSession(nullptr);
}

static void catch_send_button(uint8_t b0, uint8_t b1)
{
  ppSetVisibleButtonState(makeButtonRuntimeRefs(curSession()), b0, b1);
}

static void catch_on_event(PPLedEvent evt, const char* label)
{
  ppBoardOnEvent(evt, label);
}

static void catch_handle_led_write(const uint8_t* data, uint16_t len)
{
  stopPostAuthRuntimeScaffolding();
  ppCatchHandleLedWrite(makeCatchEngineRefs(curSession()), data, len);
}

static void manual_button_press()
{
  // One physical button press answers every connected, authenticated phone.
  int n = 0;
  for (int i = 0; i < PP_MAX_CONNECTIONS; ++i) {
    if (!gSessions[i].used) continue;
    ppSetCurrentSession(&gSessions[i]);
    if (ppCatchRequestPress(makeCatchEngineRefs(gSessions[i]), 0, "front button")) {
      ++n;
    }
  }
  ppSetCurrentSession(nullptr);
  Serial.print("Manual press requested on ");
  Serial.print(n);
  Serial.println(" session(s)");
}

// ---------------- Advertising ----------------

static void start_advertising()
{
  if (!ppBoardAdvertiseEnabled()) {
    Serial.println("Advertising suppressed (menu setting or device material missing)");
    return;
  }
  ppStartAdvertising(makeBleAdvertisingConfig());
}

static void stop_advertising()
{
  Bluefruit.Advertising.stop();
  Serial.println("Advertising stopped via console");
}

// ---------------- Device-level pairing reset ----------------

static void resetPairingFromPhysicalButtonAll(const char* reason)
{
  Serial.print("Physical reset (");
  Serial.print(reason);
  Serial.println(") -> wiping all pairing records and sessions");
  ppRecordStoreClearAll(reason);
  for (int i = 0; i < PP_MAX_CONNECTIONS; ++i) {
    if (!gSessions[i].used) continue;
    ppSetCurrentSession(&gSessions[i]);
    gSessions[i].recordIndex = -1;
    gSessions[i].boundRecord = false;
    resetUnpairedState();
    if (gSessions[i].connHandle != BLE_CONN_HANDLE_INVALID) {
      sd_ble_gap_disconnect(gSessions[i].connHandle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
    }
  }
  ppSetCurrentSession(nullptr);
  setManufacturerState(MANUFACTURER_STATE_PAIRABLE);
  start_advertising();
}

// ---------------- Boot-level wrappers ----------------

static void bootResetUnpairedAll()
{
  for (int i = 0; i < PP_MAX_CONNECTIONS; ++i) {
    ppSetCurrentSession(&gSessions[i]);
    gSessions[i].recordIndex = -1;
    gSessions[i].boundRecord = false;
    memset(gSessions[i].peerAddr, 0, 6);
    resetUnpairedState();
  }
  ppSetCurrentSession(nullptr);
}

static bool loadPersistentPairStateBoot()
{
  int n = ppRecordStoreLoadAll();
  return n > 0;
}

static void restorePairedStateFromPersistenceBoot()
{
  // Boot level: no sessions yet. Advertise as reconnect-ready when a record
  // exists; individual sessions bind their record at connect time.
  if (ppRecordStoreCount() > 0) {
    ppSetCurrentSession(&gSessions[0]);
    ppSetManufacturerState(makePairStateRuntimeRefs(gSessions[0]), MANUFACTURER_STATE_RECONNECT);
    ppSetCurrentSession(nullptr);
    Serial.println("Boot restore -> reconnect-ready advertising armed");
  }
}

// ---------------- BLE connect / disconnect (multi) ----------------

static void connect_callback(uint16_t conn_handle)
{
  int slot = ppSessionClaim(conn_handle);
  if (slot < 0) {
    Serial.println("No free session slot -> rejecting connection");
    sd_ble_gap_disconnect(conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
    return;
  }
  PPSession& s = gSessions[slot];
  ppSetCurrentSession(&s);

  // Remember the phone's address (display order) for record matching.
  NimBLEConnInfo info = NimBLEDevice::getServer()->getPeerInfoByHandle(conn_handle);
  const uint8_t* val = info.getAddress().getBase()->val;  // little-endian
  for (int i = 0; i < 6; ++i) {
    s.peerAddr[i] = val[5 - i];
  }

  s.connStartedAt = millis();
  s.serviceCNotifyEnabled = false;
  s.serviceBNotifyEnabled = false;
  s.buttonNotifyEnabled = false;
  s.serviceDNotifyEnabled = false;
  s.authControlNotifyEnabled = false;
  s.authBlobNotifyEnabled = false;

  int recIdx = ppRecordStoreMatch(s.peerAddr);
  if (recIdx >= 0) {
    s.recordIndex = recIdx;
    s.boundRecord = true;
    ppRecordStoreBindTo(recIdx,
                        s.persistedAcceptedCentralNonce, &s.persistedAcceptedCentralNonceValid,
                        s.persistedServiceDSelector4, &s.persistedServiceDSelector4Valid,
                        &s.persistedServiceDSelector5, &s.persistedServiceDSelector5Valid,
                        s.persistedServiceDRecord6, &s.persistedServiceDRecord6Valid,
                        &s.persistedServiceDRecord1A, &s.persistedServiceDRecord1AValid);
    s.pairingStage = PAIR_STAGE_AUTHENTICATED;
    s.pairEventClass = 0x25;
    s.persistentReconnectReady = true;
    s.firstPairSawReal36 = s.persistedServiceDSelector4Valid;
    s.firstPairSawReal28 = s.persistedServiceDRecord1AValid;
    rebuildServiceDRuntimeBlock();
    refreshReconnectAuthReply24("connect bind");
    Serial.print("Connected phone bound to pairing record ");
    Serial.println(recIdx);
  } else {
    s.recordIndex = -1;
    s.boundRecord = false;
    resetUnpairedState();
    Serial.println("Connected phone has no pairing record -> fresh pair flow");
  }

  resetVolatilePairSessionStateDispatch();
  {
    uint8_t lvl = BATTERY_LEVEL_DEFAULT;
    batteryLevelChar.write(&lvl, 1);
  }
  Serial.print("Connected, conn_handle=0x");
  Serial.print(conn_handle, HEX);
  Serial.print(" slot=");
  Serial.print(slot);
  Serial.print(" active=");
  Serial.println(ppSessionActiveCount());
  print_build_tag("ble connect");
  Serial.print("Pair stage at connect: ");
  Serial.println(pairingStageLabel(s.pairingStage));
  refreshPairingExports(false);
  ppBoardSetConnectedCount(ppSessionActiveCount());
  gAnyConnHandleMirror = conn_handle;  // valid while any session is up
  ppSetCurrentSession(nullptr);
}

static void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  ppBoardOnDisconnect(reason);
  int slot = ppSessionSlotByHandle(conn_handle);
  Serial.print("Disconnected, conn_handle=0x");
  Serial.print(conn_handle, HEX);
  Serial.print(" slot=");
  Serial.print(slot);
  Serial.print(" reason=0x");
  Serial.println(reason, HEX);
  if (slot < 0) {
    return;
  }
  PPSession& s = gSessions[slot];
  ppSetCurrentSession(&s);

  s.connHandle = BLE_CONN_HANDLE_INVALID;
  s.buttonPulseActive = false;
  resetRuntimeButtonEngineOne(s);
  memset(s.buttonState, 0, sizeof(s.buttonState));
  memset(s.serviceAStatus, 0, sizeof(s.serviceAStatus));
  s.connStartedAt = 0;
  s.serviceCNotifyEnabled = false;
  s.serviceBNotifyEnabled = false;
  s.buttonNotifyEnabled = false;
  s.serviceDNotifyEnabled = false;
  s.authControlNotifyEnabled = false;
  s.authBlobNotifyEnabled = false;
  resetVolatilePairSessionStateDispatch();
  stopPostAuthRuntimeScaffolding();

  if (s.pairingStage != PAIR_STAGE_AUTHENTICATED) {
    Serial.println("Disconnect before auth complete -> resetting to virgin pairable state");
    resetUnpairedState();
    s.recordIndex = -1;
    s.boundRecord = false;
  } else {
    s.persistentReconnectReady = true;
    prepareReconnectSleepDataOnAuthenticatedDisconnect();
    setManufacturerState(MANUFACTURER_STATE_RECONNECT);
    refreshPairingExports(false);
    // Make sure the record reflects the paired state (the nonce itself was
    // saved at markAuthComplete; flush again in case anything changed since).
    savePersistentPairState("authenticated disconnect");
  }

  ppSetCurrentSession(nullptr);
  ppSessionRelease(conn_handle);
  gAnyConnHandleMirror = BLE_CONN_HANDLE_INVALID;
  for (int i = 0; i < PP_MAX_CONNECTIONS; ++i) {
    if (gSessions[i].used && gSessions[i].connHandle != BLE_CONN_HANDLE_INVALID) {
      gAnyConnHandleMirror = gSessions[i].connHandle;
      break;
    }
  }
  ppBoardSetConnectedCount(ppSessionActiveCount());
  // The compat layer restarts advertising while a slot is free; for the last
  // connection also honour the console/material gates.
  if (ppSessionActiveCount() == 0) {
    start_advertising();
  }
  armDisconnectedSleepTimer("disconnect");
}

// ---------------- Per-slot read synchronisation ----------------
// Shared NimBLE characteristics hold one value buffer. When a phone reads a
// per-session mirror, rewrite the buffer from the reading session first.

static void syncCharValueForSlot(PPSession& s, BLECharacteristic* chr)
{
  if (chr == &serviceAChar3) {
    serviceAChar3.write(s.serviceARead3, sizeof(s.serviceARead3));
  } else if (chr == &serviceAChar4) {
    serviceAChar4.write(s.serviceARead4, sizeof(s.serviceARead4));
  } else if (chr == &serviceAChar5) {
    serviceAChar5.write(s.serviceAStatus, sizeof(s.serviceAStatus));
  } else if (chr == &serviceBChar1) {
    if (!s.postServiceDHandoffActive) {
      serviceBChar1.write(s.serviceBStatus1, sizeof(s.serviceBStatus1));
    }
  } else if (chr == &serviceBChar2) {
    serviceBChar2.write(s.serviceBStatus2, sizeof(s.serviceBStatus2));
  } else if (chr == &serviceBChar3) {
    serviceBChar3.write(s.serviceBRead3, sizeof(s.serviceBRead3));
  } else if (chr == &serviceCChar1) {
    serviceCChar1.write(s.serviceCState, sizeof(s.serviceCState));
  } else if (chr == &serviceDChar2) {
    if (s.serviceDReadPendingLen > 0) {
      serviceDChar2.write(s.serviceDReadBlob, s.serviceDReadPendingLen);
    } else {
      serviceDChar2.write(s.serviceDStatus, sizeof(s.serviceDStatus));
    }
  } else if (chr == &serviceDChar3) {
    if (s.serviceDReadPendingLen > 0) {
      serviceDChar3.write(s.serviceDReadBlob, s.serviceDReadPendingLen);
    } else {
      serviceDChar3.write(s.serviceDStatus, sizeof(s.serviceDStatus));
    }
  } else if (chr == &configChar) {
    configChar.write(s.configState, sizeof(s.configState));
  } else if (chr == &batteryLevelChar) {
    uint8_t lvl = BATTERY_LEVEL_DEFAULT;
    batteryLevelChar.write(&lvl, 1);
  }
}

static void cccd_write_callback(uint16_t conn_handle, BLECharacteristic* chr, uint16_t value)
{
  ppSetCurrentSessionByHandle(conn_handle);
  ppCccdWriteCallback(makeBleDispatchRefs(curSession()), conn_handle, chr, value);
  ppSetCurrentSession(nullptr);
}

static void read_authorize_callback(uint16_t conn_handle, BLECharacteristic* chr, ble_gatts_evt_read_t* request)
{
  ppSetCurrentSessionByHandle(conn_handle);
  PPSession& s = curSession();

  if (chr == &serviceBChar1 &&
      s.postServiceDHandoffActive &&
      request->handle == serviceBChar1.handles().value_handle) {
    serviceBChar1.write(&s.postServiceDServiceBValue, 1);
    s.postServiceDServiceBReadSeen = true;
    Serial.println("Post-ServiceD handoff observed ServiceB char1 read -> 04");
  } else {
    syncCharValueForSlot(s, chr);
  }

#if PP_LOG_VERBOSE_ENABLED
  Serial.print("read conn=0x");
  Serial.print(conn_handle, HEX);
  Serial.print(" handle=0x");
  Serial.println(request->handle, HEX);
#endif
  ppSetCurrentSession(nullptr);
}

static void service_write_callback(uint16_t conn_handle, BLECharacteristic* chr, uint8_t* data, uint16_t len)
{
  ppSetCurrentSessionByHandle(conn_handle);
  ppServiceWriteCallback(makeBleDispatchRefs(curSession()), conn_handle, chr, data, len);
  ppSetCurrentSession(nullptr);
}

// ---------------- GATT setup / identity ----------------

static void setup_services()
{
  ppSetupPokemonGoPlusTopology(makeBleTopologyRefs(), makeBleSetupCallbacks());
}

static void configure_identity()
{
  ppConfigureIdentity(makeBleIdentityConfig());
}

static void configure_gap_preferences()
{
  ppConfigureGapPreferences(makeBleGapPreferenceConfig());
}

// ---------------- setup / loop ----------------

void setup()
{
  ppSessionInitAll();
  ppDeviceMaterialLoad();
  ppCompatWantsAdvertising = []() { return ppBoardAdvertiseEnabled(); };
  ppRunBootSequence(makeAppBootRefs());
}

void loop()
{
  ppRunMainLoopTick(makeAppLoopRefs());
}
