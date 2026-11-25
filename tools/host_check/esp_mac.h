#pragma once
#include <cstdint>
typedef enum { ESP_MAC_WIFI_STA = 0, ESP_MAC_WIFI_SOFTAP, ESP_MAC_BT, ESP_MAC_ETH } esp_mac_type_t;
int esp_read_mac(uint8_t* mac, esp_mac_type_t type);
inline esp_err_t esp_iface_mac_addr_set(const uint8_t* mac, esp_mac_type_t type) { return ESP_OK; }
