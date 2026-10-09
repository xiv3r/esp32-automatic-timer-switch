# Full MQTT Control Commands — ESP32 16-Channel Relay Timer

**Author:** Raff Alds · **License:** GPLv3 · **Version:** 13

---

## 📌 Base Topic

The default base topic is auto-generated from the ESP32's MAC:

```
esp32/relay16_<MAC>
```

Where `<MAC>` = last 4 hex digits of efuse MAC (e.g., `A1B2`).

**Default example:** `esp32/relay16_A1B2`

**Fallback default:** `esp32/relay16` (if base_topic is empty)

You can change it in **MQTT Page → Base Topic field**. Max 31 chars, must not start with `$`, must not start/end with `/`, must not contain `//`, only allows `A-Z a-z 0-9 / _ - .`

---

## 📥 SUBSCRIBE TOPICS (You publish → Device listens)

### 1️⃣ Relay Commands

**Topic pattern:**
```
<base>/relay/N/set
```

**N = 1 to 16** (1-indexed, matches relay numbers in UI)

#### Full Command Table

| Command | Accepted Aliases | Case | Action | Result Mode |
|---|---|---|---|---|
| `ON` | `1`, `true` | insensitive | Relay ON | Manual |
| `OFF` | `0`, `false` | insensitive | Relay OFF | Manual |
| `TOGGLE` | — | insensitive | Flip current state | Manual |
| `AUTO` | — | insensitive | Return to schedule | Auto |

#### Payload Rules (from `mqttCallback` + `mqttHandleRelayCommand`)
- **Max payload:** 127 bytes (longer truncated)
- **Whitespace:** Leading and trailing whitespace trimmed
- **Case:** All commands case-insensitive
- **Unknown:** Silently ignored (no error response)
- **Empty:** Silently ignored
- **Buffer:** 128-byte char array with null termination

#### Behavior per Command

**`ON` / `1` / `true`:**
```cpp
relayConfigs[index].manualOverride = true;
relayConfigs[index].manualState    = true;
scheduleActiveCache[index]         = false;
setRelayOutput(index, true);
lastRelayOutputs[index]            = true;
lastDebouncedStateGlobal[index]    = true;
lastStateChangeGlobal[index]       = millis();
if (stateChanged) saveConfiguration();   // only if changed
mqttPublishRelayState(index);
```

**`OFF` / `0` / `false`:** Same as above with `state = false`.

**`TOGGLE`:**
```cpp
state = !lastRelayOutputs[index];   // flip from current
// then follows ON/OFF path
```

**`AUTO`:**
```cpp
relayConfigs[index].manualOverride = false;
updateScheduleCache();
bool shouldBeOn = scheduleActiveCache[index];
setRelayOutput(index, shouldBeOn);
lastRelayOutputs[index]         = shouldBeOn;
lastDebouncedStateGlobal[index] = shouldBeOn;
lastStateChangeGlobal[index]    = millis();
saveConfiguration();   // always saves
mqttPublishRelayState(index);
```

#### Examples

```bash
# Relay 1 ON
mosquitto_pub -h <broker> -t "esp32/relay16_A1B2/relay/1/set" -m "ON"

# Relay 5 OFF
mosquitto_pub -h <broker> -t "esp32/relay16_A1B2/relay/5/set" -m "OFF"

# Relay 3 TOGGLE
mosquitto_pub -h <broker> -t "esp32/relay16_A1B2/relay/3/set" -m "TOGGLE"

# Relay 16 AUTO
mosquitto_pub -h <broker> -t "esp32/relay16_A1B2/relay/16/set" -m "AUTO"

# All relay aliases work:
mosquitto_pub -t "esp32/relay16_A1B2/relay/1/set" -m "1"
mosquitto_pub -t "esp32/relay16_A1B2/relay/1/set" -m "true"
mosquitto_pub -t "esp32/relay16_A1B2/relay/1/set" -m "on"
mosquitto_pub -t "esp32/relay16_A1B2/relay/1/set" -m "  ON  "   # spaces trimmed
```

