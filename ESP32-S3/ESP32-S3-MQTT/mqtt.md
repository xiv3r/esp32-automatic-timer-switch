# Complete MQTT Control Commands — ESP32‑S3 16‑Channel Relay Timer Switch

- **Default base topic:** `esp32s3/relay16`
- **Default client ID:** `esp32s3_relay16`
- **Relay N:** `1` … `16`
- **QoS:** configurable (0 or 1)
- **Payload parsing:** case‑insensitive, whitespace‑trimmed

---

## PART 1 — ALL COMMANDS (Device Subscribes — You Publish)

### 1.1 Per‑Relay Set — `<base>/relay/N/set`

| Payload | Result |
|---|---|
| `ON` / `on` / `1` / `true` | Relay N → ON (manual override) |
| `OFF` / `off` / `0` / `false` | Relay N → OFF (manual override) |
| any other | **ignored** |

**All 16 topics:**
```
esp32s3/relay16/relay/1/set
esp32s3/relay16/relay/2/set
esp32s3/relay16/relay/3/set
esp32s3/relay16/relay/4/set
esp32s3/relay16/relay/5/set
esp32s3/relay16/relay/6/set
esp32s3/relay16/relay/7/set
esp32s3/relay16/relay/8/set
esp32s3/relay16/relay/9/set
esp32s3/relay16/relay/10/set
esp32s3/relay16/relay/11/set
esp32s3/relay16/relay/12/set
esp32s3/relay16/relay/13/set
esp32s3/relay16/relay/14/set
esp32s3/relay16/relay/15/set
esp32s3/relay16/relay/16/set
```

---

### 1.2 Per‑Relay Toggle — `<base>/relay/N/toggle`

Payload ignored. Flips current state, sets manual override.

```
esp32s3/relay16/relay/1/toggle
esp32s3/relay16/relay/2/toggle
esp32s3/relay16/relay/3/toggle
esp32s3/relay16/relay/4/toggle
esp32s3/relay16/relay/5/toggle
esp32s3/relay16/relay/6/toggle
esp32s3/relay16/relay/7/toggle
esp32s3/relay16/relay/8/toggle
esp32s3/relay16/relay/9/toggle
esp32s3/relay16/relay/10/toggle
esp32s3/relay16/relay/11/toggle
esp32s3/relay16/relay/12/toggle
esp32s3/relay16/relay/13/toggle
esp32s3/relay16/relay/14/toggle
esp32s3/relay16/relay/15/toggle
esp32s3/relay16/relay/16/toggle
```

---

### 1.3 Per‑Relay Auto — `<base>/relay/N/auto`

Payload ignored. Clears manual override, returns relay to schedule.

```
esp32s3/relay16/relay/1/auto
esp32s3/relay16/relay/2/auto
esp32s3/relay16/relay/3/auto
esp32s3/relay16/relay/4/auto
esp32s3/relay16/relay/5/auto
esp32s3/relay16/relay/6/auto
esp32s3/relay16/relay/7/auto
esp32s3/relay16/relay/8/auto
esp32s3/relay16/relay/9/auto
esp32s3/relay16/relay/10/auto
esp32s3/relay16/relay/11/auto
esp32s3/relay16/relay/12/auto
esp32s3/relay16/relay/13/auto
esp32s3/relay16/relay/14/auto
esp32s3/relay16/relay/15/auto
esp32s3/relay16/relay/16/auto
```

---

### 1.4 All Relays — `<base>/all/set`

| Payload | Result |
|---|---|
| `ON` / `on` / `1` / `true` | ALL relays → ON (manual override each) |
| `OFF` / `off` / `0` / `false` | ALL relays → OFF (manual override each) |
| any other | **ignored** |

```
esp32s3/relay16/all/set
```

---

### 1.5 System Commands — `<base>/system/<cmd>/set`

#### Restart
```
esp32s3/relay16/system/restart/set        1   or   true
```
Publishes `{"action":"restart","status":"ok"}` → `esp32s3/relay16/system/response`, waits 500 ms, restarts.

