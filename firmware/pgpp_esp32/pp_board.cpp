#include <Arduino.h>
#include <Preferences.h>
#include <esp_sleep.h>
#include <esp_random.h>
#include <string.h>

#include "pp_board.h"
#include "pp_session.h"
#include "pp_record_store.h"
#include "pp_device_material.h"
#include "nrf_soc.h"

// ---------------- Settings (NVS) ----------------

static Preferences gPrefs;
static bool gPrefsReady = false;

static bool gAutoSpin = true;
static uint8_t gCatchMode = PP_CATCH_ALL;
static uint8_t gPressSpeed = PP_SPEED_NORMAL;
static bool gAdvertise = true;

static uint32_t gCaught = 0;
static uint32_t gFled = 0;
static uint32_t gSpins = 0;
static uint32_t gPresses = 0;
static int gConnectedCount = 0;
static char gNote[48] = "boot";

static void settingsLoad()
{
  gPrefsReady = gPrefs.begin("ppboard", false);
  if (!gPrefsReady) {
    return;
  }
  gAutoSpin = gPrefs.getBool("autospin", true);
  gCatchMode = gPrefs.getUChar("catchmode", PP_CATCH_ALL);
  if (gCatchMode > PP_CATCH_NEW_ONLY) gCatchMode = PP_CATCH_ALL;
  gPressSpeed = gPrefs.getUChar("pressspeed", PP_SPEED_NORMAL);
  gAdvertise = gPrefs.getBool("advertise", true);
  gCaught = gPrefs.getULong("n_caught", 0);
  gFled = gPrefs.getULong("n_fled", 0);
  gSpins = gPrefs.getULong("n_spins", 0);
  gPresses = gPrefs.getULong("n_press", 0);
}

static void settingsSave(const char* reason)
{
  if (!gPrefsReady) {
    gPrefsReady = gPrefs.begin("ppboard", false);
  }
  if (!gPrefsReady) {
    return;
  }
  gPrefs.putBool("autospin", gAutoSpin);
  gPrefs.putUChar("catchmode", gCatchMode);
  gPrefs.putUChar("pressspeed", gPressSpeed);
  gPrefs.putBool("advertise", gAdvertise);
  gPrefs.putULong("n_caught", gCaught);
  gPrefs.putULong("n_fled", gFled);
  gPrefs.putULong("n_spins", gSpins);
  gPrefs.putULong("n_press", gPresses);
  Serial.print("Settings saved (");
  Serial.print(reason);
  Serial.println(")");
}

// ---------------- Button A (BOOT) and Button B (D2) ----------------

static bool gBtnAActive = false;
static bool gBtnAClicked = false;   // consumed by LED/console path
static bool gBtnAHoldFired = false;
static uint32_t gBtnAAt = 0;

static bool gBtnBActive = false;
static bool gBtnBHoldFired = false;
static uint32_t gBtnBAt = 0;

static bool btnARaw()
{
  int v = digitalRead(PP_BTN_A_PIN);
  return PP_BTN_A_ACTIVE_LOW ? (v == LOW) : (v == HIGH);
}

static bool btnBRaw()
{
  int v = digitalRead(PP_BTN_B_PIN);
  return PP_BTN_B_ACTIVE_LOW ? (v == LOW) : (v == HIGH);
}

// ---------------- LED ----------------
// Single-colour LED: events are blink codes, plus ambient patterns.

static uint32_t gLedPattern = 0;      // packed on/off windows, ms each, 0 = idle
static int gLedPatternRepeats = 0;    // 0 = forever, >0 = repeat count
static bool gLedOn = false;
static uint32_t gLedNextToggleAt = 0;
static uint16_t gLedPatternPos = 0;
static const uint32_t* gLedWindows = nullptr;
static int gLedWindowCount = 0;

static void ledIdlePattern(bool advertising, bool connectedAny)
{
  // Ambient: 1 s slow blink while advertising unconnected; 100 ms heartbeat
  // every 3 s while one or more phones are connected; off otherwise.
  if (connectedAny) {
    gLedWindows = nullptr;
    gLedPatternRepeats = -1;
    static uint32_t heart[2] = { 100, 2900 };
    gLedWindows = heart;
    gLedWindowCount = 2;
  } else if (advertising) {
    static uint32_t slow[2] = { 120, 880 };
    gLedWindows = slow;
    gLedWindowCount = 2;
    gLedPatternRepeats = -1;
  } else {
    gLedWindows = nullptr;
    gLedWindowCount = 0;
    gLedPatternRepeats = 0;
    gLedOn = false;
  }
  gLedPatternPos = 0;
  gLedNextToggleAt = millis();
}

