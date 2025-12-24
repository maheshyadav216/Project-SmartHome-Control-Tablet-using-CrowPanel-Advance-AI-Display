#include "mqtt_manager.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "lvgl.h"
#include <string.h>
#include <stdlib.h>

extern void ui_handle_mqtt_message_async(void *arg);

static const char *TAG = "MQTT_MGR";
static esp_mqtt_client_handle_t client = NULL;

// temporary async storage
static void *g_async_pkg = NULL;

// forward declaration
static void ui_async_timer_cb(lv_timer_t *timer);

// ------------------------------------------------------------
// MQTT EVENT HANDLER (compatible with ESP-IDF v5.5)
// ------------------------------------------------------------
static void mqtt_event_handler(void *handler_args,
                               esp_event_base_t base,
                               int32_t event_id,
                               void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch (event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected");

        // Subscribe to sensor topics
        esp_mqtt_client_subscribe(client, "home/roomhub/sensor/temperature", 1);
        esp_mqtt_client_subscribe(client, "home/roomhub/sensor/humidity", 1);
        esp_mqtt_client_subscribe(client, "home/roomhub/sensor/light", 1);

        // Subscribe to relay state topics
        esp_mqtt_client_subscribe(client, "home/roomhub/relay/ac/state", 1);
        esp_mqtt_client_subscribe(client, "home/roomhub/relay/fan/state", 1);
        esp_mqtt_client_subscribe(client, "home/roomhub/relay/tv/state", 1);
        esp_mqtt_client_subscribe(client, "home/roomhub/relay/bulb/state", 1);
        // Subscribe to Automation state topic
        esp_mqtt_client_subscribe(client, "home/roomhub/auto/all/state", 1);
        break;


    case MQTT_EVENT_DATA: {
    char *topic = strndup(event->topic, event->topic_len);
    char *payload = strndup(event->data, event->data_len);

    typedef struct {
        char *topic;
        char *payload;
    } mqtt_async_t;

    mqtt_async_t *pkg = malloc(sizeof(mqtt_async_t));
    pkg->topic = topic;
    pkg->payload = payload;

    g_async_pkg = pkg;

    // Create a one-shot timer (NO user_data, LVGL-safe)
    lv_timer_t *t = lv_timer_create(ui_async_timer_cb, 1, NULL);
    lv_timer_set_repeat_count(t, 1);
    break;
}


    default:
        break;
    }
}

// ------------------------------------------------------------
// Timer callback → runs in LVGL thread (safe)
// ------------------------------------------------------------
static void ui_async_timer_cb(lv_timer_t *timer)
{
    (void) timer;

    if (g_async_pkg) {
        ui_handle_mqtt_message_async(g_async_pkg);
        g_async_pkg = NULL;
    }
}


// ------------------------------------------------------------
// START MQTT CLIENT — (ESP-IDF v5.5 API)
// ------------------------------------------------------------
void mqtt_manager_start(const char *broker_uri,
                        const char *user,
                        const char *pass)
{
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = broker_uri,
        .credentials.username = user,
        .credentials.authentication.password = pass
    };

    client = esp_mqtt_client_init(&cfg);

    esp_mqtt_client_register_event(
        client,
        ESP_EVENT_ANY_ID,
        mqtt_event_handler,
        NULL
    );

    esp_mqtt_client_start(client);

    ESP_LOGI(TAG, "MQTT client started");
}


// ------------------------------------------------------------
// PUBLISH
// ------------------------------------------------------------
void mqtt_manager_publish(const char *topic, const char *payload)
{
    if (!client) return;
    esp_mqtt_client_publish(client, topic, payload, 0, 1, false);
}
