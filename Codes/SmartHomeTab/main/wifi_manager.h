#pragma once
#include "esp_err.h"

typedef void (*wifi_got_ip_cb_t)(void);

esp_err_t wifi_manager_init(const char *ssid, const char *pass, wifi_got_ip_cb_t cb);
void wifi_manager_deinit(void);