static void ledWrite(bool on)
{
  gLedOn = on;
  digitalWrite(PP_LED_PIN, (PP_LED_ACTIVE_LOW ? !on : on) ? HIGH : LOW);
}

static void ledPlay(const uint32_t* windows, int count, int repeats)
{
  gLedWindows = windows;
  gLedWindowCount = count;
  gLedPatternRepeats = repeats;
  gLedPatternPos = 0;
  gLedNextToggleAt = millis();
}

static const uint32_t kPokestopCode[] = { 90, 90, 90, 90, 90, 640 };          // 3 blinks
static const uint32_t kPokemonCode[] = { 90, 90, 90, 830 };                    // 1 blink
static const uint32_t kNewPokeCode[] = { 80, 80, 80, 80, 80, 800 };            // 2 blinks
static const uint32_t kCaughtCode[] = { 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 550 }; // 6 fast
static const uint32_t kFledCode[] = { 700, 500 };                              // 1 long
static const uint32_t kItemsCode[] = { 60, 60, 60, 60, 60, 880 };              // 3 short
static const uint32_t kWarnCode[] = { 300, 200, 300, 800 };                    // 2 long = warning
static const uint32_t kUnknownCode[] = { 40, 60, 40, 860 };

void ppBoardOnEvent(PPLedEvent evt, const char* label)
{
  Serial.print("Event: ");
  Serial.println(label);
  ppBoardSetNote(label);
  switch (evt) {
    case PP_LED_POKEMON: ledPlay(kPokemonCode, 4, 2); break;
    case PP_LED_NEW_POKEMON: ledPlay(kNewPokeCode, 6, 2); break;
    case PP_LED_POKESTOP: ledPlay(kPokestopCode, 6, 2); break;
    case PP_LED_CAUGHT: ++gCaught; ledPlay(kCaughtCode, 12, 1); break;
    case PP_LED_FLED: ++gFled; ledPlay(kFledCode, 2, 1); break;
    case PP_LED_ITEMS: ++gSpins; ledPlay(kItemsCode, 6, 1); break;
    case PP_LED_BAG_FULL:
    case PP_LED_BOX_FULL:
    case PP_LED_BALLS_EMPTY: ledPlay(kWarnCode, 4, 2); break;
    default: ledPlay(kUnknownCode, 4, 1); break;
  }
  settingsSave("event");
}

void ppBoardSetNote(const char* text)
{
  strncpy(gNote, text, sizeof(gNote) - 1);
  gNote[sizeof(gNote) - 1] = 0;
}

void ppBoardOnDisconnect(uint8_t reason)
{
  snprintf(gNote, sizeof(gNote), "link dropped (0x%02x)", reason);
}

void ppBoardSetConnectedCount(int n) { gConnectedCount = n; }
int ppBoardGetConnectedCount() { return gConnectedCount; }

bool ppBoardAutoSpin() { return gAutoSpin; }
uint8_t ppBoardCatchMode() { return gCatchMode; }
uint8_t ppBoardPressSpeed() { return gPressSpeed; }
bool ppBoardAdvertiseEnabled() { return gAdvertise && ppDeviceMaterialConfigured(); }

static void cycleCatchMode(const char* via)
{
  // off -> catch all -> catch new only -> spin only -> off
  if (!gAutoSpin && gCatchMode == PP_CATCH_NEW_ONLY) {
    // spin-only is the synthetic 4th state: catch off + spin on
    gCatchMode = PP_CATCH_OFF;
    gAutoSpin = false;
  } else if (gCatchMode == PP_CATCH_OFF && !gAutoSpin) {
    gCatchMode = PP_CATCH_OFF;
    gAutoSpin = true;   // spin only
  } else if (gCatchMode == PP_CATCH_OFF) {
    gCatchMode = PP_CATCH_ALL;
  } else if (gCatchMode == PP_CATCH_ALL) {
    gCatchMode = PP_CATCH_NEW_ONLY;
  } else {
    gCatchMode = PP_CATCH_OFF;
    gAutoSpin = false;
  }
  Serial.print("Catch mode (");
  Serial.print(via);
  Serial.print("): ");
  if (gCatchMode == PP_CATCH_ALL) Serial.println("catch ALL + spin");
  else if (gCatchMode == PP_CATCH_NEW_ONLY) Serial.println("catch NEW only + spin");
  else if (gAutoSpin) Serial.println("spin only (no catch)");
  else Serial.println("everything off (manual only)");
  ledPlay(gCatchMode == PP_CATCH_OFF ? kUnknownCode
          : (gCatchMode == PP_CATCH_ALL ? kCaughtCode : kNewPokeCode),
          gCatchMode == PP_CATCH_ALL ? 12 : 6, 1);
  settingsSave("mode cycle");
}

