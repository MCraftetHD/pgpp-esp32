#pragma once
// Board pin + behaviour configuration for the ESP32-DevKitC build.
// Change these to match your board. All values can be overridden from the
// build command line (-D...) or a copy of this file.

// ---- Buttons ----
// Button A: the BOOT button (GPIO0). Active low with the board's pull-up.
// Click = manual catch/spin press, hold 5 s = wipe pairing records.
#ifndef PP_BTN_A_PIN
#define PP_BTN_A_PIN 0
#endif
#ifndef PP_BTN_A_ACTIVE_LOW
#define PP_BTN_A_ACTIVE_LOW 1
#endif

// Button B: the "D2" button. On DevKitC-compatible boards GPIO2 carries the
// onboard blue LED, so the default assumes the button pulls the pin HIGH
// (active high) with an internal pulldown. If your button pulls to GND, set
// PP_BTN_B_ACTIVE_LOW to 1.
#ifndef PP_BTN_B_PIN
#define PP_BTN_B_PIN 2
#endif
#ifndef PP_BTN_B_ACTIVE_LOW
#define PP_BTN_B_ACTIVE_LOW 0
#endif
// Click cycles the catch mode; hold 3 s toggles auto spin.
#ifndef PP_BTN_B_HOLD_MS
#define PP_BTN_B_HOLD_MS 3000
#endif

// ---- LED ----
// The DevKitC's onboard LED is GPIO2 (blue, active high: drive HIGH to light).
// Set PP_LED_ACTIVE_LOW to 1 for boards whose LED pulls low, or rewire an
// external LED + resistor (1 kOhm) from another GPIO to GND.
#ifndef PP_LED_PIN
#define PP_LED_PIN 2
#endif
#ifndef PP_LED_ACTIVE_LOW
#define PP_LED_ACTIVE_LOW 0
#endif

// ---- Deep sleep ----
// After this many minutes with no connection the board deep-sleeps and waits
// for the BOOT button (GPIO0, ext0 wake on LOW). 0 = never sleep (USB power).
#ifndef PP_SLEEP_AFTER_DISCONNECTED_MIN
#define PP_SLEEP_AFTER_DISCONNECTED_MIN 5
#endif

// ---- Serial console ----
#ifndef PP_SERIAL_BAUD
#define PP_SERIAL_BAUD 115200
#endif
