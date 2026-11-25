#pragma once

// Multi-phone port: storage + pair-state composition, bound per session.

static PersistedPairRuntimeRefs makePersistedPairRuntimeRefs(PPSession& s)
{
  PersistedPairRuntimeRefs refs = {
    &gPersistentStoreReady,
    &s.persistentReconnectReady,
    &s.persistedAcceptedCentralNonceValid,
    s.persistedAcceptedCentralNonce,
    &s.persistedServiceDSelector4Valid,
    s.persistedServiceDSelector4,
    &s.persistedServiceDSelector5Valid,
    &s.persistedServiceDSelector5,
    &s.persistedServiceDRecord6Valid,
    s.persistedServiceDRecord6,
    &s.persistedServiceDRecord1AValid,
    &s.persistedServiceDRecord1A,
    &s.pairingStage,
    &s.pairEventClass,
    &s.firstPairSawReal36,
    &s.firstPairSawReal28,
    rebuildServiceDRuntimeBlock,
    refreshReconnectAuthReply24,
    setManufacturerState,
    refreshPairingExports
  };
  return refs;
}

// ---- Pair-state wrappers (current session) ----
static void copyAddressDisplay(uint8_t* dest)
{
  ppCopyAddressDisplay(makePairHelperRefs(curSession()), dest);
}

static void resetServiceDCertState()
{
  ppResetServiceDCertState(makePairStateRuntimeRefs(curSession()));
}

static void clearReconnectServiceDArm()
{
  ppClearReconnectServiceDArm(makePairStateRuntimeRefs(curSession()));
}

// ---- Persistence wrappers ----
// Multi-phone: save writes the current session's bound record.
static bool savePersistentPairState(const char* reason)
{
  PPSession& s = curSession();
  int idx = s.recordIndex;
  if (idx < 0 && s.persistentReconnectReady) {
    // First save for this session: pick the record slot matching the phone's
    // address, otherwise a free slot.
    idx = ppRecordStoreMatch(s.peerAddr);
    if (idx < 0) {
      for (int i = 0; i < PP_MAX_PAIRING_RECORDS; ++i) {
        if (ppRecordStoreValid(i) && ppRecordStoreAddrEqual(ppRecordStorePeerAddr(i), s.peerAddr)) {
          idx = i;
          break;
        }
      }
      if (idx < 0) {
        for (int i = 0; i < PP_MAX_PAIRING_RECORDS; ++i) {
          if (!ppRecordStoreValid(i)) {
            idx = i;
            break;
          }
        }
      }
      if (idx < 0) {
        idx = static_cast<int>(s - gSessions) % PP_MAX_PAIRING_RECORDS;  // reuse own slot index
      }
    }
    s.recordIndex = idx;
  }
  if (idx < 0) {
    Serial.print("Persist pair state skipped via ");
    Serial.print(reason);
    Serial.println(": no record slot bound (session not authenticated)");
    return false;
  }
  s.boundRecord = true;
  bool ok = ppRecordStoreSaveFrom(idx,
                                  s.peerAddr,
                                  s.persistentReconnectReady,
                                  s.persistedAcceptedCentralNonce, s.persistedAcceptedCentralNonceValid,
                                  s.persistedServiceDSelector4, s.persistedServiceDSelector4Valid,
                                  s.persistedServiceDSelector5, s.persistedServiceDSelector5Valid,
                                  s.persistedServiceDRecord6, s.persistedServiceDRecord6Valid,
                                  s.persistedServiceDRecord1A, s.persistedServiceDRecord1AValid);
  Serial.print("Persist pair state via ");
  Serial.print(reason);
  Serial.print(" record=");
  Serial.print(idx);
  Serial.println(ok ? " -> ok" : " -> failed");
  return ok;
}

static bool beginPersistentStore()
{
  return ppRecordStoreBegin();
}

// Multi-phone: clear only the current session's bound record.
static void clearPersistentPairState(const char* reason)
{
  PPSession& s = curSession();
  if (s.recordIndex >= 0) {
    ppRecordStoreClear(s.recordIndex, reason);
  }
  s.recordIndex = -1;
  s.boundRecord = false;
}

// Per-session restore from the bound record.
static void restorePairedStateFromPersistence()
{
  ppRestorePairedStateFromPersistence(makePersistedPairRuntimeRefs(curSession()));
}