// ---------------- Serial console ----------------

static char gLineBuf[160];
static int gLineLen = 0;
static int gKeyUploadStep = 0;   // 0 = off, 1 = waiting for lines

static void printHelp()
{
  Serial.println("--- pgpp-esp32 console ---");
  Serial.println("  ?  - this help");
  Serial.println("  s  - status (sessions, counters, settings)");
  Serial.println("  a  - cycle catch mode (off/all/new/spin-only)");
  Serial.println("  p  - toggle auto spin");
  Serial.println("  f  - toggle press speed (normal/fast)");
  Serial.println("  A  - toggle advertising enabled");
  Serial.println("  w  - wipe ALL pairing records (re-pair phones)");
  Serial.println("  k  - enter device-material upload mode (paste keys from flash dump)");
  Serial.println("  i  - device material info");
  Serial.println("  m  - per-session info");
  Serial.println("  L  - LED blink test");
  Serial.println("  S  - save settings now");
  Serial.println("  r  - restart ESP32");
}

static void printStatus()
{
  Serial.println("--- status ---");
  Serial.print("connections: ");
  Serial.print(gConnectedCount);
  Serial.print("/");
  Serial.println(PP_MAX_CONNECTIONS);
  Serial.print("catch mode: ");
  Serial.print(gCatchMode == PP_CATCH_ALL ? "ALL" : gCatchMode == PP_CATCH_NEW_ONLY ? "NEW only" : "off");
  Serial.print("  autospin: ");
  Serial.print(gAutoSpin ? "on" : "off");
  Serial.print("  press speed: ");
  Serial.println(gPressSpeed == PP_SPEED_FAST ? "fast" : "normal");
  Serial.print("advertising: ");
  Serial.println(gAdvertise ? "on" : "suppressed");
  Serial.print("counters: caught=");
  Serial.print(gCaught);
  Serial.print(" fled=");
  Serial.print(gFled);
  Serial.print(" spins=");
  Serial.print(gSpins);
  Serial.print(" presses=");
  Serial.println(gPresses);
  Serial.print("pairing records: ");
  Serial.print(ppRecordStoreCount());
  Serial.print("/");
  Serial.println(PP_MAX_PAIRING_RECORDS);
  Serial.print("last event: ");
  Serial.println(gNote);
}

static void printMaterialInfo()
{
  Serial.println("--- device material ---");
  Serial.print("compiled-in: ");
  Serial.println(ppDeviceMaterialCompiledIn() ? "yes" : "no");
  Serial.print("loaded from NVS: ");
  Serial.println(ppDeviceMaterialNvsLoaded() ? "yes" : "no");
  Serial.print("configured: ");
  Serial.println(ppDeviceMaterialConfigured() ? "yes" : "NO (upload with 'k')");
  if (ppDeviceMaterialConfigured()) {
    const uint8_t* a = ppDeviceMaterialAddrLe();
    char buf[18];
    snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
             a[5], a[4], a[3], a[2], a[1], a[0]);
    Serial.print("device address (display order): ");
    Serial.println(buf);
  }
}

