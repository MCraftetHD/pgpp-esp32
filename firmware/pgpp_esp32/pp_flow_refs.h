#pragma once

// Multi-phone port of the flow/runtime/auth composition. Every builder binds
// a PPSession's fields; every wrapper resolves the current session
// (per-core pointer, see pp_session.h). The BLE characteristic objects and
// device-global constants come from the sketch / pp_ble_refs.h.

static AppLifecycleRefs makeAppLifecycleRefs(PPSession& s)
{
  AppLifecycleRefs refs = {
    &s.connHandle,
    &s.pairingStage,
    &s.persistentReconnectReady,
    &gDisconnectedSleepArmed,
    &gDisconnectedSleepAt,
    DISCONNECTED_SLEEP_TIMEOUT_MS,
    &gPhysicalResetButtonActive,
    &gPhysicalResetButtonTriggered,
    &gPhysicalResetButtonPressedAt,
    PHYSICAL_RESET_BUTTON_RAW_PIN,
    PHYSICAL_RESET_BUTTON_HOLD_MS,
    &s.buttonPulseActive,
    s.buttonState,
    sizeof(s.buttonState),
    s.serviceAStatus,
    sizeof(s.serviceAStatus),
    &s.connStartedAt,
    &s.serviceCNotifyEnabled,
    &s.serviceBNotifyEnabled,
    &s.buttonNotifyEnabled,
    &s.serviceDNotifyEnabled,
    &s.authControlNotifyEnabled,
    &s.authBlobNotifyEnabled,
    MANUFACTURER_STATE_RECONNECT,
    &batteryLevelChar,
    BATTERY_LEVEL_DEFAULT,
    prepareReconnectSleepDataOnAuthenticatedDisconnect,
    resetRuntimeButtonEngine,
    resetVolatilePairSessionStateDispatch,
    stopPostAuthRuntimeScaffolding,
    setManufacturerState,
    refreshPairingExports,
    resetUnpairedState,
    clearPersistentPairState,
    start_advertising,
    print_build_tag,
    pairingStageLabel
  };
  return refs;
}

static AppBootRefs makeAppBootRefs()
{
  AppBootRefs refs = {
    &gServicesReady,
    &gPersistentStoreReady,
    DEVICE_NAME,
    "=== ESP32 Plus+ emulator boot (multi-phone) ===",
    PHYSICAL_RESET_BUTTON_RAW_PIN,
    print_build_tag,
    seedLongReadBlob,
    seedAuthBlob,
    configure_identity,
    configure_gap_preferences,
    setup_services,
    beginPersistentStore,
    bootResetUnpairedAll,
    loadPersistentPairStateBoot,
    restorePairedStateFromPersistenceBoot,
    start_advertising,
    armDisconnectedSleepTimer,
    connect_callback,
    disconnect_callback,
    board_init
  };
  return refs;
}

static AppLoopRefs makeAppLoopRefs()
{
  AppLoopRefs refs = {
    processPhysicalResetButtonIfNeeded,
    processDisconnectedSleepIfNeeded,
    processAuthenticatedSleepStreamAll,
    clearButtonPulseIfNeededAll,
    processRuntimeButtonEngineIfNeededAll,
    processPostAuthIdleButtonNotifyAll,
    processPostAuthReady18All,
    processServiceCFirstPairBurstAll,
    processFreshAuthSequenceAll,
    10,
    board_tick,
    catch_tick
  };
  return refs;
}

static bool handleOlderServiceDFlowWriteDispatch(const uint8_t* data, uint16_t len)
{
  PPSession& s = curSession();
  return ppFirstPairFlowHandleOlderServiceDWrite(makeFirstPairOlderServiceDRefs(s),
                                                 data,
                                                 len,
                                                 PLUSPLUS_SERVICE_D_STATUS1,
                                                 PLUSPLUS_SERVICE_D_STATUS2,
                                                 PLUSPLUS_SERVICE_D_STATUS4,
                                                 FRESH_AUTH_WAIT_FOR_28_WRITE_MS);
}

static bool handleReconnectServiceDFlowWriteDispatch(const uint8_t* data, uint16_t len)
{
  PPSession& s = curSession();
  return ppReconnectFlowHandleCompactServiceDWrite(makeReconnectCompactServiceDRefs(s),
                                                   data,
                                                   len,
                                                   DONOR_SERVICE_D_EXPECT_WRITE3_DONE,
                                                   sizeof(DONOR_SERVICE_D_EXPECT_WRITE3_DONE),
                                                   PLUSPLUS_SERVICE_D_STATUS4_STAGE1,
                                                   PLUSPLUS_SERVICE_D_STATUS5_STAGE2,
                                                   PLUSPLUS_SERVICE_D_STATUS4_DONE,
                                                   PLUSPLUS_SERVICE_D_STATUS4_FAIL,
                                                   FRESH_AUTH_WAIT_FOR_28_WRITE_MS);
}