#### Factory Reset
```
esp32s3/relay16/system/factory_reset/set  1   or   true
```
Publishes `{"action":"factory_reset","status":"ok"}` → `esp32s3/relay16/system/response`, clears NVS, waits 100 ms, restarts.

#### Verify Services
```
esp32s3/relay16/system/verify/set         (any payload)
```
Runs recovery, publishes `{"action":"verify","status":"ok"}` → `esp32s3/relay16/system/response`.

#### Request Status
```
esp32s3/relay16/system/status             (any payload)
```
Forces republish of all relay states + system JSON.

---

### 1.6 Schedule — `<base>/schedule/N/set`

JSON payload. `slot` required (0–7).

```
esp32s3/relay16/schedule/1/set
esp32s3/relay16/schedule/2/set
esp32s3/relay16/schedule/3/set
esp32s3/relay16/schedule/4/set
esp32s3/relay16/schedule/5/set
esp32s3/relay16/schedule/6/set
esp32s3/relay16/schedule/7/set
esp32s3/relay16/schedule/8/set
esp32s3/relay16/schedule/9/set
esp32s3/relay16/schedule/10/set
esp32s3/relay16/schedule/11/set
esp32s3/relay16/schedule/12/set
esp32s3/relay16/schedule/13/set
esp32s3/relay16/schedule/14/set
esp32s3/relay16/schedule/15/set
esp32s3/relay16/schedule/16/set
```

**Field reference:**

| Field | Type | Range |
|---|---|---|
| `slot` | int | 0–7 (required) |
| `enabled` | bool | true / false |
| `start` | string | `"HH:MM:SS"` (00–23 / 00–59 / 00–59) |
| `stop` | string | `"HH:MM:SS"` (00–23 / 00–59 / 00–59) |
| `days` | uint8 | 0–127 (bit0=Sun … bit6=Sat) |
| `monthDays` | uint32 | 0–2147483647 (bit0=1st … bit30=31st) |
| `monthMask` | uint16 | 0–4095 (bit0=Jan … bit11=Dec) |

**Day bit values:** Sun=1, Mon=2, Tue=4, Wed=8, Thu=16, Fri=32, Sat=64, All=127
**Month bit values:** Jan=1, Feb=2, Mar=4, Apr=8, May=16, Jun=32, Jul=64, Aug=128, Sep=256, Oct=512, Nov=1024, Dec=2048, All=4095

**Example payloads:**
```json
{"slot":0,"enabled":true,"start":"07:30:00","stop":"18:00:00","days":62,"monthDays":2147483647,"monthMask":4095}
{"slot":1,"enabled":false,"start":"00:00:00","stop":"00:00:00","days":127,"monthDays":2147483647,"monthMask":4095}
{"slot":2,"enabled":true,"start":"22:00:00","stop":"06:00:00","days":127,"monthDays":2147483647,"monthMask":4095}
{"slot":3,"enabled":true,"start":"08:00:00","stop":"12:00:00","days":1,"monthDays":1,"monthMask":1}
{"slot":4,"enabled":true,"start":"18:00:00","stop":"23:59:59","days":65,"monthDays":2147483647,"monthMask":4095}
{"slot":5,"enabled":true,"start":"00:00:00","stop":"00:00:00","days":127,"monthDays":2147483647,"monthMask":4095}
```

---

### 1.7 Time Sync — `<base>/time/sync`

| Payload | Result |
|---|---|
| `ntp` (any case) | Forces NTP resync on next loop |
| decimal epoch string | Sets time from epoch (validated) |
| anything else | **ignored** |

```
esp32s3/relay16/time/sync   →   ntp
esp32s3/relay16/time/sync   →   1735689600
esp32s3/relay16/time/sync   →   1700000000
esp32s3/relay16/time/sync   →   2000000000
```