---

### 2️⃣ System Commands

**Topic:**
```
<base>/system/set
```

#### Full Command Table

| Command | Case | Action | Requirement | Side Effects |
|---|---|---|---|---|
| `STATUS` | insensitive | Publish system state + all relay states | — | Immediate publish |
| `RESTART` | insensitive | Graceful disconnect + reboot | — | Publishes state, then "offline" LWT, then `ESP.restart()` |
| `FACTORY_RESET` | insensitive | Clear NVS + reboot | — | Erases **ALL** settings (WiFi, MQTT, schedules) |
| `DISCOVERY` | insensitive | Republish HA auto-discovery | `mqtt_ha_discovery=1` | Silent no-op if disabled |
| `SYNC_TIME` | insensitive | Force NTP re-sync | `wifiConnected && sta_enabled` | Silent no-op if not connected |

#### Behavior Details

**`STATUS`:**
```cpp
mqttPublishSystemState();      // <base>/system/state
mqttPublishAllRelayStates();   // <base>/relay/N/state + /json for each relay
```

**`RESTART`:**
```cpp
mqttPublishSystemState();
mqttClient.loop();
delay(50);
mqttGracefulDisconnect();      // publishes "offline" to availability topic
delay(50);
ESP.restart();
```

**`FACTORY_RESET`:**
```cpp
mqttGracefulDisconnect();
delay(50);
preferences.begin(NVS_NAMESPACE, false);
preferences.clear();           // wipes entire NVS namespace
preferences.end();
delay(100);
ESP.restart();
```

**`DISCOVERY`:**
```cpp
mqttPublishDiscovery();        // guarded: !mqttConnected || !mqtt_ha_discovery → return
```

**`SYNC_TIME`:**
```cpp
if (wifiConnected && extConfig.sta_enabled) {
    lastNTPSync    = 0;
    lastNTPAttempt = 0;
    // Next loop iteration triggers tryNTPSync()
}
```

#### Examples

```bash
# Request full status
mosquitto_pub -h <broker> -t "esp32/relay16_A1B2/system/set" -m "STATUS"

# Graceful restart
mosquitto_pub -h <broker> -t "esp32/relay16_A1B2/system/set" -m "RESTART"

# Factory reset (DANGER)
mosquitto_pub -h <broker> -t "esp32/relay16_A1B2/system/set" -m "FACTORY_RESET"

# Republish HA discovery
mosquitto_pub -h <broker> -t "esp32/relay16_A1B2/system/set" -m "DISCOVERY"

# Force NTP sync
mosquitto_pub -h <broker> -t "esp32/relay16_A1B2/system/set" -m "SYNC_TIME"
```

---

## 📤 PUBLISH TOPICS (Device → You subscribe)

### 1️⃣ Relay State (simple)

**Topic:**
```
<base>/relay/N/state
```

**Payload:** `ON` or `OFF`

**Retain:** Yes if `mqtt_retain=1` (default), No if `mqtt_retain=0`

**QoS:** `extConfig.mqtt_qos` (default 1)

**Published when:**
- Relay state changes (manual, schedule, MQTT command)
- Every 30 seconds (auto-refresh via `processMQTT`)
- On MQTT reconnect
- On `STATUS` system command

**Example subscribe:**
```bash
mosquitto_sub -h <broker> -t "esp32/relay16_A1B2/relay/1/state"
# Output: ON
```

---

### 2️⃣ Relay State (JSON)

**Topic:**
```
<base>/relay/N/state/json
```

**Payload:**
```json
{"state":"ON","mode":"manual","pin":23,"name":"Relay 1"}
```

**Fields:**

| Field | Type | Values | Notes |
|---|---|---|---|
| `state` | string | `"ON"` / `"OFF"` | Current logical state |
| `mode` | string | `"manual"` / `"auto"` | `manual` if override, else `auto` |
| `pin` | int | GPIO number | From `gpioConfig.pins[N-1]` |
| `name` | string | 1-15 chars | JSON-escaped (quotes, backslash, control chars) |