static ServiceDLocalRefs makeServiceDLocalRefs(PPSession& s)
{
  ServiceDLocalRefs refs = {
    &s.serviceDCertState,
    &s.connHandle,
    &serviceBChar1,
    &serviceDChar2,
    &serviceDChar3,
    s.serviceDStatus,
    sizeof(s.serviceDStatus),
    s.serviceDReadBlob,
    &s.serviceDReadPendingLen,
    s.serviceDType3ReadPayload,
    sizeof(s.serviceDType3ReadPayload),
    s.serviceDType5ReadPayload,
    sizeof(s.serviceDType5ReadPayload),
    &s.postServiceDHandoffActive,
    &s.postServiceDServiceBReadSeen,
    &s.postServiceDServiceBValue,
    DONOR_SERVICE_D_READ_TYPE3,
    sizeof(DONOR_SERVICE_D_READ_TYPE3),
    rebuildServiceDRuntimeBlock,
    printHex,
    logServiceDCertState,
    refreshPairingExports,
    handleOlderServiceDFlowWriteDispatch,
    handleReconnectServiceDFlowWriteDispatch
  };
  return refs;
}

static void handleReconnectAuthWrite25Dispatch(const uint8_t* reply,
                                               uint16_t replyLen,
                                               const char* reason);

static ReconnectRuntimeRefs makeReconnectRuntimeRefs(PPSession& s)
{
  ReconnectRuntimeRefs refs = {
    &s.connHandle,
    &s.pairingStage,
    &s.buttonNotifyEnabled,
    &s.authBlobNotifyEnabled,
    &s.persistentReconnectReady,
    &s.persistedAcceptedCentralNonceValid,
    s.persistedAcceptedCentralNonce,
    s.reconnectAuthReply24,
    sizeof(s.reconnectAuthReply24),
    &s.freshAuthBlobSent,
    &s.reconnectServiceDArmed,
    &authControl,
    updateAuthReplyIndexSlot,
    buildIndexedAuthReply24,
    printHex,
    setAuthParserSchedulerState,
    buildDonorReconnectAuthBlob,
    notifyStagedAuthBlob106,
    markAuthComplete
  };
  return refs;
}

static AuthBlobRuntimeRefs makeAuthBlobRuntimeRefs(PPSession& s)
{
  AuthBlobRuntimeRefs refs = {
    &s.authBlobStage,
    &s.authParserOutput,
    &authBlob,
    s.authBlobVisibleValue,
    sizeof(s.authBlobVisibleValue),
    setAuthParserSchedulerState
  };
  return refs;
}

static CccdSeedRefs makeCccdSeedRefs(PPSession& s)
{
  CccdSeedRefs refs = {
    &s.connHandle,
    &s.userCccdsSeeded,
    &s.serviceDNotifyEnabled,
    &serviceAChar5,
    &serviceBChar1,
    &serviceBChar2,
    &serviceCChar1,
    &serviceDChar2,
    &buttonNotifyChar,
    &authControl,
    &authBlob
  };
  return refs;
}

static ButtonRuntimeRefs makeButtonRuntimeRefs(PPSession& s)
{
  ButtonRuntimeRefs refs = {
    &s.connHandle,
    &s.pairingStage,
    &s.userCccdsSeeded,
    &s.buttonNotifyEnabled,
    &s.authControlNotifyEnabled,
    &buttonNotifyChar,
    &serviceAChar5,
    &authControl,
    &serviceBChar1,
    s.buttonState,
    s.serviceAStatus,
    &s.runtimeButtonEngineActive,
    &s.runtimeButtonActionArmed,
    &s.runtimeButtonAwaitingAck,
    &s.runtimeButtonSettlingToIdle,
    &s.runtimeButtonFamilyIndex,
    &s.runtimeButtonCurrentFamily,
    &s.runtimeButtonIdleCount,
    &s.runtimeButtonAt,
    &s.buttonPulseActive,
    &s.buttonPulseReleaseAt,
    &s.postAuthIdleButtonRemaining,
    &s.postAuthIdleButtonAt,
    &s.postAuthReady18Pending,
    &s.postAuthReady18At,
    PLUSPLUS_AUTH_CTRL_REPLY_18,
    sizeof(PLUSPLUS_AUTH_CTRL_REPLY_18),
    seedUserServiceCccds
  };
  return refs;
}

static ReconnectCompactServiceDRefs makeReconnectCompactServiceDRefs(PPSession& s)
{
  ReconnectCompactServiceDRefs refs = {
    &s.reconnectCompactServiceDMode,
    &s.reconnectServiceDArmed,
    &s.serviceBNotifyEnabled,
    &s.postServiceDHandoffActive,
    &s.postServiceDServiceBReadSeen,
    &s.postServiceDServiceBValue,
    &s.serviceDCertState,
    &s.lastOlderServiceDType2WriteValid,
    &s.freshAuthStep,
    &s.freshAuthAt,
    &s.pairingStage,
    &serviceBChar1,
    refreshServiceDLocalPayloadsFromRuntime,
    prepareSfidaReadChallenge,
    notifyServiceDStatus,
    armServiceDCertExpectation,
    buildServiceDType5PayloadFromLiveWrite,
    resetServiceDCertState,
    logServiceDCertState
  };
  return refs;
}

