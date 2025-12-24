//=============================================================================//
// Project/Tutorial       - SmartHome Control Tablet using CrowPanel Advance 7" Display
// Device                 - RoomHub
// Author                 - https://www.hackster.io/maheshyadav216
// Hardware               - ESP32-C6,4 Channel Relay module 5V     
// Sensors                - SEN0500 Fermion Multifunctional Environmental Sensor
// Software               - Arduino IDE
// GitHub Repo of Project - https://github.com/maheshyadav216/Project-SmartHome-Control-Tablet-using-CrowPanel-Advance-AI-Display 
// Code last Modified on  - 24/12/2025
// Code/Content license   - (CC BY-NC-SA 4.0) https://creativecommons.org/licenses/by-nc-sa/4.0/
//============================================================================//

/* RoomHub (final + unified auto control) — compact HA discovery + robust MQTT publishes
   ESP32-C6 + SEN0500 + 4-relay (active LOW)
   Mapping (confirmed):
     AC  -> GPIO 3
     Fan -> GPIO 2
     TV  -> GPIO 11
     Bulb-> GPIO 10
   
   AUTO CLIMATE CONTROL:
   - Temperature: AC ON at 27°C, AC OFF at 26°C (hysteresis)
   - Humidity: FAN ON at 54%, FAN OFF at 53% (hysteresis)
   - Light: BULB ON at <1%, BULB OFF at >2% (hysteresis)
   - Single unified control: home/roomhub/auto/all/cmd
*/

#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include "DFRobot_EnvironmentalSensor.h"

// ---------- Edit these ----------
const char* WIFI_SSID = "xxxx";
const char* WIFI_PASS = "xxxx";

const char* MQTT_HOST = "192.168.0.154";
const uint16_t MQTT_PORT = 1883;
const char* MQTT_USER = "mqttuser";
const char* MQTT_PASS = "mqttpassword";
// ---------------------------------

// Climate control thresholds
const float TEMP_HIGH_THRESHOLD = 27.0;  // AC turns ON
const float TEMP_LOW_THRESHOLD  = 26.0;  // AC turns OFF
const float HUMI_HIGH_THRESHOLD = 54.0;  // FAN turns ON
const float HUMI_LOW_THRESHOLD  = 53.0;  // FAN turns OFF
const float LIGHT_LOW_THRESHOLD = 1.0;   // BULB turns ON (below 1%)
const float LIGHT_HIGH_THRESHOLD = 2.0;  // BULB turns OFF (above 2%)

// Single unified auto control flag
bool auto_control_enabled = true;

// Relay pins (active-low)
const uint8_t RELAY_AC   = 3;
const uint8_t RELAY_FAN  = 2;
const uint8_t RELAY_TV   = 11;
const uint8_t RELAY_BULB = 10;

// Sensor
DFRobot_EnvironmentalSensor sensor(0x22, &Wire);

// Topics
const char* T_TEMP  = "home/roomhub/sensor/temperature";
const char* T_HUMI  = "home/roomhub/sensor/humidity";
const char* T_LIGHT = "home/roomhub/sensor/light";

const char* T_AC_CMD   = "home/roomhub/relay/ac/cmd";
const char* T_FAN_CMD  = "home/roomhub/relay/fan/cmd";
const char* T_TV_CMD   = "home/roomhub/relay/tv/cmd";
const char* T_BULB_CMD = "home/roomhub/relay/bulb/cmd";

const char* T_AC_STATE   = "home/roomhub/relay/ac/state";
const char* T_FAN_STATE  = "home/roomhub/relay/fan/state";
const char* T_TV_STATE   = "home/roomhub/relay/tv/state";
const char* T_BULB_STATE = "home/roomhub/relay/bulb/state";

// Unified auto control topic (single switch for all automations)
const char* T_AUTO_ALL_CMD   = "home/roomhub/auto/all/cmd";
const char* T_AUTO_ALL_STATE = "home/roomhub/auto/all/state";