**Retain:** Same as `mqtt_retain`

**Example subscribe:**
```bash
mosquitto_sub -h <broker> -t "esp32/relay16_A1B2/relay/+/state/json"
```

---

### 3️⃣ System State (JSON)

**Topic:**
```
<base>/system/state
```

**Payload:**
```json
{
  "status": "online",
  "uptime": 12345,
  "freeHeap": 234567,
  "wifiConnected": true,
  "wifiSSID": "MyNetwork",
  "rssi": -55,
  "ip": "192.168.1.100",
  "apIP": "192.168.4.1",
  "relayCount": 16,
  "version": 13,
  "utcEpoch": "1730000000",
  "timeSource": "ntp",
  "rtcPresent": true,
  "rtcValid": true,
  "driftComp": 1.0
}
```

**Field reference:**

| Field | Type | Notes |
|---|---|---|
| `status` | string | Always `"online"` when connected |
| `uptime` | int | Seconds since boot |
| `freeHeap` | int | Free RAM in bytes |
| `wifiConnected` | bool | STA connection status |
| `wifiSSID` | string | Configured SSID |
| `rssi` | int | WiFi RSSI in dBm (0 if not connected) |
| `ip` | string | STA IP (e.g., "0.0.0.0" if not connected) |
| `apIP` | string | AP IP (usually 192.168.4.1) |
| `relayCount` | int | Number of configured relays |
| `version` | int | `EEPROM_VERSION` = 13 |
| `utcEpoch` | string/null | Unix epoch as string, `null` if time unknown |
| `timeSource` | string | `"ntp"` / `"browser"` / `"rtc"` / `"none"` |
| `rtcPresent` | bool | DS3231 detected |
| `rtcValid` | bool | RTC time trusted |
| `driftComp` | float | Drift compensation factor (1.0 = no drift) |

**Retain:** Same as `mqtt_retain`

**Published when:**
- MQTT connects
- `STATUS` command received
- Every 30s (via `processMQTT`)
- Time source changes (NTP sync, browser sync)
- RTC state change

---

### 4️⃣ System Availability (LWT)

**Topic:**
```
<base>/system/availability
```

**Payload:** `online` or `offline`

**Retain:** **Always Yes** (hardcoded)

**QoS:** **Always 1** (hardcoded)

**Behavior:**
- Registered as **Last Will and Testament** during `mqttClient.connect()`
- Published `online` immediately after successful connect (retained)
- Published `offline` on **graceful disconnect** (via `mqttGracefulDisconnect()`)
- Broker auto-publishes `offline` if device disconnects ungracefully (~60-90s keepalive timeout)

**Triggers for `offline`:**
- `RESTART` command
- `FACTORY_RESET` command
- MQTT disabled via web UI
- WiFi disconnected
- AP settings changed (AP restart)
- WiFi credentials changed
- GPIO config changed
- Factory reset via BOOT button

**Example subscribe:**
```bash
mosquitto_sub -h <broker> -t "esp32/relay16_A1B2/system/availability"
```

---

### 5️⃣ Home Assistant Discovery Topics

**Only published when `mqtt_ha_discovery = 1`**

#### Per-relay switch discovery

**Topic:**
```
homeassistant/switch/esp32relay16_<MAC>_<N>/config
```
`<N>` = 0-indexed relay number (0 to `relayCount-1`)

**Payload (example for relay 0):**
```json
{
  "name": "Relay 1",
  "uniq_id": "esp32relay16_A1B2_0",
  "cmd_t": "esp32/relay16_A1B2/relay/1/set",
  "stat_t": "esp32/relay16_A1B2/relay/1/state",
  "avty_t": "esp32/relay16_A1B2/system/availability",
  "pl_on": "ON",
  "pl_off": "OFF",
  "stat_on": "ON",
  "stat_off": "OFF",
  "ret": true,
  "qos": 1,
  "dev": {
    "ids": "esp32relay16_A1B2",
    "name": "ESP32 16CH Relay",
    "mdl": "ESP32-38P",
    "sw": "13",
    "mf": "Raff Alds"
  }
}
```