static SleepExportRuntimeRefs makeSleepExportRuntimeRefs(PPSession& s)
{
  SleepExportRuntimeRefs refs = {
    &s.pairingStage,
    &s.freshAuthStep,
    &authControl,
    &authBlob,
    LONG_VALUE_LEN,
    PLUSPLUS_AUTH_CTRL_REPLY_26,
    sizeof(PLUSPLUS_AUTH_CTRL_REPLY_26),
    PLUSPLUS_AUTH_BLOB_SHORT,
    sizeof(PLUSPLUS_AUTH_BLOB_SHORT),
    clearPostServiceDHandoff,
    setAuthParserSchedulerState
  };
  return refs;
}

static FirstPairRuntimeRefs makeFirstPairRuntimeRefs(PPSession& s)
{
  FirstPairRuntimeRefs refs = {
    &s.connHandle,
    &s.pairingStage,
    &s.persistentReconnectReady,
    &s.serviceCNotifyEnabled,
    &s.buttonNotifyEnabled,
    &s.authControlNotifyEnabled,
    &s.authBlobNotifyEnabled,
    &serviceCChar1,
    &buttonNotifyChar,
    &authControl,
    &authBlob,
    &s.donorAuthRuntime,
    &s.authParserOutput,
    s.serviceCState,
    s.buttonState,
    s.candidatePair,
    s.activePair,
    s.stagedPair,
    s.currentAttemptCentralNonce,
    s.persistedAcceptedCentralNonce,
    plusplusManufacturerData,
    &s.stagedPairValid,
    &s.currentAttemptCentralNonceValid,
    &s.persistedAcceptedCentralNonceValid,
    &s.freshAuthStarted,
    &s.freshAuthBlobSent,
    &s.serviceCBurstStep,
    &s.serviceCBurstAt,
    &s.connStartedAt,
    &s.freshAuthStep,
    &s.freshAuthAt,
    &s.firstPairSawReal36,
    &s.firstPairSawReal28,
    &s.serviceCTransferState,
    &s.pairEventClass,
    PLUSPLUS_FRESH_AUTH_CTRL_REPLY_24,
    sizeof(PLUSPLUS_FRESH_AUTH_CTRL_REPLY_24),
    PLUSPLUS_AUTH_CTRL_REPLY_25,
    sizeof(PLUSPLUS_AUTH_CTRL_REPLY_25),
    PLUSPLUS_AUTH_CTRL_REPLY_18,
    sizeof(PLUSPLUS_AUTH_CTRL_REPLY_18),
    PLUSPLUS_AUTH_CTRL_REPLY_18_FAIL,
    sizeof(PLUSPLUS_AUTH_CTRL_REPLY_18_FAIL),
    PLUSPLUS_AUTH_CTRL_REPLY_16,
    sizeof(PLUSPLUS_AUTH_CTRL_REPLY_16),
    PLUSPLUS_AUTH_CTRL_REPLY_21_WAIT,
    sizeof(PLUSPLUS_AUTH_CTRL_REPLY_21_WAIT),
    PLUSPLUS_AUTH_CTRL_REPLY_21_READY,
    sizeof(PLUSPLUS_AUTH_CTRL_REPLY_21_READY),
    PLUSPLUS_AUTH_CTRL_REPLY_36,
    sizeof(PLUSPLUS_AUTH_CTRL_REPLY_36),
    PLUSPLUS_AUTH_CTRL_REPLY_28,
    sizeof(PLUSPLUS_AUTH_CTRL_REPLY_28),
    PLUSPLUS_AUTH_CTRL_REPLY_26,
    sizeof(PLUSPLUS_AUTH_CTRL_REPLY_26),
    PLUSPLUS_AUTH_BLOB_SHORT,
    sizeof(PLUSPLUS_AUTH_BLOB_SHORT),
    SERVICE_C_NOTIFY_LEN,
    REAL_PLUSPLUS_ADDR_LE,
    sizeof(REAL_PLUSPLUS_ADDR_LE),
    PLUSPLUS_FACTORY_CERT_BLOCK,
    sizeof(PLUSPLUS_FACTORY_CERT_BLOCK),
    FRESH_AUTH_STEP_DELAY_MS,
    FIRSTPAIR_SERVICEC_NOTIFY1_DELAY_MS,
    FIRSTPAIR_SERVICEC_NOTIFY2_DELAY_MS,
    FIRSTPAIR_SERVICEC_NOTIFY3_DELAY_MS,
    FRESH_AUTH_18_TO_16_DELAY_MS,
    FRESH_AUTH_18_TO_REPLY_DELAY_MS,
    FRESH_AUTH_WAIT_FOR_28_WRITE_MS,
    FRESH_AUTH_STEP_DELAY_MS,
    stageCandidatePair,
    commitStagedPair,
    buildIndexedAuthReply24,
    updateAuthReplyIndexSlot,
    refreshServiceDLocalPayloadsFromRuntime,
    printHex,
    setAuthParserSchedulerState,
    setFirstPairCertState,
    scheduleFreshAuthSequence,
    logDonorAuthRuntime,
    donorLocal29CommandCodeForFamily,
    queueDonorAuthRuntimeStep,
    commitPersistedServiceDSelectorStateFromAuth36,
    commitPersistedServiceDRecord1AFromAuth28,
    clearReconnectServiceDArm,
    clearPostServiceDHandoff,
    refreshReconnectAuthReply24,
    armPostAuthRuntimeScaffolding,
    savePersistentPairState,
    refreshPairingExports,
    buildGeneratedInitialAuthBlob,
    notifyStagedAuthBlob106,
    setManufacturerState,
    pairingStageLabel,
    readyStateCode,
    deriveDonorLocalFamilyFromParserBytes,
    resetAuthParserOutput,
    processReconnectCompactCompletion,
    scheduleServiceCFirstPairBurst
  };
  return refs;
}

