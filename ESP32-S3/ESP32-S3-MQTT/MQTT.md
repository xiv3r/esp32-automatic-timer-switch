Home Assistant:

· Settings → Devices & Services → MQTT
· ESP32 relays appear automatically as switches
· No YAML configuration needed

Manual Setup (if auto-discovery is off)

Add to configuration.yaml:

```yaml
mqtt:
  switch:
    - name: "Relay 1"
      command_topic: "esp32relay16/relay/1/set"
      state_topic: "esp32relay16/relay/1/state"
      payload_on: "ON"
      payload_off: "OFF"
      state_on: "ON"
      state_off: "OFF"
      availability_topic: "esp32relay16/status"
      unique_id: "esp32_relay_1"

    - name: "Relay 2"
      command_topic: "esp32relay16/relay/2/set"
      state_topic: "esp32relay16/relay/2/state"
      payload_on: "ON"
      payload_off: "OFF"
      unique_id: "esp32_relay_2"
```

HA Automations

```yaml
# Toggle relay at sunset
alias: "Relay 1 ON at sunset"
trigger:
  - platform: sun
    event: sunset
action:
  - service: mqtt.publish
    data:
      topic: esp32relay16/relay/1/set
      payload: "ON"

# Pulse relay for 5 seconds
action:
  - service: mqtt.publish
    data:
      topic: esp32relay16/relay/1/pulse
      payload: "5000"

# Turn all relays OFF
action:
  - service: mqtt.publish
    data:
      topic: esp32relay16/relay/all/set
      payload: "OFF"
```

HA Dashboard Card

```yaml
type: entities
title: ESP32 Relays
entities:
  - entity: switch.relay_1
  - entity: switch.relay_2
  # ... up to relay_16
```

---

2. openHAB Configuration

Install MQTT Binding

1. Settings → Bindings → Add Binding
2. Search MQTT Binding
3. Install it

Configure MQTT Broker

Create /etc/openhab/things/mqtt.things:

```java
Bridge mqtt:broker:mosquitto "Mosquitto Broker" [
    host="192.168.1.50",
    port=1883,
    secure=false,
    username="mqtt_user",
    password="your_password",
    clientID="openhab_esp32"
] {
    // Relay 1
    Thing topic relay1 "Relay 1" [
        stateTopic="esp32relay16/relay/1/state",
        commandTopic="esp32relay16/relay/1/set",
        availabilityTopic="esp32relay16/status"
    ]
    
    // Relay 2
    Thing topic relay2 "Relay 2" [
        stateTopic="esp32relay16/relay/2/state",
        commandTopic="esp32relay16/relay/2/set"
    ]
    
    // System stats
    Thing topic system "ESP32 System" [
        stateTopic="esp32relay16/system/stats"
    ]
}
```

Create Items

Create /etc/openhab/items/esp32.items:

```java
// Relay Switches
Switch ESP32_Relay1 "Relay 1" { channel="mqtt:topic:mosquitto:relay1" }
Switch ESP32_Relay2 "Relay 2" { channel="mqtt:topic:mosquitto:relay2" }
// ... up to relay 16

// System Info
String ESP32_Uptime "Uptime" { channel="mqtt:topic:mosquitto:system" }
String ESP32_Status "Status" { mqtt="<[mosquitto:esp32relay16/status:state:default]" }
```

Sitemap

Create /etc/openhab/sitemaps/esp32.sitemap:

```java
sitemap esp32 label="ESP32 Relays" {
    Frame label="Relays" {
        Switch item=ESP32_Relay1
        Switch item=ESP32_Relay2
        // ... all relays
    }
    Frame label="System" {
        Text item=ESP32_Uptime
        Text item=ESP32_Status
    }
}
```

openHAB Rules

```java
rule "Turn on Relay 1 at sunset"
when
    Channel 'astro:sun:home:set#event' triggered START
then
    ESP32_Relay1.sendCommand(ON)
end

rule "Turn off all relays at midnight"
when
    Time cron "0 0 0 * * ?"
then
    // Publish directly
    val actions = getActions("mqtt", "mqtt:broker:mosquitto")
    actions.publishMQTT("esp32relay16/relay/all/set", "OFF")
end
```

---

3. Node-RED

Install Node-RED

```bash
# Home Assistant add-on or standalone
npm install -g node-red
```

MQTT Nodes

Flow: ESP32 Relay Control

```json
[
    {
        "id": "mqtt-broker",
        "type": "mqtt-broker",
        "name": "Mosquitto",
        "broker": "192.168.1.50",
        "port": "1883",
        "clientid": "nodered",
        "credentials": {
            "username": "mqtt_user",
            "password": "your_password"
        }
    },
    {
        "id": "inject-on",
        "type": "inject",
        "name": "Turn ON Relay 1",
        "payload": "ON",
        "topic": "esp32relay16/relay/1/set"
    },
    {
        "id": "inject-off",
        "type": "inject",
        "name": "Turn OFF Relay 1",
        "payload": "OFF",
        "topic": "esp32relay16/relay/1/set"
    },
    {
        "id": "mqtt-out",
        "type": "mqtt out",
        "name": "ESP32 Commands",
        "broker": "mqtt-broker",
        "topic": ""
    },
    {
        "id": "mqtt-in",
        "type": "mqtt in",
        "name": "ESP32 Status",
        "broker": "mqtt-broker",
        "topic": "esp32relay16/+/state"
    },
    {
        "id": "debug",
        "type": "debug",
        "name": "Show State Changes"
    }
]
```

Flow: Schedule-Based Control

