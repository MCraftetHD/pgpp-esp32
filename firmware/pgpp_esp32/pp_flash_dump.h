#pragma once
#include <stdint.h>

// Built-in PGPP external-flash reader for the serial console.
//
// Lets the emulator firmware itself act as the reader for the donor Plus+'s
// MX25U6433F flash chip (four solder taps, see docs/keys_extraction.md), so
// ONE firmware image covers everything: dump the keys, upload them with the
// console 'k' command, then pair -- no firmware swapping, no programmer.
//
// Console commands (routed via ppFlashDumpConsoleLine):
//   j            JEDEC ID check (expect C2 25 37)
//   d            dump default region 0x0000-0x10000 + double-read verify
//   D <start> <end>   dump a hex range, e.g.  D 0 800000  (full 8 MiB backup)
//   x            passive boot sniffer: start listening (PGPP stays powered)
//   X            sniffer stop + coverage report
//
// Pins (ESP32-DevKitC V4, all < GPIO32):
//   CS=IO21  SCK=IO22  MOSI=IO23  MISO=IO25   GND common with the PGPP for
//   master mode ('d'); sniffer mode ('x') drives nothing.

// Returns true if the line was consumed as a flash-dump command.
bool ppFlashDumpConsoleLine(const char* line);

// Extra help lines appended to the console '?' help.
void ppFlashDumpHelp();

// True while the passive sniffer is running.
bool ppFlashDumpSnifferRunning();
