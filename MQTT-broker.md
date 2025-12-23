🟢 Raspberry Pi Zero W as MQTT Broker (Mosquitto)

This guide provides a battle-tested, minimal, and reliable setup for running a Mosquitto MQTT Broker on a Raspberry Pi Zero W, designed to work smoothly with:

ESP32 devices (publishers/subscribers)

ESP32-S3 CrowPanel (SmartHome Control Tab)

Home Assistant (as an optional MQTT client, not the broker)

All commands are exact and safe to copy/paste.

📋 Assumptions

You have shell access (SSH or local) to the Raspberry Pi Zero W

OS: Debian-based (Raspberry Pi OS / Raspberry Pi OS Lite)

Network access to your LAN

If your OS is different, adapt accordingly.

🧭 Quick Plan Summary

Install Mosquitto broker & client tools

Create username/password authentication

Add a minimal, secure broker configuration

Enable persistence and auto-start

Verify broker locally and from network

Point ESP32 devices and Home Assistant to this broker

1️⃣ Prepare Raspberry Pi Zero W

SSH into the Pi (or open local terminal).

Update the system
sudo apt update && sudo apt upgrade -y

Set a static IP (recommended)

Use your router’s DHCP reservation and note the IP
Example:

192.168.0.151


If you want to configure static IP on the device itself, that can be done separately.

Check system time
timedatectl

2️⃣ Install Mosquitto Broker & Clients

Install Mosquitto and command-line tools:

sudo apt install -y mosquitto mosquitto-clients


Enable and start the broker:

sudo systemctl enable mosquitto
sudo systemctl start mosquitto
sudo systemctl status mosquitto


✅ You should see:

Active: active (running)

3️⃣ Configure Authentication (Username / Password)

Create a password file and add a user:

sudo mosquitto_passwd -c /etc/mosquitto/passwd mqttuser


You’ll be prompted to enter a password (example):

mqttpassword


📌 Notes:

-c creates the file (use it only once)

To add more users later, omit -c

4️⃣ Create Minimal Secure Broker Config

Edit the local Mosquitto config:

sudo nano /etc/mosquitto/conf.d/local.conf


Paste the following minimal and clean configuration:

listener 1883
allow_anonymous false
password_file /etc/mosquitto/passwd

persistence true
persistence_location /var/lib/mosquitto/

# Optional tuning
max_inflight_messages 20
message_size_limit 65536

# Logging
log_dest syslog


Save and exit:

Ctrl + O → Enter

Ctrl + X

Restart Mosquitto:

sudo systemctl restart mosquitto
sudo systemctl status mosquitto
sudo journalctl -u mosquitto -n 50 --no-pager

5️⃣ Test the Broker (Local)
Terminal 1 — Subscribe
mosquitto_sub -h localhost -t 'test/topic' -u mqttuser -P 'mqttpassword' -v

Terminal 2 — Publish
mosquitto_pub -h localhost -t 'test/topic' -m 'hello-from-pi' -u mqttuser -P 'mqttpassword'


✅ You should see the message appear in the subscriber terminal.

🌐 Test from Another Device (PC / ESP / Tablet)

Subscribe using Pi’s LAN IP:

mosquitto_sub -h 192.168.0.151 -t 'test/topic' -u mqttuser -P 'mqttpassword' -v


Publish from any MQTT client to confirm network access.

6️⃣ Basic Hardening (Optional but Recommended)
Ensure IP Stability

Keep DHCP reservation enabled in your router

Enable Firewall (LAN-only MQTT)
sudo apt install ufw
sudo ufw allow from 192.168.0.0/24 to any port 1883 proto tcp
sudo ufw enable

Backup Mosquitto Config & Data
sudo systemctl stop mosquitto
sudo tar czf /root/mosquitto-backup-$(date +%F).tgz \
  /etc/mosquitto \
  /var/lib/mosquitto \
  /etc/mosquitto/passwd
sudo systemctl start mosquitto

🏠 Home Assistant Integration (Important)

If using Home Assistant:

Disable Mosquitto Add-on (Supervisor → Add-ons)

Keep MQTT Integration

Configure it to connect to:

Broker: 192.168.0.151

Port: 1883

Username: mqttuser

Password: mqttpassword

This makes Home Assistant a client, not the broker.

✅ Final Architecture

🟢 Raspberry Pi Zero W → MQTT Broker (Mosquitto)

🟢 ESP32-C6 (RoomHub) → MQTT Client (sensors + relays)

🟢 ESP32-S3 CrowPanel → MQTT Client (HMI control tab)

🟢 Home Assistant → Optional MQTT Client

🎯 Result

You now have:

A stable, always-on MQTT backbone

Independence from Home Assistant uptime

Clean separation of broker vs clients

A production-grade setup suitable for demos & portfolios

If you want next: