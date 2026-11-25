#include <Arduino.h>
#include <string.h>
#include <stdio.h>

#include "pp_flash_dump.h"

// ---------------------------------------------------------------------------
// Pins -- keep all < GPIO32 so the sniffer can watch them in GPIO.in (bits 0..31).

static const int PIN_CS   = 21;
static const int PIN_SCK  = 22;
static const int PIN_MOSI = 23;
static const int PIN_MISO = 25;

static const uint32_t FLASH_SIZE = 8u * 1024u * 1024u;  // MX25U6433F = 8 MiB
static const uint32_t DEFAULT_DUMP_START = 0x0000;
static const uint32_t DEFAULT_DUMP_END = 0x10000;  // first 64 KiB: FBCT + SAV

// Expected JEDEC ID: Macronix MX25U6433F (C2 25 37).
static const uint8_t JEDEC_ID[3] = { 0xC2, 0x25, 0x37 };

#if defined(PP_HOST_CHECK)
static inline uint32_t ppFdGpioIn() { return 0; }
#elif defined(ARDUINO_ARCH_ESP32)
#include "soc/gpio_struct.h"
static inline uint32_t ppFdGpioIn() { return GPIO.in; }
#else
static inline uint32_t ppFdGpioIn() { return 0; }
#endif

// ---------------------------------------------------------------------------
// Master mode: bit-banged SPI read (0x03), MSB first, sample on rising SCK.

static inline void fdCsHigh() { digitalWrite(PIN_CS, HIGH); }
static inline void fdCsLow() { digitalWrite(PIN_CS, LOW); }

static uint8_t fdShiftByte(uint8_t out)
{
  uint8_t in = 0;
  for (int bit = 7; bit >= 0; --bit) {
    digitalWrite(PIN_MOSI, (out >> bit) & 1 ? HIGH : LOW);
    digitalWrite(PIN_SCK, LOW);
    digitalWrite(PIN_SCK, HIGH);  // rising edge: flash drives MISO, we sample
    if (digitalRead(PIN_MISO)) {
      in |= (1u << bit);
    }
    digitalWrite(PIN_SCK, LOW);
  }
  return in;
}

static void fdReadBegin(uint32_t addr)
{
  fdCsLow();
  fdShiftByte(0x03);
  fdShiftByte((addr >> 16) & 0xff);
  fdShiftByte((addr >> 8) & 0xff);
  fdShiftByte(addr & 0xff);
}

static uint8_t fdReadByte()
{
  return fdShiftByte(0x00);
}

static void fdReadEnd()
{
  fdCsHigh();
}

static bool fdJedecOk(uint8_t id[3])
{
  fdCsLow();
  fdShiftByte(0x9F);
  id[0] = fdShiftByte(0x00);
  id[1] = fdShiftByte(0x00);
  id[2] = fdShiftByte(0x00);
  fdCsHigh();
  return id[0] == JEDEC_ID[0] && id[1] == JEDEC_ID[1] && id[2] == JEDEC_ID[2];
}

static void fdDumpRange(uint32_t start, uint32_t end)
{
  if (end > FLASH_SIZE) {
    end = FLASH_SIZE;
  }
  if (start >= end) {
    Serial.println("Bad range");
    return;
  }
  Serial.print("BEGIN ");
  Serial.print(start, HEX);
  Serial.print(" ");
  Serial.println(end, HEX);

  uint8_t line[32];
  uint32_t addr = start;
  while (addr < end) {
    uint32_t chunk = end - addr;
    if (chunk > sizeof(line)) {
      chunk = sizeof(line);
    }
    fdReadBegin(addr);
    for (uint32_t i = 0; i < chunk; ++i) {
      line[i] = fdReadByte();
    }
    fdReadEnd();

    Serial.print(addr, HEX);
    Serial.print(":");
    for (uint32_t i = 0; i < chunk; ++i) {
      if (line[i] < 0x10) {
        Serial.print('0');
      }
      Serial.print(line[i], HEX);
    }
    Serial.println();
    addr += chunk;
  }

  Serial.print("END ");
  Serial.println(end - start);
}

