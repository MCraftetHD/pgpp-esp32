#pragma once

// Multi-phone wiring: forward declarations for the sketch-local helpers the
// composition headers reference, plus the device-global state declarations.

#include "pp_session.h"
#include "pp_record_store.h"
#include "pp_board.h"

// ---- Device-global state (defined in the sketch) ----
extern bool gServicesReady;
extern bool gPersistentStoreReady;
extern bool gDisconnectedSleepArmed;
extern uint32_t gDisconnectedSleepAt;
extern bool gPhysicalResetButtonActive;
extern bool gPhysicalResetButtonTriggered;
extern uint32_t gPhysicalResetButtonPressedAt;
extern const char DEVICE_NAME[];
extern const char BUILD_TAG[];

// Timing constants (same values as the upstream port).
static constexpr uint32_t FIRSTPAIR_SERVICEC_NOTIFY1_DELAY_MS = 1200;
static constexpr uint32_t FIRSTPAIR_SERVICEC_NOTIFY2_DELAY_MS = 1650;
static constexpr uint32_t FIRSTPAIR_SERVICEC_NOTIFY3_DELAY_MS = 1750;
static constexpr uint32_t FRESH_AUTH_STEP_DELAY_MS = 90;
static constexpr uint32_t FRESH_AUTH_18_TO_REPLY_DELAY_MS = 80;
static constexpr uint32_t FRESH_AUTH_18_TO_16_DELAY_MS = 25;
static constexpr uint32_t FRESH_AUTH_WAIT_FOR_28_WRITE_MS = 650;
static constexpr uint32_t POST_AUTH_IDLE_BUTTON_FIRST_DELAY_MS = 1320;
static constexpr uint32_t POST_AUTH_IDLE_BUTTON_PERIOD_MS = 500;
static constexpr uint8_t POST_AUTH_IDLE_BUTTON_COUNT = 48;
static constexpr uint32_t RUNTIME_BUTTON_IDLE_PERIOD_MS = 500;
static constexpr uint32_t RUNTIME_BUTTON_FIRST_IDLE_DELAY_MS = 500;
static constexpr uint32_t RUNTIME_BUTTON_ACK_TIMEOUT_MS = 350;
static constexpr uint8_t RUNTIME_BUTTON_IDLE_COUNT_BEFORE_ACTION = 10;
static constexpr uint8_t RUNTIME_BUTTON_PRESTATE_SEND_COUNT = 3;
static constexpr uint32_t RUNTIME_BUTTON_PRESTATE_SEND_GAP_MS = 15;
static constexpr uint32_t POST_AUTH_READY_18_DELAY_MS = 20300;
static constexpr uint32_t PHYSICAL_RESET_BUTTON_RAW_PIN = PP_BTN_A_PIN;  // BOOT button, hold 5 s
static constexpr uint32_t PHYSICAL_RESET_BUTTON_HOLD_MS = 5000;
static constexpr uint32_t DISCONNECTED_SLEEP_TIMEOUT_MS = 60UL * 1000UL * PP_SLEEP_AFTER_DISCONNECTED_MIN;
static constexpr uint16_t REAL_PLUSPLUS_ADV_INTERVAL_UNITS = 160;
static constexpr uint16_t REAL_PLUSPLUS_CONN_INTERVAL_MIN_UNITS = 6;
static constexpr uint16_t REAL_PLUSPLUS_CONN_INTERVAL_MAX_UNITS = 24;
static constexpr uint16_t REAL_PLUSPLUS_CONN_TIMEOUT_UNITS = 500;

// Current-session helper for the wrapper functions below.
static inline PPSession& curSession()
{
  PPSession* s = ppCurrentSession();
  return s != nullptr ? *s : gSessions[0];
}

