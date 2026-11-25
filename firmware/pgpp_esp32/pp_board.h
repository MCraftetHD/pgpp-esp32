#pragma once
// ESP32-DevKitC (classic ESP32) board layer: two buttons, the onboard LED,
// a serial console, session counters and deep sleep. Replaces the M5Stack
// Stick S3 board code of the upstream port.
//
// Default pins (all configurable in pp_board_config.h):
//   BOOT button = GPIO0  (active low, pull-up; also the deep-sleep wake pin)
//   D2 button   = GPIO2  (see note below)
//   Onboard LED = GPIO2  (the DevKitC's blue LED)
//
// GPIO2 note: on most DevKitC-compatible boards the blue LED hangs on GPIO2,
// and some boards wire their "D2" button there too. Sharing one pin between
// LED output and button input works when the button pulls the pin towards the
// opposite level of the LED-on state (button to 3V3 + pulldown input). If
// your button pulls to GND instead, set PP_BTN_B_ACTIVE_LOW to 1 and wire an
// external LED on another pin (PP_LED_PIN) if the blink looks wrong.
//
// Buttons:
//   BOOT click      -> manual catch/spin press on every connected phone
//   BOOT hold 5 s   -> wipe pairing records
//   D2 click        -> cycle catch mode: off -> catch all -> catch new -> spin only
//   D2 hold 3 s     -> toggle auto spin separately
//   any button      -> wakes from deep sleep (BOOT; or re-plug for D2)

#include <Arduino.h>
#include <stdint.h>

#include "pp_types.h"
#include "pp_catch_engine.h"

#include "pp_board_config.h"

enum PPCatchMode : uint8_t {
  PP_CATCH_OFF = 0,
  PP_CATCH_ALL = 1,
  PP_CATCH_NEW_ONLY = 2,
};

struct BoardRefs {
  uint16_t* activeConnHandle;
  PairingStage* pairingStage;
  bool* persistentReconnectReady;
  const char* buildTag;
  const uint8_t* donorAddrLe;
  void (*manualButtonPress)();
  void (*startAdvertising)();
  void (*stopAdvertising)();
  void (*resetPairing)(const char* reason);
  const char* (*pairingStageLabel)(PairingStage stage);
};

void ppBoardInit();
void ppBoardTick(const BoardRefs& refs);

// Settings read by the catch engine and the advertising wrapper.
bool ppBoardAutoSpin();
uint8_t ppBoardCatchMode();

// Press speed: delay between the app's prompt and the emulated button press.
// Normal = 1.0-2.5 s (pgpemu's human-like default), Fast = 0.3-0.8 s. The
// game still allows one throw per encounter and paces encounters itself.
enum PPPressSpeed : uint8_t { PP_SPEED_NORMAL = 0, PP_SPEED_FAST = 1 };
uint8_t ppBoardPressSpeed();
bool ppBoardAdvertiseEnabled();

// Events from the catch engine: updates the LED, counters, console.
void ppBoardOnEvent(PPLedEvent evt, const char* label);
void ppBoardSetNote(const char* text);
// Link dropped: remembers the BLE reason for the console.
void ppBoardOnDisconnect(uint8_t reason);
void ppBoardEnterDeepSleep();
// Manual wake request: BOOT button pressed while asleep never reaches here,
// but the board can ask for advertising restart after wake.
void ppBoardOnWake();

// Serial console (runs inside ppBoardTick).
void ppBoardSetConnectedCount(int n);
int ppBoardGetConnectedCount();
