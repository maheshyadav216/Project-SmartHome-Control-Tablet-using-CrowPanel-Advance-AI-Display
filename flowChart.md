flowchart TB
    %% =======================
    %% CENTRAL MQTT BROKER
    %% =======================
    RPI[Raspberry Pi Zero W<br/>🟢 MQTT Broker (Mosquitto)<br/>• Message Routing<br/>• Topic Management]

    %% =======================
    %% ROOM SENSOR HUB
    %% =======================
    ROOMHUB[ESP32-C6<br/>🔵 RoomHub<br/><br/>MQTT Client<br/>• Publishes Sensor Data<br/>• Subscribes to Commands]

    %% =======================
    %% SMART HOME CONTROL TAB
    %% =======================
    HMI[ESP32-S3 CrowPanel<br/>🔵 SmartHome Control Tab<br/><br/>MQTT Client<br/>• Touch UI<br/>• Publishes Control Commands<br/>• Subscribes to Sensor Data]

    %% =======================
    %% HOME ASSISTANT (OPTIONAL)
    %% =======================
    HA[Home Assistant<br/>⚪ Optional / Secondary<br/><br/>MQTT Client<br/>• Automation<br/>• Dashboards]

    %% =======================
    %% CONNECTIONS
    %% =======================
    ROOMHUB <--> |Sensor Data & Commands| RPI
    HMI <--> |UI Commands & Updates| RPI
    HA <--> |Optional Automation| RPI

    %% =======================
    %% FOOTER NOTE
    %% =======================
    NOTE[System continues working<br/>even if Home Assistant is offline]
    NOTE -.-> RPI
