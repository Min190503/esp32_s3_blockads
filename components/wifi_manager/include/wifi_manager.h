#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"

esp_err_t wifi_manager_start(void);

bool wifi_manager_is_connected(void);

esp_err_t wifi_manager_wait_for_ip(uint32_t timeout_ms);

#endif