**Validation:** epoch must be 1577836800–4294967295 (2020‑01‑01 … 2100‑01‑01). If current source is trusted (< 24 h old), drift > 1 year is rejected.

---

## PART 2 — ALL STATUS TOPICS (Device Publishes — You Subscribe)

### 2.1 Per‑Relay Status (48 topics total — 16 relays × 3 suffixes)

```
esp32s3/relay16/relay/1/state      →  ON | OFF
esp32s3/relay16/relay/1/mode       →  auto | manual
esp32s3/relay16/relay/1/json       →  {"relay":1,"name":"Relay 1","state":"ON","manual":false,"pin":4,"activeLow":true}
esp32s3/relay16/relay/2/state      →  ON | OFF
esp32s3/relay16/relay/2/mode       →  auto | manual
esp32s3/relay16/relay/2/json       →  {...}
esp32s3/relay16/relay/3/state      →  ON | OFF
esp32s3/relay16/relay/3/mode       →  auto | manual
esp32s3/relay16/relay/3/json       →  {...}
esp32s3/relay16/relay/4/state      →  ON | OFF
esp32s3/relay16/relay/4/mode       →  auto | manual
esp32s3/relay16/relay/4/json       →  {...}
esp32s3/relay16/relay/5/state      →  ON | OFF
esp32s3/relay16/relay/5/mode       →  auto | manual
esp32s3/relay16/relay/5/json       →  {...}
esp32s3/relay16/relay/6/state      →  ON | OFF
esp32s3/relay16/relay/6/mode       →  auto | manual
esp32s3/relay16/relay/6/json       →  {...}
esp32s3/relay16/relay/7/state      →  ON | OFF
esp32s3/relay16/relay/7/mode       →  auto | manual
esp32s3/relay16/relay/7/json       →  {...}
esp32s3/relay16/relay/8/state      →  ON | OFF
esp32s3/relay16/relay/8/mode       →  auto | manual
esp32s3/relay16/relay/8/json       →  {...}
esp32s3/relay16/relay/9/state      →  ON | OFF
esp32s3/relay16/relay/9/mode       →  auto | manual
esp32s3/relay16/relay/9/json       →  {...}
esp32s3/relay16/relay/10/state     →  ON | OFF
esp32s3/relay16/relay/10/mode      →  auto | manual
esp32s3/relay16/relay/10/json      →  {...}
esp32s3/relay16/relay/11/state     →  ON | OFF
esp32s3/relay16/relay/11/mode      →  auto | manual
esp32s3/relay16/relay/11/json      →  {...}
esp32s3/relay16/relay/12/state     →  ON | OFF
esp32s3/relay16/relay/12/mode      →  auto | manual
esp32s3/relay16/relay/12/json      →  {...}
esp32s3/relay16/relay/13/state     →  ON | OFF
esp32s3/relay16/relay/13/mode      →  auto | manual
esp32s3/relay16/relay/13/json      →  {...}
esp32s3/relay16/relay/14/state     →  ON | OFF
esp32s3/relay16/relay/14/mode      →  auto | manual
esp32s3/relay16/relay/14/json      →  {...}
esp32s3/relay16/relay/15/state     →  ON | OFF
esp32s3/relay16/relay/15/mode      →  auto | manual
esp32s3/relay16/relay/15/json      →  {...}
esp32s3/relay16/relay/16/state     →  ON | OFF
esp32s3/relay16/relay/16/mode      →  auto | manual
esp32s3/relay16/relay/16/json      →  {...}
```

### 2.2 Aggregate Status