static FirstPairOlderServiceDRefs makeFirstPairOlderServiceDRefs(PPSession& s)
{
  FirstPairOlderServiceDRefs refs = {
    &s.serviceDCertState,
    &s.lastOlderServiceDType2WriteValid,
    s.lastOlderServiceDType2Write,
    sizeof(s.lastOlderServiceDType2Write),
    &s.freshAuthStep,
    &s.freshAuthAt,
    &s.reconnectServiceDArmed,
    &s.reconnectCompactServiceDMode,
    refreshServiceDLocalPayloadsFromRuntime,
    prepareOlderServiceDReadStage,
    notifyServiceDStatus,
    armServiceDCertExpectation,
    incrementServiceDRuntimeTailCounter,
    logServiceDCertState
  };
  return refs;
}

static void seedUserServiceCccds(const char* reason)
{
  ppSeedUserServiceCccds(makeCccdSeedRefs(curSession()), reason);
}

static void logServiceDCertState(const char* reason)
{
  ppLogServiceDCertState(makePairHelperRefs(curSession()), reason);
}

static void setAuthParserSchedulerState(uint8_t schedulerState,
                                        uint8_t outputClass,
                                        uint16_t outputLen,
                                        uint16_t outputOffset,
                                        uint8_t resultCode,
                                        uint8_t continuationKind,
                                        uint8_t lastOpcode,
                                        bool outputPending,
                                        bool streamPending,
                                        bool notifyInFlight)
{
  ppSetAuthParserSchedulerState(makePairHelperRefs(curSession()),
                                schedulerState,
                                outputClass,
                                outputLen,
                                outputOffset,
                                resultCode,
                                continuationKind,
                                lastOpcode,
                                outputPending,
                                streamPending,
                                notifyInFlight);
}

static void rebuildServiceDRuntimeBlock()
{
  ppRebuildServiceDRuntimeBlock(makePairHelperRefs(curSession()));
}

static void incrementServiceDRuntimeTailCounter()
{
  ppIncrementServiceDRuntimeTailCounter(makePairHelperRefs(curSession()));
}

static void commitPersistedServiceDSelectorStateFromAuth36()
{
  ppCommitPersistedServiceDSelectorStateFromAuth36(makePairHelperRefs(curSession()));
}

static void commitPersistedServiceDRecord1AFromAuth28()
{
  ppCommitPersistedServiceDRecord1AFromAuth28(makePairHelperRefs(curSession()));
}

static void refreshServiceDLocalPayloadsFromRuntime()
{
  ppRefreshServiceDLocalPayloadsFromRuntime(makeServiceDLocalRefs(curSession()));
}

static void resetPairingFromApp(const char* reason)
{
  ppResetPairingFromApp(makeAppLifecycleRefs(curSession()), reason);
}

static void resetPairingFromPhysicalButton(const char* reason)
{
  ppResetPairingFromApp(makeAppLifecycleRefs(curSession()), reason);
}

// Sleep timer: device-level. The lifecycle code checks *activeConnHandle;
// bind a mirror that is valid while ANY session is connected so the idle
// sleep only fires when every phone is gone.
extern uint16_t gAnyConnHandleMirror;

static AppLifecycleRefs makeSleepLifecycleRefs()
{
  AppLifecycleRefs refs = makeAppLifecycleRefs(gSessions[0]);
  refs.activeConnHandle = &gAnyConnHandleMirror;
  return refs;
}

static void armDisconnectedSleepTimer(const char* reason)
{
  ppArmDisconnectedSleepTimer(makeSleepLifecycleRefs(), reason);
}

static void processDisconnectedSleepIfNeeded()
{
  ppProcessDisconnectedSleepIfNeeded(makeSleepLifecycleRefs());
}

static void processPhysicalResetButtonIfNeeded()
{
  ppProcessPhysicalResetButtonIfNeeded(makeAppLifecycleRefs(gSessions[0]));
}

static void buildServiceDType5PayloadFromLiveWrite(const uint8_t* write36)
{
  ppBuildServiceDType5PayloadFromLiveWrite(makeAuthMaterialRefs(curSession()), write36);
}