```json
[
    {
        "id": "schedule-inject",
        "type": "inject",
        "name": "Every 5 min pulse",
        "repeat": "300",
        "payload": "5000",
        "topic": "esp32relay16/relay/2/pulse"
    }
]
```

Flow: Dashboard Control

```json
[
    {
        "id": "ui-switch",
        "type": "ui_switch",
        "name": "Relay 1",
        "group": "ESP32",
        "onvalue": "ON",
        "offvalue": "OFF",
        "topic": "esp32relay16/relay/1/set"
    }
]
```

---

4. Python Script (Any Platform)

```python
import paho.mqtt.client as mqtt
import json
import time

BROKER = "192.168.1.50"
PORT = 1883
USERNAME = "mqtt_user"
PASSWORD = "your_password"
BASE_TOPIC = "esp32relay16"

def on_connect(client, userdata, flags, rc):
    print(f"Connected with result code {rc}")
    # Subscribe to all ESP32 topics
    client.subscribe(f"{BASE_TOPIC}/#")

def on_message(client, userdata, msg):
    print(f"Topic: {msg.topic} → {msg.payload.decode()}")

client = mqtt.Client()
client.username_pw_set(USERNAME, PASSWORD)
client.on_connect = on_connect
client.on_message = on_message

client.connect(BROKER, PORT, 60)
client.loop_start()

# Control relays
def relay_on(num):
    client.publish(f"{BASE_TOPIC}/relay/{num}/set", "ON")

def relay_off(num):
    client.publish(f"{BASE_TOPIC}/relay/{num}/set", "OFF")

def relay_pulse(num, ms):
    client.publish(f"{BASE_TOPIC}/relay/{num}/pulse", str(ms))

def all_on():
    client.publish(f"{BASE_TOPIC}/relay/all/set", "ON")

def all_off():
    client.publish(f"{BASE_TOPIC}/relay/all/set", "OFF")

def get_schedule(num):
    client.publish(f"{BASE_TOPIC}/relay/{num}/schedule/get", "")

# Example usage
relay_on(1)           # Turn relay 1 ON
time.sleep(2)
relay_off(1)          # Turn relay 1 OFF
relay_pulse(2, 3000)  # Pulse relay 2 for 3 seconds
all_off()             # Turn all relays OFF

time.sleep(10)
client.loop_stop()
client.disconnect()
```

---

5. Command Line (mosquitto_pub)

```bash
# Turn relay 1 ON
mosquitto_pub -h 192.168.1.50 -u mqtt_user -P your_password \
  -t "esp32relay16/relay/1/set" -m "ON"

# Turn relay 1 OFF
mosquitto_pub -h 192.168.1.50 -u mqtt_user -P your_password \
  -t "esp32relay16/relay/1/set" -m "OFF"

# Toggle relay 1
mosquitto_pub -h 192.168.1.50 -u mqtt_user -P your_password \
  -t "esp32relay16/relay/1/set" -m "TOGGLE"

# Pulse relay 2 for 5 seconds
mosquitto_pub -h 192.168.1.50 -u mqtt_user -P your_password \
  -t "esp32relay16/relay/2/pulse" -m "5000"

# Turn all relays OFF
mosquitto_pub -h 192.168.1.50 -u mqtt_user -P your_password \
  -t "esp32relay16/relay/all/set" -m "OFF"

# Get relay 1 schedule
mosquitto_pub -h 192.168.1.50 -u mqtt_user -P your_password \
  -t "esp32relay16/relay/1/schedule/get" -m ""

# Monitor all ESP32 messages
mosquitto_sub -h 192.168.1.50 -u mqtt_user -P your_password \
  -t "esp32relay16/#" -v

# Reboot ESP32
mosquitto_pub -h 192.168.1.50 -u mqtt_user -P your_password \
  -t "esp32relay16/system/command" -m "reboot"
```

---

6. HomeBridge (Apple HomeKit)

```json
// config.json
{
    "platforms": [
        {
            "platform": "mqttthing",
            "name": "ESP32 Relays",
            "mqttBroker": "mqtt://192.168.1.50",
            "mqttOptions": {
                "username": "mqtt_user",
                "password": "your_password"
            },
            "accessories": [
                {
                    "type": "switch",
                    "name": "Relay 1",
                    "topics": {
                        "getOn": "esp32relay16/relay/1/state",
                        "setOn": "esp32relay16/relay/1/set"
                    },
                    "onValue": "ON",
                    "offValue": "OFF"
                }
            ]
        }
    ]
}
```

---

Complete MQTT Topic Reference

```
Topic Direction Payload Used By
esp32relay16/relay/{1-16}/set ← ESP ON/OFF/TOGGLE All platforms
esp32relay16/relay/{1-16}/state ESP → ON/OFF All platforms
esp32relay16/relay/{1-16}/toggle ← ESP any All platforms
esp32relay16/relay/{1-16}/pulse ← ESP milliseconds All platforms
esp32relay16/relay/all/set ← ESP ON/OFF All platforms
esp32relay16/relay/{1-16}/schedule/get ← ESP (trigger) HA, openHAB, Node-RED
esp32relay16/relay/{1-16}/schedule/set ← ESP JSON HA, openHAB, Node-RED
esp32relay16/relay/{1-16}/schedule ESP → JSON HA, openHAB, Node-RED
esp32relay16/relay/{1-16}/available ESP → online HA, openHAB
esp32relay16/system/stats ESP → JSON All platforms
esp32relay16/system/command ← ESP reboot/status All platforms
esp32relay16/status ESP → online/offline HA LWT, openHAB
homeassistant/switch/ESP32_Relay16_relay_{N}/config ESP → JSON HA Auto-Discovery
homeassistant/status ← ESP online/offline ESP (re-publishes discovery)
```
---