WiFiClient espClient;
PubSubClient mqtt(espClient);

// timing
unsigned long lastSensorMillis = 0;
const unsigned long SENSOR_INTERVAL_MS = 5000;

// last published strings to avoid duplicates
String last_temp_str = "";
String last_humi_str = "";
String last_light_str = "";

// relay state memory
bool relay_ac_on = false;
bool relay_fan_on = false;
bool relay_tv_on = false;
bool relay_bulb_on = false;

// Manual override flags (per device)
bool ac_manual_override = false;
bool fan_manual_override = false;
bool bulb_manual_override = false;

// ---------------- relay helpers ----------------
void setRelayAndPublish(uint8_t pin, bool on, const char* state_topic, bool &state_var, bool is_auto = false) {
  if (on) digitalWrite(pin, LOW);   // active low
  else    digitalWrite(pin, HIGH);
  state_var = on;
  bool ok = mqtt.publish(state_topic, on ? "ON" : "OFF", true);
  
  const char* source = is_auto ? "[AUTO]" : "[MANUAL]";
  Serial.printf("%s publish %s -> %s %s\n", source, state_topic, on ? "ON":"OFF", ok ? "OK":"FAIL");
}

void relayOn(uint8_t pin, const char* state_topic, bool &state_var, bool is_auto = false) {
  setRelayAndPublish(pin, true, state_topic, state_var, is_auto);
}
void relayOff(uint8_t pin, const char* state_topic, bool &state_var, bool is_auto = false) {
  setRelayAndPublish(pin, false, state_topic, state_var, is_auto);
}

// ---------------- helper: robust publish ----------------
bool robustPublish(const char* topic, const char* payload, bool retained = false) {
  bool ok = mqtt.publish(topic, payload, retained);
  mqtt.loop();
  delay(60);
  Serial.printf("PUB %s -> %s (%s)\n", topic, payload, ok ? "OK":"FAIL");
  return ok;
}

// ---------------- publish compact discovery ----------------
void publishDiscovery() {
  Serial.println("🔵 Sending compact Home Assistant discovery...");

  // --- SENSORS ---
  {
    char payload[256];
    snprintf(payload, sizeof(payload),
      "{\"name\":\"roomhub Temp\",\"state_topic\":\"%s\",\"unit_of_measurement\":\"°C\",\"device_class\":\"temperature\",\"unique_id\":\"roomhub_temp\"}",
      T_TEMP);
    robustPublish("homeassistant/sensor/roomhub_temperature/config", payload, true);
  }

  {
    char payload[256];
    snprintf(payload, sizeof(payload),
      "{\"name\":\"roomhub Humi\",\"state_topic\":\"%s\",\"unit_of_measurement\":\"%%\",\"device_class\":\"humidity\",\"unique_id\":\"roomhub_humi\"}",
      T_HUMI);
    robustPublish("homeassistant/sensor/roomhub_humidity/config", payload, true);
  }

  {
    char payload[256];
    snprintf(payload, sizeof(payload),
      "{\"name\":\"roomhub Light\",\"state_topic\":\"%s\",\"unit_of_measurement\":\"%%\",\"unique_id\":\"roomhub_light\"}",
      T_LIGHT);
    robustPublish("homeassistant/sensor/roomhub_light/config", payload, true);
  }

  // --- SWITCHES ---
  auto sendSwitchCompact = [&](const char* name, const char* cmd, const char* state, const char* uid) {
    char payload[256];
    snprintf(payload, sizeof(payload),
      "{\"name\":\"%s\",\"command_topic\":\"%s\",\"state_topic\":\"%s\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"unique_id\":\"%s\"}",
      name, cmd, state, uid);
    char topic[128];
    snprintf(topic, sizeof(topic), "homeassistant/switch/%s/config", uid);
    robustPublish(topic, payload, true);
  };

  sendSwitchCompact("AC",   T_AC_CMD,   T_AC_STATE,   "roomhub_ac");
  sendSwitchCompact("Fan",  T_FAN_CMD,  T_FAN_STATE,  "roomhub_fan");
  sendSwitchCompact("TV",   T_TV_CMD,   T_TV_STATE,   "roomhub_tv");
  sendSwitchCompact("Bulb", T_BULB_CMD, T_BULB_STATE, "roomhub_bulb");

  // Single unified auto control switch
  sendSwitchCompact("Auto Control", T_AUTO_ALL_CMD, T_AUTO_ALL_STATE, "roomhub_auto_all");

  Serial.println("🟢 Compact discovery published.");
}

