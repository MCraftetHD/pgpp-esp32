/*
 * dump_plusplus_flash.ino -- Pokemon GO Plus + external-flash reader for ESP32
 *
 * Companion to docs/keys_extraction.md. Two modes, selected over the serial
 * console (115200 baud):
 *
 *   m        -- switch mode
 *   j        -- read JEDEC ID (mode A wiring check: expect C2 25 37)
 *   d        -- dump flash regions as hex text (mode A, chip on wires)
 *   s        -- passive boot sniffer (mode B, chip still soldered to the PGPP)
 *   x        -- stop the sniffer / dump
 *
 * MODE A ("d", deterministic, recommended): the MX25U6433F is desoldered from
 * the Plus+ board and wired directly to the ESP32. The chip is powered at
 * ~1.9 V through two series diodes from the ESP32 3V3 rail. CS/SCK/MOSI go
 * through 4.7k-10k series resistors. Bit-banged SPI, MSB first, MISO sampled
 * on the SCK rising edge. Dump output is plain hex text between BEGIN/END
 * markers; capture it with tools/capture_dump.py and feed the file to
 * tools/read_flash_data.py.
 *
 * MODE B ("s", experimental, no desoldering): the Plus+ stays fully assembled
 * and powered. Four wires tap CS/SCK/MOSI/MISO of the flash chip; the ESP32
 * only LISTENS (inputs, no drive). At every Plus+ boot the main SoC reads the
 * flash; this mode decodes those reads and prints everything captured in the
 * FBCT/SAV regions. Coverage depends on what the SoC actually reads -- if a
 * needed region is missing, reboot the Plus+ and run the sniffer again. It is
 * best-effort: the onboard SoC may clock the flash faster than a polled ESP32
 * input can reliably sample.
 *
 * Default pin map (ESP32-DevKitC V4, safe non-strapping pins, all < GPIO32 so
 * the sniffer can read them from the single GPIO.in input register):
 *   CS=21  SCK=22  MOSI=23  MISO=25   (labels on the DevKitC: IO21, IO22, IO23, IO25)
 */

#include <Arduino.h>
#include "soc/gpio_struct.h"

// ---------------------------------------------------------------------------
// Pin map -- change here if your wiring differs.
static const int PIN_CS   = 21;
static const int PIN_SCK  = 22;
static const int PIN_MOSI = 23;
static const int PIN_MISO = 25;

// Flash geometry / interesting regions (see tools/read_flash_data.py).
static const uint32_t FLASH_SIZE = 8u * 1024u * 1024u;  // MX25U6433F = 8 MiB
static const uint32_t DEFAULT_DUMP_START = 0x0000;
static const uint32_t DEFAULT_DUMP_END   = 0x10000;     // first 64 KiB covers FBCT+SAV

// Expected JEDEC ID of the MX25U6433F: manufacturer C2, type 25, capacity 37.
static const uint8_t JEDEC_MFR = 0xC2;
static const uint8_t JEDEC_TYPE = 0x25;
static const uint8_t JEDEC_CAP = 0x37;

static int gMode = 0;              // 0 = master (mode A), 1 = sniffer (mode B)
static volatile bool gSniffRun = false;

// ---------------------------------------------------------------------------
// Raw GPIO helpers (fast paths for bit-banging / sniffing)

static inline void csHigh() {
  digitalWrite(PIN_CS, HIGH);
}
static inline void csLow() {
  digitalWrite(PIN_CS, LOW);
}

static inline uint8_t spiShiftByte(uint8_t out) {
  // Bit-bang one byte: change MOSI on falling edge, sample MISO on rising edge.
  uint8_t in = 0;
  for (int bit = 7; bit >= 0; --bit) {
    digitalWrite(PIN_MOSI, (out >> bit) & 1 ? HIGH : LOW);
    digitalWrite(PIN_SCK, LOW);
    digitalWrite(PIN_SCK, HIGH);  // sample point
    if (digitalRead(PIN_MISO)) {
      in |= (1u << bit);
    }
    digitalWrite(PIN_SCK, LOW);
  }
  return in;
}

static void flashReadBegin(uint32_t addr) {
  csLow();
  spiShiftByte(0x03);                       // READ command
  spiShiftByte((addr >> 16) & 0xff);
  spiShiftByte((addr >> 8) & 0xff);
  spiShiftByte(addr & 0xff);
}

static uint8_t flashReadByte() {
  return spiShiftByte(0x00);
}

