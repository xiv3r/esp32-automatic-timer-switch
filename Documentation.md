```markdown
# ESP32 16-Channel Relay Smart Switch - Complete Documentation

## Overview

The ESP32 16-Channel Relay Smart Switch is a comprehensive IoT relay control system featuring web-based configuration, time-based scheduling, WiFi connectivity, and self-healing capabilities. It supports up to 16 relays with individual scheduling, manual override, and persistent configuration storage.

---

## Table of Contents

- [Features](#features)
- [Hardware Requirements](#hardware-requirements)
- [Installation & Setup](#installation--setup)
- [System Architecture](#system-architecture)
- [Web Interface](#web-interface)
- [API Reference](#api-reference)
- [Configuration System](#configuration-system)
- [Time Management](#time-management)
- [WiFi & Networking](#wifi--networking)
- [Relay Control & Scheduling](#relay-control--scheduling)
- [Self-Healing System](#self-healing-system)
- [GPIO Configuration](#gpio-configuration)
- [DS3231 RTC Support](#ds3231-rtc-support)
- [NTP Synchronization](#ntp-synchronization)
- [Access Point Mode](#access-point-mode)
- [mDNS Support](#mdns-support)
- [Memory Management](#memory-management)
- [Factory Reset](#factory-reset)
- [Security Considerations](#security-considerations)
- [Troubleshooting](#troubleshooting)

---

## Features

### Core Features

- **16-Channel Relay Control** with configurable GPIO pin mapping
- **Web-based Dashboard** for real-time monitoring and control
- **8 Independent Schedules** per relay with granular time control
- **Day-of-Week Scheduling** (individual days, weekdays, weekends, everyday)
- **Day-of-Month Scheduling** (specific days 1-31 or all month days)
- **Manual Override** with individual ON/OFF control
- **Per-Relay Naming** for easy identification
- **Global Active Mode** (All LOW, All HIGH, or Per-Relay configuration)

### Networking Features

- **WiFi Station Mode** for internet connectivity (toggleable)
- **Access Point Mode** with configurable SSID, password, channel, and visibility
- **Captive Portal Support** for automatic redirect on captive networks
- **mDNS Support** for easy local network discovery (hostname.local)
- **DNS Server** for captive portal functionality
- **WiFi Network Scanning** for easy network selection

### Time Management

- **NTP Time Synchronization** with configurable servers and fallback pool
- **Browser Time Sync** as alternative time source
- **DS3231 Hardware RTC Support** for time persistence across power cycles
- **Internal Software RTC** with drift compensation
- **Multiple Time Sources** with automatic priority (NTP > Browser > RTC)
- **Configurable GMT Offset** and Daylight Saving Time offset
- **Year 2106 Support** for extended operation

### Advanced Features

- **Self-Healing System** with automatic service recovery
- **Critical State Persistence** for relay states across reboots
- **Preferences/NVS Storage** for all configurations
- **Factory Reset** via UI or hardware BOOT button (5-second hold)
- **Live Reconfiguration** without reboot
- **Response Caching** for improved performance
- **Memory Management** with automatic cleanup
- **Health Monitoring** with failure tracking

---

## Hardware Requirements

### Required Components

- ESP32 Development Board (ESP32-WROOM-32 or similar)
- 16-Channel Relay Module (typically active LOW)
- DS3231 RTC Module (optional, for time persistence)
- 5V Power Supply (sufficient for ESP32 + relays)

### Default GPIO Pin Mapping

| Relay | GPIO | Relay | GPIO |
|-------|------|-------|------|
| Relay 1 | GPIO 15 | Relay 9 | GPIO 23 |
| Relay 2 | GPIO 2 | Relay 10 | GPIO 13 |
| Relay 3 | GPIO 4 | Relay 11 | GPIO 14 |
| Relay 4 | GPIO 5 | Relay 12 | GPIO 27 |
| Relay 5 | GPIO 18 | Relay 13 | GPIO 26 |
| Relay 6 | GPIO 19 | Relay 14 | GPIO 25 |
| Relay 7 | GPIO 3 | Relay 15 | GPIO 33 |
| Relay 8 | GPIO 1 | Relay 16 | GPIO 32 |

### DS3231 RTC Connection

- **SDA**: GPIO 21
- **SCL**: GPIO 22
- **Power**: 3.3V
- **GND**: GND

### Boot Button

- **GPIO 0** - Hold for 5 seconds for hardware factory reset

---

## Installation & Setup

### 1. Software Requirements

- Arduino IDE or PlatformIO
- Required Libraries:
  - WiFi (built-in)
  - NTPClient
  - WebServer
  - DNSServer
  - Preferences
  - ArduinoJson
  - ESPmDNS
  - Wire
  - RTClib

### 2. Configuration

1. Clone the repository
2. Open the project in Arduino IDE
3. Select your ESP32 board
4. Configure GPIO pins in `DEFAULT_RELAY_PINS[]` if needed
5. Upload the code

### 3. First Boot

On first boot, the ESP32 creates an Access Point:

- **SSID**: `ESP32_16CH_Timer_Switch`
- **Password**: `ESP32-admin`
- **IP Address**: `192.168.4.1`

### 4. Initial Configuration

1. Connect to the AP
2. Open browser to `192.168.4.1`
3. Configure WiFi settings under the WiFi tab
4. Set up NTP time synchronization
5. Configure relays and schedules

---

## System Architecture

### Component Overview

```

