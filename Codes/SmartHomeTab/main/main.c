//=============================================================================//
// Project/Tutorial       - SmartHome Control Tablet using CrowPanel Advance 7" Display
// Author                 - https://www.hackster.io/maheshyadav216
// Hardware               - Elecrow CrowPanel Advance 7" HMI AI Display    
// Sensors                - NA
// Software               - Visual Studio Code, ESP-IDF Extension
// GitHub Repo of Project - https://github.com/maheshyadav216/Project-SmartHome-Control-Tablet-using-CrowPanel-Advance-AI-Display 
// Code last Modified on  - 24/12/2025
// Code/Content license   - (CC BY-NC-SA 4.0) https://creativecommons.org/licenses/by-nc-sa/4.0/
//============================================================================//

#include "waveshare_rgb_lcd_port.h"
#include "ui.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include "wifi_manager.h"
#include "mqtt_manager.h"
#include "ui_mqtt_bridge.h"
#include "esp_sntp.h"
#include "esp_timer.h"

// ---------------------------------------------------------------------
// I2C Config
// ---------------------------------------------------------------------
#define I2C_MASTER_SDA_IO          15
#define I2C_MASTER_SCL_IO          16
#define I2C_MASTER_TX_BUF_DISABLE  0
#define I2C_MASTER_RX_BUF_DISABLE  0
#define I2C_MASTER_NUM             I2C_NUM_0
#define I2C_MASTER_FREQ_HZ         100000

// Device addresses
#define DEVICE_ADDR_BACKLIGHT  0x30
#define DEVICE_ADDR_TOUCH      0x5D
#define DEVICE_ADDR_RTC        0x51

// PCF8563 registers
#define PCF8563_REG_CTRL1    0x00
#define PCF8563_REG_CTRL2    0x01
#define PCF8563_REG_SEC      0x02
#define PCF8563_REG_MIN      0x03
#define PCF8563_REG_HOUR     0x04
#define PCF8563_REG_DAY      0x05
#define PCF8563_REG_WDAY     0x06
#define PCF8563_REG_MONTH    0x07
#define PCF8563_REG_YEAR     0x08

// ---------------------------------------------------------------------
// BCD helpers
// ---------------------------------------------------------------------
static uint8_t dec2bcd(uint8_t dec) { return ((dec / 10) << 4) | (dec % 10); }
static uint8_t bcd2dec(uint8_t bcd) { return ((bcd >> 4) * 10) + (bcd & 0x0F); }

// ---------------------------------------------------------------------
// I2C INIT
// ---------------------------------------------------------------------
void i2c_master_init()
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode,
                                       I2C_MASTER_RX_BUF_DISABLE,
                                       I2C_MASTER_TX_BUF_DISABLE, 0));
}

bool i2c_scan_address(uint8_t address)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    esp_err_t r = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 50 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return r == ESP_OK;
}

esp_err_t i2c_write_byte(uint8_t dev, uint8_t data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    esp_err_t r = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 50 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return r;
}

// ---------------------------------------------------------------------
// RTC FUNCTIONS
// ---------------------------------------------------------------------
static esp_err_t rtc_write_reg(uint8_t reg, uint8_t data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DEVICE_ADDR_RTC << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    esp_err_t r = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 200 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return r;
}

static esp_err_t rtc_read_regs(uint8_t reg, uint8_t *data, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DEVICE_ADDR_RTC << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DEVICE_ADDR_RTC << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, len, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t r = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 200 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return r;
}

static esp_err_t rtc_init(void)
{
    if (rtc_write_reg(PCF8563_REG_CTRL1, 0x00) != ESP_OK) return ESP_FAIL;
    return rtc_write_reg(PCF8563_REG_CTRL2, 0x00);
}

static esp_err_t rtc_set_time(const struct tm *t)
{
    uint8_t d[7];
    d[0] = dec2bcd(t->tm_sec) & 0x7F;
    d[1] = dec2bcd(t->tm_min) & 0x7F;
    d[2] = dec2bcd(t->tm_hour) & 0x3F;
    d[3] = dec2bcd(t->tm_mday) & 0x3F;
    d[4] = t->tm_wday & 0x07;
    d[5] = dec2bcd(t->tm_mon + 1) & 0x1F;
    d[6] = dec2bcd(t->tm_year % 100);

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DEVICE_ADDR_RTC << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, PCF8563_REG_SEC, true);
    i2c_master_write(cmd, d, 7, true);
    i2c_master_stop(cmd);

    esp_err_t r = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 200 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return r;
}