static void armServiceDCertExpectation(uint8_t state, uint8_t expectedType, uint16_t expectedLen, const char* reason)
{
  PPSession& s = curSession();
  s.serviceDCertState.active = true;
  s.serviceDCertState.laneStep = state;
  s.serviceDCertState.expectedWriteType = expectedType;
  s.serviceDCertState.expectedWriteLen = expectedLen;
  logServiceDCertState(reason);
}

#include "pp_storage_refs.h"

static bool handleFirstPairAuthWriteDispatch(const uint8_t* data, uint16_t len)
{
  return ppFirstPairFlowHandleAuthWrite(makeFirstPairRuntimeRefs(curSession()), data, len);
}

static void handleReconnectAuthWrite24Dispatch(uint8_t selector, const char* reason)
{
  ppReconnectFlowHandleAuthWrite24(makeReconnectRuntimeRefs(curSession()), selector, reason);
}

static void handleReconnectAuthWrite25Dispatch(const uint8_t* reply,
                                               uint16_t replyLen,
                                               const char* reason)
{
  ppReconnectFlowHandleAuthWrite25(makeReconnectRuntimeRefs(curSession()), reply, replyLen, reason);
}

static SleepExportWriteResult handleAuthenticatedSleepWriteDispatch(const uint8_t* data, uint16_t len)
{
  return ppHandleAuthenticatedSleepWrite(makeSleepExportRuntimeRefs(curSession()), data, len);
}

static void prepareReconnectSleepDataOnAuthenticatedDisconnect()
{
  ppPrepareReconnectSleepDataForAuthenticatedDisconnect();
}

static void resetReconnectSleepDataState()
{
  ppResetReconnectSleepDataState();
}

static BleDispatchRefs makeBleDispatchRefs(PPSession& s)
{
  BleDispatchRefs refs = {
    &s.pairingStage,
    &s.buttonNotifyEnabled,
    &s.serviceCNotifyEnabled,
    &s.serviceBNotifyEnabled,
    &s.serviceDNotifyEnabled,
    &s.authControlNotifyEnabled,
    &s.authBlobNotifyEnabled,
    &s.freshAuthStarted,
    &s.freshAuthStep,
    &s.freshAuthAt,
    &s.sfidaCertStage,
    &s.reconnectServiceDArmed,
    &s.persistentReconnectReady,
    &s.freshAuthBlobSent,
    &s.postServiceDHandoffActive,
    &s.postServiceDServiceBReadSeen,
    &s.postServiceDServiceBValue,
    &s.serviceCTransferState,
    &s.pairEventClass,
    s.serviceCState,
    sizeof(s.serviceCState),
    s.serviceDStatus,
    sizeof(s.serviceDStatus),
    s.configState,
    sizeof(s.configState),
    s.freshAuthInitialChallenge,
    sizeof(s.freshAuthInitialChallenge),
    s.authBlobVisibleValue,
    sizeof(s.authBlobVisibleValue),
    &s.authBlobStage,
    &s.authParserOutput,
    &s.serviceDCertState,
    &batteryLevelChar,
    &serviceAChar1,
    &serviceAChar2,
    &serviceAChar3,
    &serviceAChar4,
    &serviceAChar5,
    &serviceBChar1,
    &serviceBChar2,
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
    PLUSPLUS_AUTH_CTRL_REPLY_25,
    sizeof(PLUSPLUS_AUTH_CTRL_REPLY_25),
    CONFIG_RESET_PAIRED_STATE_MAGIC,
    SHORT_VALUE_LEN,
    MEDIUM_VALUE_LEN,
    FRESH_AUTH_WAIT_FOR_28_WRITE_MS,
    pairingStageLabel,
    printHex,
    refreshPairingExports,
    stageCandidatePair,
    commitStagedPair,
    scheduleServiceCFirstPairBurst,
    beginServiceDOlderFlow,
    beginServiceDCertFlow,
    handleServiceDCertWrite,
    stopPostAuthRuntimeScaffolding,
    maybeTriggerButtonFromLedPayload,
    resetPairingFromApp,
    plusplusGenerateReconnectResponse,
    handleFirstPairAuthWriteDispatch,
    handleReconnectAuthWrite24Dispatch,
    handleReconnectAuthWrite25Dispatch,
    handleAuthenticatedSleepWriteDispatch,
    setAuthParserSchedulerState
  };
  return refs;
}

static void prepareOlderServiceDReadStage(uint8_t stage)
{
  ppPrepareOlderServiceDReadStage(makeAuthMaterialRefs(curSession()), stage);
}

static void prepareSfidaReadChallenge(uint8_t stage)
{
  ppPrepareSfidaReadChallenge(makeAuthMaterialRefs(curSession()), stage);
}

static void buildGeneratedInitialAuthBlob()
{
  ppBuildGeneratedInitialAuthBlob(makeAuthMaterialRefs(curSession()));
}

static void buildDonorReconnectAuthBlob()
{
  ppBuildDonorReconnectAuthBlob(makeAuthMaterialRefs(curSession()));
}