ESP32 16-Channel Relay Smart Switch
©À©¤©¤ Core Systems
©¦   ©À©¤©¤ WiFi Manager (STA + AP)
©¦   ©À©¤©¤ Web Server (HTTP on port 80)
©¦   ©À©¤©¤ DNS Server (port 53, captive portal)
©¦   ©À©¤©¤ NTP Client
©¦   ©¸©¤©¤ mDNS Server
©À©¤©¤ Storage Systems
©¦   ©À©¤©¤ Preferences/NVS (System Config)
©¦   ©À©¤©¤ Preferences/NVS (Ext Config)
©¦   ©À©¤©¤ Preferences/NVS (GPIO Config)
©¦   ©À©¤©¤ Preferences/NVS (Relay Configs)
©¦   ©¸©¤©¤ Preferences/NVS (Critical State)
©À©¤©¤ Time Systems
©¦   ©À©¤©¤ NTP Synchronization
©¦   ©À©¤©¤ Browser Time Sync
©¦   ©À©¤©¤ DS3231 Hardware RTC
©¦   ©¸©¤©¤ Internal Software RTC
©À©¤©¤ Control Systems
©¦   ©À©¤©¤ Relay Output Controller
©¦   ©À©¤©¤ Schedule Engine
©¦   ©À©¤©¤ Manual Override System
©¦   ©¸©¤©¤ GPIO Configuration Manager
©¸©¤©¤ Maintenance Systems
©À©¤©¤ Self-Healing Engine
©À©¤©¤ Memory Manager
©À©¤©¤ Health Monitor
©¸©¤©¤ Factory Reset Handler