// ---- Sketch helper forward declarations ----
static void board_init();
static void board_tick();
static void catch_tick();
// Boot-level (device-global) wrappers
static void bootResetUnpairedAll();
static bool loadPersistentPairStateBoot();
static void restorePairedStateFromPersistenceBoot();
static void resetPairingFromPhysicalButtonAll(const char* reason);
static void catch_handle_led_write(const uint8_t* data, uint16_t len);
static void catch_send_button(uint8_t b0, uint8_t b1);
static void catch_on_event(PPLedEvent evt, const char* label);
static void manual_button_press();
static void stop_advertising();
static void stageCandidatePair(const char* reason);
static void commitStagedPair(const char* reason);
static void markAuthComplete();
static bool savePersistentPairState(const char* reason);
static bool loadPersistentPairState();
static void clearPersistentPairState(const char* reason);
static void restorePairedStateFromPersistence();
static void resetPairingFromApp(const char* reason);
static void resetPairingFromPhysicalButton(const char* reason);
static void processPhysicalResetButtonIfNeeded();
static void start_advertising();
static void setManufacturerState(uint8_t state);
static void refreshPairingExports(bool notify);
static void processAuthenticatedSleepStream();
static void prepareReconnectSleepDataOnAuthenticatedDisconnect();
static void resetReconnectSleepDataState();
static void printHex(const uint8_t* data, uint16_t len);
static bool beginPersistentStore();
static void setup_services();
static void configure_identity();
static void configure_gap_preferences();
static void connect_callback(uint16_t conn_handle);
static void disconnect_callback(uint16_t conn_handle, uint8_t reason);
static void clearButtonPulseIfNeeded();
static void processPostAuthIdleButtonNotify();
static void processPostAuthReady18();
static void processDisconnectedSleepIfNeeded();
static void processFreshAuthSequence();
static void processServiceCFirstPairBurst();
static void processRuntimeButtonEngineIfNeeded();
static void processAuthenticatedSleepStreamAll();
static void clearButtonPulseIfNeededAll();
static void processRuntimeButtonEngineIfNeededAll();
static void processPostAuthIdleButtonNotifyAll();
static void processPostAuthReady18All();
static void processServiceCFirstPairBurstAll();
static void processFreshAuthSequenceAll();
static PairStateRuntimeRefs makePairStateRuntimeRefs(PPSession& s);
static FirstPairOlderServiceDRefs makeFirstPairOlderServiceDRefs(PPSession& s);
static ReconnectCompactServiceDRefs makeReconnectCompactServiceDRefs(PPSession& s);
static PersistedPairRuntimeRefs makePersistedPairRuntimeRefs(PPSession& s);
static BleDispatchRefs makeBleDispatchRefs(PPSession& s);
static ServiceDLocalRefs makeServiceDLocalRefs(PPSession& s);
static ReconnectRuntimeRefs makeReconnectRuntimeRefs(PPSession& s);
static AuthBlobRuntimeRefs makeAuthBlobRuntimeRefs(PPSession& s);
static AuthMaterialRefs makeAuthMaterialRefs(PPSession& s);
static CccdSeedRefs makeCccdSeedRefs(PPSession& s);
static ButtonRuntimeRefs makeButtonRuntimeRefs(PPSession& s);
static SleepExportRuntimeRefs makeSleepExportRuntimeRefs(PPSession& s);
static FirstPairRuntimeRefs makeFirstPairRuntimeRefs(PPSession& s);
static PairRuntimeSupportRefs makePairRuntimeSupportRefs(PPSession& s);
static PairHelperRefs makePairHelperRefs(PPSession& s);
static AuthBootstrapRefs makeAuthBootstrapRefs(PPSession& s);
static void rebuildServiceDRuntimeBlock();
static void logServiceDCertState(const char* reason);
static void buildGeneratedInitialAuthBlob();
static void buildDonorReconnectAuthBlob();
static void buildIndexedAuthReply24(uint8_t index, uint8_t* out5, const char* reason);
static void buildServiceDType5PayloadFromLiveWrite(const uint8_t* write36);
static void cccd_write_callback(uint16_t conn_handle, BLECharacteristic* chr, uint16_t value);
static void clearAuthBlobStage();
static void clearPostServiceDHandoff();
static void clearReconnectServiceDArm();
static void setFirstPairCertState(uint8_t nextState, const char* reason);
static void commitPersistedServiceDSelectorStateFromAuth36();
static void commitPersistedServiceDRecord1AFromAuth28();
static uint8_t deriveDonorLocalFamilyFromParserBytes();
static void incrementServiceDRuntimeTailCounter();
static bool handleFirstPairAuthWriteDispatch(const uint8_t* data, uint16_t len);
static void handleReconnectAuthWrite24Dispatch(uint8_t selector, const char* reason);
static void handleReconnectAuthWrite25Dispatch(const uint8_t* reply,
                                               uint16_t replyLen,
                                               const char* reason);