static void flashReadEnd() {
  csHigh();
}

static bool flashReadJedec(uint8_t id[3]) {
  csLow();
  spiShiftByte(0x9F);
  id[0] = spiShiftByte(0x00);
  id[1] = spiShiftByte(0x00);
  id[2] = spiShiftByte(0x00);
  csHigh();
  return id[0] == JEDEC_MFR && id[1] == JEDEC_TYPE && id[2] == JEDEC_CAP;
}

// ---------------------------------------------------------------------------
// Mode A: hex dump over serial

static void dumpRegion(uint32_t start, uint32_t end) {
  if (end > FLASH_SIZE) {
    end = FLASH_SIZE;
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
    flashReadBegin(addr);
    for (uint32_t i = 0; i < chunk; ++i) {
      line[i] = flashReadByte();
    }
    flashReadEnd();

    // Print in uppercase hex, 32 bytes per line, offset prefix.
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

// Integrity: read the region twice and compare; report first mismatch.
static bool verifyRegion(uint32_t start, uint32_t end) {
  const uint32_t step = 64;
  uint8_t a[step], b[step];
  for (uint32_t addr = start; addr < end; addr += step) {
    uint32_t chunk = min((uint32_t)step, end - addr);
    flashReadBegin(addr);
    for (uint32_t i = 0; i < chunk; ++i) {
      a[i] = flashReadByte();
    }
    flashReadEnd();
    delay(2);
    flashReadBegin(addr);
    for (uint32_t i = 0; i < chunk; ++i) {
      b[i] = flashReadByte();
    }
    flashReadEnd();
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
// Mode B: passive boot sniffer (no drive on any pin)
//
// A spin-loop on core 0 watches CS/SCK/MISO in the GPIO input register and
// samples MISO on rising SCK edges while CS is low. Decodes 0x03/0x0B reads:
// after the command byte come 24 address bits, then data bytes (0x0B skips 8
// dummy bits after the address). Captured bytes print with their flash
// address; a per-256-byte-page coverage map is printed on 'x'. Timing is the
// limit: the onboard SoC may clock the flash faster than this poller can
// reliably sample -- treat mode B results as best-effort and cross-check
// against a mode A dump when in doubt.

static volatile bool gSniffRun = false;
static volatile uint32_t gSniffBytes = 0;
static volatile uint32_t gSniffReads = 0;
static uint8_t gCoverage[0x10000 / 256];   // one bit per 256-byte page, first 64 KiB
static uint8_t gCaptured[0x10000 / 256];   // last byte seen per page (first 64 KiB)
static volatile int gAddrBytes = 0;        // address byte counter within one read cmd

// Collects decoded bytes and updates coverage; called from the sniff task.
static void sniffRecordByte(uint32_t addr, uint8_t byteVal) {
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

static void sniffTask(void*) {
  const uint32_t SCK_BIT = (1UL << PIN_SCK);
  const uint32_t CS_BIT = (1UL << PIN_CS);
  const uint32_t MISO_BIT = (1UL << PIN_MISO);

  bool inTransaction = false;
  int bitCount = 0;
  uint32_t shift = 0;
  int phase = 0;            // 0=cmd 1=addr 2=dummy 3=data
  uint8_t cmd = 0;
  uint32_t addr = 0;

  while (gSniffRun) {
    uint32_t regs = GPIO.in;
    if ((regs & CS_BIT) == 0) {
      if (!inTransaction) {
        inTransaction = true;
        bitCount = 0;
        shift = 0;
        phase = 0;
        addr = 0;
        gAddrBytes = 0;
      }
      bool prevSck = false;
      while (gSniffRun && ((GPIO.in & CS_BIT) == 0)) {
        uint32_t now = GPIO.in;
        bool sck = (now & SCK_BIT) != 0;
        if (sck && !prevSck) {
          shift = (shift << 1) | ((now & MISO_BIT) ? 1 : 0);
          if (++bitCount == 8) {
            bitCount = 0;
            uint8_t byteVal = (uint8_t)(shift & 0xff);
            shift = 0;
            if (phase == 0) {
              cmd = byteVal;
              phase = (cmd == 0x03 || cmd == 0x0B) ? 1 : 4;  // 4 = ignore rest
              if (phase == 1) {
                gSniffReads++;
              }
            } else if (phase == 1) {
              addr = (addr << 8) | byteVal;
              // 24 address bits = 3 bytes; byte #2 -> count to 3
              if (++gAddrBytes >= 3) {
                phase = (cmd == 0x0B) ? 2 : 3;
              }
            } else if (phase == 2) {
              phase = 3;   // 8 dummy bits for 0x0B
            } else if (phase == 3) {
              sniffRecordByte(addr, byteVal);
              addr = (addr + 1) & 0xffffff;
            }
          }
        }
        prevSck = sck;
      }
      inTransaction = false;
    }
    delayMicroseconds(50);
  }
  vTaskDelete(nullptr);
}

static void startSniffer() {
  memset(gCoverage, 0, sizeof(gCoverage));
  memset(gCaptured, 0, sizeof(gCaptured));
  gSniffBytes = 0;
  gSniffReads = 0;
  gSniffRun = true;
  xTaskCreatePinnedToCore(sniffTask, "sniff", 4096, nullptr, 1, nullptr, 0);
  Serial.println("Sniffer running -- now power-cycle / boot the Plus+");
}

static void stopSniffer() {
  gSniffRun = false;
  delay(50);
  Serial.print("SNIFF-STOP bytes=");
  Serial.print(gSniffBytes);
  Serial.print(" reads=");
  Serial.println(gSniffReads);
  Serial.println("Captured bytes (page addr : last byte on that page):");
  for (uint32_t page = 0; page < sizeof(gCoverage); ++page) {
    if (gCoverage[page]) {
      Serial.print((page << 8), HEX);
      Serial.print(":");
      if (gCaptured[page] < 0x10) Serial.print('0');
      Serial.print(gCaptured[page], HEX);
      Serial.print(" ");
    }
  }
  Serial.println();
  Serial.println("Needs: every page from 0x2000-0x21a2 (FBCT) and 0x3000-0x3288 (SAV).");
  Serial.println("Missing pages? Reboot the Plus+ with the sniffer running again.");
}

// ---------------------------------------------------------------------------
// Console + setup

static void printHelp() {
  Serial.println("m=switch mode (0=master/dumper, 1=passive sniffer)");
  Serial.println("  mode 0: j=JEDEC check, d=dump default region, D <start> <end>=dump range, v=verify region");
  Serial.println("  mode 1: s=start sniff (then boot the Plus+), x=stop + coverage report");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== PGPP flash dump tool ===");
  Serial.println("Wiring: CS=IO32 SCK=IO33 MOSI=IO25 MISO=IO26 (mode A adds series resistors)");
  pinMode(PIN_CS, OUTPUT);
  pinMode(PIN_SCK, OUTPUT);
  pinMode(PIN_MOSI, OUTPUT);
  pinMode(PIN_MISO, INPUT_PULLDOWN);
  csHigh();
  digitalWrite(PIN_SCK, LOW);
  digitalWrite(PIN_MOSI, LOW);
  printHelp();
}

void loop() {
  if (!Serial.available()) {
    delay(5);
    return;
  }
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) {
    return;
  }
  char c0 = line.charAt(0);

  if (c0 == 'm') {
    gMode = gMode ? 0 : 1;
    Serial.print("Mode -> ");
    Serial.println(gMode ? "1 passive sniffer" : "0 master/dumper");
    return;
  }
  if (c0 == 'j') {
    uint8_t id[3];
    if (flashReadJedec(id)) {
      Serial.print("JEDEC OK: ");
    } else {
      Serial.print("JEDEC UNEXPECTED (check wiring/power): ");
    }
    for (int i = 0; i < 3; ++i) {
      if (id[i] < 0x10) Serial.print('0');
      Serial.print(id[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
    return;
  }
  if (c0 == 'd') {
    dumpRegion(DEFAULT_DUMP_START, DEFAULT_DUMP_END);
    verifyRegion(DEFAULT_DUMP_START, DEFAULT_DUMP_END);
    return;
  }
  if (c0 == 'D') {
    uint32_t start = 0, end = 0;
    sscanf(line.c_str(), "D %lx %lx", &start, &end);
    if (start >= end || end > FLASH_SIZE) {
      Serial.println("Bad range");
      return;
    }
    dumpRegion(start, end);
    return;
  }
  if (c0 == 'v') {
    verifyRegion(DEFAULT_DUMP_START, DEFAULT_DUMP_END);
    return;
  }
  if (c0 == 's') {
    gAddrBytes = 0;
    startSniffer();
    return;
  }
  if (c0 == 'x') {
    stopSniffer();
    return;
  }
  printHelp();
}
