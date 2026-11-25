#pragma once
#include <cstdint>
typedef int gpio_num_t;
inline void esp_deep_sleep_start() {}
inline void esp_sleep_enable_ext0_wakeup(gpio_num_t, int) {}
