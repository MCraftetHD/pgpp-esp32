#include <Arduino.h>
#include <Preferences.h>
#include <string.h>

#include "pp_record_store.h"

static Preferences gRecordPrefs;
static bool gRecordStoreReady = false;
static PPRecord gRecords[PP_MAX_PAIRING_RECORDS];

static constexpr char kRecordNamespace[] = "pprecords";

static void recordToImage(const PPRecord& rec, PPRecordImage& img)
{
  memset(&img, 0, sizeof(img));
  img.magic = PP_RECORD_MAGIC;
  img.version = PP_RECORD_VERSION;
  img.size = sizeof(img);
  img.paired = rec.valid ? 1 : 0;
  img.acceptedNonceValid = rec.acceptedNonceValid ? 1 : 0;
  img.serviceDSelector4Valid = rec.serviceDSelector4Valid ? 1 : 0;
  img.serviceDSelector5Valid = rec.serviceDSelector5Valid ? 1 : 0;
  img.serviceDRecord6Valid = rec.serviceDRecord6Valid ? 1 : 0;
  img.serviceDRecord1AValid = rec.serviceDRecord1AValid ? 1 : 0;
  memcpy(img.acceptedNonce, rec.acceptedNonce, 4);
  memcpy(img.serviceDSelector4, rec.serviceDSelector4, 16);
  img.serviceDSelector5 = rec.serviceDSelector5;
  memcpy(img.serviceDRecord6, rec.serviceDRecord6, 10);
  img.serviceDRecord1A = rec.serviceDRecord1A;
  memcpy(img.peerAddr, rec.peerAddr, 6);
  img.lastSeenAt = rec.lastSeenAt;
}

static bool imageToRecord(const PPRecordImage& img, PPRecord& rec)
{
  if (img.magic != PP_RECORD_MAGIC || img.version != PP_RECORD_VERSION ||
      img.size != sizeof(img) || !img.paired) {
    return false;
  }
  memset(&rec, 0, sizeof(rec));
  rec.valid = true;
  rec.acceptedNonceValid = img.acceptedNonceValid != 0;
  memcpy(rec.acceptedNonce, img.acceptedNonce, 4);
  rec.serviceDSelector4Valid = img.serviceDSelector4Valid != 0;
  memcpy(rec.serviceDSelector4, img.serviceDSelector4, 16);
  rec.serviceDSelector5Valid = img.serviceDSelector5Valid != 0;
  rec.serviceDSelector5 = img.serviceDSelector5;
  rec.serviceDRecord6Valid = img.serviceDRecord6Valid != 0;
  memcpy(rec.serviceDRecord6, img.serviceDRecord6, 10);
  rec.serviceDRecord1AValid = img.serviceDRecord1AValid != 0;
  rec.serviceDRecord1A = img.serviceDRecord1A;
  memcpy(rec.peerAddr, img.peerAddr, 6);
  rec.lastSeenAt = img.lastSeenAt;
  return true;
}

bool ppRecordStoreBegin()
{
  if (gRecordStoreReady) {
    return true;
  }
  gRecordStoreReady = gRecordPrefs.begin(kRecordNamespace, false);
  return gRecordStoreReady;
}

int ppRecordStoreLoadAll()
{
  memset(gRecords, 0, sizeof(gRecords));
  if (!gRecordStoreReady) {
    return 0;
  }
  int loaded = 0;
  for (int i = 0; i < PP_MAX_PAIRING_RECORDS; ++i) {
    char key[8];
    snprintf(key, sizeof(key), "ppr%d", i);
    PPRecordImage img;
    memset(&img, 0, sizeof(img));
    size_t n = gRecordPrefs.getBytesLength(key);
    if (n == 0) {
      continue;
    }
    if (n > sizeof(img)) {
      n = sizeof(img);
    }
    gRecordPrefs.getBytes(key, &img, n);
    if (imageToRecord(img, gRecords[i])) {
      ++loaded;
    }
  }
  Serial.print("Record store: loaded ");
  Serial.print(loaded);
  Serial.print(" of ");
  Serial.print(PP_MAX_PAIRING_RECORDS);
  Serial.println(" pairing record slots");
  return loaded;
}

int ppRecordStoreCount()
{
  int n = 0;
  for (int i = 0; i < PP_MAX_PAIRING_RECORDS; ++i) {
    if (gRecords[i].valid) {
      ++n;
    }
  }
  return n;
}

bool ppRecordStoreValid(int idx)
{
  return idx >= 0 && idx < PP_MAX_PAIRING_RECORDS && gRecords[idx].valid;
}

const uint8_t* ppRecordStorePeerAddr(int idx)
{
  if (idx < 0 || idx >= PP_MAX_PAIRING_RECORDS) {
    return nullptr;
  }
  return gRecords[idx].peerAddr;
}

