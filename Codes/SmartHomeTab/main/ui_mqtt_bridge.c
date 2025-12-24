#include "ui_mqtt_bridge.h"
#include "mqtt_manager.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_timer.h" 
#include "driver/i2c.h" 

static bool is_muted = false; 

// --- Extended Backlight Settings ---
#define BRIGHTNESS_FULL    0    
#define BRIGHTNESS_DIM     200  
#define BRIGHTNESS_OFF     245   
#define DIM_TIMEOUT_MS     60000 
#define OFF_TIMEOUT_MS     300000 

static bool is_dimmed = false;

typedef enum {
    STATE_BRIGHT,
    STATE_DIMMED,
    STATE_OFF
} backlight_state_t;

static backlight_state_t current_backlight_state = STATE_BRIGHT;

#define TEMP_THRESHOLD_HIGH 27.0f
#define TEMP_THRESHOLD_LOW  26.0f
#define ALARM_INTERVAL_MS   60000  

static bool alarm_active = false;
static int64_t last_alarm_time = 0;

#define DEVICE_ADDR_STC8   0x30
#define CMD_V13_BUZZER_ON  0xF6
#define CMD_V13_BUZZER_OFF 0xF7

static const char *TAG = "UI_MQTT_BRIDGE";

static float to_float(const char *s) { return atof(s); }

static void ui_update_label(lv_obj_t *label, const char *value) {
    if (label == NULL) return;
    lv_label_set_text(label, value);
}

static void ui_update_bar(lv_obj_t *bar, int value) {
    if (bar == NULL) return;
    lv_bar_set_value(bar, value, LV_ANIM_OFF);
}

static int clamp_0_100(float v) {
    if (v < 0) return 0;
    if (v > 100) return 100;
    return (int)v;
}

static void trigger_temp_alert() {
    uint8_t on = CMD_V13_BUZZER_ON;
    uint8_t off = CMD_V13_BUZZER_OFF;
    i2c_master_write_to_device(I2C_NUM_0, DEVICE_ADDR_STC8, &on, 1, pdMS_TO_TICKS(100));
    vTaskDelay(pdMS_TO_TICKS(100));
    i2c_master_write_to_device(I2C_NUM_0, DEVICE_ADDR_STC8, &off, 1, pdMS_TO_TICKS(100));
    vTaskDelay(pdMS_TO_TICKS(150)); 
    i2c_master_write_to_device(I2C_NUM_0, DEVICE_ADDR_STC8, &on, 1, pdMS_TO_TICKS(100));
    vTaskDelay(pdMS_TO_TICKS(100));
    i2c_master_write_to_device(I2C_NUM_0, DEVICE_ADDR_STC8, &off, 1, pdMS_TO_TICKS(100));
}

void ui_handle_mqtt_message_async(void *arg)
{
    typedef struct { char *topic; char *payload; } mqtt_async_data_t;
    mqtt_async_data_t *d = (mqtt_async_data_t *)arg;
    const char *topic = d->topic;
    const char *msg   = d->payload;

    if (strcmp(topic, "home/roomhub/sensor/temperature") == 0) {
        float t = to_float(msg);
        int64_t now = esp_timer_get_time() / 1000;
        if (t >= TEMP_THRESHOLD_HIGH) alarm_active = true;
        else if (t < TEMP_THRESHOLD_LOW) alarm_active = false;

        if (alarm_active && !is_muted) {
            if (now - last_alarm_time >= ALARM_INTERVAL_MS) {
                trigger_temp_alert();
                last_alarm_time = now;
            }
        }
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2f", t);
        ui_update_label(uic_temperature, buf);
        ui_update_bar(uic_tempBar, clamp_0_100(t));
        if (uic_tempChart) {
            lv_chart_series_t *s = lv_chart_get_series_next(uic_tempChart, NULL);
            if (s) lv_chart_set_next_value(uic_tempChart, s, t);
        }
    }
    else if (strcmp(topic, "home/roomhub/sensor/humidity") == 0) {
        float h = to_float(msg);
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2f", h);
        ui_update_label(uic_humidity, buf);
        ui_update_bar(uic_humiBar, clamp_0_100(h));
        if (uic_humiChart) {
            lv_chart_series_t *s = lv_chart_get_series_next(uic_humiChart, NULL);
            if (s) lv_chart_set_next_value(uic_humiChart, s, h);
        }
    }
    else if (strcmp(topic, "home/roomhub/sensor/light") == 0) {
        float percent_float = to_float(msg); 
        int p = clamp_0_100(percent_float);
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", p);
        ui_update_label(uic_light, buf);
        ui_update_bar(uic_lightBar, p);
        if (uic_lightChart) {
            lv_chart_series_t *s = lv_chart_get_series_next(uic_lightChart, NULL);
            if (s) lv_chart_set_next_value(uic_lightChart, s, p);
        }
    }

    // --- MODIFIED AUTO STATE LOGIC (Unchecked = Enabled, Checked = Disabled) ---
    if (strcmp(topic, "home/roomhub/auto/all/state") == 0) {
        if (uic_autoDisBtn) {
            // RoomHub says ON (Enabled) -> UI Unchecked
            if (strcmp(msg, "ON") == 0) {
                lv_obj_clear_state(uic_autoDisBtn, LV_STATE_CHECKED);
            } 
            // RoomHub says OFF (Disabled) -> UI Checked
            else {
                lv_obj_add_state(uic_autoDisBtn, LV_STATE_CHECKED);
            }
        }
    }

    else if (strcmp(topic, "home/roomhub/relay/ac/state") == 0) {
        if (uic_ac) (strcmp(msg, "ON") == 0) ? lv_obj_add_state(uic_ac, LV_STATE_CHECKED) : lv_obj_clear_state(uic_ac, LV_STATE_CHECKED);
    }
    else if (strcmp(topic, "home/roomhub/relay/fan/state") == 0) {
        if (uic_fan) (strcmp(msg, "ON") == 0) ? lv_obj_add_state(uic_fan, LV_STATE_CHECKED) : lv_obj_clear_state(uic_fan, LV_STATE_CHECKED);
    }
    else if (strcmp(topic, "home/roomhub/relay/tv/state") == 0) {
        if (uic_tv) (strcmp(msg, "ON") == 0) ? lv_obj_add_state(uic_tv, LV_STATE_CHECKED) : lv_obj_clear_state(uic_tv, LV_STATE_CHECKED);
    }
    else if (strcmp(topic, "home/roomhub/relay/bulb/state") == 0) {
        if (uic_bulb) (strcmp(msg, "ON") == 0) ? lv_obj_add_state(uic_bulb, LV_STATE_CHECKED) : lv_obj_clear_state(uic_bulb, LV_STATE_CHECKED);
    }

    free(d->topic); free(d->payload); free(d);
}