```

### Data Flow

1. **Time Acquisition**: NTP ¡ú Internal RTC ¡ú Schedule Engine
2. **Schedule Processing**: Config ¡ú Day/Time Check ¡ú Relay State
3. **Web Control**: Browser ¡ú HTTP API ¡ú Relay Controller
4. **State Persistence**: Changes ¡ú NVS Storage ¡ú Critical State Backup

---

## Web Interface

### Pages

#### 1. Relays Page (`/`)

Main dashboard for relay control and scheduling:

- Real-time relay status display (ON/OFF/MANUAL)
- Individual relay ON/OFF buttons
- Manual override toggle with AUTO return
- 8 schedules per relay with time, day, and month-day settings
- Day-of-week selection (Sun-Sat) with visual toggle
- Day-of-month selection (1-31) for monthly schedules
- Relay name editing (double-click to rename)
- Night badge indicator for overnight schedules
- Always ON indicator for 24/7 schedules
- Save button per relay for schedule persistence
- Real-time clock display with time source indicator
- WiFi connection status indicator
- Color-coded status badges (green=ON, red=OFF, orange=MANUAL)

#### 2. WiFi Page (`/wifi`)

Station mode configuration:

- WiFi Station Mode toggle (ON/OFF)
- Network SSID input
- Password input
- Network scanner with signal strength display
- Connection status display
- RSSI signal bars visualization
- Encryption lock indicator
- Save & Connect button
- Warning messages for disabled station mode

#### 3. Time Page (`/ntp`)

Time synchronization settings:

- NTP server configuration
- GMT offset setting (in seconds)
- Daylight saving offset (in seconds)
- Auto-sync interval (1-24 hours)
- Manual NTP sync button
- Browser time sync button
- Time source status display
- DS3231 RTC status and sync age
- Real-time clock update
- Warning/Info alerts for time source status

#### 4. AP Page (`/ap`)

Access Point configuration:

- AP SSID configuration
- AP Password (optional, 8+ characters)
- Channel selection (1-13)
- SSID visibility toggle (Visible/Hidden)
- Warning about AP restart on changes
- Save Settings button with auto-reload

#### 5. GPIO Page (`/gpio`)

GPIO pin management:

- Global Active Mode selector (Per-Relay/All LOW/All HIGH)
- Add new relay pin dropdown (shows only available pins)
- Configured relay list with pin numbers
- Per-relay active level toggle (LOW/HIGH)
- Relay removal with confirmation
- Reset to default 16-pin configuration
- Relay count display
- Maximum relay limit enforcement (16)
- Warning banner for global mode override

#### 6. System Page (`/system`)

System information and control:

- STA IP and AP IP display
- Free heap memory (KB)
- System uptime
- WiFi RSSI with quality description
- Time source with color coding
- UTC epoch timestamp
- NTP last sync age
- Browser last sync age
- NTP server address
- Chip model (ESP32-38P)
- mDNS hostname and status
- Relay count / Maximum relays
- GMT offset with UTC notation
- Drift compensation value
- GPIO Active Mode status
- DS3231 RTC presence and last sync
- WiFi Station status
- Verify Services button
- Factory Reset button with confirmation

---

## API Reference

### Relay API Endpoints

#### GET `/api/relays`

Returns all relay states and schedules.

**Response:**
```json
[
  {
    "state": false,
    "manual": false,
    "name": "Relay 1",
    "pin": 15,
    "schedules": [
      {
        "startHour": 8,
        "startMinute": 0,
        "startSecond": 0,
        "stopHour": 17,
        "stopMinute": 0,
        "stopSecond": 0,
        "enabled": true,
        "days": 127,
        "monthDays": 0
      }
    ]
  }
]
```

POST /api/relay/manual

Set manual override state.

Request:

```json
{
  "relay": 0,
  "state": true
}
```

Response:

```json
{"success": true}
```

POST /api/relay/reset

Remove manual override (return to schedule).

Request:

```json
{"relay": 0}
```

POST /api/relay/save

Save relay schedules.

Request:

```json
{
  "relay": 0,
  "schedules": [
    {
      "startHour": 8,
      "startMinute": 0,
      "startSecond": 0,
      "stopHour": 17,
      "stopMinute": 0,
      "stopSecond": 0,
      "enabled": true,
      "days": 127,
      "monthDays": 0
    }
  ]
}
```

POST /api/relay/name

Update relay name.

Request:

```json
{
  "relay": 0,
  "name": "Living Room"
}
```

Time API Endpoints

GET /api/time

Returns current time and status.

Response:

```json
{
  "time": "14:30:00",
  "wifi": true,
  "ntp": true,
  "timeSource": "ntp",
  "rtcPresent": true,
  "rtcSynced": true,
  "rtcSyncAge": 3600
}
```

POST /api/time/browser-sync

Synchronize time from browser.

Request:

```json
{"utc_epoch": 1718548800}
```

Response:

```json
{
  "success": true,
  "utc_epoch": 1718548800,
  "local_time": "2024-06-16 14:30:00",
  "gmt_offset": 28800,
  "time_source": "browser",
  "rtc_present": true,
  "rtc_synced": true
}
```

WiFi API Endpoints

GET /api/wifi

Returns WiFi status.

Response:

```json
{
  "ssid": "MyNetwork",
  "connected": true,
  "ip": "192.168.1.100",
  "rssi": -45,
  "sta_enabled": true
}
```

POST /api/wifi

Save WiFi credentials or toggle station mode.

Save credentials:

```json
{
  "ssid": "MyNetwork",
  "password": "MyPassword"
}
```

Toggle station:

```json
{"sta_enabled": false}
```

POST /api/wifi/scan

Initiate WiFi network scan.

Response:

```json
{"scanning": true}
```

GET /api/wifi/scan

Retrieve scan results.

Response:

```json
{
  "scanning": false,
  "networks": [
    {"ssid": "Network1", "rssi": -45, "enc": true}
  ]
}
```

NTP API Endpoints

GET /api/ntp

Returns NTP configuration.

Response:

```json
{
  "ntpServer": "time.google.com",
  "gmtOffset": 28800,
  "daylightOffset": 0,
  "syncHours": 1,
  "globalActiveMode": 0
}
```

POST /api/ntp

Save NTP settings.

Request:

```json
{
  "ntpServer": "time.google.com",
  "gmtOffset": 28800,
  "daylightOffset": 3600,
  "syncHours": 6
}
```

POST /api/ntp/sync

Trigger immediate NTP sync.

Response:

```json
{
  "success": true,
  "message": "NTP sync initiated"
}
```

Access Point API Endpoints

GET /api/ap

Returns AP configuration.

Response:

```json
{
  "ap_ssid": "ESP32_16CH_Timer_Switch",
  "ap_password": "ESP32-admin",
  "ap_channel": 6,
  "ap_hidden": false
}
```

POST /api/ap

Save AP settings.

Request:

```json
{
  "ap_ssid": "MyRelayAP",
  "ap_password": "SecurePass123",
  "ap_channel": 11,
  "ap_hidden": true
}
```

Response:

```json
{
  "success": true,
  "restarted": true
}
```

GPIO API Endpoints

GET /api/gpio

Returns GPIO configuration.

Response:

```json
{
  "count": 16,
  "maxRelays": 16,
  "pins": [15, 2, 4, 5, 18, 19, 3, 1, 23, 13, 14, 27, 26, 25, 33, 32],
  "activeLow": [true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true],
  "availablePins": [16, 17]
}
```

POST /api/gpio/save

Save complete GPIO configuration.

Request:

```json
{"pins": [15, 2, 4, 5, 18, 19, 3, 1]}
```

POST /api/gpio/add

Add a new relay pin.

Request:

```json
{"pin": 16}
```

Response:

```json
{"success": true, "count": 9}
```

POST /api/gpio/delete

Remove a relay by index.

Request:

```json
{"index": 3}
```

POST /api/gpio/toggle-active-low

Toggle relay active level.

Request:

```json
{"index": 0}
```

Response:

```json
{"success": true, "activeLow": false}
```

POST /api/gpio/global-mode

Set global active mode.

Request:

```json
{"mode": 1}
```

Modes:

¡¤ 0: Per-Relay Configuration
¡¤ 1: Global Active LOW
¡¤ 2: Global Active HIGH

System API Endpoints

GET /api/system

Returns system information.

Response:

```json
{
  "ip": "192.168.1.100",
  "ap_ip": "192.168.4.1",
  "uptime": 3600,
  "freeHeap": 200000,
  "utcEpoch": 1718548800,
  "timeSource": "NTP",
  "ntpServer": "time.google.com",
  "ntpSyncAge": 300,
  "browserSyncAge": 4294967295,
  "rtcSyncAge": 0,
  "wifiConnected": true,
  "wifiSSID": "MyNetwork",
  "rssi": -45,
  "version": 9,
  "chipModel": "ESP32-38P",
  "mdnsHostname": "esp32",
  "mdnsStarted": true,
  "relayCount": 16,
  "maxRelays": 16,
  "gmtOffset": 28800,
  "driftComp": 1.0000,
  "globalActiveMode": 0,
  "rtcPresent": true,
  "staEnabled": true
}
```

POST /api/reset

Trigger service verification.

Response:

```json
{
  "success": true,
  "message": "Performing live service verification..."
}
```

POST /api/factory-reset

Perform factory reset.

Response:

```json
{
  "success": true,
  "message": "Factory reset complete. Settings restored to defaults."
}
```

mDNS API Endpoints

GET /api/mdns

Returns mDNS configuration.

Response:

```json
{
  "hostname": "esp32",
  "started": true,
  "url": "http://esp32.local"
}
```

POST /api/mdns

Set mDNS hostname.

Request:

```json
{"hostname": "my-relay-controller"}
```

POST /api/mdns/restart

Restart mDNS service.

Response:

```json
{"success": true}
```

Captive Portal Endpoints

¡¤ /hotspot-detect.html - Apple Captive Network Assistant
¡¤ /library/test/success.html - Apple Captive Network Assistant
¡¤ /generate_204 - Android/Chrome captive portal detection
¡¤ /success.txt - Windows captive portal detection
¡¤ /canonical.html - Windows captive portal detection
¡¤ /connecttest.txt - Windows NCSI
¡¤ /ncsi.txt - Windows NCSI
¡¤ /redirect - General redirect
¡¤ All unknown paths redirect to home page

---

Configuration System

Storage Structure

The system uses ESP32 Preferences (NVS) to store all configuration data:

Namespace Key Structure Description
relay16 sysConfig SystemConfig System configuration
relay16 extConfig ExtConfig Extended configuration
relay16 relayConfigs RelayConfig[16] Relay schedules
relay16 gpioConfig GPIOPinConfig GPIO pin mapping
relay16 criticalState CriticalRelayState Persistent relay states

SystemConfig Structure

```cpp
struct SystemConfig {
    uint16_t magic;           // Validation: 0x1234
    uint8_t  version;         // Current: 9
    char     sta_ssid[32];    // WiFi station SSID
    char     sta_password[64];// WiFi station password
    char     ap_ssid[32];     // Access point SSID
    char     ap_password[32]; // Access point password
    char     ntp_server[48];  // NTP server address
    int32_t  gmt_offset;      // GMT offset in seconds
    int32_t  daylight_offset; // DST offset in seconds
    time_t   last_rtc_epoch;  // Last saved RTC epoch
    float    rtc_drift;       // Drift compensation factor
    char     hostname[32];    // Network hostname
};
```

ExtConfig Structure

```cpp
struct ExtConfig {
    uint8_t magic;              // Validation: 0xEC
    uint8_t ap_channel;         // AP channel (1-13)
    uint8_t ntp_sync_hours;     // NTP sync interval (1-24)
    uint8_t ap_hidden;          // SSID visibility
    uint8_t global_active_mode; // 0=per-relay, 1=all LOW, 2=all HIGH
    uint8_t sta_enabled;        // Station mode enabled
    uint8_t reserved[26];       // Future use
};
```

GPIOPinConfig Structure

```cpp
struct GPIOPinConfig {
    uint8_t pins[16];       // GPIO pin numbers
    uint8_t count;          // Number of active relays
    uint16_t magic;         // Validation: 0xD002
    bool activeLow[16];     // Active level per relay
};
```

RelayConfig Structure

```cpp
struct RelayConfig {
    TimerSchedule schedule;  // 8 schedule entries
    bool manualOverride;     // Manual override flag
    bool manualState;        // Manual state value
    char name[16];           // Custom relay name
};
```

TimerSchedule Structure

```cpp
struct TimerSchedule {
    uint8_t  startHour[8];     // Schedule start hour
    uint8_t  startMinute[8];   // Schedule start minute
    uint8_t  startSecond[8];   // Schedule start second
    uint8_t  stopHour[8];      // Schedule stop hour
    uint8_t  stopMinute[8];    // Schedule stop minute
    uint8_t  stopSecond[8];    // Schedule stop second
    bool     enabled[8];       // Schedule enabled
    uint8_t  days[8];          // Day-of-week bitmask
    uint32_t monthDays[8];     // Day-of-month bitmask
};
```

Day-of-Week Bitmask Constants

```cpp
#define DAY_SUNDAY    (1 << 0)  // 0x01
#define DAY_MONDAY    (1 << 1)  // 0x02
#define DAY_TUESDAY   (1 << 2)  // 0x04
#define DAY_WEDNESDAY (1 << 3)  // 0x08
#define DAY_THURSDAY  (1 << 4)  // 0x10
#define DAY_FRIDAY    (1 << 5)  // 0x20
#define DAY_SATURDAY  (1 << 6)  // 0x40
#define DAY_ALL       0x7F       // All days
#define DAY_WEEKDAYS  0x3E       // Mon-Fri
#define DAY_WEEKENDS  0x41       // Sat-Sun
```

CriticalRelayState Structure

```cpp
struct CriticalRelayState {
    uint32_t magic;              // 0xDEADBEEF
    bool relayStates[16];        // Current relay states
    bool manualOverrides[16];    // Manual override flags
    uint32_t timestamp;          // Save timestamp
    uint32_t checksum;           // Data integrity check
};
```

---

Time Management

Time Sources (Priority Order)

1. NTP (Network Time Protocol)
   ¡¤ Highest priority when available
   ¡¤ Configurable server with fallback pool
   ¡¤ Fallback order: Google ¡ú Windows ¡ú Cloudflare ¡ú Facebook
   ¡¤ Auto-sync interval: 1-24 hours (configurable)
   ¡¤ Provides UTC time for internal RTC
2. Browser Time Sync
   ¡¤ Secondary time source
   ¡¤ User-initiated from web interface
   ¡¤ Syncs device time from browser's system clock
   ¡¤ Automatically overridden when NTP becomes available
3. DS3231 Hardware RTC
   ¡¤ Tertiary time source
   ¡¤ Battery-backed for power-off persistence
   ¡¤ Provides time at startup before NTP sync
   ¡¤ Automatically synced from internal RTC after time is set

Internal RTC System

The ESP32 maintains an internal software RTC with:

¡¤ Microsecond-precision time tracking
¡¤ Drift compensation for accurate long-term operation
¡¤ Periodic rebasing to prevent overflow (every 5 minutes)
¡¤ Auto-save to NVS every hour for persistence
¡¤ Seamless integration with hardware RTC

Time Functions

```cpp
// Get current UTC epoch (with drift compensation)
time_t getCurrentEpoch();