#### Status sensor discovery

**Topic:**
```
homeassistant/sensor/esp32relay16_<MAC>_status/config
```

**Payload:**
```json
{
  "name": "Status",
  "uniq_id": "esp32relay16_A1B2_status",
  "stat_t": "esp32/relay16_A1B2/system/state",
  "val_tpl": "{{value_json.status}}",
  "json_attr_t": "esp32/relay16_A1B2/system/state",
  "avty_t": "esp32/relay16_A1B2/system/availability",
  "dev": {
    "ids": "esp32relay16_A1B2",
    "name": "ESP32 16CH Relay",
    "mdl": "ESP32-38P",
    "sw": "13",
    "mf": "Raff Alds"
  }
}
```

**Retain:** **Always Yes**

**Published when:**
- MQTT connects (if `ha_discovery=1`)
- Every **30 minutes** (auto-refresh, `MQTT_DISCOVERY_INTERVAL`)
- On `DISCOVERY` system command
- On relay rename (single discovery)
- On `ha_discovery` toggle 0 → 1

**Cleared when:**
- `ha_discovery` toggle 1 → 0 (empty retained payloads)

---

## 📊 Complete Command Matrix

### You Publish → Device (Subscribe)

| Topic | Payload | Action | Guards |
|---|---|---|---|
| `<base>/relay/1/set` | `ON` / `1` / `true` | Relay 1 ON (manual) | mqtt_enabled, wifiConnected |
| `<base>/relay/1/set` | `OFF` / `0` / `false` | Relay 1 OFF (manual) | Same |
| `<base>/relay/1/set` | `TOGGLE` | Flip relay 1 | Same |
| `<base>/relay/1/set` | `AUTO` | Relay 1 → schedule | Same |
| `<base>/relay/2/set` ... `<base>/relay/16/set` | Same 4 commands | Relay 2-16 | Same |
| `<base>/system/set` | `STATUS` | Publish full state | Same |
| `<base>/system/set` | `RESTART` | Reboot | Same |
| `<base>/system/set` | `FACTORY_RESET` | Erase + reboot | Same |
| `<base>/system/set` | `DISCOVERY` | Republish HA config | + `ha_discovery=1` |
| `<base>/system/set` | `SYNC_TIME` | Force NTP sync | + `sta_enabled && wifiConnected` |

### Device → You (Subscribe)

| Topic | Payload | Retain | QoS | Trigger |
|---|---|---|---|---|
| `<base>/relay/N/state` | `ON`/`OFF` | Config | Config | State change, 30s refresh, reconnect, STATUS |
| `<base>/relay/N/state/json` | JSON | Config | Config | Same as above |
| `<base>/system/state` | JSON | Config | Config | Reconnect, STATUS, 30s refresh, time sync |
| `<base>/system/availability` | `online`/`offline` | **Yes** | **1** | Connect/disconnect |
| `homeassistant/switch/esp32relay16_<MAC>_<N>/config` | JSON | **Yes** | Config | Connect, 30min, DISCOVERY, rename |
| `homeassistant/sensor/esp32relay16_<MAC>_status/config` | JSON | **Yes** | Config | Connect, 30min, DISCOVERY |

---

## 🎯 Complete Working Examples

### Setup (one-time)

```bash
# Environment variables for convenience
export BROKER="192.168.1.10"
export BASE="esp32/relay16_A1B2"
```

### Example 1: Subscribe to everything

```bash
mosquitto_sub -h $BROKER -t "$BASE/#" -v
```

### Example 2: Control relay 1

```bash
# Turn ON
mosquitto_pub -h $BROKER -t "$BASE/relay/1/set" -m "ON"

# Toggle
mosquitto_pub -h $BROKER -t "$BASE/relay/1/set" -m "TOGGLE"

# Return to schedule
mosquitto_pub -h $BROKER -t "$BASE/relay/1/set" -m "AUTO"
```

### Example 3: Control ALL 16 relays