// ---------------- publish sensor snapshot ----------------
void publishSensorSnapshot() {
  float t = sensor.getTemperature(TEMP_C);
  float h = sensor.getHumidity();
  float lux = sensor.getLuminousIntensity();
  float pct = constrain((lux / 800.0f) * 100.0f, 0.0f, 100.0f);

  char buf[32];

  dtostrf(t, 5, 2, buf);
  robustPublish(T_TEMP, buf, true);
  dtostrf(h, 5, 2, buf);
  robustPublish(T_HUMI, buf, true);
  dtostrf(pct, 5, 2, buf);
  robustPublish(T_LIGHT, buf, true);

  Serial.println("📤 Sensor snapshot published");
}

// ---------------- Auto Climate Control Logic ----------------
void autoClimateControl(float temperature, float humidity, float light_pct) {
  if (!auto_control_enabled) {
    return; // All automations disabled
  }

  // Temperature-based AC control with hysteresis
  if (!ac_manual_override) {
    if (temperature >= TEMP_HIGH_THRESHOLD && !relay_ac_on) {
      Serial.printf("🌡️  TEMP %.2f°C >= %.2f°C → AUTO AC ON\n", temperature, TEMP_HIGH_THRESHOLD);
      relayOn(RELAY_AC, T_AC_STATE, relay_ac_on, true);
    }
    else if (temperature <= TEMP_LOW_THRESHOLD && relay_ac_on) {
      Serial.printf("🌡️  TEMP %.2f°C <= %.2f°C → AUTO AC OFF\n", temperature, TEMP_LOW_THRESHOLD);
      relayOff(RELAY_AC, T_AC_STATE, relay_ac_on, true);
    }
  }

  // Humidity-based FAN control with hysteresis
  if (!fan_manual_override) {
    if (humidity >= HUMI_HIGH_THRESHOLD && !relay_fan_on) {
      Serial.printf("💧 HUMI %.2f%% >= %.2f%% → AUTO FAN ON\n", humidity, HUMI_HIGH_THRESHOLD);
      relayOn(RELAY_FAN, T_FAN_STATE, relay_fan_on, true);
    }
    else if (humidity <= HUMI_LOW_THRESHOLD && relay_fan_on) {
      Serial.printf("💧 HUMI %.2f%% <= %.2f%% → AUTO FAN OFF\n", humidity, HUMI_LOW_THRESHOLD);
      relayOff(RELAY_FAN, T_FAN_STATE, relay_fan_on, true);
    }
  }

  // Light-based BULB control with hysteresis
  if (!bulb_manual_override) {
    if (light_pct < LIGHT_LOW_THRESHOLD && !relay_bulb_on) {
      Serial.printf("💡 LIGHT %.2f%% < %.2f%% → AUTO BULB ON\n", light_pct, LIGHT_LOW_THRESHOLD);
      relayOn(RELAY_BULB, T_BULB_STATE, relay_bulb_on, true);
    }
    else if (light_pct > LIGHT_HIGH_THRESHOLD && relay_bulb_on) {
      Serial.printf("💡 LIGHT %.2f%% > %.2f%% → AUTO BULB OFF\n", light_pct, LIGHT_HIGH_THRESHOLD);
      relayOff(RELAY_BULB, T_BULB_STATE, relay_bulb_on, true);
    }
  }
}

