here’s a clear, battle-tested, step-by-step guide to set up a reliable Mosquitto MQTT broker on your Raspberry Pi Zero W and wire it into your existing system. I keep commands exact so you can copy/paste. I’ll also include verification and troubleshooting tips.

Assumption: you have shell access (SSH or local) to the Pi Zero W and are running a Debian-based OS (Raspberry Pi OS / Raspberry Pi OS Lite). If not, tell me the OS and I’ll adapt.

0) Quick plan summary

Install Mosquitto (broker) on Pi Zero W.

Create a user/password auth file.

Add a small broker config with persistence and minimal security.

Enable/auto-start systemd service.

Point ESP32 devices and Tablet to this broker IP & creds.

Configure Home Assistant to use external broker (disable Mosquitto add-on).

Verify end-to-end & test retained discovery + availability.

1) Prepare Pi Zero W

SSH into Pi (or open terminal).

# update OS
sudo apt update && sudo apt upgrade -y


Set a static IP (recommended) via your router DHCP reservation — do that in router UI and note the IP (e.g., 192.168.0.151). If you prefer static on device, let me know and I’ll give commands.

Ensure time is correct:

timedatectl

2) Install Mosquitto broker and clients
sudo apt install -y mosquitto mosquitto-clients


Enable auto start:

sudo systemctl enable mosquitto
sudo systemctl start mosquitto
sudo systemctl status mosquitto


You should see active (running).

3) Configure authentication (username/password)

Create password file and add a user (replace <username> and choose a strong <password>):

sudo mosquitto_passwd -c /etc/mosquitto/passwd mqttuser
# you will be prompted to enter password (enter mqttpassword or your chosen one)


-c creates file; omit -c to add more users later.

4) Create a minimal secure config file (use /etc/mosquitto/conf.d/local.conf)

Create/edit the file:

sudo nano /etc/mosquitto/conf.d/local.conf


Paste this minimal config:

listener 1883
allow_anonymous false
password_file /etc/mosquitto/passwd

persistence true
persistence_location /var/lib/mosquitto/

# Optional: tune
max_inflight_messages 20
message_size_limit 65536

# Logging
log_dest syslog


Save and exit (Ctrl+O, Enter, Ctrl+X).

Restart broker:

sudo systemctl restart mosquitto
sudo systemctl status mosquitto
sudo journalctl -u mosquitto -n 50 --no-pager

5) Test broker locally from Pi

Open one terminal to subscribe:

mosquitto_sub -h localhost -t 'test/topic' -u mqttuser -P 'mqttpassword' -v


Open another terminal to publish:

mosquitto_pub -h localhost -t 'test/topic' -m 'hello-from-pi' -u mqttuser -P 'mqttpassword'


You should see the message in the subscriber terminal.

If using a remote client (tablet / ESP), test from your PC:

mosquitto_sub -h 192.168.0.151 -t 'test/topic' -u mqttuser -P 'mqttpassword' -v


Then publish from any device to verify connectivity.

6) Harden basics (optional but recommended)

Ensure Pi has a DHCP reservation so its IP never changes.

Enable firewall if you want (ufw) but allow LAN 1883:

sudo apt install ufw
sudo ufw allow from 192.168.0.0/24 to any port 1883 proto tcp
sudo ufw enable


Make backups:

sudo systemctl stop mosquitto
sudo tar czf /root/mosquitto-backup-$(date +%F).tgz /etc/mosquitto /var/lib/mosquitto /etc/mosquitto/passwd
sudo systemctl start mosquitto