```
esp32s3/relay16/relays/json   →  {"count":16,"timestamp":123456,"relays":[{"id":1,"name":"Relay 1","state":"ON","manual":false,"pin":4}, ...]}
esp32s3/relay16/all/state     →  ON | OFF
esp32s3/relay16/system/json   →  {"version":13,"uptime":12345,"freeHeap":123456,"wifi_connected":true,"wifi_ssid":"MyWiFi","rssi":-55,"ip":"192.168.1.50","ap_ip":"192.168.4.1","utc_epoch":"1735689600","time_source":"ntp","rtc_present":true,"rtc_temp":24.75,"relay_count":16,"mdns_hostname":"esp32s3"}
esp32s3/relay16/status        →  online | offline
esp32s3/relay16/system/response  →  {"action":"restart","status":"ok"} | {"action":"factory_reset","status":"ok"} | {"action":"verify","status":"ok"}
```

### 2.3 Home Assistant Discovery (if enabled)

```
homeassistant/switch/esp32s3_relay16_relay1/config
homeassistant/switch/esp32s3_relay16_relay2/config
homeassistant/switch/esp32s3_relay16_relay3/config
homeassistant/switch/esp32s3_relay16_relay4/config
homeassistant/switch/esp32s3_relay16_relay5/config
homeassistant/switch/esp32s3_relay16_relay6/config
homeassistant/switch/esp32s3_relay16_relay7/config
homeassistant/switch/esp32s3_relay16_relay8/config
homeassistant/switch/esp32s3_relay16_relay9/config
homeassistant/switch/esp32s3_relay16_relay10/config
homeassistant/switch/esp32s3_relay16_relay11/config
homeassistant/switch/esp32s3_relay16_relay12/config
homeassistant/switch/esp32s3_relay16_relay13/config
homeassistant/switch/esp32s3_relay16_relay14/config
homeassistant/switch/esp32s3_relay16_relay15/config
homeassistant/switch/esp32s3_relay16_relay16/config
homeassistant/switch/esp32s3_relay16_all/config
```

---

## PART 3 — FULL COMMAND MASTER LIST (Copy‑Paste)

