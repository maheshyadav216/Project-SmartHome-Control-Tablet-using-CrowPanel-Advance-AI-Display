🟢 Raspberry Pi Zero W as MQTT Broker (Mosquitto)

This guide explains how to set up a reliable Mosquitto MQTT Broker on a Raspberry Pi Zero W, used as the central messaging backbone for a Smart Home / IoT system.

This setup is used in the SmartHome Control Tab project and works with:

ESP32-C6 (RoomHub – sensors & relays)

ESP32-S3 CrowPanel (Portable HMI Tablet)

Home Assistant (optional, as MQTT client only)

📋 Assumptions

Raspberry Pi Zero W

Raspberry Pi OS / Raspberry Pi OS Lite (Debian-based)

SSH or local terminal access

Pi connected to the same LAN as ESP32 devices

🧭 Architecture Overview
Raspberry Pi Zero W
└── Mosquitto MQTT Broker (port 1883)

ESP32-C6 (RoomHub)
└── MQTT Client (publish sensors, subscribe commands)

ESP32-S3 CrowPanel
└── MQTT Client (subscribe sensors, publish UI commands)

Home Assistant (optional)
└── MQTT Client (NO broker)

1️⃣ Prepare Raspberry Pi Zero W

Update the system:

sudo apt update && sudo apt upgrade -y


Check system time:

timedatectl


⚠️ Recommended:
Set a DHCP reservation in your router so the Pi always gets the same IP
Example used below: 192.168.0.151

2️⃣ Install Mosquitto Broker & Clients

Install Mosquitto and client tools:

sudo apt install -y mosquitto mosquitto-clients


Enable and start Mosquitto:

sudo systemctl enable mosquitto
sudo systemctl start mosquitto
sudo systemctl status mosquitto


✅ You should see:

Active: active (running)

3️⃣ Create MQTT User & Password

Create the password file and user:

sudo mosquitto_passwd -c /etc/mosquitto/passwd mqttuser


You’ll be prompted for a password:

Password: mqttpassword


📌 Notes:

Use -c only once

To add more users later, omit -c

4️⃣ Create Minimal Broker Configuration

Create a local config file:

sudo nano /etc/mosquitto/conf.d/local.conf


Paste exactly this:

listener 1883
allow_anonymous false
password_file /etc/mosquitto/passwd


Save and exit:

Ctrl + O → Enter
Ctrl + X


Restart Mosquitto:

sudo systemctl restart mosquitto
sudo systemctl status mosquitto

5️⃣ Verify Broker Locally (IMPORTANT)
Terminal 1 — Subscribe
mosquitto_sub -h localhost -t test/topic -u mqttuser -P mqttpassword -v

Terminal 2 — Publish
mosquitto_pub -h localhost -t test/topic -m "hello-from-pi" -u mqttuser -P mqttpassword


✅ You should see:

test/topic hello-from-pi

6️⃣ Verify from Another Device (LAN Test)

From PC / Laptop / another Pi:

mosquitto_sub -h 192.168.0.151 -t test/topic -u mqttuser -P mqttpassword -v


Publish from any MQTT client to confirm network access.

7️⃣ (Optional) Enable Persistence

Edit main config:

sudo nano /etc/mosquitto/mosquitto.conf


Ensure only one place has persistence:

persistence true
persistence_location /var/lib/mosquitto/
include_dir /etc/mosquitto/conf.d


⚠️ Do NOT duplicate persistence_location in local.conf

Restart:

sudo systemctl restart mosquitto

8️⃣ Home Assistant Configuration (IMPORTANT)

If using Home Assistant:

✅ What to DO

Keep MQTT Integration

Configure it to use external broker:

Broker:   192.168.0.151
Port:     1883
Username: mqttuser
Password: mqttpassword

❌ What NOT to DO

Do NOT run Mosquitto add-on in HA

Do NOT let HA be the broker

✅ Final Result

You now have:

🟢 Raspberry Pi Zero W as independent MQTT Broker

🟢 ESP32 devices fully decoupled from Home Assistant uptime

🟢 Portable CrowPanel HMI working standalone

🟢 Production-grade architecture for demos & portfolio

📌 Why This Setup Matters

Home Assistant can reboot → system still works

ESP32 devices stay in sync

HMI tablet remains responsive

Clean separation of broker vs clients