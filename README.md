# ESP8266
👉 https://github.com/xiv3r/esp8266-automatic-timer-switch

# Requirements
- ESP32 38P Pins
- DS3231 RTC Module (recommended)
- 1-16 Channel 5V Relay
- Female to Female Dupont Wire
- Stable Wifi Connection for NTP/RTC sync
- 5v 3-5a Power supply
  
`Optional`
- 5v UPS (Maintain RTC Time without DS3231)

# Libraries
- ArduinoJson
- NTPClient
- RTClib 1.14.1

# Installation
- Download the [Firmware](https://github.com/xiv3r/esp32-automatic-timer-switch/releases/tag/esp32) and flash.

- Offset address
```
firmware : 0x0
```

# WiFi Key
- WiFi SSID: `ESP32_16CH_Timer_Switch`
- Password: `ESP32-admin`

# Activation
- Go to `wifi settings` and connect to your home wifi after the NTP is synchronized everything will work

# Relay Naming 
- Double click relay name to edit

# GMT offset
> ⚠️ Set to your country time
- Search your country `gmt offsets in seconds` and paste it in the Time -> GMT Offset

# Access
° Direct Access
- mDNS:`esp32-16ch-timer.local`
- Captive Portal: Auto redirect
- Gateway:`192.168.4.1`
- WAN:`192.168.1.123`
  
° Global:`Enable Port Forwarding on your router to access anywhere`

# Reset
- Hold BOOT button for 5 seconds

# Restart
- Press EN button

# 16 CHANNEL GPIO Connection 
```
RELAY  |  ESP32 38P
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
# DS3231 GPIO Connection 
```
DS3231 | ESP32 38P
VCC → 3.3V
SDA → 21
SCL → 22
GND → GND
```

<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/libraries/img1.png">
<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/libraries/img2.png">
<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/libraries/img3.png">
<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/libraries/img5.jpg">
<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/libraries/img8.jpg">

<details><summary>
  
# Full Features
</summary>

# ESP32 16‑Channel Relay Smart Switch – Full Feature Documentation

**Author:** Raff Alds  
**GitHub:** [xiv3r](https://www.github.com/xiv3r)  
**Project:** Home, Business, Farm Automation, etc.

---

## Table of Contents

1. [Overview](#overview)
2. [Hardware Requirements](#hardware-requirements)
3. [Key Features](#key-features)
4. [System Architecture](#system-architecture)
5. [Relay Management](#relay-management)
   - GPIO Configuration
   - Active Level (LOW/HIGH) & Global Mode
   - Manual Override & Naming
6. [Scheduling Engine](#scheduling-engine)
   - Schedule Slots (8 per relay)
   - Day of Week & Day of Month Masks
   - Overnight & Always‑ON Support
7. [Time Management](#time-management)
   - NTP Synchronisation
   - Browser Time Sync
   - DS3231 Hardware RTC
   - Drift Compensation
   - RTC Rebase
8. [WiFi & Networking](#wifi--networking)
   - Station (STA) Mode
   - Access Point (AP) Mode
   - Captive Portal & DNS
   - mDNS
   - WiFi Scanning
9. [Web Interface](#web-interface)
   - Pages Overview
   - Responsive UI
   - Real‑time Clock Display
   - Edit Relay Names (Double‑click)
10. [API Reference](#api-reference)
    - Relay Control
    - WiFi, NTP, AP, GPIO
    - System & Factory Reset
11. [Self‑Healing System](#self-healing-system)
    - Live Reconfiguration
    - Critical State Saving
    - Service Verification
12. [Memory Management](#memory-management)
    - Heap Monitoring
    - Stale Resource Cleanup
    - Connection Timeout
13. [Factory Reset](#factory-reset)
    - Boot Button (Hardware)
    - API Factory Reset
14. [Persistent Configuration (NVS)](#persistent-configuration-nvs)
15. [Health Metrics & Diagnostics](#health-metrics--diagnostics)
16. [Limitations & Notes](#limitations--notes)

---

## Overview

The **ESP32 16‑Channel Relay Smart Switch** is a full‑featured automation controller capable of driving up to 16 relays. It combines:

- **WiFi** (station + access point)
- **Precise scheduling** with day‑of‑week and day‑of‑month selectors
- **Manual override** for each relay
- **Time synchronisation** via NTP or browser (fallback to DS3231 RTC)
- **Self‑healing** mechanisms to recover WiFi, mDNS, DNS, and web server
- **Intuitive web interface** with no external dependencies

All settings (GPIO mapping, schedules, WiFi credentials, NTP parameters, relay names) are stored in non‑volatile storage (NVS / Preferences) and survive power cycles.

---

## Hardware Requirements

| Component            | Specification / Pinout                                |
| -------------------- | ----------------------------------------------------- |
| **Microcontroller**  | ESP32 (38‑pin variant recommended)                   |
| **Relays**           | Up to 16, any voltage/current rating (controlled by GPIO) |
| **GPIO Pins**        | Defaults: `15,2,4,5,18,19,3,1,23,13,14,27,26,25,33,32` |
| **RTC (optional)**   | DS3231 on I²C pins **GPIO21 (SDA)** – **GPIO22 (SCL)** |
| **Boot button**      | GPIO0 (pulled up, active LOW) – used for factory reset |
| **Power**            | 5V / USB (relay coil power may require external supply) |

> **Note:** The firmware can reassign GPIO pins at runtime. The default pins cover the most common ESP32 development boards.

---

## Key Features

- **16 independent relays** – each with its own name, schedule, and manual override.
- **8 schedule slots** per relay – allowing complex time patterns.
- **Day‑of‑week** (Sun–Sat) and **day‑of‑month** (1–31) selection per schedule.
- **Overnight schedules** (start > stop) and “Always ON” (start == stop) support.
- **Manual override** – any relay can be forced ON or OFF, independent of schedules.
- **Two time synchronisation sources**:
  - NTP (configurable server + 4 fallback servers)
  - Browser‑based sync (when NTP is unreachable)
- **DS3231 hardware RTC** – battery‑backed timekeeping across power failures.
- **Active level configuration** – relays can be active LOW or HIGH, with a **global override** mode.
- **WiFi station** with automatic reconnection and exponential backoff.
- **Access point** (AP) with configurable SSID, password, channel and hidden option.
- **Captive portal** – redirects all DNS requests to the device IP.
- **mDNS** – device accessible as `hostname.local` (default `esp32.local`).
- **Self‑healing** – continuously verifies and restores WiFi, web server, DNS, mDNS.
- **Critical state saving** – restores manual relay states after unexpected restart.
- **Web interface** with:
  - Real‑time clock display
  - Relay state and schedule editor
  - WiFi scanner
  - GPIO configuration page
  - System information dashboard
- **No external libraries** – uses only Arduino core, standard ESP32 libraries.
- **Factory reset** via boot button hold (5 seconds) or API.
- **Memory management** – periodic heap cleanup, stale connection pruning.

---

## System Architecture

The firmware is structured around several core modules:

1. **Time Engine** – maintains an internal monotonic clock (`internalEpoch`) that is periodically corrected by NTP, browser sync, or the DS3231 RTC. Drift compensation is applied.
2. **Schedule Cache** – evaluates all schedules every second (or on demand) and stores the desired relay state in `scheduleActiveCache[]`.
3. **Relay Update Loop** – applies either the cached schedule state or the manual override state to the GPIO pins, respecting active‑low/high configuration.
4. **Web Server** – serves the UI and REST API.
5. **Self‑Healing** – runs background checks and recovers subsystems.
6. **NVS Storage** – saves all settings (`sysConfig`, `extConfig`, `relayConfigs`, `gpioConfig`, `criticalState`).

The `loop()` processes:
- Web server and DNS requests
- Boot button detection
- WiFi connection state machine
- NTP sync (periodic or on failure)
- Schedule processing (every 250 ms)
- Relay output updates (every 500 ms)
- RTC rebase (every 5 minutes)
- Health checks and memory cleanup

---

## Relay Management

### GPIO Configuration

- Each relay is assigned a **GPIO pin**.
- The firmware supports dynamic pin assignment; pins can be added/removed via the web interface.
- Up to **16 relays**.
- After changing pins, schedules and names for existing relays are preserved (relays are “shifted”).
- The list of available GPIOs is limited to those safe for output (e.g. `15,2,4,5,18,19,3,1,23,13,14,27,26,25,33,32`).

### Active Level & Global Mode

- **Active LOW** – relay energises when the GPIO is pulled **LOW**.
- **Active HIGH** – relay energises when the GPIO is pulled **HIGH**.
- Each relay can have its own active level (stored in `gpioConfig.activeLow[]`).
- **Global Active Mode** (in `ExtConfig`) overrides per‑relay settings:
  - `0` – per‑relay configuration (individual).
  - `1` – **Global LOW** (all relays active LOW).
  - `2` – **Global HIGH** (all relays active HIGH).
- Changing the global mode takes effect immediately without restart.

### Manual Override & Naming

- Each relay has a **name** (max 15 characters) – editable from the web interface (double‑click the relay title).
- Manual override forces the relay to a specific ON/OFF state, bypassing schedules.
- The override persists across reboots (saved in NVS and critical state).
- To return to automatic scheduling, click the **Auto** button.

---

## Scheduling Engine

### Schedule Slots

- **8 schedule slots** per relay (`TimerSchedule` structure).
- Each slot contains:
  - Start time (hour, minute, second)
  - Stop time (hour, minute, second)
  - Enabled flag
  - Day‑of‑week mask (8‑bit)
  - Day‑of‑month mask (32‑bit)
- Slots are evaluated in order; if any slot matches the current time and its `enabled == true`, the relay is turned **ON**.

### Day Masks

- **Day‑of‑week** – bitmask from `DAY_SUNDAY` (bit 0) to `DAY_SATURDAY` (bit 6).  
  Predefined constants: `DAY_ALL`, `DAY_WEEKDAYS`, `DAY_WEEKENDS`.
- **Day‑of‑month** – 32‑bit mask where bit `n` (0‑based) corresponds to day `n+1`.  
  Value `0` means “every day of the month”. Value `0xFFFFFFFF` means “all month days”.

### Overnight & Always‑ON

- **Overnight schedule** – when stop time is earlier than start time (e.g. 22:00 → 06:00), the schedule is active across midnight.
- **Always ON** – when start time == stop time, the schedule is **always active** when the day mask matches.

### Schedule Caching

To avoid repeated time‑to‑struct conversions, the schedule cache is refreshed:
- Every `SCHEDULE_CACHE_INTERVAL` (1 second)
- After any time sync (NTP, browser, RTC)
- After manual override reset
- After saving a relay configuration

---

## Time Management

### NTP Synchronisation

- **Primary NTP server** – configurable via web (default: `time.google.com`).
- **Fallback servers** (hardcoded):
  1. `time.google.com`
  2. `time.windows.com`
  3. `time.cloudflare.com`
  4. `time.facebook.com`
- Sync interval: configurable from **1 to 24 hours** (default 1 hour).
- On failure, the next fallback server is tried; after all fail, the firmware retries after `NTP_RETRY_INTERVAL` (30 seconds).
- Upon successful sync:
  - Internal epoch is corrected.
  - Drift compensation is updated (based on difference between RTC and NTP over time).
  - DS3231 RTC is adjusted (if present).
  - Schedule cache is refreshed.

### Browser Time Sync

- Used when NTP is unreachable (e.g. captive portal, isolated network).
- User clicks **“Sync Browser”** on the **Time** page.
- The browser sends its UTC epoch (`Date.now() / 1000`) to `/api/time/browser-sync`.
- The firmware sets the internal epoch to this value and marks the time source as `TIME_SOURCE_BROWSER`.
- As soon as NTP succeeds later, it will override the browser time.
- This sync also updates the DS3231 RTC.

### DS3231 Hardware RTC

- **Auto‑detected** on I²C address `0x68`.
- If present and has not lost power (`rtc.lostPower()`), the firmware loads time from RTC at boot.
- The RTC is **re‑synced**:
  - Every minute from the internal epoch (to keep it accurate).
  - Immediately after any NTP or browser sync.
- The firmware also uses the RTC as a **fallback** when no WiFi is available (`getCurrentEpoch()` returns RTC time if internal epoch is invalid).

### Drift Compensation

- The internal time is maintained using `micros()` (`internalEpoch` + elapsed microseconds * `driftCompensation`).
- After each NTP sync, a drift factor is calculated:  
  `measured = (actual_seconds) / (nominal_seconds)`.
- The drift factor is filtered over the last 4 measurements and blended into `driftCompensation` (80% old, 20% new).
- Limits: `0.95 ≤ driftCompensation ≤ 1.05`.

### RTC Rebase

- Every `RTC_REBASE_INTERVAL` (5 minutes) the internal epoch is adjusted to correct for micros() overflow and drift accumulation (`performRTCReabase()`).
- This ensures long‑term accuracy without relying solely on NTP.

### Time Source Tracking

- `timeSource` can be:
  - `TIME_SOURCE_NONE` – no valid time.
  - `TIME_SOURCE_NTP` – synchronised via NTP.
  - `TIME_SOURCE_BROWSER` – synchronised via browser.
  - `TIME_SOURCE_RTC` – loaded from DS3231 at boot.

---

## WiFi & Networking

### Station (STA) Mode

- Connects to a user‑provided SSID / password.
- **Automatic reconnection**:
  - Checks every `WIFI_CHECK_INTERVAL` (10 seconds).
  - After `MAX_RECONNECT` (10) failures, backs off for 5 minutes.
- While connecting, a timeout of `WIFI_CONNECT_TIMEOUT` (20 seconds) applies.
- Once connected:
  - IP address is displayed in the web interface.
  - mDNS announces `_http._tcp` service.
  - NTP sync is attempted.

### Access Point (AP) Mode

- Always active (even when STA is connected) – `WIFI_AP_STA` mode.
- Configurable:
  - SSID (max 31 chars)
  - Password (min 8 chars, can be empty for open AP)
  - Channel (1–13)
  - Hidden SSID (broadcast or not)
- Default AP credentials:  
  **SSID:** `ESP32_16CH_Timer_Switch`  
  **Password:** `ESP32-admin`
- The AP is restarted automatically when its settings change.

### Captive Portal & DNS

- A `DNSServer` instance runs on port 53, redirecting **all** domain names to the AP IP address.
- This allows any device that connects to the AP to be automatically redirected to the web interface, regardless of the URL they try to access.
- The following well‑known captive portal paths are intercepted:
  - `/hotspot-detect.html`
  - `/library/test/success.html`
  - `/generate_204`
  - `/success.txt`
  - `/canonical.html`
  - `/connecttest.txt`
  - `/ncsi.txt`
  - `/redirect`

### mDNS

- The device advertises itself as `http://<hostname>.local`.
- Default hostname: `esp32` (after sanitising the AP SSID).
- Hostname can be changed via API (`/api/mdns` POST) or is automatically derived from the AP SSID.
- Service TXT records: `model=ESP32`, `version=v9`, `channels=N`.
- If mDNS fails to start, the self‑healing system will retry.

### WiFi Scanning

- The web interface includes a **“Scan”** button on the **WiFi** page.
- Scanning is asynchronous:
  - `POST /api/wifi/scan` starts a background scan.
  - `GET /api/wifi/scan` returns the list of networks (or `scanning: true` while in progress).
- Results include SSID, RSSI, and encryption status.
- The user can click any discovered network to autofill the SSID field.

---

## Web Interface

The UI is entirely self‑contained (no external CSS/JS). All pages are served from PROGMEM.

### Pages Overview

| Page       | URL        | Description                                                                 |
| ---------- | ---------- | --------------------------------------------------------------------------- |
| **Relays** | `/`        | Main control panel: shows all relays, current state, manual buttons, and schedule editor (8 slots per relay). Double‑click a relay name to edit. |
| **WiFi**   | `/wifi`    | Configure STA SSID/password, scan networks, view connection status.        |
| **Time**   | `/ntp`     | Set NTP server, GMT offset, DST offset, sync interval. Shows current time source. Buttons to sync NTP now or sync from browser. |
| **AP**     | `/ap`      | Configure Access Point SSID, password, channel, hidden option.              |
| **GPIO**   | `/gpio`    | Set global active mode, add/remove relays (GPIO pins), toggle active LOW/HIGH per relay. |
| **System** | `/system`  | Dashboard with STA/AP IP, heap usage, uptime, RSSI, time source, NTP sync age, drift compensation, chip model, mDNS hostname, RTC presence. Buttons to verify services and factory reset. |

### UI Highlights

- **Real‑time clock** – shown in the top‑right corner, updates every second via `/api/time`.
- **Status dots** – WiFi (green/red) and time source (green = NTP, blue = browser/RTC, yellow = none).
- **Relay cards** – each card shows:
  - Name (editable by double‑click)
  - Current state badge (ON/OFF/MANUAL)
  - ON/OFF/Auto buttons
  - Up to 8 schedule tabs (expandable)
  - Day‑of‑week and day‑of‑month selectors
  - Time pickers (with seconds)
  - “Save Relay” button
- **Toast notifications** – feedback for every action (save, sync, error).
- **Responsive** – works on mobile phones (grid adjusts).

---

## API Reference

All endpoints return `application/json`. Unless otherwise noted, POST requests require a JSON body.

### Relay Endpoints

| Method | Endpoint                       | Description                                          |
| ------ | ------------------------------ | ---------------------------------------------------- |
| GET    | `/api/relays`                  | Returns full list of relays (state, manual, schedules, name, pin). |
| POST   | `/api/relay/manual`            | Set manual override. Body: `{"relay":0,"state":true}` |
| POST   | `/api/relay/reset`             | Remove manual override (return to auto). Body: `{"relay":0}` |
| POST   | `/api/relay/save`              | Save schedules for a relay. Body: `{"relay":0,"schedules":[...]}` |
| POST   | `/api/relay/name`              | Change relay name. Body: `{"relay":0,"name":"New name"}` |

### Time Endpoints

| Method | Endpoint                       | Description                                                                 |
| ------ | ------------------------------ | --------------------------------------------------------------------------- |
| GET    | `/api/time`                    | Returns `{"time":"HH:MM:SS","wifi":bool,"ntp":bool,"timeSource":"...","rtcPresent":bool}` |
| POST   | `/api/time/browser-sync`       | Sync internal time from browser. Body: `{"utc_epoch":1704067200}`           |

### WiFi Endpoints

| Method | Endpoint                       | Description                                                                 |
| ------ | ------------------------------ | --------------------------------------------------------------------------- |
| GET    | `/api/wifi`                    | Returns STA SSID, connection status, IP, RSSI.                              |
| POST   | `/api/wifi`                    | Save STA credentials. Body: `{"ssid":"...","password":"..."}`               |
| POST   | `/api/wifi/scan`               | Start a WiFi scan (async).                                                  |
| GET    | `/api/wifi/scan`               | Get scan results: `{"scanning":bool,"networks":[{"ssid","rssi","enc"}]}`    |

### NTP Endpoints

| Method | Endpoint                       | Description                                                                 |
| ------ | ------------------------------ | --------------------------------------------------------------------------- |
| GET    | `/api/ntp`                     | Returns `ntpServer, gmtOffset, daylightOffset, syncHours, globalActiveMode`. |
| POST   | `/api/ntp`                     | Save NTP settings. Body: `{"ntpServer":"...","gmtOffset":...,"daylightOffset":...,"syncHours":...}` |
| POST   | `/api/ntp/sync`                | Force an immediate NTP sync.                                                |

### AP Endpoints

| Method | Endpoint                       | Description                                                                 |
| ------ | ------------------------------ | --------------------------------------------------------------------------- |
| GET    | `/api/ap`                      | Returns AP SSID, password (masked), channel, hidden flag.                   |
| POST   | `/api/ap`                      | Save AP settings. Body: `{"ap_ssid":"...","ap_password":"...","ap_channel":6,"ap_hidden":false}` |

### GPIO Endpoints

| Method | Endpoint                               | Description                                                                 |
| ------ | -------------------------------------- | --------------------------------------------------------------------------- |
| GET    | `/api/gpio`                            | Returns `count, pins[], activeLow[], availablePins[]`.                      |
| POST   | `/api/gpio/save`                       | Replace entire pin list. Body: `{"pins":[15,2,...]}`                         |
| POST   | `/api/gpio/add`                        | Add a relay at the end. Body: `{"pin":12}`                                  |
| POST   | `/api/gpio/delete`                     | Remove relay at index. Body: `{"index":3}`                                  |
| POST   | `/api/gpio/toggle-active-low`          | Toggle active LOW/HIGH for a relay. Body: `{"index":0}`                     |
| GET    | `/api/gpio/global-mode`                | Returns `{"mode":0|1|2}`                                                    |
| POST   | `/api/gpio/global-mode`                | Set global mode. Body: `{"mode":1}`                                         |

### mDNS Endpoints

| Method | Endpoint                       | Description                                                                 |
| ------ | ------------------------------ | --------------------------------------------------------------------------- |
| GET    | `/api/mdns`                    | Returns `hostname, started, url`.                                           |
| POST   | `/api/mdns`                    | Set mDNS hostname. Body: `{"hostname":"myesp"}`                             |
| POST   | `/api/mdns/restart`            | Force restart of mDNS service.                                              |

### System Endpoints

| Method | Endpoint                       | Description                                                                 |
| ------ | ------------------------------ | --------------------------------------------------------------------------- |
| GET    | `/api/system`                  | Rich system info (IPs, uptime, heap, time source, sync ages, drift, RTC status). |
| POST   | `/api/reset`                   | Trigger self‑healing service verification (no restart).                     |
| POST   | `/api/factory-reset`           | Erase all settings and restore defaults.                                    |

---

## Self‑Healing System

The `SelfHealingSystem` class runs in the background to keep the device operational without human intervention.

### Live Reconfiguration

Without restarting the device, it can:

- **Reconfigure WiFi** – if credentials changed or connection lost, call `WiFi.begin()` again.
- **Reconfigure mDNS** – restart mDNS service if it stopped advertising.
- **Reconfigure DNS** – (currently a placeholder, but can be extended).
- **Reconfigure web server** – check that the server is still accepting connections.
- **Reconfigure AP** – if the softAP IP becomes `0.0.0.0`, restart AP with saved settings.

### Critical State Saving

- The `CriticalRelayState` structure stores:
  - Current relay ON/OFF states
  - Manual override flags
  - Timestamp and checksum
- Saved every 5 minutes and after any manual change.
- On boot, `restoreCriticalState()` reapplies manual overrides, so relays return to their pre‑reboot state (only for manual‑controlled relays).

### Service Verification

- `smartRecovery()` runs every 10 seconds.
- It checks WiFi connection, mDNS announcements, and calls `verifyRelayStates()` every 30 seconds to ensure physical relay outputs match the expected states (fixes any stray changes).
- `performTargetedRecovery()` reinitialises all services on demand (e.g., after factory reset or API call).

### Health Metrics

The `HealthMetrics` struct tracks failures:
- `wifiFailures`, `ntpFailures`, `mdnsFailures`, `dnsFailures`, `webServerFailures`
- Used to avoid excessive recovery attempts (e.g., only reconfigure WiFi after 3 failures).

---

## Memory Management

Given the ESP32’s limited heap (~300 KB free after boot), the firmware actively manages memory:

- **Periodic cleanup** – every `MEMORY_CLEANUP_INTERVAL` (30 seconds), stale resources are cleaned:
  - `WiFi.scanDelete()` if a scan is not in progress.
  - `preferences.end()` / `begin()` to free internal buffers.
  - A small “heap walk” (allocate/free 512 bytes) to defragment.
- **Heap monitoring** – every 5 minutes, free heap is checked; if below 20 KB, a cleanup is forced.
- **Response caching** – `/api/relays` JSON is cached for up to 2 seconds, then regenerated.
- **Connection timeout** – if no web client activity for 5 minutes, `checkWebServerHealth()` reconfigures the server.
- **Client pruning** – every minute, the server checks for stale clients and stops them.

---

## Factory Reset

### Hardware Factory Reset (Boot Button)

- The BOOT button (GPIO0) is monitored in `loop()`.
- When pressed and held for **5 seconds**, the firmware:
  1. Clears all NVS preferences (`preferences.clear()`).
  2. Calls `initDefaults()` to restore default configuration.
  3. Reloads GPIO config, system config, extended config.
  4. Turns all relays OFF.
  5. Restarts AP and services.
  6. Tries to reconnect to the previously saved STA (which was cleared, so it will not reconnect).
- The reset does **not** reboot the ESP32 – all services are reinitialised in‑place.

### API Factory Reset

- `POST /api/factory-reset` performs the same steps as the hardware reset.
- Returns a JSON success message; the device remains operational.

---

## Persistent Configuration (NVS)

All settings are stored in the **Preferences** library (namespace `relay16`).

| Key             | Type                 | Description                                                       |
| --------------- | -------------------- | ----------------------------------------------------------------- |
| `sysConfig`     | `SystemConfig`       | WiFi STA, AP, NTP, GMT, hostname, last RTC epoch, drift.          |
| `extConfig`     | `ExtConfig`          | AP channel, NTP sync hours, AP hidden, global active mode.        |
| `relayConfigs`  | `RelayConfig[16]`    | Schedules (8 slots), manual override, manual state, name.         |
| `gpioConfig`    | `GPIOPinConfig`      | Pin numbers, active LOW flags, count, magic.                      |
| `criticalState` | `CriticalRelayState` | Last known manual relay states (for recovery).                    |

All structs have magic numbers and version fields to detect uninitialised or corrupt storage.

---

## Health Metrics & Diagnostics

The **System** page (`/system`) shows:

- **STA IP** / **AP IP**
- **Free heap** (KB)
- **Uptime** (human‑readable)
- **WiFi RSSI** with description (Excellent/Good/Fair/Weak)
- **Time source** (NTP, Browser, RTC, None)
- **UTC epoch** (raw)
- **NTP last sync** (age in minutes/hours)
- **Browser last sync** (age)
- **Active NTP server**
- **Chip model**
- **mDNS hostname** and status
- **Relay count** (active relays)
- **GMT offset** (UTC±X)
- **Drift compensation** value
- **Global active mode** (Per‑Relay / Global LOW / Global HIGH)
- **DS3231 RTC presence** (✅ / ❌)

A **“Verify Services”** button calls `POST /api/reset`, which runs `healer.performTargetedRecovery()` – this checks and restores WiFi, mDNS, DNS, web server, and AP without erasing any settings.

---

## Limitations & Notes

- **Maximum relays** – 16 (hardware limit, but can be changed by redefining `MAX_RELAYS` and recompiling).
- **Schedules** – 8 slots per relay. If more are needed, the JSON transfer size will increase; the ESP32 can handle it, but the web UI may become slower.
- **Day‑of‑month** – the UI provides 31 buttons (days 1–31). On months with fewer days, the schedule engine still respects the mask but the physical day 31 will never occur in short months.
- **GPIO restrictions** – Some ESP32 pins have bootstrapping functions (e.g., GPIO12, GPIO0). The list of available pins in the GPIO page excludes problematic ones, but users can still modify the code to include them.
- **RTC drift compensation** – works best when NTP syncs occur regularly. Without NTP, drift is not corrected; however, the DS3231 has very low drift (≈2 ppm).
- **Self‑healing** – does **not** reboot the ESP32. In rare cases of complete stack corruption, a hardware watchdog could be added.
- **Memory** – The firmware compiles to ~1.5 MB of flash; heap usage is typically 100–150 KB, leaving ample room for future extensions.

---

## Conclusion

The **ESP32 16‑Channel Relay Smart Switch** is a production‑ready, feature‑rich automation controller. Its combination of hardware RTC, dual WiFi modes, flexible scheduling, self‑healing, and an intuitive web interface makes it suitable for home, business, and farm automation. The code is well‑structured and can be extended with sensors, MQTT, or custom logic without breaking the core functionality.

For updates and support, refer to the [GitHub repository](https://www.github.com/xiv3r).

</details>

# Customize GPIO Pins
> Find this line in sketch.ino
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
```
// Change GPIO PIN
int validPins[] = {15, 2, 4, 5, 18, 19, 3, 1, 23, 13, 14, 27, 26, 25, 33, 32};
```