// Get local time (UTC + GMT offset + DST offset)
time_t getLocalEpoch(time_t utcEpoch);

// Synchronize internal RTC from NTP time
void syncInternalRTC(time_t rawUtcEpoch);

// Check if epoch is within valid range (2020-2106)
#define VALID_UNIX_TIME(epoch) ((epoch) > MIN_UNIX_TIME && (epoch) < MAX_UNIX_TIME)
```

Timing Constants

Constant Value Description
NTP_RETRY_INTERVAL 30s NTP retry interval
RTC_REBASE_INTERVAL 300s Internal RTC rebase interval
RTC_SYNC_INTERVAL 3600s RTC save interval
DS3231_SYNC_INTERVAL 3600s DS3231 sync interval
NTP_SERVER_TIMEOUT 5s Per-server timeout

---

WiFi & Networking

Dual-Mode Operation

The ESP32 operates simultaneously in:

¡¤ Station Mode (STA): Connects to existing WiFi network
¡¤ Access Point Mode (AP): Creates its own WiFi network

WiFi State Machine

The system implements a robust WiFi connection state machine:

1. IDLE: No connection attempt
2. CONNECTING: Active connection attempt (20s timeout)
3. CONNECTED: Successfully connected
4. GIVE_UP: Too many failures, wait 5 minutes
5. PAUSED: Temporarily paused for WiFi scan

WiFi Features

¡¤ Automatic Reconnection with exponential backoff
¡¤ Maximum 10 retry attempts before cooldown
¡¤ 5-minute cooldown after max failures
¡¤ Scan Pause System to prevent interference during network scans
¡¤ Station Enable/Disable toggle for AP-only operation
¡¤ RSSI Monitoring with quality classification

WiFi Scan System

The WiFi scanner features:

¡¤ 15-second pause on connection attempts during scan
¡¤ 10-second scan timeout
¡¤ Results sorted by signal strength
¡¤ Signal strength visualization (1-4 bars)
¡¤ Encryption indicator (lock icon)
¡¤ Click-to-select for easy SSID entry

Captive Portal

The device implements a full captive portal with:

¡¤ DNS server on port 53 (all domains resolve to AP IP)
¡¤ Automatic redirect for Android, iOS, Windows, and macOS
¡¤ Support for all major captive portal detection methods
¡¤ Custom AP IP as landing page

---

Relay Control & Scheduling

Active Level Configuration

Relays can be configured with three active level modes:

1. Global Active LOW (Mode 1): All relays activate on LOW signal
2. Global Active HIGH (Mode 2): All relays activate on HIGH signal
3. Per-Relay Configuration (Mode 0): Individual active level per relay

```cpp
bool isActiveLow(uint8_t index) {
    if (extConfig.global_active_mode == 1) return true;
    if (extConfig.global_active_mode == 2) return false;
    return gpioConfig.activeLow[index];
}
```

Schedule Engine

The schedule engine processes relay states with:

¡¤ 250ms update interval for responsive changes
¡¤ 1-second cache refresh for schedule evaluation
¡¤ 500ms debouncing to prevent rapid state changes
¡¤ Priority system: Manual override > Schedule

Schedule Types

1. Standard Schedule: Start time < Stop time (same day)
   ```
   Example: 08:00:00 to 17:00:00 (office hours)
   ```
2. Overnight Schedule: Start time > Stop time (crosses midnight)
   ```
   Example: 22:00:00 to 06:00:00 (night lights)
   ```
3. Always ON Schedule: Start time == Stop time
   ```
   Example: 00:00:00 to 00:00:00 (24/7 operation)
   ```

Day Filtering

Schedules can be filtered by:

¡¤ Day of Week: Individual days or combinations
¡¤ Day of Month: Specific days (1-31) for monthly schedules
¡¤ Combined Filters: Both day-of-week AND day-of-month must match
¡¤ No Month Day Filter: 0x00000000 means no month-day restriction
¡¤ All Month Days: 0xFFFFFFFF means all days of month

Schedule Evaluation Logic

```cpp
// For each enabled schedule:
if (dayOfWeek matches AND monthDay matches) {
    if (startTime == stopTime) ¡ú Always ON
    if (startTime < stopTime AND currentTime is between) ¡ú ON
    if (startTime > stopTime AND currentTime is before start OR after stop) ¡ú ON
}
```

Critical State System

The critical state system ensures relay states survive reboots:

¡¤ Auto-save every 5 minutes when state changes
¡¤ CRC32 checksum for data integrity validation
¡¤ Magic number (0xDEADBEEF) for structure validation
¡¤ Restored on boot for manual overrides

---

Self-Healing System

Recovery Functions

Function Target Interval Description
recoverWiFi() STA Connection 30s Attempts WiFi reconnection
recoverMDNS() mDNS Service 60s Restarts mDNS if needed
recoverDNS() DNS Server 60s Validates DNS server
recoverWebServer() HTTP Server 30s Checks web server health
recoverNTP() NTP Client On demand Tries all NTP servers
recoverRTC() Hardware RTC On demand Reconnects DS3231

Health Metrics

```cpp
struct HealthMetrics {
    uint32_t wifiFailures;       // WiFi connection failures
    uint32_t ntpFailures;        // NTP sync failures
    uint32_t mdnsFailures;       // mDNS service failures
    uint32_t dnsFailures;        // DNS server failures
    uint32_t webServerFailures;  // Web server failures
    unsigned long lastRecoveryAttempt;
    bool inRecoveryMode;
};
```

Automatic Recovery Actions

1. WiFi Recovery:
   ¡¤ After 3 consecutive failures: Force reconnection
   ¡¤ Resets connection state machine
   ¡¤ Clears failure counters on success
2. Web Server Recovery:
   ¡¤ After 5 minutes of no client activity: Refresh services
   ¡¤ Re-initializes server if needed
3. mDNS Recovery:
   ¡¤ Every 5 minutes: Re-announce services
   ¡¤ Automatic restart if service dies
4. Full Health Check:
   ¡¤ Every 30 minutes: Complete system verification
   ¡¤ All services checked and restored if needed

Live Reconfiguration

Services can be reconfigured without reboot:

¡¤ liveReconfigureWiFi() - Update WiFi settings
¡¤ liveReconfigureMDNS() - Refresh mDNS services
¡¤ liveReconfigureDNS() - Reinitialize DNS server
¡¤ liveReconfigureWebServer() - Refresh web server
¡¤ liveReconfigureAP() - Restart AP if needed

---

GPIO Configuration

Supported GPIO Pins

The default configuration supports the following GPIO pins for relays:

```
GPIO 1, 2, 3, 4, 5, 13, 14, 15, 18, 19, 23, 25, 26, 27, 32, 33
```

Additional available pins:

```
GPIO 16, 17
```

GPIO Management

¡¤ Dynamic Pin Mapping: Relays can be reassigned to any supported GPIO
¡¤ Add/Remove Relays: Expand or shrink the relay count (1-16)
¡¤ Pin Conflict Prevention: Prevents duplicate pin assignments
¡¤ Automatic Re-initialization: Pins configured on change
¡¤ Shift on Delete: Removing a relay shifts subsequent relay configurations

Active Level Control

Each relay can be individually configured as:

¡¤ Active LOW: Relay activates when GPIO is LOW (common for relay modules)
¡¤ Active HIGH: Relay activates when GPIO is HIGH

The global mode overrides individual settings:

¡¤ Mode 0: Use per-relay configuration
¡¤ Mode 1: Force all relays to Active LOW
¡¤ Mode 2: Force all relays to Active HIGH

---

DS3231 RTC Support

Features

¡¤ Automatic Detection: System detects DS3231 on GPIO 21/22
¡¤ Battery-Backed: Maintains time across power cycles
¡¤ Automatic Synchronization: Updated every hour from internal RTC
¡¤ Power Loss Detection: Detects if RTC lost power
¡¤ Immediate Sync: Option to sync immediately when time is set

Integration

```cpp
// Initialize RTC
void initRTC() {
    Wire.begin(21, 22);
    if (rtc.begin()) {
        rtcPresent = true;
        // Check if valid time exists
        DateTime now = rtc.now();
        if (now.year() >= 2020 && now.year() <= 2100) {
            rtcTimeValid = true;
        }
    }
}
```

Time Priority at Boot

1. Load DS3231 time (if available and valid)
2. Load internal RTC state from NVS (if DS3231 unavailable)
3. Wait for NTP sync (if WiFi connected)
4. Wait for browser sync (user-initiated)

---

NTP Synchronization

NTP Server Pool

The system tries NTP servers in order:

1. time.google.com
2. time.windows.com
3. time.cloudflare.com
4. time.facebook.com

Async NTP State Machine

```
IDLE ¡ú CONNECTING ¡ú WAITING ¡ú IDLE (success)
                          ¡ý
                     TIMEOUT ¡ú CONNECTING (next server)
                          ¡ý
                     ALL FAILED ¡ú IDLE (fail)
