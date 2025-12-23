# 🟢 Raspberry Pi Zero W as MQTT Broker (Mosquitto)

## Project Context

This broker is used in the **SmartHome Control Tab** project:

* **Raspberry Pi Zero W** → MQTT **Broker** (Mosquitto)
* **ESP32‑C6 (RoomHub)** → MQTT Client (sensors + relays)
* **ESP32‑S3 CrowPanel (HMI Tablet)** → MQTT Client (UI + control)
* **Home Assistant** → Optional MQTT Client (NOT the broker)

---

## Assumptions

* Raspberry Pi Zero W
* Raspberry Pi OS / Raspberry Pi OS Lite
* LAN access + SSH or local terminal
* Static IP via router DHCP reservation (example: `192.168.0.151`)

---

## 1. System Update

```bash
sudo apt update && sudo apt upgrade -y
```

Verify time:

```bash
timedatectl
```

---

## 2. Install Mosquitto Broker & Clients

```bash
sudo apt install -y mosquitto mosquitto-clients
```

Enable and start the broker:

```bash
sudo systemctl enable mosquitto
sudo systemctl start mosquitto
sudo systemctl status mosquitto
```

Expected result:

```
Active: active (running)
```

---

## 3. Create MQTT User & Password

Create password file and user:

```bash
sudo mosquitto_passwd -c /etc/mosquitto/passwd mqttuser
```

Enter password when prompted:

```
mqttpassword
```

⚠️ Use `-c` only the **first time**.

---

## 4. Broker Configuration (Minimal & Correct)

Create config file:

```bash
sudo nano /etc/mosquitto/conf.d/local.conf
```

Paste **exactly**:

```conf
listener 1883
allow_anonymous false
password_file /etc/mosquitto/passwd
```

Save & exit:

```
Ctrl+O → Enter
Ctrl+X
```

Restart broker:

```bash
sudo systemctl restart mosquitto
sudo systemctl status mosquitto
```

---

## 5. Local Broker Test (MANDATORY)

### Terminal 1 — Subscribe

```bash
mosquitto_sub -h localhost -t test/topic -u mqttuser -P mqttpassword -v
```

### Terminal 2 — Publish

```bash
mosquitto_pub -h localhost -t test/topic -m "hello-from-pi" -u mqttuser -P mqttpassword
```

Expected output:

```
test/topic hello-from-pi
```

---

## 6. LAN Test (From Another Device)

```bash
mosquitto_sub -h 192.168.0.151 -t test/topic -u mqttuser -P mqttpassword -v
```

---

## 7. Enable Persistence (SAFE METHOD)

Edit main config:

```bash
sudo nano /etc/mosquitto/mosquitto.conf
```

Ensure it contains:

```conf
persistence true
persistence_location /var/lib/mosquitto/
include_dir /etc/mosquitto/conf.d
```

⚠️ **DO NOT** add persistence settings in `local.conf`.

Restart:

```bash
sudo systemctl restart mosquitto
```

---

## 8. Home Assistant Configuration

### Correct Setup

* Keep **MQTT Integration**
* Use **external broker**

```
Broker:   192.168.0.151
Port:     1883
Username: mqttuser
Password: mqttpassword
```

### Do NOT

* ❌ Do NOT run Mosquitto add‑on in HA
* ❌ Do NOT let HA act as broker

---

## Final Result

* MQTT broker runs independently on Pi Zero W
* ESP32 devices continue working even if Home Assistant is offline
* CrowPanel HMI remains responsive
* Clean, production‑grade architecture

---

## Troubleshooting

Check logs:

```bash
sudo journalctl -u mosquitto -n 50 --no-pager
```

Verify config files:

```bash
sudo mosquitto -c /etc/mosquitto/mosquitto.conf -v
```

---

✅ This file is **ready to commit** as `README.md`.