static esp_err_t rtc_get_time(struct tm *t)
{
    uint8_t d[7];
    if (rtc_read_regs(PCF8563_REG_SEC, d, 7) != ESP_OK) return ESP_FAIL;

    t->tm_sec  = bcd2dec(d[0] & 0x7F);
    t->tm_min  = bcd2dec(d[1] & 0x7F);
    t->tm_hour = bcd2dec(d[2] & 0x3F);
    t->tm_mday = bcd2dec(d[3] & 0x3F);
    t->tm_wday = d[4] & 0x07;
    t->tm_mon  = bcd2dec(d[5] & 0x1F) - 1;
    t->tm_year = bcd2dec(d[6]) + 100; // 2000+
    return ESP_OK;
}

// ---------------------------------------------------------------------
// SNTP SYNC (silent, timeout 30s, update RTC once) - TZ fixed (IST)
// ---------------------------------------------------------------------
static bool sntp_sync_rtc_once(void)
{
    // Set timezone to IST (UTC+5:30). POSIX form "IST-5:30" works on ESP-IDF.
    setenv("TZ", "IST-5:30", 1);
    tzset();

    // Configure SNTP (silent, polling)
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();

    const int64_t start = esp_timer_get_time() / 1000; // ms
    const int64_t timeout_ms = 30000;

    while ((esp_timer_get_time() / 1000 - start) < timeout_ms)
    {
        time_t now = 0;
        struct tm timeinfo = {0};
        time(&now);
        localtime_r(&now, &timeinfo);

        // If year looks valid ( > 1970 ), break and use it
        if (timeinfo.tm_year > (1970 - 1900)) {
            // write the local time (IST) to RTC once
            rtc_set_time(&timeinfo);
            // Stop SNTP quietly
            esp_sntp_stop();
            return true;
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }

    // Timeout — stop SNTP and keep existing RTC
    esp_sntp_stop();
    return false;
}


// ---------------------------------------------------------------------
// RTC Task — SAME AS BEFORE
// ---------------------------------------------------------------------
void rtc_display_task(void *arg)
{
    const char *days[] = {
        "Sunday","Monday","Tuesday","Wednesday",
        "Thursday","Friday","Saturday"
    };

    struct tm tm_now;

    while (1)
    {
        if (rtc_get_time(&tm_now) == ESP_OK)
        {
            if (lvgl_port_lock(-1))
            {
                char buf1[16], buf2[16], buf3[32];

                sprintf(buf1, "%02d:%02d:%02d",
                        tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);
                sprintf(buf2, "%s", days[tm_now.tm_wday]);
                sprintf(buf3, "%04d-%02d-%02d",
                        tm_now.tm_year + 1900,
                        tm_now.tm_mon + 1,
                        tm_now.tm_mday);

                lv_label_set_text(uic_time, buf1);
                lv_label_set_text(uic_day, buf2);
                lv_label_set_text(uic_date, buf3);

                lvgl_port_unlock();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ---------------------------------------------------------------------
// WiFi callback
// ---------------------------------------------------------------------
static void on_wifi_got_ip(void)
{
    // First SNTP sync → update RTC once
    sntp_sync_rtc_once();

    // MQTT
    mqtt_manager_start("mqtt://192.168.0.154:1883",
                       "mqttuser", "mqttpassword");

    ui_mqtt_bridge_init();
}


// ---------------------------------------------------------------------
// MAIN — minimal changes
// ---------------------------------------------------------------------
void app_main()
{
    ESP_LOGI("MAIN", "Starting System...");

    i2c_master_init();
    vTaskDelay(pdMS_TO_TICKS(50));

    i2c_write_byte(DEVICE_ADDR_BACKLIGHT, 0x18);
    i2c_write_byte(DEVICE_ADDR_BACKLIGHT, 0x10);

    if (rtc_init() == ESP_OK)
        ESP_LOGI("MAIN", "RTC OK");


    waveshare_esp32_s3_rgb_lcd_init();

    if (lvgl_port_lock(-1))
    {
        ui_init();

        wifi_manager_init("xxxx", "xxxx", on_wifi_got_ip);

        lv_label_set_text(uic_time, "00:00:00");
        lv_label_set_text(uic_day,  "Loading...");
        lv_label_set_text(uic_date, "0000-00-00");

        lvgl_port_unlock();
    }

    xTaskCreate(rtc_display_task, "rtc_display_task", 4096, NULL, 5, NULL);

    ESP_LOGI("MAIN", "System ready!");
}
