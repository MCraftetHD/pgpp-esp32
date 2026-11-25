#pragma once
// Persisted pairing records for the multi-phone emulator.
//
// The single-phone firmware persisted exactly one pairing record (accepted
// central nonce + Service D selector state). With several phones paired to
// one ESP32 we keep one record per phone, keyed by the phone's BLE address
// at first-pair time:
//
//   - On connect, the session binds the record matching the peer address.
//     If exactly one record exists, it binds that one regardless of address
//     (matches the proven single-phone behaviour, and survives iOS rotating
//     its private address).
//   - If no record matches, the session starts the fresh-pair flow; a
//     successful first pair writes a new record for that address.
//   - On reconnect, the app's indexed 0x24 write is answered from the bound
//     record's accepted nonce, exactly like the single-phone firmware.
//
// Records live in NVS via the pp_compat_fs shim (keys "ppr0".."pprN").

#include <stdint.h>

#include "pp_types.h"
#include "pp_session.h"

#ifndef PP_MAX_PAIRING_RECORDS
#define PP_MAX_PAIRING_RECORDS PP_MAX_CONNECTIONS
#endif

static constexpr uint32_t PP_RECORD_MAGIC = 0x50475052;  // 'PGPR'
static constexpr uint16_t PP_RECORD_VERSION = 3;

struct PPRecord {
  bool valid;
  uint8_t peerAddr[6];       // central BLE address at first pairing (as seen, big-endian display order)
  uint8_t acceptedNonce[4];
  bool acceptedNonceValid;
  uint8_t serviceDSelector4[16];
  bool serviceDSelector4Valid;
  uint8_t serviceDSelector5;
  bool serviceDSelector5Valid;
  uint8_t serviceDRecord6[10];
  bool serviceDRecord6Valid;
  uint32_t serviceDRecord1A;
  bool serviceDRecord1AValid;
  uint32_t lastSeenAt;       // epoch-ish timestamp from millis()/1000 at save
};

// Serialised image (fixed size, little-endian fields as laid out by the
// compiler — matches how pp_persistence wrote its record).
struct PPRecordImage {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  uint8_t paired;
  uint8_t acceptedNonceValid;
  uint8_t serviceDSelector4Valid;
  uint8_t serviceDSelector5Valid;
  uint8_t serviceDRecord6Valid;
  uint8_t serviceDRecord1AValid;
  uint8_t reserved0;
  uint8_t reserved1;
  uint8_t acceptedNonce[4];
  uint8_t serviceDSelector4[16];
  uint8_t serviceDSelector5;
  uint8_t serviceDRecord6[10];
  uint32_t serviceDRecord1A;
  uint8_t peerAddr[6];
  uint32_t lastSeenAt;
};

bool ppRecordStoreBegin();
int ppRecordStoreLoadAll();
int ppRecordStoreCount();
bool ppRecordStoreValid(int idx);
const uint8_t* ppRecordStorePeerAddr(int idx);

// Find the record index for a peer address; -1 if none. When exactly one
// record exists it is returned regardless of address (single-phone legacy).
int ppRecordStoreMatch(const uint8_t* peerAddr6);

// Write the session's persisted fields into record `idx` and flush to NVS.
bool ppRecordStoreSaveFrom(int idx,
                           const uint8_t* peerAddr6,
                           bool persistentReconnectReady,
                           const uint8_t* acceptedNonce, bool acceptedNonceValid,
                           const uint8_t* sel4, bool sel4Valid,
                           uint8_t sel5, bool sel5Valid,
                           const uint8_t* rec6, bool rec6Valid,
                           uint32_t rec1A, bool rec1AValid);

// Copy record `idx` into the given output fields (bind at connect).
void ppRecordStoreBindTo(int idx,
                         uint8_t* outAcceptedNonce, bool* outAcceptedNonceValid,
                         uint8_t* outSel4, bool* outSel4Valid,
                         uint8_t* outSel5, bool* outSel5Valid,
                         uint8_t* outRec6, bool* outRec6Valid,
                         uint32_t* outRec1A, bool* outRec1AValid);

// Erase one record (or all with idx = -1) and flush.
void ppRecordStoreClear(int idx, const char* reason);
void ppRecordStoreClearAll(const char* reason);

// Address display helper: NimBLEConnInfo addresses are big-endian display
// order, so store them the way they print.
void ppRecordStoreCopyAddr(const uint8_t* srcDisplayOrder, uint8_t* dst6);
bool ppRecordStoreAddrEqual(const uint8_t* a6, const uint8_t* b6);