```
# ── RELAY SET ────────────────────────────────────────
esp32s3/relay16/relay/1/set                              ON | OFF | 1 | 0 | true | false
esp32s3/relay16/relay/2/set                              ON | OFF | 1 | 0 | true | false
esp32s3/relay16/relay/3/set                              ON | OFF | 1 | 0 | true | false
esp32s3/relay16/relay/4/set                              ON | OFF | 1 | 0 | true | false
esp32s3/relay16/relay/5/set                              ON | OFF | 1 | 0 | true | false
esp32s3/relay16/relay/6/set                              ON | OFF | 1 | 0 | true | false
esp32s3/relay16/relay/7/set                              ON | OFF | 1 | 0 | true | false
esp32s3/relay16/relay/8/set                              ON | OFF | 1 | 0 | true | false
esp32s3/relay16/relay/9/set                              ON | OFF | 1 | 0 | true | false
esp32s3/relay16/relay/10/set                             ON | OFF | 1 | 0 | true | false
esp32s3/relay16/relay/11/set                             ON | OFF | 1 | 0 | true | false
esp32s3/relay16/relay/12/set                             ON | OFF | 1 | 0 | true | false
esp32s3/relay16/relay/13/set                             ON | OFF | 1 | 0 | true | false
esp32s3/relay16/relay/14/set                             ON | OFF | 1 | 0 | true | false
esp32s3/relay16/relay/15/set                             ON | OFF | 1 | 0 | true | false
esp32s3/relay16/relay/16/set                             ON | OFF | 1 | 0 | true | false

# ── RELAY TOGGLE ─────────────────────────────────────
esp32s3/relay16/relay/1/toggle                           (any)
esp32s3/relay16/relay/2/toggle                           (any)
esp32s3/relay16/relay/3/toggle                           (any)
esp32s3/relay16/relay/4/toggle                           (any)
esp32s3/relay16/relay/5/toggle                           (any)
esp32s3/relay16/relay/6/toggle                           (any)
esp32s3/relay16/relay/7/toggle                           (any)
esp32s3/relay16/relay/8/toggle                           (any)
esp32s3/relay16/relay/9/toggle                           (any)
esp32s3/relay16/relay/10/toggle                          (any)
esp32s3/relay16/relay/11/toggle                          (any)
esp32s3/relay16/relay/12/toggle                          (any)
esp32s3/relay16/relay/13/toggle                          (any)
esp32s3/relay16/relay/14/toggle                          (any)
esp32s3/relay16/relay/15/toggle                          (any)
esp32s3/relay16/relay/16/toggle                          (any)

# ── RELAY AUTO ───────────────────────────────────────
esp32s3/relay16/relay/1/auto                             (any)
esp32s3/relay16/relay/2/auto                             (any)
esp32s3/relay16/relay/3/auto                             (any)
esp32s3/relay16/relay/4/auto                             (any)
esp32s3/relay16/relay/5/auto                             (any)
esp32s3/relay16/relay/6/auto                             (any)
esp32s3/relay16/relay/7/auto                             (any)
esp32s3/relay16/relay/8/auto                             (any)
esp32s3/relay16/relay/9/auto                             (any)
esp32s3/relay16/relay/10/auto                            (any)
esp32s3/relay16/relay/11/auto                            (any)
esp32s3/relay16/relay/12/auto                            (any)
esp32s3/relay16/relay/13/auto                            (any)
esp32s3/relay16/relay/14/auto                            (any)
esp32s3/relay16/relay/15/auto                            (any)
esp32s3/relay16/relay/16/auto                            (any)

# ── ALL RELAYS ───────────────────────────────────────
esp32s3/relay16/all/set                                  ON | OFF | 1 | 0 | true | false

# ── SYSTEM ───────────────────────────────────────────
esp32s3/relay16/system/restart/set                       1 | true
esp32s3/relay16/system/factory_reset/set                 1 | true
esp32s3/relay16/system/verify/set                        (any)
esp32s3/relay16/system/status                            (any)

# ── SCHEDULE ─────────────────────────────────────────
esp32s3/relay16/schedule/1/set                           JSON
esp32s3/relay16/schedule/2/set                           JSON
esp32s3/relay16/schedule/3/set                           JSON
esp32s3/relay16/schedule/4/set                           JSON
esp32s3/relay16/schedule/5/set                           JSON
esp32s3/relay16/schedule/6/set                           JSON
esp32s3/relay16/schedule/7/set                           JSON
esp32s3/relay16/schedule/8/set                           JSON
esp32s3/relay16/schedule/9/set                           JSON
esp32s3/relay16/schedule/10/set                          JSON
esp32s3/relay16/schedule/11/set                          JSON
esp32s3/relay16/schedule/12/set                          JSON
esp32s3/relay16/schedule/13/set                          JSON
esp32s3/relay16/schedule/14/set                          JSON
esp32s3/relay16/schedule/15/set                          JSON
esp32s3/relay16/schedule/16/set                          JSON

# ── TIME SYNC ────────────────────────────────────────
esp32s3/relay16/time/sync                                ntp | <epoch>
```

---

## PART 4 — READY‑TO‑RUN SCRIPTS

### Relay Control (`relay.sh`)
```bash
#!/bin/bash
BROKER="192.168.1.100"
BASE="esp32s3/relay16"

on()     { mosquitto_pub -h $BROKER -t "$BASE/relay/$1/set"    -m "ON"; }
off()    { mosquitto_pub -h $BROKER -t "$BASE/relay/$1/set"    -m "OFF"; }
toggle() { mosquitto_pub -h $BROKER -t "$BASE/relay/$1/toggle" -m ""; }
auto()   { mosquitto_pub -h $BROKER -t "$BASE/relay/$1/auto"   -m ""; }
all_on() { mosquitto_pub -h $BROKER -t "$BASE/all/set"         -m "ON"; }
all_off(){ mosquitto_pub -h $BROKER -t "$BASE/all/set"         -m "OFF"; }

case "$1" in
  on)     on     $2 ;;
  off)    off    $2 ;;
  toggle) toggle $2 ;;
  auto)   auto   $2 ;;
  allon)  all_on    ;;
  alloff) all_off   ;;
  *) echo "Usage: $0 {on|off|toggle|auto} <N> | {allon|alloff}" ;;
esac
```

