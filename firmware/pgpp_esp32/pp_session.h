#pragma once
// Per-connection session state for the multi-phone Plus+ emulator.
//
// A real Pokemon GO Plus+ serves exactly one phone at a time. This firmware
// generalises that: every BLE connection gets its own PPSession with a full
// copy of the volatile protocol state (pairing stage, auth worker, Service D
// cert flow, runtime button engine, catch engine, auth blobs, ...), so two or
// more phones can each run their own pairing / reconnect / catch session
// against the same GATT table.
//
// Device-global state (identity, keys, GATT topology, advertising payload)
// stays in the sketch. The persisted pairing records are device-global too,
// but each session BINDS one record (see pp_record_store.h) and works on a
// copy while the phone is connected.
//
// Cross-task "current session":
//   BLE callbacks run on the NimBLE host task (core 0); the Arduino loop()
//   runs on core 1. Both call the protocol modules, and the refs structs
//   carry helper function pointers without an explicit session argument.
//   To keep those helpers targeting the right session, every entry point
//   calls ppSetCurrentSession() before invoking protocol code and helpers
//   call ppCurrentSession(). Each core keeps its own pointer, so the two
//   tasks never race on it.

#include <stdint.h>
#include <string.h>

#include "pp_types.h"
#include "pp_protocol_data.h"
#include "pp_catch_engine.h"

#ifndef PP_MAX_CONNECTIONS
#define PP_MAX_CONNECTIONS 4
#endif

struct PPSession {
  bool used;                 // slot claimed by a live BLE connection
  uint16_t connHandle;       // NimBLE connection handle (BLE_CONN_HANDLE_INVALID if none)
  bool boundRecord;          // session is bound to a persisted pairing record
  int recordIndex;           // index in the record store, -1 if none

  // Pairing / auth state (one full copy per phone)
  PairingStage pairingStage;
  uint8_t pairEventClass;
  uint8_t activePair[2];
  uint8_t candidatePair[2];
  uint8_t stagedPair[2];
  bool stagedPairValid;
  uint32_t connStartedAt;
  uint8_t serviceCTransferState;
  uint8_t serviceCBurstStep;
  uint32_t serviceCBurstAt;
  bool serviceCNotifyEnabled;
  bool serviceBNotifyEnabled;
  bool buttonNotifyEnabled;
  bool serviceDNotifyEnabled;
  bool authControlNotifyEnabled;
  bool authBlobNotifyEnabled;
  uint8_t freshAuthStep;
  uint32_t freshAuthAt;
  bool freshAuthStarted;
  bool freshAuthBlobSent;
  bool userCccdsSeeded;
  uint8_t postAuthIdleButtonRemaining;
  uint32_t postAuthIdleButtonAt;
  bool postAuthReady18Pending;
  uint32_t postAuthReady18At;
  bool persistentReconnectReady;
  bool reconnectServiceDArmed;
  bool reconnectCompactServiceDMode;
  uint8_t reconnectAuthReply24[sizeof(PLUSPLUS_AUTH_CTRL_REPLY_2401)];
  AuthReplyIndexSlotState authReplyIndexTable[32];
  uint8_t currentAttemptCentralNonce[4];
  bool currentAttemptCentralNonceValid;
  uint8_t persistedAcceptedCentralNonce[4];
  bool persistedAcceptedCentralNonceValid;
  bool firstPairSawReal36;
  bool firstPairSawReal28;
  DonorAuthRuntimeState donorAuthRuntime;
  AuthParserOutputState authParserOutput;
  uint8_t firstPairCertState;

  // Characteristic value mirrors (read sync is done per slot at read time)
  uint8_t serviceARead3[6];
  uint8_t serviceARead4[8];
  uint8_t serviceAStatus[2];
  uint8_t serviceBStatus1[4];
  uint8_t serviceBStatus2[4];
  uint8_t serviceBRead3[2];
  uint8_t serviceCState[74];
  uint8_t serviceDStatus[4];
  uint8_t configState[1];
  uint8_t buttonState[2];
  bool buttonPulseActive;
  uint32_t buttonPulseReleaseAt;

  // Service D cert flow + blobs
  ServiceDCertState serviceDCertState;
  uint8_t serviceDReadBlob[378];
  uint16_t serviceDReadPendingLen;
  uint8_t serviceDType3ReadPayload[36];
  uint8_t serviceDType5ReadPayload[20];
  uint8_t serviceDRuntimeBlock[0x80];
  uint8_t lastOlderServiceDType2Write[52];
  bool lastOlderServiceDType2WriteValid;
  bool postServiceDHandoffActive;
  bool postServiceDServiceBReadSeen;
  uint8_t postServiceDServiceBValue;

  // Auth blobs
  uint8_t authBlobValue[378];
  uint8_t authBlobVisibleValue[AUTH_BLOB_VISIBLE_LEN];
  AuthBlobStageState authBlobStage;
  uint8_t liveCertKey[16];
  uint8_t freshAuthInitialChallenge[16];
  uint8_t freshAuthSessionKey[16];
  uint8_t sfidaCertStage;
  uint8_t peerAddr[6];  // phone's BLE address at connect (display order)

  // Persisted Service D state (the session's bound pairing record copy)
  uint8_t persistedServiceDSelector4[16];
  bool persistedServiceDSelector4Valid;
  uint8_t persistedServiceDSelector5;
  bool persistedServiceDSelector5Valid;
  uint8_t persistedServiceDRecord6[10];
  bool persistedServiceDRecord6Valid;
  uint32_t persistedServiceDRecord1A;
  bool persistedServiceDRecord1AValid;

  // Runtime button engine
  bool runtimeButtonEngineActive;
  bool runtimeButtonActionArmed;
  bool runtimeButtonAwaitingAck;
  bool runtimeButtonSettlingToIdle;
  uint8_t runtimeButtonFamilyIndex;
  uint8_t runtimeButtonCurrentFamily;
  uint8_t runtimeButtonIdleCount;
  uint32_t runtimeButtonAt;

  // Catch engine
  PPCatchState catchState;
};

// One session per possible BLE connection.
extern PPSession gSessions[PP_MAX_CONNECTIONS];

void ppSessionInitAll();

// Map a NimBLE connection handle to its slot; -1 if unknown.
int ppSessionSlotByHandle(uint16_t connHandle);
// Claim a free slot for a connection handle; -1 if the table is full.
int ppSessionClaim(uint16_t connHandle);
// Release the slot of a connection handle; returns the slot or -1.
int ppSessionRelease(uint16_t connHandle);
// Number of live connections.
int ppSessionActiveCount();
// First slot bound to a persisted record (used for aggregate state), -1 if none.
int ppSessionAnyPaired();

// Per-core "current session" used by the refs helper wrappers.
void ppSetCurrentSession(PPSession* s);
PPSession* ppCurrentSession();
// Convenience: set current session from a connection handle.
void ppSetCurrentSessionByHandle(uint16_t connHandle);

// Aggregate advertised pair-state byte across all sessions (0x01/0x02).
uint8_t ppSessionAggregateManufacturerState();