// hex helpers for the 'k' upload path
static int hexVal(char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static int parseHexBytes(const char* s, uint8_t* out, int maxBytes)
{
  int n = 0;
  while (*s && n < maxBytes) {
    while (*s == ' ' || *s == ':' || *s == ',') ++s;
    int hi = hexVal(s[0]);
    int lo = hexVal(s[1]);
    if (hi < 0 || lo < 0) {
      return n;
    }
    out[n++] = static_cast<uint8_t>((hi << 4) | lo);
    s += 2;
  }
  return n;
}

static Preferences gMatPrefs;

static bool materialSaveToNvs(const uint8_t* addrLe, const uint8_t* cert,
                              const uint8_t* sel3, const uint8_t* sel4, const uint8_t* sel6)
{
  if (!gMatPrefs.begin("ppmat", false)) {
    Serial.println("material NVS open failed");
    return false;
  }
  gMatPrefs.putBytes("addr", addrLe, 6);
  gMatPrefs.putBytes("cert", cert, 256);
  gMatPrefs.putBytes("sel3", sel3, 16);
  gMatPrefs.putBytes("sel4", sel4, 16);
  gMatPrefs.putBytes("sel6", sel6, 10);
  gMatPrefs.end();
  Serial.println("Device material saved to NVS. Restarting...");
  return true;
}

// Key-upload staging buffers: each line arrives separately, so the values
// must persist between lines until 'save'.
static uint8_t gUpAddrLe[6] = { 0 };
static uint8_t gUpCert[256] = { 0 };
static uint8_t gUpSel3[16] = { 0 };
static uint8_t gUpSel4[16] = { 0x85, 0x6a, 0x07, 0x37, 0xb1, 0x30, 0x47, 0x6e,
                               0x6c, 0x82, 0xf1, 0x00, 0xd3, 0x06, 0x7a, 0xe1 };
static uint8_t gUpSel6[10] = { 0 };
static bool gUpHaveAddr = false;
static bool gUpHaveCert = false;
static bool gUpHaveSel3 = false;
static bool gUpHaveSel6 = false;

static void handleConsoleLine(const char* line)
{
  if (gKeyUploadStep == 1) {
    if (line[0] == '.' || strcmp(line, "done") == 0 || strcmp(line, "cancel") == 0) {
      gKeyUploadStep = 0;
      Serial.println("Key upload mode exited. Send 'r' to restart and use the keys.");
      return;
    }
    bool ok = true;
    uint8_t tmp[300];
    if (strncmp(line, "addr=", 5) == 0) {
      int n = parseHexBytes(line + 5, tmp, sizeof(tmp));
      // Accept display order AA:BB:.. (6 bytes) - store little-endian
      if (n == 6) {
        for (int i = 0; i < 6; ++i) gUpAddrLe[i] = tmp[5 - i];
        gUpHaveAddr = true;
        Serial.println("addr stored (converted to little-endian)");
      } else {
        Serial.print("addr line: expected 6 bytes, got ");
        Serial.println(n);
        ok = false;
      }
      return;
    }
    if (strncmp(line, "cert=", 5) == 0) {
      int n = parseHexBytes(line + 5, tmp, sizeof(tmp));
      if (n == 256) {
        memcpy(gUpCert, tmp, 256);
        gUpHaveCert = true;
        Serial.println("cert stored (256 bytes)");
      } else {
        Serial.print("cert line: expected 512 hex chars, got ");
        Serial.println(n);
        ok = false;
      }
      return;
    }
    if (strncmp(line, "sel3=", 5) == 0) {
      int n = parseHexBytes(line + 5, tmp, sizeof(tmp));
      if (n == 16) {
        memcpy(gUpSel3, tmp, 16);
        gUpHaveSel3 = true;
        Serial.println("sel3 stored (16 bytes)");
      } else {
        Serial.print("sel3: expected 32 hex chars, got ");
        Serial.println(n);
        ok = false;
      }
      return;
    }
    if (strncmp(line, "sel4=", 5) == 0) {
      int n = parseHexBytes(line + 5, tmp, sizeof(tmp));
      if (n == 16) {
        memcpy(gUpSel4, tmp, 16);
        Serial.println("sel4 override stored (16 bytes)");
      } else {
        Serial.print("sel4: expected 32 hex chars, got ");
        Serial.println(n);
      }
      return;
    }
    if (strncmp(line, "sel6=", 5) == 0) {
      int n = parseHexBytes(line + 5, tmp, sizeof(tmp));
      if (n == 10) {
        memcpy(gUpSel6, tmp, 10);
        gUpHaveSel6 = true;
        Serial.println("sel6 stored (10 bytes)");
      } else {
        Serial.print("sel6: expected 20 hex chars, got ");
        Serial.println(n);
        ok = false;
      }
      return;
    }
    if (strcmp(line, "save") == 0) {
      if (!(gUpHaveAddr && gUpHaveCert && gUpHaveSel3 && gUpHaveSel6)) {
        Serial.println("Missing required line(s): need addr, cert, sel3 and sel6 before save.");
        Serial.print("have: addr=");
        Serial.print(gUpHaveAddr);
        Serial.print(" cert=");
        Serial.print(gUpHaveCert);
        Serial.print(" sel3=");
        Serial.print(gUpHaveSel3);
        Serial.print(" sel6=");
        Serial.println(gUpHaveSel6);
        return;
      }
      if (materialSaveToNvs(gUpAddrLe, gUpCert, gUpSel3, gUpSel4, gUpSel6)) {
        delay(300);
        ESP.restart();
      }
      return;
    }
    Serial.println("Unknown key-upload line. Use addr=/cert=/sel3=/sel4=/sel6=/save, or 'cancel'.");
    (void) ok;
    return;
  }

  if (line[0] == 0 || line[0] == '\n' || line[0] == '\r') {
    return;
  }
  switch (line[0]) {
    case '?':
    case 'h': printHelp(); break;
    case 's': printStatus(); break;
    case 'a': cycleCatchMode("console"); break;
    case 'p':
      gAutoSpin = !gAutoSpin;
      Serial.println(gAutoSpin ? "Auto spin on" : "Auto spin off");
      settingsSave("autospin toggle");
      break;
    case 'f':
      gPressSpeed = (gPressSpeed == PP_SPEED_FAST) ? PP_SPEED_NORMAL : PP_SPEED_FAST;
      Serial.println(gPressSpeed == PP_SPEED_FAST ? "Press speed fast (0.3-0.8 s)" : "Press speed normal (1.0-2.5 s)");
      settingsSave("press speed");
      break;
    case 'A':
      gAdvertise = !gAdvertise;
      Serial.println(gAdvertise ? "Advertising enabled" : "Advertising suppressed");
      settingsSave("advertise toggle");
      break;
    case 'w':
      Serial.println("Wiping ALL pairing records...");
      ppRecordStoreClearAll("console wipe");
      Serial.println("Done. Re-pair your phones (Settings > Connected devices).");
      break;
    case 'k':
      gKeyUploadStep = 1;
      Serial.println("Device material upload. Paste one line at a time:");
      Serial.println("  addr=AA:BB:CC:DD:EE:FF      (your Plus+ public BLE address)");
      Serial.println("  cert=<512 hex chars>        (selector2 cert block from the dump)");
      Serial.println("  sel3=<32 hex chars>         (selector3 key lane)");
      Serial.println("  sel6=<20 hex chars>         (factory selector6)");
      Serial.println("  sel4=<32 hex chars>         (optional override)");
      Serial.println("  save                        (write to NVS and restart)");
      Serial.println("  cancel                      (abort)");
      break;
    case 'i': printMaterialInfo(); break;
    case 'm': {
      Serial.println("--- sessions ---");
      for (int i = 0; i < PP_MAX_CONNECTIONS; ++i) {
        PPSession& s = gSessions[i];
        Serial.print(i);
        Serial.print(": ");
        if (!s.used) {
          Serial.println("free");
          continue;
        }
        Serial.print("conn=0x");
        Serial.print(s.connHandle, HEX);
        Serial.print(" stage=");
        switch (s.pairingStage) {
          case PAIR_STAGE_DISCOVERY: Serial.print("discovery"); break;
          case PAIR_STAGE_STAGED: Serial.print("staged"); break;
          case PAIR_STAGE_COMMITTED: Serial.print("committed"); break;
          case PAIR_STAGE_AUTHENTICATED: Serial.print("authenticated"); break;
        }
        Serial.print(" record=");
        Serial.print(s.recordIndex);
        Serial.print(" reconnectReady=");
        Serial.println(s.persistentReconnectReady ? "true" : "false");
      }
      break;
    }
    case 'L':
      Serial.println("LED test: 1+2+3 blinks, long, fast");
      ledPlay(kPokemonCode, 4, 1);
      break;
    case 'S': settingsSave("manual"); break;
    case 'r':
      Serial.println("Restarting...");
      delay(200);
      ESP.restart();
      break;
    default:
      Serial.println("Unknown command (? for help)");
      break;
  }
}

static void consoleTick()
{
  while (Serial.available() > 0) {
    char c = static_cast<char>(Serial.read());
    if (c == '\r') continue;
    if (c == '\n') {
      gLineBuf[gLineLen] = 0;
      if (gLineLen > 0) {
        handleConsoleLine(gLineBuf);
      }
      gLineLen = 0;
      continue;
    }
    if (gLineLen < static_cast<int>(sizeof(gLineBuf)) - 1) {
      gLineBuf[gLineLen++] = c;
    }
  }
}

// ---------------- Board init / tick / sleep ----------------

void ppBoardInit()
{
  pinMode(PP_LED_PIN, OUTPUT);
  ledWrite(false);
  pinMode(PP_BTN_A_PIN, PP_BTN_A_ACTIVE_LOW ? INPUT_PULLUP : INPUT_PULLDOWN);
  if (PP_BTN_B_PIN != PP_LED_PIN) {
    pinMode(PP_BTN_B_PIN, PP_BTN_B_ACTIVE_LOW ? INPUT_PULLUP : INPUT_PULLDOWN);
  } else {
    // Shared LED/button pin: the button polarity decides the pull direction.
    pinMode(PP_BTN_B_PIN, PP_BTN_B_ACTIVE_LOW ? INPUT_PULLUP : INPUT_PULLDOWN);
  }
  settingsLoad();
  pp_soc_set_wake_pin(PP_BTN_A_PIN);
  Serial.println("Board: ESP32-DevKitC, BOOT=manual press/hold= wipe, D2=mode cycle");
  printMaterialInfo();
  if (!ppDeviceMaterialConfigured()) {
    Serial.println();
    Serial.println("!! No device material configured.");
    Serial.println("!! This firmware needs the keys of your own Pokemon GO Plus+");
    Serial.println("!! (see README: extract them from the device flash, then use 'k').");
    Serial.println("!! Advertising stays off until material is configured.");
    Serial.println();
  }
}

void ppBoardTick(const BoardRefs& refs)
{
  uint32_t now = millis();

  // Button A: click = manual press, hold >= 5 s = wipe pairing.
  bool aPressed = btnARaw();
  if (aPressed && !gBtnAActive) {
    gBtnAActive = true;
    gBtnAAt = now;
    gBtnAHoldFired = false;
  } else if (!aPressed && gBtnAActive) {
    gBtnAActive = false;
    if (!gBtnAHoldFired) {
      refs.manualButtonPress();
      ++gPresses;
    }
  } else if (aPressed && gBtnAActive && !gBtnAHoldFired && now - gBtnAAt >= 5000) {
    gBtnAHoldFired = true;
    Serial.println("BOOT held 5 s -> wiping pairing records");
    refs.resetPairing("BOOT hold");
  }

  // Button B: click = cycle mode, hold = toggle autospin.
  bool bDown = btnBRaw();
  if (bDown && !gBtnBActive) {
    gBtnBActive = true;
    gBtnBAt = now;
    gBtnBHoldFired = false;
  } else if (!bDown && gBtnBActive) {
    gBtnBActive = false;
    if (!gBtnBHoldFired) {
      cycleCatchMode("D2 button");
    }
  } else if (bDown && gBtnBActive && !gBtnBHoldFired && now - gBtnBAt >= PP_BTN_B_HOLD_MS) {
    gBtnBHoldFired = true;
    gAutoSpin = !gAutoSpin;
    Serial.println(gAutoSpin ? "Auto spin on (D2 hold)" : "Auto spin off (D2 hold)");
    settingsSave("autospin toggle");
  }

  // LED state machine.
  if (gLedWindows != nullptr && gLedWindowCount > 0) {
    if (static_cast<int32_t>(now - gLedNextToggleAt) >= 0) {
      bool windowOn = (gLedPatternPos & 1) == 0;
      ledWrite(windowOn);
      gLedNextToggleAt = now + gLedWindows[gLedPatternPos];
      ++gLedPatternPos;
      if (gLedPatternPos >= gLedWindowCount) {
        gLedPatternPos = 0;
        if (gLedPatternRepeats > 0) {
          --gLedPatternRepeats;
          if (gLedPatternRepeats == 0) {
            ledWrite(false);
            gLedWindows = nullptr;
            gLedWindowCount = 0;
            gLedPatternRepeats = 0;
          }
        }
      }
    }
  } else {
    ledIdlePattern(gAdvertise && ppDeviceMaterialConfigured(), gConnectedCount > 0);
  }

  consoleTick();
}

void ppBoardOnWake()
{
  Serial.println("Woke from deep sleep (BOOT button)");
}

void ppBoardEnterDeepSleep()
{
  Serial.println("Board: entering deep sleep (BOOT button wakes)");
  Serial.flush();
  delay(20);
  esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(PP_BTN_A_PIN), PP_BTN_A_ACTIVE_LOW ? 0 : 1);
  esp_deep_sleep_start();
}