static SleepExportWriteResult handleAuthenticatedSleepWriteDispatch(const uint8_t* data, uint16_t len);
static bool handleOlderServiceDFlowWriteDispatch(const uint8_t* data, uint16_t len);
static bool handleReconnectServiceDFlowWriteDispatch(const uint8_t* data, uint16_t len);
static void refreshServiceDLocalPayloadsFromRuntime();
static void beginServiceDCertFlow();
static void beginServiceDOlderFlow();
static bool handleServiceDCertWrite(const uint8_t* data, uint16_t len);
static void logDonorAuthRuntime(const char* reason);
static void notifyServiceDStatus(const uint8_t* status, const char* label);
static void notifyStagedAuthBlob106(const char* reason);
static void prepareOlderServiceDReadStage(uint8_t stage);
static void prepareSfidaReadChallenge(uint8_t stage);
static void print_build_tag(const char* context);
static void queueDonorAuthRuntimeStep(uint8_t nextStep, uint32_t delayMs, const char* reason);
static uint8_t readyStateCode();
static void read_authorize_callback(uint16_t conn_handle, BLECharacteristic* chr, ble_gatts_evt_read_t* request);
static void refreshReconnectAuthReply24(const char* reason);
static void resetAuthParserOutput();
static void resetDonorAuthRuntime();
static void resetRuntimeButtonEngine();
static void resetServiceDCertState();
static void resetUnpairedState();
static void scheduleFreshAuthSequence(uint8_t firstStep, const char* reason);
static void scheduleServiceCFirstPairBurst();
static void seedAuthBlob();
static void seedAuthReplyIndexTable();
static void seedLongReadBlob();
static void seedUserServiceCccds(const char* reason);
static void service_write_callback(uint16_t conn_handle, BLECharacteristic* chr, uint8_t* data, uint16_t len);
static void setAuthParserSchedulerState(uint8_t schedulerState,
                                        uint8_t outputClass,
                                        uint16_t outputLen,
                                        uint16_t outputOffset,
                                        uint8_t resultCode,
                                        uint8_t continuationKind,
                                        uint8_t lastOpcode,
                                        bool outputPending,
                                        bool streamPending,
                                        bool notifyInFlight);
static void stopPostAuthRuntimeScaffolding();
static void updateAuthReplyIndexSlot(uint8_t index, const uint8_t* value3, const char* reason);
static void armDisconnectedSleepTimer(const char* reason);
static void armServiceDCertExpectation(uint8_t state, uint8_t expectedType, uint16_t expectedLen, const char* reason);
static void armPostAuthRuntimeScaffolding();
static uint8_t donorLocal29CommandCodeForFamily(uint8_t family);
static bool plusplusGenerateReconnectResponse(const uint8_t* challenge32, uint8_t* response16);
static const char* pairingStageLabel(PairingStage stage);
static bool processReconnectCompactCompletion();
static void maybeTriggerButtonFromLedPayload(const uint8_t* data, uint16_t len);
static void resetVolatilePairSessionStateDispatch();
static void seedVisibleAuthBlobStageHelpers();

#include "pp_ble_refs.h"

#include "pp_flow_refs.h"