```bash
# All ON
for i in $(seq 1 16); do
  mosquitto_pub -h $BROKER -t "$BASE/relay/$i/set" -m "ON"
done

# All OFF
for i in $(seq 1 16); do
  mosquitto_pub -h $BROKER -t "$BASE/relay/$i/set" -m "OFF"
done

# All AUTO
for i in $(seq 1 16); do
  mosquitto_pub -h $BROKER -t "$BASE/relay/$i/set" -m "AUTO"
done
```

### Example 4: Monitor relay 5 only

```bash
mosquitto_sub -h $BROKER -t "$BASE/relay/5/state" -t "$BASE/relay/5/state/json"
```

### Example 5: Monitor connection

```bash
mosquitto_sub -h $BROKER -t "$BASE/system/availability" -v
# Output:
# esp32/relay16_A1B2/system/availability online
# ... (on disconnect)
# esp32/relay16_A1B2/system/availability offline
```

### Example 6: Full system status

```bash
# Request fresh status
mosquitto_pub -h $BROKER -t "$BASE/system/set" -m "STATUS"

# Read it (retained)
mosquitto_sub -h $BROKER -t "$BASE/system/state" -C 1
```

### Example 7: Node-RED / Home Assistant automation

```yaml
# Home Assistant automation example
automation:
  - alias: "Turn on porch light at sunset"
    trigger:
      - platform: sun
        event: sunset
    action:
      - service: mqtt.publish
        data:
          topic: "esp32/relay16_A1B2/relay/3/set"
          payload: "ON"
```

```javascript
// Node-RED example
msg.topic = "esp32/relay16_A1B2/relay/5/set";
msg.payload = "TOGGLE";
return msg;
```

### Example 8: Python paho-mqtt

```python
import paho.mqtt.client as mqtt

BROKER = "192.168.1.10"
BASE = "esp32/relay16_A1B2"

def on_message(client, userdata, msg):
    print(f"{msg.topic}: {msg.payload.decode()}")

client = mqtt.Client()
client.on_message = on_message
client.connect(BROKER, 1883, 60)

# Subscribe to all device topics
client.subscribe(f"{BASE}/#")
client.loop_start()

# Control relay 1
client.publish(f"{BASE}/relay/1/set", "ON")
client.publish(f"{BASE}/relay/2/set", "TOGGLE")
client.publish(f"{BASE}/system/set", "STATUS")
```

---

## ⚙️ Processing Rules & Guards

### Command Processing Chain

```
MQTT Broker delivers message
        ↓
mqttClient.loop() → mqttCallback(topic, payload, length)
        ↓
Guard: if (!extConfig.mqtt_enabled) return;
        ↓
Buffer: char msg[128], truncated if length >= 128
Trim: leading/trailing whitespace removed
        ↓
Match topic:
  - Loop i=0..gpioConfig.count-1:
      if (strcmp(topic, mqttTopicRelayCmd[i]) == 0)
          → mqttHandleRelayCommand(i, payload)
  - if (strcmp(topic, mqttTopicSystemCmd) == 0)
          → mqttHandleSystemCommand(payload)
        ↓
Unknown topic → ignored
```

### Relay Command Guards

```cpp
if (!extConfig.mqtt_enabled) return;
if (index >= gpioConfig.count) return;
```

### System Command Guards

```cpp
if (!extConfig.mqtt_enabled) return;
// Per-command:
//   DISCOVERY → mqtt_ha_discovery must be 1
//   SYNC_TIME → wifiConnected && sta_enabled
```

### Publish Guards

```cpp
// mqttPublishRelayState
if (!mqttConnected || index >= gpioConfig.count) return;

// mqttPublishSystemState
if (!mqttConnected) return;

// mqttPublishDiscovery
if (!mqttConnected || !extConfig.mqtt_ha_discovery) return;

// mqttPublishSingleDiscovery
if (!mqttConnected || index >= gpioConfig.count) return;
```

---

## 🔄 Reconnect & Backoff

