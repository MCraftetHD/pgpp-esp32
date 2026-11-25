#include <Arduino.h>
#include <esp_sleep.h>

#include "nrf_soc.h"

static uint32_t gWakePin = 0;

void pp_soc_set_wake_pin(uint32_t gpio)
{
  gWakePin = gpio;
}

uint32_t sd_power_system_off(void)
{
  // Classic ESP32: deep sleep stands in for the nRF's System OFF. The BOOT
  // button (RTC-capable GPIO) wakes the board on the configured level.
  // RTC-capable GPIOs on the classic ESP32 only.
  switch (gWakePin) {
    case 0: case 2: case 4:
    case 12: case 13: case 14: case 15:
    case 25: case 26: case 27:
    case 32: case 33: case 34: case 35:
    case 36: case 37: case 38: case 39:
      esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(gWakePin), 0);  // wake on LOW
      break;
    default:
      Serial.println("Wake pin is not RTC-capable; sleeping without a wake source (reset to wake)");
      break;
  }
  esp_deep_sleep_start();
  return NRF_SUCCESS;  // not reached
}