// ---- PairStateRuntimeRefs ----
static PairStateRuntimeRefs makePairStateRuntimeRefs(PPSession& s)
{
  PairStateRuntimeRefs refs = {
    &s.serviceDCertState,
    &s.reconnectServiceDArmed,
    &s.reconnectCompactServiceDMode,
    s.persistedServiceDSelector4,
    &s.persistedServiceDSelector4Valid,
    &s.persistedServiceDSelector5,
    &s.persistedServiceDSelector5Valid,
    s.persistedServiceDRecord6,
    &s.persistedServiceDRecord6Valid,
    &s.persistedServiceDRecord1A,
    &s.persistedServiceDRecord1AValid,
    s.serviceDRuntimeBlock,
    s.liveCertKey,
    PLUSPLUS_FACTORY_SELECTOR3,
    sizeof(PLUSPLUS_FACTORY_SELECTOR3),
    PLUSPLUS_FACTORY_SELECTOR6,
    sizeof(PLUSPLUS_FACTORY_SELECTOR6),
    &s.pairingStage,
    &s.pairEventClass,
    s.candidatePair,
    s.activePair,
    s.stagedPair,
    &s.stagedPairValid,
    &s.connStartedAt,
    &s.serviceCTransferState,
    &s.serviceCBurstStep,
    &s.serviceCBurstAt,
    &s.serviceCNotifyEnabled,
    &s.serviceBNotifyEnabled,
    &s.buttonNotifyEnabled,
    &s.serviceDNotifyEnabled,
    &s.authControlNotifyEnabled,
    &s.authBlobNotifyEnabled,
    &s.postServiceDHandoffActive,
    &s.postServiceDServiceBReadSeen,
    &s.postServiceDServiceBValue,
    &s.freshAuthStep,
    &s.freshAuthAt,
    &s.freshAuthStarted,
    &s.freshAuthBlobSent,
    &s.sfidaCertStage,
    &s.userCccdsSeeded,
    &s.persistentReconnectReady,
    s.reconnectAuthReply24,
    sizeof(s.reconnectAuthReply24),
    PLUSPLUS_AUTH_CTRL_REPLY_2401,
    sizeof(PLUSPLUS_AUTH_CTRL_REPLY_2401),
    s.currentAttemptCentralNonce,
    &s.currentAttemptCentralNonceValid,
    s.persistedAcceptedCentralNonce,
    &s.persistedAcceptedCentralNonceValid,
    &s.firstPairSawReal36,
    &s.firstPairSawReal28,
    &s.firstPairCertState,
    plusplusManufacturerData,
    sizeof(plusplusManufacturerData),
    s.serviceARead3,
    s.serviceARead4,
    s.serviceAStatus,
    s.serviceBStatus1,
    s.serviceBStatus2,
    s.serviceBRead3,
    s.serviceCState,
    s.serviceDStatus,
    s.configState,
    &gServicesReady,
    &s.connHandle,
    &serviceAChar3,
    &serviceAChar4,
    &serviceAChar5,
    &serviceBChar1,
    &serviceBChar2,
    &serviceBChar3,
    &serviceCChar1,
    &serviceDChar2,
    &configChar,
    resetReconnectSleepDataState,
    resetRuntimeButtonEngine,
    stopPostAuthRuntimeScaffolding,
    resetDonorAuthRuntime,
    seedAuthReplyIndexTable,
    clearAuthBlobStage,
    resetAuthParserOutput,
    setManufacturerState,
    refreshPairingExports,
    copyAddressDisplay,
    readyStateCode
  };
  return refs;
}

// ---- Pair-state export wrappers ----
static void setManufacturerState(uint8_t state)
{
  // The advertised byte is device-global; always write the aggregate state.
  ppSetManufacturerState(makePairStateRuntimeRefs(curSession()), ppSessionAggregateManufacturerState());
  refreshPairingExports(false);
}

static void refreshPairingExports(bool notify = false)
{
  ppRefreshPairingExports(makePairStateRuntimeRefs(curSession()), notify);
}

// ---- Reset unpaired wrapper ----
static void resetUnpairedState()
{
  ppResetUnpairedState(makePairStateRuntimeRefs(curSession()));
}
