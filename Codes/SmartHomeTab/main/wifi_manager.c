#include "wifi_manager.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include <string.h>

static const char *TAG = "WIFI_MGR";

static wifi_got_ip_cb_t g_got_ip_cb = NULL;
static esp_event_handler_instance_t instance_any_id;
static esp_event_handler_instance_t instance_got_ip;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi started -> connecting...");
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi disconnected -> reconnecting...");
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        if (g_got_ip_cb) g_got_ip_cb();
    }
}

esp_err_t wifi_manager_init(const char *ssid, const char *pass, wifi_got_ip_cb_t cb)
{
    g_got_ip_cb = cb;

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_cfg = { 0 };
    strncpy((char*)wifi_cfg.sta.ssid, ssid, sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char*)wifi_cfg.sta.password, pass, sizeof(wifi_cfg.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to SSID: %s", ssid);
    return ESP_OK;
}

void wifi_manager_deinit(void)
{
    esp_event_handler_instance_unregister(IP_EVENT,
                                          IP_EVENT_STA_GOT_IP,
                                          instance_got_ip);
    esp_event_handler_instance_unregister(WIFI_EVENT,
                                          ESP_EVENT_ANY_ID,
                                          instance_any_id);

    esp_wifi_stop();
    esp_wifi_deinit();
    nvs_flash_deinit();
}