static bool plusplusGenerateReconnectResponse(const uint8_t* challenge32, uint8_t* response16)
{
  return ppGenerateReconnectResponse(makeAuthMaterialRefs(curSession()), challenge32, response16);
}

static uint8_t readyStateCode()
{
  return ppReadyStateCode(curSession().pairingStage);
}

static const char* pairingStageLabel(PairingStage stage)
{
  return ppPairingStageLabel(stage);
}

static void scheduleServiceCFirstPairBurst()
{
  ppFirstPairFlowScheduleServiceCBurst(makeFirstPairRuntimeRefs(curSession()));
}

static void processServiceCFirstPairBurstOne(PPSession& s)
{
  ppFirstPairFlowProcessServiceCBurst(makeFirstPairRuntimeRefs(s));
}

static void processPostAuthIdleButtonNotifyOne(PPSession& s)
{
  ppProcessPostAuthIdleButtonNotify(makeButtonRuntimeRefs(s),
                                    POST_AUTH_IDLE_BUTTON_PERIOD_MS);
}

static void processPostAuthReady18One(PPSession& s)
{
  ppProcessPostAuthReady18(makeButtonRuntimeRefs(s));
}

static void armPostAuthRuntimeScaffolding()
{
  ppArmPostAuthRuntimeScaffolding(makeButtonRuntimeRefs(curSession()),
                                  POST_AUTH_IDLE_BUTTON_COUNT,
                                  POST_AUTH_IDLE_BUTTON_FIRST_DELAY_MS,
                                  POST_AUTH_READY_18_DELAY_MS);
}

static bool processReconnectCompactCompletion()
{
  return ppReconnectFlowProcessCompactCompletion(makeReconnectCompactServiceDRefs(curSession()));
}

static void scheduleFreshAuthSequence(uint8_t firstStep, const char* reason)
{
  ppFirstPairFlowScheduleFreshAuthSequence(makeFirstPairRuntimeRefs(curSession()), firstStep, reason);
}

static void clearAuthBlobStage()
{
  ppClearAuthBlobStage(makeAuthBlobRuntimeRefs(curSession()));
}

static void refreshReconnectAuthReply24(const char* reason)
{
  ppReconnectFlowRefreshAuthReply24(makeReconnectRuntimeRefs(curSession()), reason);
}

static void setFirstPairCertState(uint8_t nextState, const char* reason)
{
  ppSetFirstPairCertState(makePairRuntimeSupportRefs(curSession()), nextState, reason);
}

static void resetDonorAuthRuntime()
{
  ppResetDonorAuthRuntime(makePairRuntimeSupportRefs(curSession()));
}

static void resetAuthParserOutput()
{
  ppResetAuthParserOutput(makePairRuntimeSupportRefs(curSession()));
}

static void logDonorAuthRuntime(const char* reason)
{
  ppLogDonorAuthRuntime(makePairRuntimeSupportRefs(curSession()), reason);
}

static uint8_t donorLocal29CommandCodeForFamily(uint8_t family)
{
  return ppDonorLocal29CommandCodeForFamily(family);
}

static uint8_t deriveDonorLocalFamilyFromParserBytes()
{
  return ppDeriveDonorLocalFamilyFromParserBytes(makePairRuntimeSupportRefs(curSession()));
}

static void queueDonorAuthRuntimeStep(uint8_t nextStep, uint32_t delayMs, const char* reason)
{
  ppQueueDonorAuthRuntimeStep(makePairRuntimeSupportRefs(curSession()), nextStep, delayMs, reason);
}

static void seedAuthReplyIndexTable()
{
  ppSeedAuthReplyIndexTable(makePairRuntimeSupportRefs(curSession()), PLUSPLUS_AUTH_CTRL_REPLY_2401);
}

static void updateAuthReplyIndexSlot(uint8_t index, const uint8_t* value3, const char* reason)
{
  ppUpdateAuthReplyIndexSlot(makePairRuntimeSupportRefs(curSession()), index, value3, reason);
}

static void buildIndexedAuthReply24(uint8_t index, uint8_t* out5, const char* reason)
{
  ppBuildIndexedAuthReply24(makePairRuntimeSupportRefs(curSession()), index, out5, reason);
}

static void stageVisibleAuthBlob(const uint8_t* payload, uint16_t payloadLen, uint8_t stageClass, uint8_t lastOpcode)
{
  ppStageVisibleAuthBlob(makeAuthBlobRuntimeRefs(curSession()), payload, payloadLen, stageClass, lastOpcode);
}

static void stageVisibleAuthBlob106()
{
  ppStageVisibleAuthBlob106(makeAuthBlobRuntimeRefs(curSession()));
}

static void notifyStagedAuthBlob106(const char* reason)
{
  ppNotifyStagedAuthBlob106(makeAuthBlobRuntimeRefs(curSession()), reason);
}

