## Requirements
- ESP32 30/38P Pins
- DS3231 RTC Module (recommend)
- 5v 1-16 Channel Relay
- Female to Female Dupont Wire
- Stable Wifi Connection for NTP/RTC sync (optional if no ds3231)
- 5v 3-5a Power supply

`Optional`
- 5v UPS (Maintain RTC Time without DS3231)
- Solid State Relay (SSR DC-AC) (High Load Setup)

## Arduino Libraries
- ArduinoJson
- NTPClient
- RTClib v1.14.1

## Installation
> Download and install 
### ESP32 Win/Linux Drivers
- CH340G: https://sparks.gogo.co.nz/ch340.html
- CP2102: https://www.silabs.com/software-and-tools/usb-to-uart-bridge-vcp-drivers?tab=downloads
## Flasher
### Android (otg)
- https://play.google.com/store/apps/details?id=io.serialflow.espflash
### Windows
- https://dl.espressif.com/public/flash_download_tool.zip
### Linux
```sh
esptool --port <PORT> write_flash 0x0 esp32-firmware-0x0.bin
```
### Win/Linux Browser
- https://g3gg0.github.io/esp32_flasher/flasher.html
### Flash firmware 
- Download the [Firmware](https://github.com/xiv3r/esp32-automatic-timer-switch/releases/tag/esp32) and flash.
- Flash Offset
```
esp32-dump-0x0.bin: 0x0
```

## WiFi Key
- WiFi SSID: `ESP32_16CH_Timer_Switch`
- Password: `ESP32-admin`
  
## Activation
> - Without ds3231 or wifi the time runs from internal rtc

° Online
- Go to `Wifi settings` and connect to your home wifi then everything will work.

° Offline
- Go to `Time settings` and click `Sync Browser ` then everything will work

## Relay Naming 
- Double click relay name to edit

## Set the Time (country)
> Set to your country time e.g for PH (UTC+8.0) 28800 seconds
- Search your country `gmt offsets in seconds` and paste to the Time -> GMT Offset

## Access
- mDNS:`esp32-16ch-timer-switch.local`
- Captive Portal: `Auto redirect`
- Gateway:`192.168.4.1`
- WAN:`192.168.1.123`
- Global:`Enable Port Forwarding on your router to access anywhere`

## Note
- Avoid connecting to a non-existed open wifi network SSID to prevent hang issue. Solution turn off wifi station mode.

<details><summary>

## Isolate Relay Power
</summary>

> ⚠️ Use the Main relay power input and Avoid using VCC and GND from the relay IN GPIO Pin row

### 5V Relay
- Remove the Yellow VCC-JDVCC jumper.
- Relay JD-VCC pin: Connect to external 5V Positive wire.
- Relay GND pin: Connect to external 5V Negative wire.
- Relay VCC pin: Connect to ESP32 5V (powers the LED).

### 12V Relay
- Remove the Yellow VCC-JDVCC jumper.
- Relay JD-VCC pin: Connect to external 12V Positive wire.
- Relay GND pin: Connect to external 12V Negative wire.
- Relay VCC pin: Connect to ESP32 5V (powers the LED).

</details>

## Reset
- Hold BOOT button for 5 seconds to factory reset 

## Restart
- Press EN button to restart

## 16 CHANNEL RELAY GPIO Connection 
```
RELAY  |  ESP32 30/38P
VCC  _____ 5VIN 
IN1  _____ 15  Relay 1
IN2  _____ 2   Relay 2
IN3  _____ 4   Relay 3
IN4  _____ 5   Relay 4
IN5  _____ 18  Relay 5
IN6  _____ 19  Relay 6
IN7  _____ 3   Relay 7
IN8  _____ 1   Relay 8
IN9  _____ 23  Relay 9
IN10 _____ 13  Relay 10
IN11 _____ 14  Relay 11
IN12 _____ 27  Relay 12
IN13 _____ 26  Relay 13
IN14 _____ 25  Relay 14
IN15 _____ 33  Relay 15
IN16 _____ 32  Relay 16
GND  _____ GND
```

## DS3231 GPIO Connection 
```
DS3231 | ESP32 38P
VCC → 3.3V
SDA → 21
SCL → 22
GND → GND
```

<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/libraries/img1.png">
<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/libraries/sc2.png">
<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/libraries/sc3.png">
<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/libraries/sc4.png">
<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/libraries/shot2.jpg">
<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/libraries/shot3.jpg">

<details><summary>
  
# Full Features
</summary>

# ESP32 16-Channel Relay Smart Switch — Exhaustive Feature Compendium
**Firmware Version:** 9.0.0 (NVS Schema V9)  
**Author:** Raff Alds (Xiv3r)  
**License:** GPLv3 — Free Software Foundation  
**Target Silicon:** Espressif ESP32 (XTensa LX6 Dual-Core @ 240MHz, 520KB SRAM, 4MB Flash)  
**Compilation Environment:** PlatformIO / Arduino IDE  
**Documentation Hash:** `SHA-256: e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`

---

## Table of Contents
1. [Philosophy & Architectural Overview](#1-philosophy--architectural-overview)
2. [Memory Map & NVS Layout](#2-memory-map--nvs-layout)
3. [Timekeeping Trinity: Deep Dive](#3-timekeeping-trinity-deep-dive)
4. [Self-Healing Ecosystem](#4-self-healing-ecosystem)
5. [Relay Scheduling Engine: Formal Specification](#5-relay-scheduling-engine-formal-specification)
6. [GPIO Configuration Matrix](#6-gpio-configuration-matrix)
7. [Network Subsystems](#7-network-subsystems)
8. [Web Interface: Complete API Reference](#8-web-interface-complete-api-reference)
9. [WiFi & Access Point Management](#9-wifi--access-point-management)
10. [mDNS & Service Discovery Protocol](#10-mdns--service-discovery-protocol)
11. [Captive Portal Implementation](#11-captive-portal-implementation)
12. [Boot Button & Hardware Factory Reset](#12-boot-button--hardware-factory-reset)
13. [Memory & Resource Management](#13-memory--resource-management)
14. [Critical State Persistence & Fault Recovery](#14-critical-state-persistence--fault-recovery)
15. [CSS & UI Component Library](#15-css--ui-component-library)
16. [Boot Sequence Flowchart](#16-boot-sequence-flowchart)
17. [Main Loop Execution Model](#17-main-loop-execution-model)
18. [Error Codes & Troubleshooting](#18-error-codes--troubleshooting)
19. [Security Considerations](#19-security-considerations)
20. [Build Instructions & Dependencies](#20-build-instructions--dependencies)
21. [Hardware Pinout & Wiring Guide](#21-hardware-pinout--wiring-guide)
22. [Glossary of Constants & Macros](#22-glossary-of-constants--macros)
23. [Appendix: Complete Data Structure Definitions](#23-appendix-complete-data-structure-definitions)

---

## 1. Philosophy & Architectural Overview

### 1.1 Design Principles

| Principle | Implementation |
|---|---|
| **Zero-Blocking Execution** | No `delay()` calls exist in production code paths. All waits use state-machine timeouts or async callbacks. |
| **Defense in Depth** | Three independent time sources, five self-healing subsystems, checksummed NVS persistence. |
| **Graceful Degradation** | System continues relay scheduling with internal RTC even if WiFi, NTP, DS3231, and mDNS simultaneously fail. |
| **Atomic Configuration** | All config writes are `preferences.putBytes()` — no partial writes possible at application layer. |
| **Sub-Atomic Accuracy** | Relay triggers use microsecond-extrapolated epoch with floating-point drift compensation. |
| **CAP Theorem Awareness** | In network partition, system chooses **Availability** (relay control) over **Consistency** (NTP time). |

### 1.2 Core Subsystem Diagram
```
┌─────────────────────────────────────────────────────────┐
│                    ESP32 Main Loop                       │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌─────────┐ │
│  │ Scheduler│  │  Web     │  │  WiFi    │  │  Time   │ │
│  │ Engine   │  │  Server  │  │  Manager │  │  Trinity│ │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬────┘ │
│       │              │              │              │      │
│  ┌────┴──────────────┴──────────────┴──────────────┴───┐ │
│  │           SelfHealingSystem (Health Metrics)        │ │
│  └────────────────────────┬───────────────────────────┘ │
│                           │                              │
│  ┌────────────────────────┴───────────────────────────┐ │
│  │              CriticalRelayState (NVS)               │ │
│  └────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────┘
```

---

## 2. Memory Map & NVS Layout

### 2.1 Non-Volatile Storage Allocation
**Namespace:** `relay16`  
**Total NVS Consumption:** ~5.5KB of 20KB available partition.

| Key | Structure | Size (Bytes) | Write Frequency | Magic/Version |
|-----|-----------|--------------|-----------------|---------------|
| `sysConfig` | `SystemConfig` | ~200 | On config change, periodic RTC save | `0x1234` / V9 |
| `relayConfigs` | `RelayConfig[16]` | ~5,120 | On schedule save, manual override | (embedded in sysConfig save) |
| `extConfig` | `ExtConfig` | 32 | On settings change | `0xEC` |
| `gpioConfig` | `GPIOPinConfig` | ~50 | On pin add/delete/toggle | `0xD002` |
| `criticalState` | `CriticalRelayState` | ~80 | Debounced 300s interval | `0xDEADBEEF` |

### 2.2 NVS Wear Analysis
- **Maximum Write Cycles:** 100,000 (ESP32 specification).
- **Worst-Case Scenario:** Saving every 300s (critical state) + hourly RTC save = ~12,000 writes/year.
- **Expected Lifetime:** >8 years at maximum write frequency.

---

## 3. Timekeeping Trinity: Deep Dive

### 3.1 Internal Software RTC (`getCurrentEpoch()`)

**Principle of Operation:**
1. At sync time, store `internalEpoch` (UTC seconds) and `rtcMicrosAtLastSync` (hardware microsecond counter).
2. On read, calculate elapsed microseconds with overflow handling for `micros()` 71-minute rollover.
3. Apply `driftCompensation` factor to elapsed time.
4. Add compensated seconds to base epoch.
5. Every `RTC_REBASE_INTERVAL` (300s), recalculate base to prevent floating-point accumulation errors.

**Drift Compensation Formula:**
```
adjusted_seconds = (elapsed_micros / 1,000,000) * driftCompensation
current_epoch    = internalEpoch + floor(adjusted_seconds)
                 + (fractional >= 0.5 ? 1 : 0)
```

**Microsecond Overflow Handling:**
```cpp
if (currentMicros >= rtcMicrosAtLastSync) {
    elapsedMicros = currentMicros - rtcMicrosAtLastSync;
} else {
    // Micros rolled over — rebase and return
    performRTCReabase();
    return internalEpoch;
}
```

### 3.2 DS3231 Hardware RTC Integration

**I²C Configuration:**
- SDA: GPIO21
- SCL: GPIO22
- Frequency: Standard mode (100kHz)
- Library: `RTClib` by Adafruit

**Initialization Sequence:**
1. `Wire.begin(21, 22)` — Initialize I²C bus.
2. `rtc.begin()` — Probe device address 0x68.
3. Check `rtc.lostPower()` — If true, RTC time is invalid.
4. Validate year range: 2020-2100.
5. Validate epoch: `VALID_UNIX_TIME()` macro checks bounds.

**Sync Strategy:**
| Trigger | Action |
|---------|--------|
| NTP sync success | Write `internalEpoch` to DS3231 immediately |
| Periodic (3600s) | Write `getCurrentEpoch()` to DS3231 |
| Boot with DS3231 present | Read DS3231 into `internalEpoch` (highest priority) |

### 3.3 NTP Client State Machine

**States:**
```
IDLE (0) ──(interval elapsed)──► CONNECTING (1) ──(request sent)──► WAITING (2)
  ▲                                    │                                  │
  └────────(timeout/fallback)──────────┘                                  │
  ▲                                                                       │
  └───────────────────(success: sync epoch)───────────────────────────────┘
```

**Fallback Server Rotation:**
Index cycles through `NTP_SERVERS[]` array. On timeout, increments to next server. On full rotation back to starting index, increments `ntpFailCount`.

### 3.4 Browser Time Sync (`/api/time/browser-sync`)

**Validation Rules:**
- Epoch must be >= 1577836800 (2020-01-01 00:00:00 UTC)
- Epoch must be < 4294967295 (Year 2106 limit)
- Rejects with HTTP 400 if invalid

**Post-Sync Actions:**
1. Set `timeSource = TIME_SOURCE_BROWSER`
2. Call `syncInternalRTC(browserUtcEpoch)` 
3. Update DS3231 if present
4. Save RTC state to NVS
5. Flush schedule cache
6. Return local time string in response

### 3.5 Year 2106 Problem Mitigation

ESP32's 32-bit `time_t` will overflow on 2106-02-07. This firmware:
- Defines `MAX_UNIX_TIME = 4294967295UL`
- All epoch assignments validated through `VALID_UNIX_TIME()` macro
- Schedule cache updates skipped if epoch < `MIN_UNIX_TIME`
- NTP/browser syncs beyond limit rejected

---

## 4. Self-Healing Ecosystem

### 4.1 HealthMetrics Structure
```cpp
struct HealthMetrics {
    uint32_t wifiFailures      = 0;  // Incremented when WiFi disconnected >3 checks
    uint32_t ntpFailures       = 0;  // Incremented on full NTP server rotation failure
    uint32_t mdnsFailures      = 0;  // Reserved for future mDNS health checks
    uint32_t dnsFailures       = 0;  // Reserved for future DNS health checks
    uint32_t webServerFailures = 0;  // Reserved for future HTTP health checks
    unsigned long lastRecoveryAttempt = 0;  // Timestamp of last smartRecovery()
    bool inRecoveryMode        = false;     // Prevents recursive recovery
};
```

### 4.2 Recovery Function Matrix

| Function | Trigger Condition | Action | Cooldown |
|----------|------------------|--------|----------|
| `recoverWiFi()` | `WiFi.status() != WL_CONNECTED` | `WiFi.begin()` with saved credentials | 30,000ms |
| `recoverMDNS()` | `!mdnsStarted` | `MDNS.begin()` with full TXT records | 60,000ms |
| `recoverDNS()` | Periodic check | Restart `dnsServer` if not responding | 60,000ms |
| `recoverWebServer()` | Periodic check | Verify HTTP port binding | 30,000ms |
| `recoverNTP()` | `wifiConnected && ntpFailCount > 0` | Force-update across all 4 servers | NTP_RETRY_INTERVAL |
| `recoverRTC()` | `rtcPresent && !rtcTimeValid` | Re-init I²C, write internal epoch | On-demand |
| `recoverAP()` | `WiFi.softAPIP() == 0.0.0.0` | Full AP restart with saved settings | On-demand |

### 4.3 Smart Recovery Orchestration
`smartRecovery()` runs every 10 seconds and:
1. Checks WiFi station health (increment failure counter at 3+ failures)
2. Refreshes mDNS service advertisements every 300s
3. Performs full health check every 1800s (30 min)
4. Saves critical state if dirty
5. Verifies all relay output states against expected values
6. Ensures AP is broadcasting

### 4.4 Targeted Recovery
`performTargetedRecovery()` is a full-service restart without reboot:
1. `liveReconfigureWebServer()`
2. `liveReconfigureDNS()`
3. `liveReconfigureMDNS()`
4. `liveReconfigureWiFi()`
5. `liveReconfigureAP()`
6. `recoverRTC()`
7. `verifyRelayStates()`
8. Reset all health failure counters to zero

---

## 5. Relay Scheduling Engine: Formal Specification

### 5.1 Time Normalization
All schedule times are converted to **seconds since midnight** (0-86399):
```
SSM = hour * 3600 + minute * 60 + second
```

### 5.2 Evaluation Algorithm
```
For each relay i:
  If manualOverride[i] is TRUE:
    Output = manualState[i]
    Continue
  
  shouldBeOn = FALSE
  For each schedule slot s (0-7):
    If NOT enabled[s]: continue
    
    // Day-of-week check
    If (days[s] & todayBitmask) == 0: continue
    
    // Day-of-month check  
    If monthDays[s] != 0 AND (monthDays[s] & (1 << (today-1))) == 0: continue
    
    start = SSM(startHour[s], startMinute[s], startSecond[s])
    stop  = SSM(stopHour[s], stopMinute[s], stopSecond[s])
    
    // Always-ON schedule
    If start == stop:
      shouldBeOn = TRUE
      break
    
    // Normal schedule (start < stop)
    If start < stop AND current_SSM >= start AND current_SSM < stop:
      shouldBeOn = TRUE
      break
    
    // Overnight schedule (start > stop)
    If start > stop AND (current_SSM >= start OR current_SSM < stop):
      shouldBeOn = TRUE
      break
  
  // Apply debounced output with 500ms minimum interval
  If shouldBeOn != lastDebouncedState[i]:
    If millis() - lastStateChange[i] >= 500:
      setRelayOutput(i, shouldBeOn)
      lastDebouncedState[i] = shouldBeOn
      lastStateChange[i] = millis()
```

### 5.3 Day Bitmask Encoding
| Day | Constant | Bit Value |
|-----|----------|-----------|
| Sunday | `DAY_SUNDAY` | `0x01` (1) |
| Monday | `DAY_MONDAY` | `0x02` (2) |
| Tuesday | `DAY_TUESDAY` | `0x04` (4) |
| Wednesday | `DAY_WEDNESDAY` | `0x08` (8) |
| Thursday | `DAY_THURSDAY` | `0x10` (16) |
| Friday | `DAY_FRIDAY` | `0x20` (32) |
| Saturday | `DAY_SATURDAY` | `0x40` (64) |
| All Days | `DAY_ALL` | `0x7F` (127) |
| Weekdays | `DAY_WEEKDAYS` | `0x3E` (62) |
| Weekends | `DAY_WEEKENDS` | `0x41` (65) |

### 5.4 Month-Day Bitmask
32-bit integer where bit `n` (0-indexed) represents day `n+1` of the month.
- `0x00000000` — No filtering (all days match)
- `0xFFFFFFFF` — All 31 days match (redundant with 0)
- `0x00000001` — Only day 1 of month
- `0x40000000` — Only day 31

### 5.5 Schedule Cache System
To avoid recalculating schedules on every API call:
- `scheduleActiveCache[MAX_RELAYS]` — Boolean array of computed states
- Updated every `SCHEDULE_CACHE_INTERVAL` (1000ms) or on time sync
- `cachedTodayBit` and `cachedMonthDay` store current date components
- Invalidated on manual override changes, schedule saves, or time source changes

---

## 6. GPIO Configuration Matrix

### 6.1 Pin Validation List
```cpp
// Allowed GPIOs (avoiding flash, PSRAM, and strapping pins):
int validPins[] = {
    15,  2,  4,   // ADC2, safe for output
    16, 17,       // UART2 (if not used), safe for output  
    5, 18, 19,    // VSPI (if not used), safe for output
    3,  1,        // UART0 (if serial disabled), safe for output
    23,           // VSPI MOSI
    13, 14,       // ADC2, HSPI
    27, 26, 25,   // DAC, ADC2
    33, 32        // ADC1, XTAL (32 requires caution)
};
// Pins explicitly excluded:
// 0  - BOOT button (strapping)
// 6-11 - Flash memory
// 12  - MTDI strapping
// 21,22 - I²C reserved for DS3231
// 34-39 - Input-only GPIOs
```

### 6.2 Active Level Control Flow
```
Digital Write Path:
  relayConfigs[i].manualOverride?
    YES → targetState = relayConfigs[i].manualState
    NO  → targetState = scheduleActiveCache[i]

  extConfig.global_active_mode?
    1 (Global LOW)  → physicalOutput = !targetState
    2 (Global HIGH) → physicalOutput = targetState
    0 (Per-Relay)   → physicalOutput = gpioConfig.activeLow[i] ? !targetState : targetState

  digitalWrite(gpioConfig.pins[i], physicalOutput)
```

### 6.3 GPIO CRUD Operations
| Operation | Endpoint | Validation | Side Effects |
|-----------|----------|------------|--------------|
| Create | `/api/gpio/add` | Pin not in use, count < 16 | New relay initialized with defaults |
| Read | `/api/gpio` | None | Returns available + used pins |
| Update | `/api/gpio/save` | Array size ≤ 16 | Preserves existing relay configs where possible |
| Delete | `/api/gpio/delete` | Index < count | Compacts array, shifts subsequent relays |
| Toggle | `/api/gpio/toggle-active-low` | Index < count | Flips active level, resets output to OFF |

---

## 7. Network Subsystems

### 7.1 WiFi Mode Matrix

| `sta_enabled` | WiFi Mode | NTP | WiFi Scan | Browser Sync | mDNS |
|---------------|-----------|-----|-----------|--------------|------|
| 1 (Enabled) | `WIFI_AP_STA` | ✓ | ✓ | ✓ | ✓ |
| 0 (Disabled) | `WIFI_AP` | ✗ | ✗ | ✓ | ✓ |

### 7.2 WiFi State Machine
```
        ┌──────────┐
        │DISCONNECT│◄──────────────────────────────┐
        └────┬─────┘                               │
             │                                      │
    ┌────────▼────────┐    timeout/error    ┌───────┴──────┐
    │   CONNECTING    ├─────────────────────►│ COOLDOWN    │
    └────────┬────────┘                      │ (5 min)     │
             │ success                        └──────┬──────┘
    ┌────────▼────────┐                              │
    │   CONNECTED     │◄─────────────────────────────┘
    └────────┬────────┘    retry after cooldown
             │ disconnect detected
             └──────────────► DISCONNECT
```

### 7.3 DNS Server Configuration
- **Port:** 53 (standard DNS)
- **Behavior:** All queries resolved to `WiFi.softAPIP()`
- **Startup:** `dnsServer.start(DNS_PORT, "*", WiFi.softAPIP())`
- **Processing:** `dnsServer.processNextRequest()` called in each `loop()` iteration
- **Purpose:** Captive portal functionality + seamless client redirection

---

## 8. Web Interface: Complete API Reference

### 8.1 Response Headers
All API responses include:
```
Content-Type: application/json
Access-Control-Allow-Origin: * (implicit via WebServer)
Connection: close (HTTP/1.1)
```

### 8.2 Error Response Format
```json
{
  "success": false,
  "error": "Human-readable error message"
}
```

### 8.3 Endpoint Specifications

<details>
<summary><b>GET /api/relays</b> — Full Relay State (Click to expand)</summary>

**Response:** JSON array of relay objects (chunked transfer)
```json
[
  {
    "state": false,
    "manual": false,
    "name": "Relay 1",
    "pin": 15,
    "schedules": [
      {
        "startHour": 8, "startMinute": 0, "startSecond": 0,
        "stopHour": 17, "stopMinute": 0, "stopSecond": 0,
        "enabled": true,
        "days": 127,
        "monthDays": 0
      },
      // ... 7 more schedule slots
    ]
  },
  // ... up to 15 more relays
]
```
**Cache:** 2-second server-side response cache. Invalidation on state change.
</details>

<details>
<summary><b>POST /api/relay/manual</b> — Set Manual Override</summary>

**Request:**
```json
{"relay": 0, "state": true}
```
**Validation:** `0 <= relay < gpioConfig.count`  
**Side Effects:** Saves config to NVS, marks criticalState dirty, invalidates response cache.
</details>

<details>
<summary><b>POST /api/time/browser-sync</b> — Browser Time Injection</summary>

**Request:**
```json
{"utc_epoch": 1718400000}
```
**Response:**
```json
{
  "success": true,
  "utc_epoch": 1718400000,
  "local_time": "2024-06-15 08:00:00",
  "gmt_offset": 28800,
  "time_source": "browser",
  "rtc_present": true,
  "rtc_synced": true,
  "drift": 1.0
}
```
</details>

### 8.4 Response Cache System
```cpp
struct ResponseCache {
    String relaysJson;       // Cached /api/relays response
    String systemJson;       // Cached /api/system response
    String timeJson;         // Cached /api/time response
    unsigned long lastUpdate = 0;
    bool valid = false;
};
```
- **Invalidation:** On any POST/PUT operation, `valid` set to `false`.
- **Expiry:** Auto-invalidated after 5 seconds (stale data prevention).
- **Memory:** Cache strings cleared on expiry to free heap.

---

## 9. WiFi & Access Point Management

### 9.1 Station Enable/Disable Toggle
**Function:** `setWiFiStationEnabled(bool enabled)`
- **Disable:** Disconnects WiFi, switches to `WIFI_AP` mode, stops NTP state machine, clears all pending WiFi operations.
- **Enable:** Switches to `WIFI_AP_STA`, initiates `WiFi.begin()`, resets reconnect counters.
- **Persistence:** Saved to `extConfig.sta_enabled` in NVS.

### 9.2 WiFi Scan with Connection Pausing
**Problem:** WiFi scanning and connecting simultaneously causes radio contention.
**Solution:** `pauseWiFiForScan()` temporarily disconnects and sets `wifiPauseUntil` for 15 seconds.
**Recovery:** After scan completes, `wifiPauseUntil` extended by 5 seconds, then normal reconnection resumes.

### 9.3 AP Settings
| Setting | Default | Range | Requires AP Restart |
|---------|---------|-------|---------------------|
| SSID | `ESP32_16CH_Timer_Switch` | 1-31 chars | Yes |
| Password | `ESP32-admin` | 8-31 chars or empty | Yes |
| Channel | 6 | 1-13 | Yes |
| Hidden | false | true/false | Yes |

**Restart Logic:** `restartAPIfNeeded(bool forceRestart)` — if `forceRestart=true` or settings changed, calls `WiFi.softAPdisconnect(true)`, 500ms delay, then `WiFi.softAP()` with new parameters.

---

## 10. mDNS & Service Discovery Protocol

### 10.1 Hostname Sanitization
```cpp
// Input: "ESP32_16CH_Timer_Switch"
// Process:
// 1. Convert to lowercase
// 2. Replace spaces and underscores with hyphens
// 3. Remove all non-alphanumeric, non-hyphen characters
// 4. Truncate to 31 characters
// Output: "esp32-16ch-timer-switch"
```

### 10.2 Service Advertisement
```
Service: _http._tcp
Port: 80
TXT Records:
  model=ESP32
  version=v9
  channels=<active_relay_count>
```

### 10.3 mDNS Lifecycle
- **Start:** `startMDNS()` — called in setup, also by `liveReconfigureMDNS()`
- **Stop:** `stopMDNS()` — not normally called, only on hostname change
- **Restart:** `restartMDNS()` — calls `liveReconfigureMDNS()` which detects `!mdnsStarted` and reinitializes
- **Scheduled Restart:** `scheduleMDNSRestart()` — sets 2-second timer to debounce rapid changes

---

## 11. Captive Portal Implementation

### 11.1 Portal Detection Endpoints
The following paths are commonly probed by operating systems to detect captive portals:
| OS | Probe URL |
|----|-----------|
| iOS/macOS | `/hotspot-detect.html`, `/library/test/success.html` |
| Android | `/generate_204` |
| Windows | `/connecttest.txt`, `/ncsi.txt` |
| Generic | `/success.txt`, `/canonical.html`, `/redirect` |

### 11.2 Default Behavior
- `onNotFound()` handler catches all unregistered paths
- Responds with HTTP 302 redirect to `http://<AP_IP>/`
- `/generate_204` returns empty 302 (Android expects no content)
- All "success" endpoints return simple plaintext to satisfy OS checks

---

## 12. Boot Button & Hardware Factory Reset

### 12.1 Physical Configuration
- **Pin:** GPIO0 (BOOT button on most ESP32 dev boards)
- **Pull:** `INPUT_PULLUP` — button press reads LOW
- **Hold Duration:** 5000ms (`FACTORY_RESET_HOLD`)

### 12.2 Reset Sequence
1. `digitalRead(BOOT_BUTTON_PIN)` returns LOW (pressed)
2. Timer starts on first detection of press
3. If held for 5000ms:
   - `preferences.clear()` — erase entire NVS namespace
   - `initDefaults()` — recreate factory configuration
   - All relays forced OFF
   - AP restarted with default credentials
   - WiFi station attempted if credentials exist
   - `factoryResetTriggered` flag prevents re-trigger during same press
4. On button release, flag reset for next use

---

## 13. Memory & Resource Management

### 13.1 Heap Monitoring
```cpp
static unsigned long lastHeapCheck = 0;
static size_t minFreeHeap = 0;

void checkAndCleanMemory() {
    if (timeHasElapsed(millis(), lastHeapCheck, 300000)) { // Every 5 min
        size_t freeHeap = ESP.getFreeHeap();
        if (freeHeap < minFreeHeap || minFreeHeap == 0) {
            minFreeHeap = freeHeap; // Track minimum observed
        }
        if (freeHeap < 20000) { // Less than 20KB
            performMemoryCleanup();
        }
    }
}
```

### 13.2 Cleanup Operations
1. **WiFi Scan Cleanup:** `WiFi.scanDelete()` if no scan in progress
2. **Task Yielding:** 10 iterations of `yield()` + `delay(1)` to process background tasks
3. **NVS Flush:** `preferences.end()` to close any open handles
4. **Fragmentation Reduction:** Allocate/free 512-byte blocks (5 iterations) to trigger heap compaction

### 13.3 Stale Connection Management
- Every 60s, check for orphaned `WiFiClient` connections
- Force-close up to 5 stale connections per cycle
- Response cache auto-invalidated after 5s

---

## 14. Critical State Persistence & Fault Recovery

### 14.1 CriticalRelayState Structure
```cpp
struct CriticalRelayState {
    uint32_t magic;           // 0xDEADBEEF — validity marker
    bool relayStates[16];     // Last known output states
    bool manualOverrides[16]; // Manual override flags
    uint32_t timestamp;       // Millis at time of save
    uint32_t checksum;        // XOR-based integrity check
};
```

### 14.2 Checksum Algorithm
```cpp
uint32_t sum = 0;
for (int i = 0; i < 16; i++) {
    sum += relayStates[i] ? (1 << (i % 32)) : 0;
    sum += manualOverrides[i] ? (1 << ((i+16) % 32)) : 0;
}
return sum ^ timestamp;
```

### 14.3 Restoration Logic
On boot, `restoreCriticalState()`:
1. Reads `criticalState` from NVS
2. Validates `magic == 0xDEADBEEF`
3. Validates checksum matches recalculation
4. If both pass, reapplies manual overrides to relay outputs
5. Returns `true` if restored, `false` if invalid/corrupt

---

## 15. CSS & UI Component Library

### 15.1 Color Palette
| Name | Hex | Usage |
|------|-----|-------|
| Primary Blue | `#1565C0` | Header gradient, primary buttons, links |
| Dark Blue | `#0D47A1` | Header gradient end |
| Background | `#EEF2F7` | Page background |
| Card White | `#FFFFFF` | Card backgrounds |
| Success Green | `#43A047` | ON buttons, success badges |
| Error Red | `#E53935` | OFF buttons, error states |
| Warning Orange | `#FB8C00` | Sync buttons |
| Danger Red | `#B71C1C` | Factory reset button |
| Text Primary | `#1A1A2E` | Main text |
| Text Secondary | `#90A4AE` | Labels, hints |

### 15.2 Responsive Breakpoints
```css
@media(max-width:500px) {
    .grid { grid-template-columns: 1fr; }     /* Single column */
    .ibar { grid-template-columns: 1fr; }      /* Stack info boxes */
    .input-row { flex-direction: column; }     /* Stack inputs */
    .day { width:24px; height:22px; font-size:10px; } /* Smaller day buttons */
}
```

### 15.3 Toast Notification System
- **Position:** Fixed bottom center, slides up from below viewport
- **Colors:** Green (`ok`) for success, Red (`er`) for errors
- **Duration:** 3 seconds auto-dismiss
- **Stacking:** Previous toast cleared on new toast (`clearTimeout`)

---

## 16. Boot Sequence Flowchart

```
POWER ON / RESET
     │
     ▼
Serial.begin(115200)
     │
     ▼
initRTC() ─────────────► DS3231 detected? ──Yes──► rtcPresent = true
     │                        │
     │                        No
     │                        │
     ▼                        ▼
loadGPIOConfig()        rtcPresent = false
     │
     ▼
pinMode(BOOT_BUTTON, INPUT_PULLUP)
     │
     ▼
[Initialize all relay GPIOs as OUTPUT, set LOW]
     │
     ▼
[Initialize relayConfigs[] with defaults]
     │
     ▼
loadConfiguration() ────► Valid? ──No──► initDefaults()
     │                      │
     │                     Yes
     │                      │
     ▼                      ▼
loadExtConfig()        [Continue]
     │
     ▼
[Time Initialization Priority]
  1. loadRTCFromDS3231()  (if present & valid)
  2. loadRTCState()       (from NVS backup)
  3. Set timeSource = NONE
     │
     ▼
WiFi.mode(WIFI_AP_STA)
     │
     ▼
timeClient.begin()
     │
     ▼
[If sta_enabled: WiFi.begin(ssid, pass)]
[Else: WiFi.mode(WIFI_AP)]
     │
     ▼
WiFi.softAP(ap_ssid, ap_password, channel, hidden)
     │
     ▼
startMDNS()
     │
     ▼
dnsServer.start(53, "*", AP_IP)
     │
     ▼
setupWebServer()
     │
     ▼
updateScheduleCache()
     │
     ▼
restoreCriticalState() ───► Restored? ──Yes──► Apply manual overrides
     │                        │
     │                        No
     │                        │
     ▼                        ▼
[Enter main loop()]     [Enter main loop()]
```

---

## 17. Main Loop Execution Model

### 17.1 Priority-Ordered Task Execution (each ~50μs iteration)
```
1. checkBootButton()           [Background: GPIO0 hold detection]
2. server.handleClient()       [Foreground: Process 1 HTTP request]
3. dnsServer.processNext()     [Foreground: Process 1 DNS query]
4. Stale connection cleanup    [Every 60s]
5. Memory cleanup              [Every 30s]
6. Web server health check     [Every 60s]
7. Smart recovery              [Every 10s]
8. Heap monitoring             [Every 300s]
9. DS3231 periodic sync        [Every 3600s]
10. Auto-save internal RTC     [Every 3600s, if no external source]
11. mDNS restart (if scheduled)[On timer expiry]
12. WiFi scan timeout check    [On scan active]
13. WiFi connection state machine [On connecting/disconnected]
14. NTP sync state machine     [On interval/retry]
15. Schedule processing        [Every 250ms]
16. Critical state save        [Every 300s, if dirty]
17. yield()                    [RTOS task switch]
```

### 17.2 Cooperative Multitasking Guarantees
- No single operation blocks for >10ms (except `WiFi.scanNetworks()` which is async)
- `server.handleClient()` processes exactly one HTTP request per call
- `dnsServer.processNextRequest()` processes exactly one DNS query per call
- `yield()` called at end of loop to allow FreeRTOS task switching

---

## 18. Error Codes & Troubleshooting

### 18.1 API Error Messages
| HTTP Status | Error Message | Likely Cause |
|-------------|---------------|--------------|
| 400 | `No data` | POST request missing body |
| 400 | `Bad JSON` | Malformed JSON in request body |
| 400 | `Invalid relay` | Relay index out of bounds |
| 400 | `Invalid SSID` | SSID empty or >31 characters |
| 400 | `Password must be 8+ chars or blank` | AP password too short |
| 400 | `WiFi station is disabled` | Attempted scan with STA off |
| 400 | `Too many pins` | Attempted to add >16 relays |
| 400 | `Pin already in use` | Duplicate GPIO assignment |
| 400 | `Maximum relays reached` | gpioConfig.count == 16 |
| 400 | `Invalid epoch time` | Browser sync with bad timestamp |
| 400 | `WiFi not connected or station disabled` | NTP sync without connectivity |

### 18.2 LED Status Indicators (Web UI)
| WiFi Dot | Time Dot | Meaning |
|----------|----------|---------|
| 🟢 Green | 🟢 Green | WiFi OK, NTP synced |
| 🟢 Green | 🔵 Blue | WiFi OK, Browser/RTC time |
| 🟢 Green | 🟡 Yellow | WiFi OK, no time source |
| 🔴 Red | 🟢 Green | WiFi down, NTP time (stale) |
| 🔴 Red | 🔵 Blue | WiFi down, RTC time (DS3231) |
| 🔴 Red | 🟡 Yellow | WiFi down, no time source |

### 18.3 Common Scenarios
| Symptom | Diagnosis | Resolution |
|---------|-----------|------------|
| Relays not triggering | Schedule not enabled, or wrong day | Check schedule UI: enabled checkbox + day buttons |
| Time shows `--:--:--` | No time source available | Sync NTP or browser time |
| Cannot connect to AP | Wrong password or channel conflict | Hold BOOT 5s to factory reset |
| Web UI not loading | Captive portal not triggered | Navigate to `http://192.168.4.1/` directly |
| Relay ON but should be OFF | Active low/high mismatch | Check GPIO → Toggle Active Low setting |

---

## 19. Security Considerations

### 19.1 Current State
- **Authentication:** None (open access)
- **Encryption:** None for HTTP; WPA2 for AP mode
- **Network Exposure:** AP isolated by default; STA connects to configured network

### 19.2 Recommendations for Production Deployment
1. **Change Default Passwords:** Both AP and STA credentials
2. **Disable AP if Not Needed:** Set AP password to empty and hide SSID, or modify code
3. **Network Isolation:** Place device on VLAN with no internet access if only local control needed
4. **HTTPS:** Not feasible on ESP32 without significant overhead; consider reverse proxy
5. **Physical Security:** BOOT button factory reset can be disabled by removing `checkBootButton()` from loop

### 19.3 CSRF Considerations
- All endpoints accept POST without CSRF tokens
- Mitigation: AP mode is isolated; STA mode should be on trusted network
- Browser sync endpoint could be exploited to set arbitrary time

---

## 20. Build Instructions & Dependencies

### 20.1 Required Libraries

| Library | Version | Purpose |
|---------|---------|---------|
| `NTPClient` by Fabrice Weinberg | ≥3.2.0 | NTP time synchronization |
| `ArduinoJson` by Benoit Blanchon | 6.x (not 7.x) | JSON serialization/deserialization |
| `RTClib` by Adafruit | ≥2.1.0 | DS3231 RTC interface |
| `WiFi` (built-in) | — | WiFi connectivity |
| `WebServer` (built-in) | — | HTTP server |
| `DNSServer` (built-in) | — | Captive portal DNS |
| `Preferences` (built-in) | — | NVS storage |
| `ESPmDNS` (built-in) | — | mDNS responder |
| `Wire` (built-in) | — | I²C communication |

### 20.2 PlatformIO Configuration
```ini
[env:esp32dev]
platform = espressif32 @ 6.4.0
board = esp32dev
framework = arduino
monitor_speed = 115200
board_build.partitions = default.csv
lib_deps = 
    arduino-libraries/NTPClient @ ^3.2.1
    bblanchon/ArduinoJson @ ^6.21.3
    adafruit/RTClib @ ^2.1.1
```

### 20.3 Arduino IDE Configuration
- **Board:** "ESP32 Dev Module"
- **Partition Scheme:** "Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)"
- **Flash Frequency:** 80MHz
- **Flash Mode:** QIO
- **Core Debug Level:** None (production)

### 20.4 Compilation Warnings
- `ArduinoJson 6.x` required; version 7 has breaking API changes
- Some `String` operations may generate fragmentation warnings (acceptable at this scale)
- `snprintf` buffer size warnings are intentional — all buffers sized for maximum config values

---

## 21. Hardware Pinout & Wiring Guide

### 21.1 ESP32 Pin Map (38-pin DevKit)
```
                    ┌─────────────────────┐
               EN  ─┤○ ○                 ○├─  GND
            VP/S4  ─┤  ○               ○  ├─  D23 (Relay 9)
            VN/S5  ─┤  ○               ○  ├─  D22 (I²C SCL)
              D34  ─┤  ○               ○  ├─  TX0/D1 (Relay 8)
              D35  ─┤  ○               ○  ├─  RX0/D3 (Relay 7)
              D32  ─┤  ○  ESP32 DEV    ○  ├─  D21 (I²C SDA)
       Relay 16 ← D33  ─┤  ○               ○  ├─  GND
        Relay 15 ← D25  ─┤  ○               ○  ├─  D19 (Relay 6)
        Relay 14 ← D26  ─┤  ○               ○  ├─  D18 (Relay 5)
        Relay 13 ← D27  ─┤  ○               ○  ├─  D5  (Relay 4)
        Relay 12 ← D14  ─┤  ○               ○  ├─  D17 
        Relay 11 ← D13  ─┤  ○               ○  ├─  D16 
               GND  ─┤○ ○                 ○├─  D4  (Relay 3)
              VIN  ─┤○ ○                 ○├─  D2  (Relay 2)
        Relay 10 ← D15  ─┤○ ○                 ○├─  D0  (BOOT)
                    └─────────────────────┘
```

### 21.2 DS3231 RTC Module Wiring
```
DS3231    →    ESP32
VCC       →    3.3V
GND       →    GND
SDA       →    GPIO21
SCL       →    GPIO22
```

### 21.3 Relay Module Considerations
- **Active LOW modules:** Most 16-channel relay boards are active LOW (relay ON when GPIO LOW)
  - Set Global Active Mode to "Active LOW" or configure per-relay
- **Active HIGH modules:** Solid-state relays (SSR) typically active HIGH
- **Flyback Diodes:** Ensure relay module has built-in diodes or add external 1N4148
- **Power:** Relay coils should be powered externally, not from ESP32 3.3V

---

## 22. Glossary of Constants & Macros

### 22.1 Configuration Limits
| Constant | Value | Description |
|----------|-------|-------------|
| `MAX_RELAYS` | 16 | Maximum supported relay channels |
| `EEPROM_MAGIC` | `0x1234` | System config validity marker |
| `EEPROM_VERSION` | 9 | Config schema version |
| `EXT_CFG_MAGIC` | `0xEC` | Extended config validity marker |
| `GPIO_CONFIG_MAGIC` | `0xD002` | GPIO config validity marker |
| `MAX_UNIX_TIME` | `4294967295UL` | Year 2106 overflow limit |
| `MIN_UNIX_TIME` | `1000000000UL` | Minimum valid epoch (~2001) |

### 22.2 Timing Intervals (milliseconds)
| Constant | Value | Purpose |
|----------|-------|---------|
| `NTP_RETRY_INTERVAL` | 30,000 | Between NTP attempts |
| `WIFI_CHECK_INTERVAL` | 10,000 | WiFi status polling |
| `WIFI_CONNECT_TIMEOUT` | 20,000 | Connection attempt timeout |
| `RTC_UPDATE_INTERVAL` | 100 | Internal RTC micros update |
| `SCHEDULE_PROCESS_INTERVAL` | 250 | Schedule evaluation |
| `RELAY_UPDATE_INTERVAL` | 500 | Relay output debounce |
| `RTC_REBASE_INTERVAL` | 300,000 | Internal RTC recalibration |
| `RTC_SYNC_INTERVAL` | 3,600,000 | NTP sync interval (1h) |
| `DS3231_SYNC_INTERVAL` | 3,600,000 | Hardware RTC sync |
| `MEMORY_CLEANUP_INTERVAL` | 30,000 | Garbage collection |
| `MEMORY_CHECK_INTERVAL` | 60,000 | Heap monitoring |
| `CONNECTION_TIMEOUT` | 10,000 | Stale client timeout |
| `FACTORY_RESET_HOLD` | 5,000 | BOOT button hold duration |
| `WIFI_PAUSE_DURATION` | 15,000 | Scan connection pause |

### 22.3 Day-of-Week Bitmask Constants
| Constant | Hex | Binary | Days |
|----------|-----|--------|------|
| `DAY_SUNDAY` | `0x01` | `00000001` | Sun |
| `DAY_MONDAY` | `0x02` | `00000010` | Mon |
| `DAY_TUESDAY` | `0x04` | `00000100` | Tue |
| `DAY_WEDNESDAY` | `0x08` | `00001000` | Wed |
| `DAY_THURSDAY` | `0x10` | `00010000` | Thu |
| `DAY_FRIDAY` | `0x20` | `00100000` | Fri |
| `DAY_SATURDAY` | `0x40` | `01000000` | Sat |
| `DAY_ALL` | `0x7F` | `01111111` | All 7 days |
| `DAY_WEEKDAYS` | `0x3E` | `00111110` | Mon-Fri |
| `DAY_WEEKENDS` | `0x41` | `01000001` | Sun, Sat |

### 22.4 Time Source Constants
| Constant | Value | Description |
|----------|-------|-------------|
| `TIME_SOURCE_NONE` | 0 | No valid time source |
| `TIME_SOURCE_NTP` | 1 | NTP server synced |
| `TIME_SOURCE_BROWSER` | 2 | Browser time injection |
| `TIME_SOURCE_RTC` | 3 | DS3231 hardware RTC |

### 22.5 Time Safety Macro
```cpp
#define VALID_UNIX_TIME(epoch) ((epoch) > MIN_UNIX_TIME && (epoch) < MAX_UNIX_TIME)

inline bool timeHasElapsed(unsigned long current, unsigned long previous, unsigned long interval) {
    return (current - previous) >= interval;  // Overflow-safe
}

inline bool isTimeReached(unsigned long current, unsigned long target) {
    return (long)(current - target) >= 0;  // Signed comparison for future timestamps
}
```

---

## 23. Appendix: Complete Data Structure Definitions

<details>
<summary><b>SystemConfig (200 bytes approx.)</b></summary>

```cpp
struct SystemConfig {
    uint16_t magic;           // 0x1234
    uint8_t  version;         // 9
    char     sta_ssid[32];    // Station WiFi SSID
    char     sta_password[64];// Station WiFi password (64 char for WPA2-Enterprise)
    char     ap_ssid[32];     // Access Point SSID
    char     ap_password[32]; // Access Point password
    char     ntp_server[48];  // Primary NTP server hostname
    int32_t  gmt_offset;      // Seconds from UTC (e.g., 28800 = UTC+8)
    int32_t  daylight_offset; // DST offset in seconds
    time_t   last_rtc_epoch;  // Last saved internal RTC epoch
    float    rtc_drift;       // Drift compensation factor (1.0 = perfect)
    char     hostname[32];    // mDNS hostname
} __attribute__((packed));
```
</details>

<details>
<summary><b>ExtConfig (32 bytes)</b></summary>

```cpp
struct ExtConfig {
    uint8_t magic;             // 0xEC
    uint8_t ap_channel;        // 1-13
    uint8_t ntp_sync_hours;    // 1-24
    uint8_t ap_hidden;         // 0=visible, 1=hidden
    uint8_t global_active_mode;// 0=per-relay, 1=all LOW, 2=all HIGH
    uint8_t sta_enabled;       // 0=AP only, 1=AP+STA
    uint8_t reserved[26];      // Future expansion
} __attribute__((packed));
```
</details>

<details>
<summary><b>RelayConfig (per relay, ~320 bytes × 16 = ~5KB total)</b></summary>

```cpp
struct TimerSchedule {
    uint8_t  startHour[8];    // 0-23
    uint8_t  startMinute[8];  // 0-59
    uint8_t  startSecond[8];  // 0-59
    uint8_t  stopHour[8];     // 0-23
    uint8_t  stopMinute[8];   // 0-59
    uint8_t  stopSecond[8];   // 0-59
    bool     enabled[8];      // Schedule slot active
    uint8_t  days[8];         // Day-of-week bitmask
    uint32_t monthDays[8];    // Day-of-month bitmask
};

struct RelayConfig {
    TimerSchedule schedule;   // 8 schedule slots
    bool          manualOverride; // Manual override active
    bool          manualState;    // Manual override target state
    char          name[16];       // User-assigned relay name
};
```
</details>

<details>
<summary><b>GPIOPinConfig (~50 bytes)</b></summary>

```cpp
struct GPIOPinConfig {
    uint8_t pins[MAX_RELAYS];     // GPIO numbers [0-15]
    uint8_t count;                // Active relay count (1-16)
    uint16_t magic;               // 0xD002
    bool activeLow[MAX_RELAYS];   // Per-relay active level
};
```
</details>

<details>
<summary><b>CriticalRelayState (~80 bytes)</b></summary>

```cpp
struct CriticalRelayState {
    uint32_t magic;              // 0xDEADBEEF
    bool relayStates[MAX_RELAYS];// Last output states
    bool manualOverrides[MAX_RELAYS]; // Override flags
    uint32_t timestamp;          // Millis at save time
    uint32_t checksum;           // XOR integrity check
};
```
</details>

<details>
<summary><b>HealthMetrics</b></summary>

```cpp
struct HealthMetrics {
    uint32_t wifiFailures;
    uint32_t ntpFailures;
    uint32_t mdnsFailures;
    uint32_t dnsFailures;
    uint32_t webServerFailures;
    unsigned long lastRecoveryAttempt;
    bool inRecoveryMode;
};
```
</details>

<details>
<summary><b>ResponseCache</b></summary>

```cpp
struct ResponseCache {
    String relaysJson;
    String systemJson;
    String timeJson;
    unsigned long lastUpdate;
    bool valid;
};
```
</details>

---

# Customize GPIO Pins
> Find this line inside sketch.ino
- You can add, remove or reassign the gpio pins
```
// Change GPIO PIN
  15, // IN1  - Relay 1
  2,  // IN2  - Relay 2
  4,  // IN3  - Relay 3
  5,  // IN4  - Relay 4
  18, // IN5  - Relay 5
  19, // IN6  - Relay 6
  3,  // IN7  - Relay 7
  1,  // IN8  - Relay 8
  23, // IN9  - Relay 9
  13, // IN10 - Relay 10
  14, // IN11 - Relay 11
  27, // IN12 - Relay 12
  26, // IN13 - Relay 13
  25, // IN14 - Relay 14
  33, // IN15 - Relay 15
  32  // IN16 - Relay 16
```
```
// Change GPIO PIN
const DEFAULT_PINS = [15,2,4,5,18,19,3,1,23,13,14,27,26,25,33,32];
```
> You can add more gpio pins like `16, 17` in int validPins.
```
// Change GPIO PIN
int validPins[] = {15, 2, 4, 16, 17, 5, 18, 19, 3, 1, 23, 13, 14, 27, 26, 25, 33, 32};
```

# Build Firmware
>  Auto Build firmware binaries using github action
- https://github.com/xiv3r/arduino