static void ac_event_cb(lv_event_t * e) {
    bool on = lv_obj_has_state(uic_ac, LV_STATE_CHECKED);
    mqtt_manager_publish("home/roomhub/relay/ac/cmd", on ? "ON" : "OFF");
}
static void fan_event_cb(lv_event_t * e) {
    bool on = lv_obj_has_state(uic_fan, LV_STATE_CHECKED);
    mqtt_manager_publish("home/roomhub/relay/fan/cmd", on ? "ON" : "OFF");
}
static void tv_event_cb(lv_event_t * e) {
    bool on = lv_obj_has_state(uic_tv, LV_STATE_CHECKED);
    mqtt_manager_publish("home/roomhub/relay/tv/cmd", on ? "ON" : "OFF");
}
static void bulb_event_cb(lv_event_t * e) {
    bool on = lv_obj_has_state(uic_bulb, LV_STATE_CHECKED);
    mqtt_manager_publish("home/roomhub/relay/bulb/cmd", on ? "ON" : "OFF");
}

void mute_btn_event_cb(lv_event_t * e) {
    lv_obj_t * obj = lv_event_get_target(e);
    if(lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        is_muted = lv_obj_has_state(obj, LV_STATE_CHECKED);
        if (is_muted) {
            uint8_t off = CMD_V13_BUZZER_OFF;
            i2c_master_write_to_device(I2C_NUM_0, DEVICE_ADDR_STC8, &off, 1, pdMS_TO_TICKS(100));
        }
    }
}

// --- MODIFIED AUTO CALLBACK (Checked = Disabled, Unchecked = Enabled) ---
static void auto_all_event_cb(lv_event_t * e)
{
    // If the button is Checked, we send "OFF" to disable.
    // If Unchecked, we send "ON" to enable.
    bool is_checked = lv_obj_has_state(uic_autoDisBtn, LV_STATE_CHECKED);
    mqtt_manager_publish("home/roomhub/auto/all/cmd", is_checked ? "OFF" : "ON");
}

static void fade_in_backlight() {
    for (int b = BRIGHTNESS_DIM; b >= BRIGHTNESS_FULL; b -= 5) {
        uint8_t val = (uint8_t)b;
        i2c_master_write_to_device(I2C_NUM_0, DEVICE_ADDR_STC8, &val, 1, pdMS_TO_TICKS(50));
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}

static void update_backlight_dimming() {
    uint32_t idle_time = lv_disp_get_inactive_time(NULL); 
    if (idle_time > OFF_TIMEOUT_MS) {
        if (current_backlight_state != STATE_OFF) {
            uint8_t val = BRIGHTNESS_OFF;
            i2c_master_write_to_device(I2C_NUM_0, DEVICE_ADDR_STC8, &val, 1, pdMS_TO_TICKS(100));
            current_backlight_state = STATE_OFF;
            lv_obj_clear_flag(uic_WakePanel, LV_OBJ_FLAG_HIDDEN); 
        }
    } else if (idle_time > DIM_TIMEOUT_MS) {
        if (current_backlight_state != STATE_DIMMED) {
            uint8_t val = BRIGHTNESS_DIM;
            i2c_master_write_to_device(I2C_NUM_0, DEVICE_ADDR_STC8, &val, 1, pdMS_TO_TICKS(100));
            current_backlight_state = STATE_DIMMED;
            lv_obj_clear_flag(uic_WakePanel, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        if (current_backlight_state != STATE_BRIGHT) {
            fade_in_backlight();
            current_backlight_state = STATE_BRIGHT;
            if(uic_WakePanel) lv_obj_add_flag(uic_WakePanel, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void dimming_timer_cb(lv_timer_t * timer) { update_backlight_dimming(); }

void ui_mqtt_bridge_init(void)
{
    lv_obj_set_parent(uic_WakePanel, lv_layer_sys());
    if (uic_ac) lv_obj_add_event_cb(uic_ac, ac_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    if (uic_fan) lv_obj_add_event_cb(uic_fan, fan_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    if (uic_tv) lv_obj_add_event_cb(uic_tv, tv_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    if (uic_bulb) lv_obj_add_event_cb(uic_bulb, bulb_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    if (uic_MuteBtn) lv_obj_add_event_cb(uic_MuteBtn, mute_btn_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    if (uic_autoDisBtn) lv_obj_add_event_cb(uic_autoDisBtn, auto_all_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_timer_create(dimming_timer_cb, 500, NULL);
}