static AuthMaterialRefs makeAuthMaterialRefs(PPSession& s)
{
  AuthMaterialRefs refs = {
    s.freshAuthInitialChallenge,
    s.freshAuthSessionKey,
    s.liveCertKey,
    &s.serviceDCertState,
    &serviceDChar3,
    s.serviceDReadBlob,
    &s.serviceDReadPendingLen,
    s.authBlobValue,
    s.authBlobVisibleValue,
    &s.authBlobStage,
    s.serviceDType3ReadPayload,
    s.serviceDType5ReadPayload,
    s.persistedServiceDSelector4,
    &s.persistedServiceDSelector4Valid,
    s.persistedServiceDRecord6,
    &s.persistedServiceDRecord6Valid,
    s.lastOlderServiceDType2Write,
    &s.lastOlderServiceDType2WriteValid,
    &s.donorAuthRuntime,
    refreshServiceDLocalPayloadsFromRuntime,
    stageVisibleAuthBlob106,
    stageVisibleAuthBlob,
    printHex
  };
  return refs;
}

static PairRuntimeSupportRefs makePairRuntimeSupportRefs(PPSession& s)
{
  PairRuntimeSupportRefs refs = {
    &s.firstPairCertState,
    &s.donorAuthRuntime,
    &s.authParserOutput,
    s.authReplyIndexTable,
    sizeof(s.authReplyIndexTable) / sizeof(s.authReplyIndexTable[0]),
    &s.freshAuthStep,
    &s.freshAuthAt,
    printHex,
    setAuthParserSchedulerState
  };
  return refs;
}

static PairHelperRefs makePairHelperRefs(PPSession& s)
{
  PairHelperRefs refs = {
    &s.serviceDCertState,
    &s.authParserOutput,
    s.serviceDRuntimeBlock,
    sizeof(s.serviceDRuntimeBlock),
    s.persistedServiceDSelector4,
    &s.persistedServiceDSelector4Valid,
    &s.persistedServiceDSelector5,
    &s.persistedServiceDSelector5Valid,
    s.persistedServiceDRecord6,
    &s.persistedServiceDRecord6Valid,
    &s.persistedServiceDRecord1A,
    &s.persistedServiceDRecord1AValid,
    FIRMWARE_RECORD4_DEFAULT,
    sizeof(s.persistedServiceDSelector4),
    sizeof(s.persistedServiceDRecord6),
    REAL_PLUSPLUS_ADDR_LE,
    sizeof(REAL_PLUSPLUS_ADDR_LE),
    &s.pairingStage,
    printHex
  };
  return refs;
}

static AuthBootstrapRefs makeAuthBootstrapRefs(PPSession& s)
{
  AuthBootstrapRefs refs = {
    s.liveCertKey,
    sizeof(s.liveCertKey),
    PLUSPLUS_FACTORY_SELECTOR3,
    seedAuthReplyIndexTable,
    clearAuthBlobStage,
    buildGeneratedInitialAuthBlob
  };
  return refs;
}

static void notifyServiceDStatus(const uint8_t* status, const char* label)
{
  ppNotifyServiceDStatus(makeServiceDLocalRefs(curSession()), status, label);
}

static void clearPostServiceDHandoff()
{
  ppClearPostServiceDHandoff(makeServiceDLocalRefs(curSession()));
}

static void beginServiceDCertFlow()
{
  ppReconnectFlowBeginCompactServiceD(makeReconnectCompactServiceDRefs(curSession()),
                                      PLUSPLUS_SERVICE_D_STATUS3_START,
                                      FRESH_AUTH_WAIT_FOR_28_WRITE_MS);
}

static void beginServiceDOlderFlow()
{
  ppFirstPairFlowBeginOlderServiceD(makeFirstPairOlderServiceDRefs(curSession()),
                                    PLUSPLUS_SERVICE_D_STATUS0);
}

static bool handleServiceDCertWrite(const uint8_t* data, uint16_t len)
{
  return ppHandleServiceDCertWrite(makeServiceDLocalRefs(curSession()), data, len);
}

static void processFreshAuthSequenceOne(PPSession& s)
{
  ppFirstPairFlowProcessFreshAuthSequence(makeFirstPairRuntimeRefs(s));
}

static void stageCandidatePair(const char* reason)
{
  ppFirstPairFlowStageCandidatePair(makeFirstPairRuntimeRefs(curSession()), reason);
}

static void commitStagedPair(const char* reason)
{
  ppFirstPairFlowCommitStagedPair(makeFirstPairRuntimeRefs(curSession()), reason);
}

static void markAuthComplete()
{
  ppFirstPairFlowMarkAuthComplete(makeFirstPairRuntimeRefs(curSession()));
}

static void printHex(const uint8_t* data, uint16_t len)
{
  ppPrintHex(data, len);
}

static void print_build_tag(const char* context)
{
  ppPrintBuildTag(BUILD_TAG, context);
}

static void seedLongReadBlobAll()
{
  for (int i = 0; i < PP_MAX_CONNECTIONS; ++i) {
    ppSeedLongReadBlob(gSessions[i].serviceDReadBlob, LONG_VALUE_LEN, VERSION_STRING);
  }
}

static void seedLongReadBlob()
{
  seedLongReadBlobAll();
}