// ---------------- MQTT callback ----------------
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  Serial.printf("MQTT RX: %s -> %s\n", topic, msg.c_str());

  // Manual AC control
  if (String(topic) == T_AC_CMD) {
    ac_manual_override = true;  // User took manual control
    if (msg == "ON") relayOn(RELAY_AC, T_AC_STATE, relay_ac_on);
    else relayOff(RELAY_AC, T_AC_STATE, relay_ac_on);
  } 
  // Manual Fan control
  else if (String(topic) == T_FAN_CMD) {
    fan_manual_override = true;  // User took manual control
    if (msg == "ON") relayOn(RELAY_FAN, T_FAN_STATE, relay_fan_on);
    else relayOff(RELAY_FAN, T_FAN_STATE, relay_fan_on);
  } 
  // TV control (always manual)
  else if (String(topic) == T_TV_CMD) {
    if (msg == "ON") relayOn(RELAY_TV, T_TV_STATE, relay_tv_on);
    else relayOff(RELAY_TV, T_TV_STATE, relay_tv_on);
  } 
  // Manual Bulb control
  else if (String(topic) == T_BULB_CMD) {
    bulb_manual_override = true;  // User took manual control
    if (msg == "ON") relayOn(RELAY_BULB, T_BULB_STATE, relay_bulb_on);
    else relayOff(RELAY_BULB, T_BULB_STATE, relay_bulb_on);
  }
  // UNIFIED auto control enable/disable (controls ALL automations)
  else if (String(topic) == T_AUTO_ALL_CMD) {
    if (msg == "ON") {
      auto_control_enabled = true;
      // Clear all manual overrides when re-enabling automation
      ac_manual_override = false;
      fan_manual_override = false;
      bulb_manual_override = false;
      robustPublish(T_AUTO_ALL_STATE, "ON", true);
      Serial.println("✅ ALL AUTOMATIONS ENABLED (Temp/Humi/Light)");
    } else {
      auto_control_enabled = false;
      robustPublish(T_AUTO_ALL_STATE, "OFF", true);
      
      // Turn off AC if it was on
      if (relay_ac_on) relayOff(RELAY_AC, T_AC_STATE, relay_ac_on, true);
      
      // Turn off FAN if it was on
      if (relay_fan_on) relayOff(RELAY_FAN, T_FAN_STATE, relay_fan_on, true);
      
      // Turn off BULB if it was on
      if (relay_bulb_on) relayOff(RELAY_BULB, T_BULB_STATE, relay_bulb_on, true);

      Serial.println("❌ DISABLING AUTOMATIONS AND RESETTING RELAYS");
    }
  }
}

// ---------------- MQTT reconnect ----------------
unsigned long lastReconnectAttempt = 0;
bool mqttReconnect() {
  if (mqtt.connected()) return true;

  unsigned long now = millis();
  if (now - lastReconnectAttempt < 2000) return false;
  lastReconnectAttempt = now;

  String clientId = String("roomhub_") + String((uint64_t)ESP.getEfuseMac(), HEX);

  Serial.print("MQTT connecting... ");
  if (mqtt.connect(clientId.c_str(), MQTT_USER, MQTT_PASS)) {
    Serial.println("connected");

    // Subscribe to all command topics
    mqtt.subscribe(T_AC_CMD);
    mqtt.subscribe(T_FAN_CMD);
    mqtt.subscribe(T_TV_CMD);
    mqtt.subscribe(T_BULB_CMD);
    mqtt.subscribe(T_AUTO_ALL_CMD);  // Single unified auto control topic

    publishDiscovery();
    mqtt.loop();
    delay(200);
    publishSensorSnapshot();

    // Publish current relay states
    mqtt.publish(T_AC_STATE, relay_ac_on ? "ON" : "OFF", true);
    mqtt.loop(); delay(60);
    mqtt.publish(T_FAN_STATE, relay_fan_on ? "ON" : "OFF", true);
    mqtt.loop(); delay(60);
    mqtt.publish(T_TV_STATE, relay_tv_on ? "ON" : "OFF", true);
    mqtt.loop(); delay(60);
    mqtt.publish(T_BULB_STATE, relay_bulb_on ? "ON" : "OFF", true);
    mqtt.loop(); delay(60);

    // Publish unified auto control state
    mqtt.publish(T_AUTO_ALL_STATE, auto_control_enabled ? "ON" : "OFF", true);
    mqtt.loop(); delay(60);

    return true;
  } else {
    Serial.printf("failed, rc=%d\n", mqtt.state());
    return false;
  }
}