int ppRecordStoreMatch(const uint8_t* peerAddr6)
{
  int count = ppRecordStoreCount();
  if (count == 0) {
    return -1;
  }
  if (peerAddr6 != nullptr) {
    for (int i = 0; i < PP_MAX_PAIRING_RECORDS; ++i) {
      if (gRecords[i].valid && ppRecordStoreAddrEqual(gRecords[i].peerAddr, peerAddr6)) {
        return i;
      }
    }
  }
  // Single record: bind it whatever the address (legacy single-phone path).
  if (count == 1) {
    for (int i = 0; i < PP_MAX_PAIRING_RECORDS; ++i) {
      if (gRecords[i].valid) {
        return i;
      }
    }
  }
  return -1;
}

bool ppRecordStoreSaveFrom(int idx,
                           const uint8_t* peerAddr6,
                           bool persistentReconnectReady,
                           const uint8_t* acceptedNonce, bool acceptedNonceValid,
                           const uint8_t* sel4, bool sel4Valid,
                           uint8_t sel5, bool sel5Valid,
                           const uint8_t* rec6, bool rec6Valid,
                           uint32_t rec1A, bool rec1AValid)
{
  if (idx < 0 || idx >= PP_MAX_PAIRING_RECORDS || !gRecordStoreReady) {
    return false;
  }
  PPRecord& rec = gRecords[idx];
  memset(&rec, 0, sizeof(rec));
  rec.valid = persistentReconnectReady;
  if (peerAddr6 != nullptr) {
    memcpy(rec.peerAddr, peerAddr6, 6);
  }
  rec.acceptedNonceValid = acceptedNonceValid;
  if (acceptedNonce) memcpy(rec.acceptedNonce, acceptedNonce, 4);
  rec.serviceDSelector4Valid = sel4Valid;
  if (sel4) memcpy(rec.serviceDSelector4, sel4, 16);
  rec.serviceDSelector5Valid = sel5Valid;
  rec.serviceDSelector5 = sel5;
  rec.serviceDRecord6Valid = rec6Valid;
  if (rec6) memcpy(rec.serviceDRecord6, rec6, 10);
  rec.serviceDRecord1AValid = rec1AValid;
  rec.serviceDRecord1A = rec1A;
  rec.lastSeenAt = millis() / 1000UL;

  PPRecordImage img;
  recordToImage(rec, img);
  char key[8];
  snprintf(key, sizeof(key), "ppr%d", idx);
  gRecordPrefs.putBytes(key, &img, sizeof(img));
  Serial.print("Record store: saved record ");
  Serial.print(idx);
  Serial.print(" addr=");
  for (int i = 0; i < 6; ++i) {
    if (i) Serial.print(':');
    if (rec.peerAddr[i] < 0x10) Serial.print('0');
    Serial.print(rec.peerAddr[i], HEX);
  }
  Serial.print(" nonceValid=");
  Serial.println(rec.acceptedNonceValid ? "true" : "false");
  return true;
}

void ppRecordStoreBindTo(int idx,
                         uint8_t* outAcceptedNonce, bool* outAcceptedNonceValid,
                         uint8_t* outSel4, bool* outSel4Valid,
                         uint8_t* outSel5, bool* outSel5Valid,
                         uint8_t* outRec6, bool* outRec6Valid,
                         uint32_t* outRec1A, bool* outRec1AValid)
{
  if (idx < 0 || idx >= PP_MAX_PAIRING_RECORDS) {
    return;
  }
  const PPRecord& rec = gRecords[idx];
  *outAcceptedNonceValid = rec.acceptedNonceValid;
  memcpy(outAcceptedNonce, rec.acceptedNonce, 4);
  *outSel4Valid = rec.serviceDSelector4Valid;
  memcpy(outSel4, rec.serviceDSelector4, 16);
  *outSel5Valid = rec.serviceDSelector5Valid;
  *outSel5 = rec.serviceDSelector5;
  *outRec6Valid = rec.serviceDRecord6Valid;
  memcpy(outRec6, rec.serviceDRecord6, 10);
  *outRec1AValid = rec.serviceDRecord1AValid;
  *outRec1A = rec.serviceDRecord1A;
}

void ppRecordStoreClear(int idx, const char* reason)
{
  if (!gRecordStoreReady) {
    return;
  }
  if (idx >= 0 && idx < PP_MAX_PAIRING_RECORDS) {
    memset(&gRecords[idx], 0, sizeof(PPRecord));
    char key[8];
    snprintf(key, sizeof(key), "ppr%d", idx);
    gRecordPrefs.remove(key);
    Serial.print("Record store: cleared record ");
    Serial.print(idx);
    Serial.print(" via ");
    Serial.println(reason);
  }
}

void ppRecordStoreClearAll(const char* reason)
{
  if (!gRecordStoreReady) {
    return;
  }
  memset(gRecords, 0, sizeof(gRecords));
  for (int i = 0; i < PP_MAX_PAIRING_RECORDS; ++i) {
    char key[8];
    snprintf(key, sizeof(key), "ppr%d", i);
    gRecordPrefs.remove(key);
  }
  Serial.print("Record store: cleared all records via ");
  Serial.println(reason);
}

void ppRecordStoreCopyAddr(const uint8_t* srcDisplayOrder, uint8_t* dst6)
{
  memcpy(dst6, srcDisplayOrder, 6);
}

bool ppRecordStoreAddrEqual(const uint8_t* a6, const uint8_t* b6)
{
  return memcmp(a6, b6, 6) == 0;
}
