#pragma once
// Minimal host-side stub of NimBLE-Arduino 2.x, just enough to type-check the
// pgpp_esp32 compat layer. NOT for production use.

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>

#include "Arduino.h"

#define BLE_HS_ERR_HCI_BASE 0x100
#define BLE_GAP_CONN_MODE_UND 0
#define BLE_GAP_CONN_MODE_NON 3

namespace NIMBLE_PROPERTY {
  enum : uint32_t {
    READ = 0x01,
    WRITE = 0x02,
    WRITE_NR = 0x04,
    NOTIFY = 0x10,
    INDICATE = 0x20,
    BROADCAST = 0x40,
  };
}

class NimBLEUUID {
public:
  NimBLEUUID() {}
  NimBLEUUID(const char*) {}
  NimBLEUUID(uint16_t) {}
  std::string toString() const { return ""; }
};

struct nimble_ble_addr_val { uint8_t val[6]; };
class NimBLEAddress {
public:
  NimBLEAddress() {}
  std::string toString() const { return ""; }
  int getType() const { return 0; }
  const nimble_ble_addr_val* getBase() const { static nimble_ble_addr_val b = { {0,0,0,0,0,0} }; return &b; }
};

class NimBLEAttValue {
public:
  NimBLEAttValue() {}
  const uint8_t* data() const { return nullptr; }
  size_t length() const { return 0; }
};

class NimBLEConnInfo {
public:
  uint16_t getConnHandle() const { return 0; }
  NimBLEAddress getAddress() const { return NimBLEAddress(); }
  uint16_t getMTU() const { return 517; }
};

class NimBLECharacteristic;
class NimBLEService;
class NimBLEServer;
class NimBLEAdvertising;
class NimBLEDescriptor;

class NimBLECharacteristicCallbacks {
public:
  virtual ~NimBLECharacteristicCallbacks() {}
  virtual void onRead(NimBLECharacteristic*, NimBLEConnInfo&) {}
  virtual void onWrite(NimBLECharacteristic*, NimBLEConnInfo&) {}
  virtual void onSubscribe(NimBLECharacteristic*, NimBLEConnInfo&, uint16_t) {}
  virtual void onStatus(NimBLECharacteristic*, int, int) {}
};

class NimBLEServerCallbacks {
public:
  virtual ~NimBLEServerCallbacks() {}
  virtual void onConnect(NimBLEServer*, NimBLEConnInfo&) {}
  virtual void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) {}
  virtual void onMTUChange(uint16_t, NimBLEConnInfo&) {}
  virtual uint32_t onPassKeyRequest() { return 0; }
  virtual void onAuthenticationComplete(NimBLEConnInfo&) {}
};

class NimBLECharacteristic {
public:
  void setValue(const uint8_t*, size_t) {}
  void setValue(const std::vector<uint8_t>&) {}
  NimBLEAttValue getValue() { return NimBLEAttValue(); }
  size_t getDataLength() { return 0; }
  uint16_t getHandle() const { return 0; }
  void setCallbacks(NimBLECharacteristicCallbacks*) {}
  bool notify(const uint8_t*, size_t, uint16_t) { return true; }
  bool notify(uint16_t) { return true; }
  NimBLEDescriptor* createDescriptor(const NimBLEUUID&, uint32_t = 0) { return nullptr; }
};

class NimBLEService {
public:
  NimBLECharacteristic* createCharacteristic(const NimBLEUUID&, uint32_t = 0, uint16_t = 0) { return nullptr; }
  NimBLECharacteristic* getCharacteristic(uint16_t) { return nullptr; }
  uint16_t getHandle() const { return 0; }
};

class NimBLEServer {
public:
  NimBLEService* createService(const NimBLEUUID&) { return nullptr; }
  void setCallbacks(NimBLEServerCallbacks*) {}
  void advertiseOnDisconnect(bool) {}
  void start() {}
  bool disconnect(uint16_t, uint8_t) { return true; }
  uint16_t getPeerMTU(uint16_t) { return 517; }
  void updateConnParams(uint16_t, uint16_t, uint16_t, uint16_t, uint16_t) {}
  NimBLEConnInfo getPeerInfo(uint16_t) { return NimBLEConnInfo(); }
};

class NimBLEAdvertisementData {
public:
  void setFlags(uint8_t) {}
  void addData(const uint8_t*, size_t) {}
  void setName(const char*) {}
};

class NimBLEAdvertising {
public:
  void setAdvertisementData(NimBLEAdvertisementData&) {}
  void setScanResponseData(NimBLEAdvertisementData&) {}
  void enableScanResponse(bool) {}
  void setMinInterval(uint16_t) {}
  void setMaxInterval(uint16_t) {}
  void setConnectableMode(uint8_t) {}
  bool start(uint32_t) { return true; }
  void stop() {}
  bool isAdvertising() { return false; }
};

struct ble_gap_event;
class NimBLEDevice {
public:
  static void init(const char*) {}
  static void setDeviceName(const char*) {}
  static void setMTU(uint16_t) {}
  static void setPower(int) {}
  static NimBLEServer* createServer() { return nullptr; }
  static NimBLEServer* getServer() { return nullptr; }
  static NimBLEAdvertising* getAdvertising() { return nullptr; }
  static NimBLEAddress getAddress() { return NimBLEAddress(); }
  static void setCustomGapHandler(int (*)(ble_gap_event*, void*)) {}
};

// nimble stack types used by the compat layer
struct ble_uuid_t { int dummy; };
struct ble_uuid16_t { ble_uuid_t u; uint16_t value; };
#define BLE_UUID16_INIT(v) { { { 0 } }, (v) }
struct ble_gap_event {
  int type;
  struct {
    uint16_t attr_handle;
    uint8_t cur_indicate;
    uint8_t cur_notify;
    uint8_t reason;
    int status;
  } subscribe;
};
#define BLE_GAP_EVENT_SUBSCRIBE 8
struct ble_gatt_svc_def { int dummy; };
inline int ble_gatts_find_chr(const ble_uuid_t*, const ble_uuid_t*, uint16_t*, uint16_t*) { return 0; }
inline void ble_svc_gatt_changed(uint16_t, uint16_t) {}