// ---------------- WiFi connect ----------------
void wifiConnect() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.print("Connecting WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
    if (millis() - start > 20000) {
      Serial.println("\nWiFi connect timeout — restarting.");
      ESP.restart();
    }
  }
  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
}

// ---------------- setup ----------------
void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(RELAY_AC, OUTPUT);
  pinMode(RELAY_FAN, OUTPUT);
  pinMode(RELAY_TV, OUTPUT);
  pinMode(RELAY_BULB, OUTPUT);

  // Ensure all relays start OFF
  digitalWrite(RELAY_AC, HIGH);
  digitalWrite(RELAY_FAN, HIGH);
  digitalWrite(RELAY_TV, HIGH);
  digitalWrite(RELAY_BULB, HIGH);

  Wire.begin(6, 7); // SDA=6 SCL=7 for ESP32-C6

  if (sensor.begin() != 0) Serial.println("❌ SEN0500 init FAILED");
  else Serial.println("✅ SEN0500 OK");

  wifiConnect();

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);

  // Attempt to connect immediately
  mqttReconnect();
  
  Serial.println("\n🤖 AUTO CLIMATE CONTROL ACTIVE:");
  Serial.printf("   🌡️  AC: ON at %.1f°C, OFF at %.1f°C\n", TEMP_HIGH_THRESHOLD, TEMP_LOW_THRESHOLD);
  Serial.printf("   💧 FAN: ON at %.1f%%, OFF at %.1f%%\n", HUMI_HIGH_THRESHOLD, HUMI_LOW_THRESHOLD);
  Serial.printf("   💡 BULB: ON at <%.1f%%, OFF at >%.1f%%\n", LIGHT_LOW_THRESHOLD, LIGHT_HIGH_THRESHOLD);
}

// ---------------- loop ----------------
void loop() {
  if (!mqtt.connected()) mqttReconnect();
  else mqtt.loop();

  unsigned long now = millis();
  if (now - lastSensorMillis >= SENSOR_INTERVAL_MS) {
    lastSensorMillis = now;

    float t = sensor.getTemperature(TEMP_C);
    float h = sensor.getHumidity();
    float lux = sensor.getLuminousIntensity();
    float pct = constrain((lux / 800.0f) * 100.0f, 0.0f, 100.0f);

    // Run auto climate control logic (temp, humidity, and light)
    autoClimateControl(t, h, pct);

    char buf[32];

    dtostrf(t, 5, 2, buf); String s_t = String(buf);
    if (s_t != last_temp_str) { robustPublish(T_TEMP, s_t.c_str(), true); last_temp_str = s_t; }

    dtostrf(h, 5, 2, buf); String s_h = String(buf);
    if (s_h != last_humi_str) { robustPublish(T_HUMI, s_h.c_str(), true); last_humi_str = s_h; }

    dtostrf(pct, 5, 2, buf); String s_l = String(buf);
    if (s_l != last_light_str) { robustPublish(T_LIGHT, s_l.c_str(), true); last_light_str = s_l; }

    Serial.printf("SENSORS: T=%.2f°C H=%.2f%% LUX=%.2f PCT=%.2f%%\n", t, h, lux, pct);
  }
}

//============================================== hackster.io/maheshyadav216 ==============================================//