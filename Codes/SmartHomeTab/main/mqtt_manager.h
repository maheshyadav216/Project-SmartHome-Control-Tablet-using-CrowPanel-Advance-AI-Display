#pragma once
#include "esp_err.h"

void mqtt_manager_start(const char *broker_uri, const char *user, const char *pass);
void mqtt_manager_publish(const char *topic, const char *payload);