| Parameter | Value |
|---|---|
| Reconnect interval | 5 seconds |
| Max attempts before backoff | 10 |
| Backoff duration | 5 minutes (300,000 ms) |
| Client ID auto-gen | `esp32relay16_<MAC>` if empty |
| Keep-alive | 60 seconds |
| Socket timeout | 10 seconds |
| Buffer size | 1024 bytes |
| LWT topic | `<base>/system/availability` |
| LWT payload | `offline` |
| LWT QoS | 1 |
| LWT retain | true |

---

## 🧪 Verification Tests

### Test 1: Basic connectivity
```bash
mosquitto_sub -h $BROKER -t "$BASE/system/availability" -v
# Expected: online (retained)
```

### Test 2: Relay control
```bash
mosquitto_sub -h $BROKER -t "$BASE/relay/1/state" &
mosquitto_pub -h $BROKER -t "$BASE/relay/1/set" -m "ON"
# Expected: ON
```

### Test 3: Alias verification
```bash
for cmd in ON on On 1 true TRUE; do
  mosquitto_pub -h $BROKER -t "$BASE/relay/1/set" -m "$cmd"
  sleep 0.3
done
# All should turn relay ON
```

### Test 4: Toggle test
```bash
for i in 1 2 3 4; do
  mosquitto_pub -h $BROKER -t "$BASE/relay/1/set" -m "TOGGLE"
  sleep 0.3
done
# State should alternate ON/OFF/ON/OFF
```

### Test 5: System status
```bash
mosquitto_pub -h $BROKER -t "$BASE/system/set" -m "STATUS"
mosquitto_sub -h $BROKER -t "$BASE/system/state" -C 1
# Should print full JSON
```

### Test 6: LWT simulation
```bash
# 1. Subscribe
mosquitto_sub -h $BROKER -t "$BASE/system/availability" -v

# 2. Pull power on ESP32 (not graceful)
# 3. Wait ~90 seconds
# Expected: offline (broker publishes LWT)
```

### Test 7: HA discovery
```bash
mosquitto_sub -h $BROKER -t "homeassistant/switch/esp32relay16_+/config" -v
# Should show per-relay configs
```

### Test 8: Retained message check
```bash
mosquitto_sub -h $BROKER -t "$BASE/relay/1/state" -C 1
# Should immediately receive retained value
```

### Test 9: Wildcard monitoring
```bash
mosquitto_sub -h $BROKER -t "$BASE/#" -v
# Shows all device activity
```

---

## 📋 Quick Reference Card

```
╔══════════════════════════════════════════════════════════════╗
║  ESP32 16CH RELAY — MQTT QUICK REFERENCE                    ║
║  Default base: esp32/relay16_<MAC>                           ║
╠══════════════════════════════════════════════════════════════╣
║  RELAY COMMANDS → <base>/relay/N/set  (N = 1..16)           ║
║    ON   | 1    | true    → Relay ON (manual)                 ║
║    OFF  | 0    | false   → Relay OFF (manual)                ║
║    TOGGLE                → Flip current state                ║
║    AUTO                  → Return to schedule                ║
╠══════════════════════════════════════════════════════════════╣
║  SYSTEM COMMANDS → <base>/system/set                         ║
║    STATUS          → Publish full state                      ║
║    RESTART         → Reboot                                  ║
║    FACTORY_RESET   → Erase all + reboot                      ║
║    DISCOVERY       → Republish HA config                     ║
║    SYNC_TIME       → Force NTP sync                          ║
╠══════════════════════════════════════════════════════════════╣
║  STATE TOPICS ← subscribe                                    ║
║    <base>/relay/N/state         → ON | OFF                   ║
║    <base>/relay/N/state/json    → JSON per relay             ║
║    <base>/system/state          → Full JSON state            ║
║    <base>/system/availability   → online | offline (LWT)     ║
╠══════════════════════════════════════════════════════════════╣
║  All commands: case-insensitive, whitespace-trimmed          ║
║  Max payload: 127 bytes                                      ║
║  Reconnect: 5s intervals, 10 tries → 5min backoff            ║
╚══════════════════════════════════════════════════════════════╝
```

---