static bool fdVerifyRange(uint32_t start, uint32_t end)
{
  const uint32_t kStep = 64;
  uint8_t a[kStep], b[kStep];
  for (uint32_t addr = start; addr < end; addr += kStep) {
    uint32_t chunk = end - addr;
    if (chunk > kStep) {
      chunk = kStep;
    }
    fdReadBegin(addr);
    for (uint32_t i = 0; i < chunk; ++i) {
      a[i] = fdReadByte();
    }
    fdReadEnd();
    delay(2);
    fdReadBegin(addr);
    for (uint32_t i = 0; i < chunk; ++i) {
      b[i] = fdReadByte();
    }
    fdReadEnd();
    for (uint32_t i = 0; i < chunk; ++i) {
      if (a[i] != b[i]) {
        Serial.print("MISMATCH at 0x");
        Serial.println(addr + i, HEX);
        return false;
      }
    }
  }
  Serial.println("VERIFY-OK (two passes identical)");
  return true;
}

// ---------------------------------------------------------------------------
// Passive sniffer: PGPP stays powered; at boot its SoC reads the flash and we
// listen. Best-effort (BLE activity on this core costs us samples).

static volatile bool gSniffRun = false;
static volatile uint32_t gSniffBytes = 0;
static volatile uint32_t gSniffReads = 0;
static uint32_t gSniffDeadlineMs = 0;
static uint8_t gCoverage[0x10000 / 256];  // page hit map, first 64 KiB
static uint8_t gCaptured[0x10000 / 256];  // last byte seen per page

static void sniffRecordByte(uint32_t addr, uint8_t byteVal)
{
  if (addr < 0x10000) {
    gCoverage[addr >> 8] = 1;
    gCaptured[addr >> 8] = byteVal;
  }
  if (++gSniffBytes % 256 == 0) {
    Serial.print("SNIFF progress bytes=");
    Serial.print(gSniffBytes);
    Serial.print(" last-addr=0x");
    Serial.println(addr, HEX);
  }
}

static void sniffTask(void*)
{
  const uint32_t SCK_BIT = (1UL << PIN_SCK);
  const uint32_t CS_BIT = (1UL << PIN_CS);
  const uint32_t MISO_BIT = (1UL << PIN_MISO);

  while (gSniffRun) {
    if (millis() >= gSniffDeadlineMs) {
      break;  // auto-stop
    }
    uint32_t regs = ppFdGpioIn();
    if ((regs & CS_BIT) == 0) {
      bool inTransaction = true;
      int bitCount = 0;
      uint32_t shift = 0;
      int phase = 0;  // 0=cmd 1=addr 2=dummy 3=data 4=ignore
      uint8_t cmd = 0;
      uint32_t addr = 0;
      int addrBytes = 0;
      bool prevSck = false;

      while (gSniffRun && ((ppFdGpioIn() & CS_BIT) == 0)) {
        uint32_t now = ppFdGpioIn();
        bool sck = (now & SCK_BIT) != 0;
        if (sck && !prevSck) {
          shift = (shift << 1) | ((now & MISO_BIT) ? 1 : 0);
          if (++bitCount == 8) {
            bitCount = 0;
            uint8_t byteVal = (uint8_t)(shift & 0xff);
            shift = 0;
            if (phase == 0) {
              cmd = byteVal;
              phase = (cmd == 0x03 || cmd == 0x0B) ? 1 : 4;
              if (phase == 1) {
                gSniffReads++;
              }
            } else if (phase == 1) {
              addr = (addr << 8) | byteVal;
              if (++addrBytes >= 3) {
                phase = (cmd == 0x0B) ? 2 : 3;
              }
            } else if (phase == 2) {
              phase = 3;  // 8 dummy clocks for 0x0B
            } else if (phase == 3) {
              sniffRecordByte(addr, byteVal);
              addr = (addr + 1) & 0xffffff;
            }
          }
        }
        prevSck = sck;
      }
      inTransaction = false;
      (void)inTransaction;
    }
    delayMicroseconds(50);
  }

  gSniffRun = false;
  Serial.println();
  Serial.print("SNIFF-STOP bytes=");
  Serial.print(gSniffBytes);
  Serial.print(" reads=");
  Serial.println(gSniffReads);
  Serial.println("Captured pages (addr:lastByte) -- need 0x2000-0x21a2 and 0x3000-0x3288:");
  for (uint32_t page = 0; page < sizeof(gCoverage); ++page) {
    if (gCoverage[page]) {
      Serial.print(page << 8, HEX);
      Serial.print(":");
      if (gCaptured[page] < 0x10) {
        Serial.print('0');
      }
      Serial.print(gCaptured[page], HEX);
      Serial.print(" ");
    }
  }
  Serial.println();
  Serial.println("Missing pages? Boot the Plus+ again while the sniffer runs (x).");
  vTaskDelete(nullptr);
}

