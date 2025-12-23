Setting up Mosquitto MQTT Broker on Raspberry Pi Zero W
This guide provides a step-by-step process to set up a reliable Mosquitto MQTT broker on a Raspberry Pi Zero W for use with the SmartHome Control Tab system.

Assumptions
You have shell access (SSH or local) to the Pi Zero W.

You are running a Debian-based OS (Raspberry Pi OS / Raspberry Pi OS Lite).

0. Quick Plan Summary
Install Mosquitto (broker) on Pi Zero W.

Create a user/password auth file.

Add a broker config with persistence and minimal security.

Enable/auto-start systemd service.

Point devices (ESP32 and Tablet) to this broker IP & credentials.

Configure Home Assistant to use this external broker.

Verify end-to-end connectivity.

1. Prepare Pi Zero W
SSH into the Pi and update the operating system:

Bash

# Update OS
sudo apt update && sudo apt upgrade -y
Static IP: It is recommended to set a static IP via your router's DHCP reservation (e.g., 192.168.0.151).

Check Time: Ensure the system time is correct.

Bash

timedatectl
2. Install Mosquitto Broker and Clients
Install the necessary packages:

Bash

sudo apt install -y mosquitto mosquitto-clients
Enable and start the service:

Bash

sudo systemctl enable mosquitto
sudo systemctl start mosquitto
sudo systemctl status mosquitto
Note: You should see the status as active (running).

3. Configure Authentication
Create a password file and add a user. Replace <username> with your desired name (e.g., mqttuser):

Bash

sudo mosquitto_passwd -c /etc/mosquitto/passwd <username>
# You will be prompted to enter and confirm your password
(The -c flag creates the file; omit it if you add more users later.)

4. Create Minimal Secure Config
Create a new configuration file:

Bash

sudo nano /etc/mosquitto/conf.d/local.conf
Paste the following configuration into the file:

Ini, TOML

listener 1883
allow_anonymous false
password_file /etc/mosquitto/passwd

persistence true
persistence_location /var/lib/mosquitto/

# Optional: Performance tuning
max_inflight_messages 20
message_size_limit 65536

# Logging
log_dest syslog
Save and exit (Ctrl+O, Enter, Ctrl+X).

Restart the broker to apply changes:

Bash

sudo systemctl restart mosquitto
sudo systemctl status mosquitto
# To check logs:
sudo journalctl -u mosquitto -n 50 --no-pager
5. Test Broker Connectivity
Local Test (from the Pi)
Open one terminal to subscribe:

Bash

mosquitto_sub -h localhost -t 'test/topic' -u mqttuser -P 'mqttpassword' -v
Open a second terminal to publish:

Bash

mosquitto_pub -h localhost -t 'test/topic' -m 'hello-from-pi' -u mqttuser -P 'mqttpassword'
Remote Test (from PC or Tablet)
Test connectivity from a remote device using the Pi's IP:

Bash

mosquitto_sub -h 192.168.0.151 -t 'test/topic' -u mqttuser -P 'mqttpassword' -v
6. Hardening & Maintenance (Recommended)
DHCP Reservation: Ensure the Pi's IP never changes via router settings.

Firewall (ufw): If using a firewall, allow LAN traffic on port 1883:

Bash

sudo apt install ufw
sudo ufw allow from 192.168.0.0/24 to any port 1883 proto tcp
sudo ufw enable
Manual Backups:

Bash

sudo systemctl stop mosquitto
sudo tar czf /root/mosquitto-backup-$(date +%F).tgz /etc/mosquitto /var/lib/mosquitto /etc/mosquitto/passwd
sudo systemctl start mosquitto