```

Sync Conditions

¡¤ Initial Sync: Immediately after WiFi connection
¡¤ Periodic Sync: Based on configured interval (1-24 hours)
¡¤ Forced Sync: User-initiated from web interface
¡¤ Recovery Sync: After NTP failures, retry after 30 seconds

---

Access Point Mode

Configuration

¡¤ SSID: Customizable (default: ESP32_16CH_Timer_Switch)
¡¤ Password: Optional (default: ESP32-admin), minimum 8 characters
¡¤ Channel: 1-13 (default: 6)
¡¤ Visibility: Visible or Hidden SSID
¡¤ IP Address: 192.168.4.1

Operation

¡¤ AP always runs, even when STA is connected
¡¤ Supports up to 4 client connections (ESP32 limitation)
¡¤ Captive portal redirects all DNS queries to AP IP
¡¤ Web interface accessible via AP IP or mDNS name

---

mDNS Support

Features

¡¤ Automatic Hostname: Generated from AP SSID or custom
¡¤ Service Advertisement: HTTP service on port 80
¡¤ TXT Records: Model, version, and channel count
¡¤ Automatic Recovery: Service re-announced periodically
¡¤ Custom Hostname: Configurable via API

TXT Records

```
model=ESP32
version=v9
channels=16
```

---

Memory Management

Automatic Cleanup

¡¤ Stale Resource Cleanup: Every 30 seconds
¡¤ Full Memory Cleanup: Every 1 hour
¡¤ WiFi Scan Results: Deleted after scan completion
¡¤ Response Cache: Invalidated after 5 seconds
¡¤ Client Connections: Stale connections closed

Heap Monitoring

¡¤ Minimum Free Heap Tracking for diagnostics
¡¤ Automatic Cleanup: Triggered when heap drops below 20KB
¡¤ Connection Activity Monitoring: Resources freed when idle

---

Factory Reset

Methods

1. Web Interface:
   ¡¤ Navigate to System page
   ¡¤ Click "Factory Reset" button
   ¡¤ Confirm the action
   ¡¤ All settings restored to defaults
2. Hardware Button:
   ¡¤ Hold BOOT button (GPIO 0) for 5 seconds
   ¡¤ System clears all NVS data
   ¡¤ Reinitializes with default settings
   ¡¤ No restart required

Reset Behavior

¡¤ Clears all preferences in NVS
¡¤ Reinitializes default GPIO configuration
¡¤ Restores default system configuration
¡¤ Resets all relay configurations
¡¤ Reinitializes WiFi (reconnects to saved network if available)
¡¤ Restarts AP with default settings
¡¤ Performs service verification

---

Security Considerations

Network Security

¡¤ AP Password: Minimum 8 characters recommended
¡¤ Hidden SSID: Option to not broadcast AP name
¡¤ Channel Selection: Choose less congested channels
¡¤ No Open Ports: Only HTTP on port 80 and DNS on port 53

Access Control

¡¤ No Authentication: Web interface has no login
¡¤ Local Network Only: Accessible only on local WiFi
¡¤ AP Isolation: WiFi clients can't communicate directly

Recommendations

¡¤ Change default AP password immediately
¡¤ Use strong passwords for both AP and STA
¡¤ Place device on isolated network segment
¡¤ Consider disabling STA mode if internet not needed
¡¤ Update firmware regularly

---

Troubleshooting

Common Issues

WiFi Won't Connect

1. Verify SSID and password are correct
2. Check if network is 2.4GHz (ESP32 doesn't support 5GHz)
3. Use WiFi Scan to find available networks
4. Check RSSI signal strength
5. Try disabling and re-enabling STA mode

Time Not Synchronizing

1. Ensure WiFi is connected
2. Check NTP server is reachable
3. Try "Sync Browser" as alternative
4. Verify GMT offset is correct
5. Check if DS3231 RTC is installed

Relay Not Responding

1. Check GPIO pin assignment in GPIO page
2. Verify active level (LOW vs HIGH)
3. Check if relay is in Manual mode
4. Verify schedule is enabled and correct
5. Check physical wiring connections

Web Interface Not Loading

1. Verify connected to correct WiFi
2. Check IP address (192.168.4.1 for AP, or STA IP)
3. Try mDNS: http://esp32.local
4. Clear browser cache
5. Restart ESP32

Settings Not Saving

1. Check NVS partition has space
2. Try factory reset and reconfigure
3. Verify settings are within valid ranges
4. Check for NVS corruption in serial output

Diagnostic Information

The System page provides:

¡¤ Free Heap Memory: Should be above 50KB for stability
¡¤ Uptime: System running time
¡¤ WiFi RSSI: Signal strength
¡¤ Time Source: Current time source
¡¤ NTP Sync Age: Time since last NTP sync
¡¤ RTC Status: DS3231 presence and sync status

Serial Debug Output

Enable Serial Monitor (115200 baud) for:

¡¤ Boot sequence information
¡¤ WiFi connection status
¡¤ NTP sync attempts
¡¤ Error messages
¡¤ Memory statistics

---

Technical Specifications

Performance

Component Interval
Schedule Processing Every 250ms
Schedule Cache Update Every 1 second
Relay Output Update Every 500ms (debounced)
WiFi Check Interval Every 10 seconds
NTP Retry Interval Every 30 seconds
RTC Rebase Every 5 minutes
Memory Cleanup (stale) Every 30 seconds
Memory Cleanup (full) Every 1 hour

Limitations

¡¤ Maximum Relays: 16
¡¤ Schedules Per Relay: 8
¡¤ WiFi Channels: 2.4GHz only (1-13)
¡¤ AP Clients: 4 maximum (ESP32 hardware limit)
¡¤ NVS Storage: Limited by ESP32 partition size
¡¤ Year Range: 2020 to 2106

Power Consumption

Component Consumption
ESP32 Active ~240mA (WiFi + Bluetooth active)
ESP32 Modem Sleep ~20mA (WiFi connected, CPU idle)
Relay Active ~70mA per relay (typical 5V relay)
DS3231 RTC ~200¦ÌA (battery-backed)
Total Maximum ~1.3A (all relays active + ESP32)

---

License

GPLv3 - GNU General Public License v3.0

Author

Raff Alds

¡¤ GitHub: https://www.github.com/xiv3r

---

Version History

Version Date Changes
v9 Current Enhanced scheduling, DS3231 support, self-healing, browser sync, captive portal
v8 Previous GPIO configuration, global active mode, critical state persistence
v7 Earlier WiFi improvements, NTP fallback, response caching
v6 Earlier mDNS support, AP configuration, memory management
v1-5 Initial Core relay control, scheduling, web interface

---

Support

For issues, feature requests, or contributions:

¡¤ Open an issue on GitHub
¡¤ Check existing documentation and troubleshooting guide
¡¤ Provide serial debug output when reporting issues
¡¤ Include hardware configuration details

```