### System Control (`system.sh`)
```bash
#!/bin/bash
BROKER="192.168.1.100"
BASE="esp32s3/relay16"

case "$1" in
  restart) mosquitto_pub -h $BROKER -t "$BASE/system/restart/set"       -m "1" ;;
  factory) mosquitto_pub -h $BROKER -t "$BASE/system/factory_reset/set" -m "true" ;;
  verify)  mosquitto_pub -h $BROKER -t "$BASE/system/verify/set"        -m "" ;;
  status)  mosquitto_pub -h $BROKER -t "$BASE/system/status"            -m "" ;;
  ntp)     mosquitto_pub -h $BROKER -t "$BASE/time/sync"                -m "ntp" ;;
  epoch)   mosquitto_pub -h $BROKER -t "$BASE/time/sync"                -m "$2" ;;
  *) echo "Usage: $0 {restart|factory|verify|status|ntp|epoch <n>}" ;;
esac
```

### Schedule Set (`schedule.sh`)
```bash
#!/bin/bash
BROKER="192.168.1.100"
BASE="esp32s3/relay16"

# set_schedule <relay> <slot> <enabled> <start> <stop> <days> <monthDays> <monthMask>
set_schedule() {
  mosquitto_pub -h $BROKER -t "$BASE/schedule/$1/set" -m \
    "{\"slot\":$2,\"enabled\":$3,\"start\":\"$4\",\"stop\":\"$5\",\"days\":$6,\"monthDays\":$7,\"monthMask\":$8}"
}

set_schedule 1 0 true "07:30:00" "18:00:00" 62 2147483647 4095
```

### Monitor (`monitor.sh`)
```bash
#!/bin/bash
mosquitto_sub -h 192.168.1.100 -t 'esp32s3/relay16/#' -v
```

---

## PART 5 — HOME ASSISTANT YAML (Manual, if auto‑discovery disabled)

```yaml
mqtt:
  switch:
    - name: "Relay 1"
      unique_id: esp32s3_relay16_relay1
      state_topic: "esp32s3/relay16/relay/1/state"
      command_topic: "esp32s3/relay16/relay/1/set"
      json_attributes_topic: "esp32s3/relay16/relay/1/json"
      availability_topic: "esp32s3/relay16/status"
      payload_on: "ON"
      payload_off: "OFF"
      state_on: "ON"
      state_off: "OFF"
      retain: true
    # ... repeat for relays 2-16 ...
    - name: "All Relays"
      unique_id: esp32s3_relay16_all_relays
      state_topic: "esp32s3/relay16/all/state"
      command_topic: "esp32s3/relay16/all/set"
      availability_topic: "esp32s3/relay16/status"
      payload_on: "ON"
      payload_off: "OFF"
      state_on: "ON"
      state_off: "OFF"
      retain: true
```

---

## PART 6 — QUICK REFERENCE TABLE

| Purpose | Topic | Payload |
|---|---|---|
| Relay ON | `<base>/relay/N/set` | `ON` |
| Relay OFF | `<base>/relay/N/set` | `OFF` |
| Relay Toggle | `<base>/relay/N/toggle` | (any) |
| Relay Auto | `<base>/relay/N/auto` | (any) |
| All ON | `<base>/all/set` | `ON` |
| All OFF | `<base>/all/set` | `OFF` |
| Restart | `<base>/system/restart/set` | `1` |
| Factory Reset | `<base>/system/factory_reset/set` | `1` |
| Verify | `<base>/system/verify/set` | (any) |
| Status Request | `<base>/system/status` | (any) |
| Schedule Set | `<base>/schedule/N/set` | JSON |
| NTP Sync | `<base>/time/sync` | `ntp` |
| Epoch Sync | `<base>/time/sync` | `<epoch>` |

---