static void seedAuthBlobAll()
{
  for (int i = 0; i < PP_MAX_CONNECTIONS; ++i) {
    ppSetCurrentSession(&gSessions[i]);
    ppSeedAuthBlob(makeAuthBootstrapRefs(gSessions[i]));
  }
  ppSetCurrentSession(nullptr);
}

static void seedAuthBlob()
{
  seedAuthBlobAll();
}

static void resetRuntimeButtonEngineOne(PPSession& s)
{
  ppResetRuntimeButtonEngine(makeButtonRuntimeRefs(s));
}

static void resetRuntimeButtonEngine()
{
  resetRuntimeButtonEngineOne(curSession());
}

static void stopPostAuthRuntimeScaffolding()
{
  ppStopPostAuthRuntimeScaffolding(makeButtonRuntimeRefs(curSession()));
}

static void clearButtonPulseIfNeededOne(PPSession& s)
{
  ppClearButtonPulseIfNeeded(makeButtonRuntimeRefs(s));
}

static void processRuntimeButtonEngineIfNeededOne(PPSession& s)
{
  ppProcessRuntimeButtonEngineIfNeeded(makeButtonRuntimeRefs(s),
                                       RUNTIME_BUTTON_IDLE_COUNT_BEFORE_ACTION,
                                       RUNTIME_BUTTON_IDLE_PERIOD_MS);
}

static void maybeTriggerButtonFromLedPayload(const uint8_t* data, uint16_t len)
{
  // ESP32 port: once authenticated, the pgpemu-derived catch engine owns the
  // LED lane. Before that, keep the original pairing-time pulse behaviour.
  if (curSession().pairingStage == PAIR_STAGE_AUTHENTICATED) {
    catch_handle_led_write(data, len);
    return;
  }
  ppMaybeTriggerButtonFromLedPayload(makeButtonRuntimeRefs(curSession()),
                                     data,
                                     len,
                                     RUNTIME_BUTTON_IDLE_COUNT_BEFORE_ACTION,
                                     RUNTIME_BUTTON_PRESTATE_SEND_COUNT,
                                     RUNTIME_BUTTON_PRESTATE_SEND_GAP_MS,
                                     RUNTIME_BUTTON_ACK_TIMEOUT_MS,
                                     RUNTIME_BUTTON_FIRST_IDLE_DELAY_MS,
                                     RUNTIME_BUTTON_IDLE_PERIOD_MS);
}

// ---- Multi-slot loop adapters (device level, called once per loop) ----

static void processAuthenticatedSleepStreamAll()
{
  for (int i = 0; i < PP_MAX_CONNECTIONS; ++i) {
    if (!gSessions[i].used) continue;
    ppSetCurrentSession(&gSessions[i]);
    ppProcessAuthenticatedSleepStream(makeSleepExportRuntimeRefs(gSessions[i]));
  }
  ppSetCurrentSession(nullptr);
}

static void clearButtonPulseIfNeededAll()
{
  for (int i = 0; i < PP_MAX_CONNECTIONS; ++i) {
    if (!gSessions[i].used) continue;
    ppSetCurrentSession(&gSessions[i]);
    clearButtonPulseIfNeededOne(gSessions[i]);
  }
  ppSetCurrentSession(nullptr);
}

static void processRuntimeButtonEngineIfNeededAll()
{
  for (int i = 0; i < PP_MAX_CONNECTIONS; ++i) {
    if (!gSessions[i].used) continue;
    ppSetCurrentSession(&gSessions[i]);
    processRuntimeButtonEngineIfNeededOne(gSessions[i]);
  }
  ppSetCurrentSession(nullptr);
}

static void processPostAuthIdleButtonNotifyAll()
{
  for (int i = 0; i < PP_MAX_CONNECTIONS; ++i) {
    if (!gSessions[i].used) continue;
    ppSetCurrentSession(&gSessions[i]);
    processPostAuthIdleButtonNotifyOne(gSessions[i]);
  }
  ppSetCurrentSession(nullptr);
}

static void processPostAuthReady18All()
{
  for (int i = 0; i < PP_MAX_CONNECTIONS; ++i) {
    if (!gSessions[i].used) continue;
    ppSetCurrentSession(&gSessions[i]);
    processPostAuthReady18One(gSessions[i]);
  }
  ppSetCurrentSession(nullptr);
}

static void processServiceCFirstPairBurstAll()
{
  for (int i = 0; i < PP_MAX_CONNECTIONS; ++i) {
    if (!gSessions[i].used) continue;
    ppSetCurrentSession(&gSessions[i]);
    processServiceCFirstPairBurstOne(gSessions[i]);
  }
  ppSetCurrentSession(nullptr);
}

static void processFreshAuthSequenceAll()
{
  for (int i = 0; i < PP_MAX_CONNECTIONS; ++i) {
    if (!gSessions[i].used) continue;
    ppSetCurrentSession(&gSessions[i]);
    processFreshAuthSequenceOne(gSessions[i]);
  }
  ppSetCurrentSession(nullptr);
}

static void resetVolatilePairSessionStateDispatch()
{
  ppResetVolatilePairSessionState(makePairStateRuntimeRefs(curSession()));
}