static void sniffStart()
{
  if (gSniffRun) {
    Serial.println("Sniffer already running ('X' to stop)");
    return;
  }
  memset(gCoverage, 0, sizeof(gCoverage));
  memset(gCaptured, 0, sizeof(gCaptured));
  gSniffBytes = 0;
  gSniffReads = 0;
  gSniffDeadlineMs = millis() + 60000;  // 60 s auto-stop
  gSniffRun = true;
  pinMode(PIN_CS, INPUT);
  pinMode(PIN_SCK, INPUT);
  pinMode(PIN_MOSI, INPUT);
  pinMode(PIN_MISO, INPUT);
  xTaskCreatePinnedToCore(sniffTask, "ppfd-sniff", 4096, nullptr, 1, nullptr, 0);
  Serial.println("Sniffer listening (auto-stops after 60 s) -- now boot the Plus+");
  Serial.println("Note: BLE keeps running; if pages are missing, stop BLE activity and retry.");
}

static void sniffStop()
{
  if (!gSniffRun) {
    Serial.println("Sniffer not running");
    return;
  }
  gSniffRun = false;
  delay(100);
}

// ---------------------------------------------------------------------------
// Console interface

void ppFlashDumpHelp()
{
  Serial.println("  j  - flash reader: JEDEC ID check (expect C2 25 37)");
  Serial.println("  d  - flash reader: dump 0x0000-0x10000 + verify (PGPP off, taps wired)");
  Serial.println("  D <start> <end> - flash reader: dump hex range (D 0 800000 = full backup)");
  Serial.println("  x  - flash reader: passive boot sniffer start (PGPP powered normally)");
  Serial.println("  X  - flash reader: sniffer stop + coverage report");
}

bool ppFlashDumpSnifferRunning()
{
  return gSniffRun;
}

bool ppFlashDumpConsoleLine(const char* line)
{
  if (line[0] == 'j' && line[1] == 0) {
    pinMode(PIN_CS, OUTPUT);
    pinMode(PIN_SCK, OUTPUT);
    pinMode(PIN_MOSI, OUTPUT);
    pinMode(PIN_MISO, INPUT_PULLDOWN);
    fdCsHigh();
    digitalWrite(PIN_SCK, LOW);
    digitalWrite(PIN_MOSI, LOW);
    uint8_t id[3];
    if (fdJedecOk(id)) {
      Serial.print("JEDEC OK: ");
    } else {
      Serial.print("JEDEC UNEXPECTED (check wiring/power; 'x' sniffer mode needs none): ");
    }
    for (int i = 0; i < 3; ++i) {
      if (id[i] < 0x10) {
        Serial.print('0');
      }
      Serial.print(id[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
    return true;
  }
  if (line[0] == 'd' && line[1] == 0) {
    pinMode(PIN_CS, OUTPUT);
    pinMode(PIN_SCK, OUTPUT);
    pinMode(PIN_MOSI, OUTPUT);
    pinMode(PIN_MISO, INPUT_PULLDOWN);
    fdDumpRange(DEFAULT_DUMP_START, DEFAULT_DUMP_END);
    fdVerifyRange(DEFAULT_DUMP_START, DEFAULT_DUMP_END);
    return true;
  }
  if (line[0] == 'D') {
    uint32_t start = 0;
    uint32_t end = 0;
    if (sscanf(line, "D %lx %lx", &start, &end) != 2 || start >= end || end > FLASH_SIZE) {
      Serial.println("Bad range. Example: D 0 800000");
      return true;
    }
    pinMode(PIN_CS, OUTPUT);
    pinMode(PIN_SCK, OUTPUT);
    pinMode(PIN_MOSI, OUTPUT);
    pinMode(PIN_MISO, INPUT_PULLDOWN);
    fdDumpRange(start, end);
    return true;
  }
  if (line[0] == 'x' && line[1] == 0) {
    sniffStart();
    return true;
  }
  if (line[0] == 'X' && line[1] == 0) {
    sniffStop();
    return true;
  }
  return false;
}
