#pragma once
// Minimal host-side stub of the Arduino core, just enough to type-check the
// pgpp_esp32 firmware with g++ -fsyntax-only. NOT for production use.

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>

#define HEX 16
#define DEC 10
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define INPUT_PULLDOWN 3

typedef uint8_t byte;
typedef unsigned int unsigned_int;

using std::string;

inline unsigned long millis() { return 0; }
inline unsigned long micros() { return 0; }
inline void delay(unsigned long) {}
inline void delayMicroseconds(unsigned int) {}

inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline int digitalRead(int) { return 0; }
inline void analogWrite(int, int) {}

struct EspClass {
  void restart() {}
};
extern EspClass ESP;

class SerialStub {
public:
  void begin(unsigned long) {}
  void end() {}
  void setTxTimeoutMs(long) {}
  operator bool() const { return true; }
  int available() { return 0; }
  int read() { return -1; }
  void flush() {}
  template <typename T> void print(T) {}
  template <typename T> void print(T, int) {}
  template <typename T> void println(T) {}
  template <typename T> void println(T, int) {}
  void println() {}
};
extern SerialStub Serial;

class String {
public:
  String() {}
  String(const char*) {}
  String(int) {}
  String(long) {}
  String(unsigned long) {}
  String(double) {}
  String(const String&) {}
  const char* c_str() const { return ""; }
};

template <typename T, typename L>
inline T min(T a, L b) { return a < static_cast<T>(b) ? a : static_cast<T>(b); }
template <typename T, typename L>
inline T max(T a, L b) { return a > static_cast<T>(b) ? a : static_cast<T>(b); }
template <typename T, typename L>
inline T min(T a, T b) { return a < b ? a : b; }
template <typename T, typename L>
inline T max(T a, T b) { return a > b ? a : b; }
template <typename T>
inline T min(T a, T b) { return a < b ? a : b; }
template <typename T>
inline T max(T a, T b) { return a > b ? a : b; }

class Preferences {
public:
  bool begin(const char*, bool = false) { return true; }
  void end() {}
  bool isKey(const char*) { return false; }
  bool remove(const char*) { return true; }
  bool clear() { return true; }
  size_t putBytes(const char*, const void*, size_t) { return 0; }
  size_t getBytes(const char*, void*, size_t) { return 0; }
  size_t getBytesLength(const char*) { return 0; }
  size_t putBool(const char*, bool) { return 0; }
  bool getBool(const char*, bool = false) { return false; }
  size_t putChar(const char*, int8_t) { return 0; }
  int8_t getChar(const char*, int8_t = 0) { return 0; }
  size_t putUChar(const char*, uint8_t) { return 0; }
  uint8_t getUChar(const char*, uint8_t = 0) { return 0; }
  size_t putShort(const char*, int16_t) { return 0; }
  int16_t getShort(const char*, int16_t = 0) { return 0; }
  size_t putUShort(const char*, uint16_t) { return 0; }
  uint16_t getUShort(const char*, uint16_t = 0) { return 0; }
  size_t putInt(const char*, int32_t) { return 0; }
  int32_t getInt(const char*, int32_t = 0) { return 0; }
  size_t putUInt(const char*, uint32_t) { return 0; }
  uint32_t getUInt(const char*, uint32_t = 0) { return 0; }
  size_t putLong(const char*, int32_t) { return 0; }
  int32_t getLong(const char*, int32_t = 0) { return 0; }
  size_t putULong(const char*, uint32_t) { return 0; }
  uint32_t getULong(const char*, uint32_t = 0) { return 0; }
  size_t putString(const char*, const char*) { return 0; }
};

// esp32-specific bits used by the firmware
typedef enum { ESP_OK = 0, ESP_ERR_INVALID_ARG = -1 } esp_err_t;
#include "esp_mac.h"
#define ESP_PWR_LVL_P3 3

inline int xPortGetCoreID() { return 0; }
typedef void (*esp_timer_cb_t)(void*);
