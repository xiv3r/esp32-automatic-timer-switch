# Requirements
- ESP32 38P Pins
- 1 -> 16 Channel 5V Relay
- Female Dupont Wire
- Home Wifi for NTP/RTC sync
- 5v 5-8a Power supply
  
`Optional`
- 5v UPS (Maintain Power and Timer)

# Arduino Libraries
- ArduinoJson
- Preferences
- NTPClient
- RTClib 1.14.1

# Installation
- Download the [flasher](https://github.com/xiv3r/esp32-automatic-timer-switch/releases/tag/flasher) and firmware and then flash.

- offset address
```
bootloader: 0x1000
partition: 0x8000
firmware: 0x10000
```

# WiFi Key
- WiFi SSID: `ESP32_16CH_Timer_Switch`
- Password: `ESP32-admin`

# Activation
- Go to `wifi settings` and connect to your home wifi after the NTP is synchronized everything will work

# Relay Name
- Double click relay name to edit

# Access
° Direct Access
- mDNS:`esp32-16ch-timer.local`
- Captive Portal: Auto redirect
- Gateway:`192.168.4.1`
- WAN:`192.168.1.123`
  
° Global:`Enable Port Forwarding on your router to access anywhere`

# 16 Channel GPIO Connection 
```
RELAY    ESP32 38P
VCC  _____ 5VIN 
IN1  _____ 15
IN2  _____ 2
IN3  _____ 4
IN4  _____ 5
IN5  _____ 18
IN6  _____ 3
IN7  _____ 1
IN8  _____ 23
IN9  _____ 13
IN10 _____ 12
IN11 _____ 14
IN12 _____ 27
IN13 _____ 26
IN14 _____ 25
IN15 _____ 33
IN16 _____ 32
GND  _____ GND
```

<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/libraries/pic1.png">
<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/libraries/pic2.png">

<details><summary>
  
# Full Features
</summary>

```
ESP32 16-Channel Relay Smart Switch — Complete Feature Documentation

Table of Contents

1. Overview
2. Hardware Support
3. GPIO Management System
4. Relay Control System
5. Scheduling Engine
6. Time Management
7. WiFi Connectivity
8. Access Point & Captive Portal
9. Web Interface
10. API Reference
11. mDNS Service Discovery
12. Persistent Storage
13. Factory Reset Mechanisms
14. Performance Optimizations
15. Security Features
16. Diagnostics & Monitoring
17. Configuration Migration

---

Overview

Author: github.com/xiv3r
Platform: ESP32 (tested on ESP32-38P)
Framework: Arduino
License: Open source

A comprehensive smart switch controller capable of managing up to 16 relays with independent scheduling, multiple time sources, WiFi connectivity, and a full-featured web-based management interface.

---

Hardware Support

Supported ESP32 Boards

· ESP32 (all variants with sufficient GPIO)
· ESP32-38P (primary test platform)
· Any ESP32 with accessible GPIO pins

GPIO Pin Considerations

· Reserved pins automatically excluded: Flash pins, PSRAM pins
· Available GPIO pool: 0, 2, 3, 4, 5, 12, 13, 14, 15, 18, 23, 25, 26, 27, 32, 33
· UART pins available for relay use: GPIO 1 (TX), GPIO 3 (RX)

Default Pin Mapping

Relay GPIO Relay GPIO
IN1 15 IN9 13
IN2 2 IN10 12
IN3 4 IN11 14
IN4 5 IN12 27
IN5 18 IN13 26
IN6 3 IN14 25
IN7 1 IN15 33
IN8 23 IN16 32

Boot Button

· Pin: GPIO 0
· Pull-up: Internal pull-up enabled
· Function: Hardware factory reset trigger

---

GPIO Management System

Dynamic Pin Configuration

· Runtime GPIO assignment via web UI — add or remove relays without recompiling
· Per-relay active level control — independently set each relay as active-HIGH or active-LOW
· Persistent storage in NVS with magic number validation (0xD002)
· Maximum 16 relays enforced in software
· Pin conflict detection prevents duplicate GPIO assignments

GPIO Operations

Operation Description
Add Relay Assign a new GPIO pin to the next available relay slot
Delete Relay Remove a relay, shifting subsequent relays to fill the gap
Toggle Active Low Switch between active-HIGH and active-LOW logic per relay
Reset Defaults Restore all 16 default pin assignments

GPIO Configuration Structure

struct GPIOPinConfig {
    uint8_t pins[MAX_RELAYS];      // GPIO pin numbers
    uint8_t count;                  // Number of active relays
    uint16_t magic;                 // Validation magic (0xD002)
    bool activeLow[MAX_RELAYS];     // Active-low flag per relay
};

---

Relay Control System

Control Modes

Mode Description Priority
Automatic Relay state determined by schedule engine Normal
Manual ON Force relay ON regardless of schedule Override
Manual OFF Force relay OFF regardless of schedule Override

Relay State Management

· Output caching — GPIO only written when state changes, minimizing toggling
· Initialization state — all relays default to OFF on boot
· Active-low handling — transparent logic inversion in setRelayOutput()
· Per-relay naming — user-assignable names up to 15 characters

Relay Configuration Structure

struct RelayConfig {
    TimerSchedule schedule;      // 8 independent schedules
    bool manualOverride;         // Manual mode active flag
    bool manualState;            // Manual target state (ON/OFF)
    char name[16];               // User-assigned relay name
};

---

Scheduling Engine

Schedule Capacity

· 8 independent schedules per relay (128 total across 16 relays)
· Per-schedule enable/disable toggle
· Visual indicators in web UI for overnight schedules and always-ON schedules

Time Components

Component Range Description
Start Time 00:00:00 – 23:59:59 Schedule activation time
Stop Time 00:00:00 – 23:59:59 Schedule deactivation time
Days of Week Bitmask (0x00–0x7F) Which days schedule is active
Days of Month Bitmask (0x00000000–0xFFFFFFFF) Which month days schedule is active

Day of Week Constants

Constant Value Description
DAY_SUNDAY 0x01 Sunday only
DAY_MONDAY 0x02 Monday only
DAY_TUESDAY 0x04 Tuesday only
DAY_WEDNESDAY 0x08 Wednesday only
DAY_THURSDAY 0x10 Thursday only
DAY_FRIDAY 0x20 Friday only
DAY_SATURDAY 0x40 Saturday only
DAY_ALL 0x7F Every day
DAY_WEEKDAYS 0x3E Monday–Friday
DAY_WEEKENDS 0x41 Sunday + Saturday

Schedule Types

Type Condition Behavior
Always ON Start time = Stop time Relay ON entire day when schedule active
Day Schedule Start < Stop ON between start and stop (e.g., 08:00–17:00)
Overnight Schedule Start > Stop ON from start to midnight AND midnight to stop (e.g., 22:00–06:00)

Schedule Processing

· Processing interval: 250ms (non-blocking)
· Cache interval: 1000ms (reduces CPU load)
· Pre-calculation: Determines which relays have any active schedule for today
· Priority: Manual override always takes precedence over schedules
· Month-day filtering: Schedules with month-day masks only activate on specified days

Schedule Engine Optimization

// Cached values to avoid repeated calculations
static uint8_t  cachedTodayBit = 0;        // Today's day-of-week bit
static int      cachedMonthDay = 0;         // Today's month day (1-31)
static time_t   lastScheduleEpoch = 0;      // Last epoch when cache was updated
static bool     scheduleActiveCache[16];    // Pre-calculated active status per relay

---

Time Management

Time Sources (Priority Order)

Source Accuracy Indicator Fallback Behavior
NTP High (milliseconds) Green dot Primary source; auto-retries on failure
Browser Sync Moderate (seconds) Blue dot Used when NTP unavailable; auto-overridden
None None Yellow dot System waits for valid time source

NTP Client Features

· Server pool with automatic fallback:
  1. ph.pool.ntp.org (default, Asia-Pacific)
  2. pool.ntp.org (global)
  3. time.nist.gov (US government)
  4. time.google.com (Google)
· Automatic server rotation on sync failure
· Configurable sync interval: 1–24 hours (default: 1 hour)
· Retry interval: 30 seconds on failure
· Force sync button in web UI
· Fallback protection: Does not sync if WiFi is disconnected

Internal RTC System

// Precision timekeeping variables
time_t        internalEpoch = 0;              // UTC epoch at last sync
unsigned long rtcMicrosAtLastSync = 0;        // Microsecond timestamp of sync
float         driftCompensation = 1.0f;       // Corrects for ESP32 clock drift

Drift Compensation Algorithm

· Measurement: Compares elapsed micros to actual NTP time difference
· History buffer: Maintains 4-sample rolling average
· Smoothing: Exponential moving average (80% old, 20% new)
· Bounds: Clamped to 0.95–1.05 (±5% correction range)
· Minimum interval: Only calculates after 60-second intervals

RTC Persistence

· NVS storage: Saves epoch + drift on every sync
· Boot recovery: Restores last known time on restart
· Validation: Only accepts epochs between 2020–2100

Browser Time Sync

· Trigger: User clicks "Sync from Browser" button
· Method: JavaScript Math.floor(Date.now() / 1000) for UTC epoch
· Validation: Rejects timestamps outside 2020–2100
· Override: NTP automatically overrides browser time when available
· Tracking: Separate lastBrowserSync timestamp for UI display

Time Configuration

Setting Range Default Description
NTP Server String (48 chars) ph.pool.ntp.org Primary NTP server
GMT Offset ±43200 seconds 28800 (UTC+8) Timezone offset from UTC
DST Offset 0–7200 seconds 0 Daylight saving adjustment
Sync Interval 1–24 hours 1 Auto-sync frequency

Time Display

· Live clock in header (updates every 1 second)
· UTC epoch display on System page
· Sync age indicators showing time since last NTP and browser sync
· Time source label with color-coded status

---

WiFi Connectivity

Station Mode

Feature Specification
WiFi Standard 802.11 b/g/n (2.4 GHz)
Security Open, WPA, WPA2
IP Assignment DHCP
SSID Length Up to 31 characters
Password Length Up to 63 characters

Connection Management

· Non-blocking state machine — does not freeze during connection attempts
· Connection timeout: 20 seconds per attempt
· Maximum reconnect attempts: 10
· Backoff strategy: After 10 failures, pauses for 5 minutes
· First-attempt flag: Allows immediate retry on initial boot without backoff
· WiFi check interval: 10 seconds (status polling)

Connection States

bool wifiConnected = false;        // Currently connected to station
bool wifiConnecting = false;       // Connection attempt in progress
unsigned long wifiConnectStart;    // Millis when connection started
uint8_t wifiReconnectAttempts;     // Consecutive failure count
unsigned long wifiGiveUpUntil;     // Backoff timeout timestamp

WiFi Scanning

· Async scanning — non-blocking, results polled via API
· Scan timeout: 10 seconds
· Results include:
  · SSID (network name)
  · RSSI (signal strength in dBm)
  · Encryption type (open vs secured)
· Visual display: Signal strength bars (1–4 bars based on RSSI)
· Click-to-fill: Click a network in scan results to populate SSID field

Signal Strength Visualization

RSSI Range Bars Quality
≥ -50 dBm ▂▄▆█ 4 bars Excellent
-60 to -51 ▂▄▆ 3 bars Good
-70 to -61 ▂▄ 2 bars Fair
< -70 ▂ 1 bar Weak

WiFi Handling in UI

// RSSI bar HTML generation
function rssiBar(rssi){
  const b = rssi >= -50 ? 4 : rssi >= -60 ? 3 : rssi >= -70 ? 2 : 1;
  // Returns color-coded bar visualization
}

---

Access Point & Captive Portal

AP Specifications

Feature Specification
Mode Simultaneous AP + STA
Default SSID ESP32_16CH_Timer_Switch
Default Password ESP32-admin
Default Channel 6
Security Open or WPA2 (8+ chars)
Max Clients 4 (ESP32 hardware limit)
Hidden SSID Configurable (broadcast on/off)
DNS Server Built-in, redirects all queries to portal

Captive Portal Implementation

Intercepts the following platform-specific connectivity check endpoints:

Endpoint Platform Response
/generate_204 Android Redirect to AP IP
/hotspot-detect.html Apple iOS/macOS Success page
/library/test/success.html Apple (alternate) Success page
/success.txt Various Plain text "success"
/canonical.html Various Redirect to AP IP
/connecttest.txt Microsoft Windows "Microsoft Connect Test"
/ncsi.txt Microsoft NCSI "Microsoft NCSI"
/redirect Generic Redirect to AP IP
* (all other) Catch-all Redirect to AP IP

AP Configuration Options

Setting Range Default Description
SSID 1–31 chars ESP32_16CH_Timer_Switch Access point name
Password 0 or 8–31 chars ESP32-admin AP security key
Channel 1–13 6 WiFi channel
Hidden true/false false SSID broadcast control

AP Restart Behavior

· Triggered on save — automatically restarts AP with new settings
· Disconnects all clients — warning displayed in UI
· mDNS restart scheduled 2 seconds after AP restart
· UI auto-refresh after 4 seconds

---

Web Interface

Architecture

· Single-page application (SPA) design
· No external dependencies — all HTML, CSS, JS inline
· PROGMEM storage for all page templates and CSS
· Responsive design — adapts to mobile and desktop

Navigation Tabs

Tab Route Icon Function
Relays / ⚡ Relay control, manual override, schedule editing
WiFi /wifi - Station network settings & scan
Time /ntp - NTP config, timezone, sync
AP /ap - Access point settings
GPIO /gpio - Pin assignment & active-level config
System /system - Status dashboard & device control

Header Bar (All Pages)

┌─────────────────────────────────────────────────────────┐
│ ⚡ ESP32 16-CH │ Relays WiFi Time AP GPIO System │ ● ● 12:34:56 │
└─────────────────────────────────────────────────────────┘
                                                 │   │
                                        WiFi dot─┘   └─Time source dot
                                        (green=connected   (green=NTP
                                         red=disconnected)  blue=Browser
                                                             yellow=none)

Design System

/* Color Palette */
Primary:     #1565C0 (Blue)
Primary Dark:#0D47A1
Success:     #43A047 (Green), #2E7D32
Danger:      #E53935 (Red), #C62828, #B71C1C
Warning:     #F9A825 (Amber), #FB8C00 (Orange)
Info:        #0288D1, #42A5F5
Neutral:     #546E7A, #607D8B, #90A4AE, #CFD8DC
Background:  #EEF2F7
Card:        #FFFFFF

/* Typography */
Font:     -apple-system, BlinkMacSystemFont, Segoe UI, Roboto, Arial
Base:     14px
Headers:  15-17px
Labels:   11-12px uppercase
Mono:     monospace (time inputs)

/* Spacing */
Card gap:    14px
Padding:     16px cards, 10px header
Border radius: 7-10px

UI Components

Cards

.card {
  background: #fff;
  border-radius: 10px;
  box-shadow: 0 2px 8px rgba(0,0,0,.08);
  padding: 16px;
  transition: box-shadow .2s;
}
.card:hover {
  box-shadow: 0 4px 18px rgba(0,0,0,.13);  /* Elevation effect */
}

Buttons

Class Color Use Case
.bon-b Green ON action, save
.boff-b Red OFF action, delete
.baut Gray Auto/reset
.bsave Blue Save/submit
.bsync Orange Sync actions
.bdanger Dark red Factory reset
.bwarn Amber Warning actions
.bscan Blue WiFi scan

Badges

Class Background Text Meaning
.bon #E8F5E9 #2E7D32 Relay ON
.boff #FFEBEE #C62828 Relay OFF
.bman #FFF3E0 #E65100 Manual override

Schedule Items

· Active schedule: Blue border, light blue background
· Inactive schedule: Gray border, white background
· Overnight badge: Purple text on light purple background
· Always ON badge: Green text on light green background

Toast Notifications

┌──────────────────────────────────┐
│ ✓ Settings saved!                │  ← Green (success)
└──────────────────────────────────┘
┌──────────────────────────────────┐
│ ✗ Connection failed              │  ← Red (error)
└──────────────────────────────────┘

· Position: Fixed bottom center
· Animation: Slide up from below
· Duration: 3 seconds auto-dismiss
· Stacking: Single toast, replaces previous

Relay Page Features

· Grid layout: Auto-filling columns (min 340px)
· Per-relay card:
  · Clickable name (double-click to rename inline)
  · Status badge (ON/OFF/MANUAL)
  · ON/OFF/Auto buttons
  · Expandable schedule list (8 slots)
· Schedule editor:
  · Enable/disable checkbox per slot
  · Start/stop time inputs (with seconds)
  · Day-of-week selector (7 clickable day buttons)
  · Day-of-month selector (31 clickable day buttons)
  · Auto-updating overnight/always-ON badge
· Save button per relay (saves all 8 schedules at once)

WiFi Page Features

· Current connection status with signal bars
· SSID input with integrated scan button
· Password input
· Scan results list (clickable to fill SSID)
· Save & Connect button

Time Page Features

· Current time source display
· NTP server input with fallback documentation
· GMT offset input (seconds)
· DST offset input (seconds)
· Sync interval input (hours)
· Sync Now button (NTP)
· Sync from Browser button (fallback)

AP Page Features

· Warning about client disconnection
· AP SSID input
· AP password input (min 8 or blank)
· Channel dropdown (1–13)
· SSID visibility dropdown (visible/hidden)
· Save & Restart AP button

GPIO Page Features

· Current relay count display
· Add relay dropdown (shows only available pins)
· Per-relay listing with:
  · Relay number
  · GPIO pin
  · Current active level (LOW/HIGH)
  · Toggle active-level button
  · Remove button
· Reset to Default button
· Maximum 16 relays indicator

System Page Features

Status Dashboard (2-column grid)

Metric Description
STA IP Station IP or "(not connected)"
AP IP Access point IP
Free Heap Available RAM in KB
Uptime Formatted as Xh Ym Zs
WiFi RSSI Signal strength with quality label
Time Source Color-coded: NTP/Browser/None
UTC Epoch Raw Unix timestamp
NTP Last Sync Relative time ("3 min ago")
Last Browser Sync Relative time
NTP Server Currently active server
Chip Model ESP32 variant
mDNS Hostname .local address
Relay Count X / 16
GMT Offset Formatted as +8.0h (UTC+8.0)

Device Control Buttons

· Restart Device — Software reboot
· Factory Reset — Clear all settings
· Boot button instructions — Hold GPIO 0 for 5 seconds

---

API Reference

Response Format

All API responses use application/json content type.

Relay Endpoints

GET /api/relays

Returns array of all relay configurations and current states.

Response:

[
  {
    "state": true,
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

POST /api/relay/manual

Set manual override ON or OFF.

Request:

{
  "relay": 0,
  "state": true
}

Response: {"success": true} or {"success": false, "error": "Invalid relay"}

POST /api/relay/reset

Clear manual override, return to automatic schedule control.

Request:

{
  "relay": 0
}

POST /api/relay/save

Save all 8 schedules for a specific relay.

Request:

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

POST /api/relay/name

Rename a relay (max 15 characters).

Request:

{
  "relay": 0,
  "name": "Pump"
}

Time Endpoints

GET /api/time

Get current time and source status.

Response:

{
  "time": "12:34:56",
  "wifi": true,
  "ntp": true,
  "timeSource": "ntp"
}

POST /api/time/browser-sync

Sync internal RTC using browser's UTC time.

Request:

{
  "utc_epoch": 1700000000
}

Response:

{
  "success": true,
  "utc_epoch": 1700000000,
  "local_time": "2023-11-15 04:26:40",
  "gmt_offset": 28800,
  "time_source": "browser"
}

Validation: Epoch must be between 1577836800 (2020-01-01) and 4102444800 (2100-01-01).

WiFi Endpoints

GET /api/wifi

Get current WiFi station configuration and status.

Response:

{
  "ssid": "MyNetwork",
  "connected": true,
  "ip": "192.168.1.100",
  "rssi": -45
}

POST /api/wifi

Save WiFi credentials and initiate connection.

Request:

{
  "ssid": "MyNetwork",
  "password": "MyPassword"
}

POST /api/wifi/scan

Start async WiFi scan. Returns 202 Accepted.

Response: {"scanning": true} or {"scanning": false, "error": "WiFi busy"}

GET /api/wifi/scan

Poll scan results.

Response (scanning):

{
  "scanning": true
}

Response (complete):

{
  "scanning": false,
  "networks": [
    {
      "ssid": "MyNetwork",
      "rssi": -45,
      "enc": true
    }
  ]
}

NTP Endpoints

GET /api/ntp

Get NTP configuration.

Response:

{
  "ntpServer": "ph.pool.ntp.org",
  "gmtOffset": 28800,
  "daylightOffset": 0,
  "syncHours": 1
}

POST /api/ntp

Save NTP settings.

Request:

{
  "ntpServer": "pool.ntp.org",
  "gmtOffset": 28800,
  "daylightOffset": 3600,
  "syncHours": 6
}

POST /api/ntp/sync

Force immediate NTP synchronization.

Response: {"success": true} or {"success": false, "error": "WiFi not connected"}

AP Endpoints

GET /api/ap

Get access point configuration.

Response:

{
  "ap_ssid": "ESP32_16CH_Timer_Switch",
  "ap_password": "ESP32-admin",
  "ap_channel": 6,
  "ap_hidden": false
}

POST /api/ap

Save AP configuration and restart AP.

Request:

{
  "ap_ssid": "MyRelayAP",
  "ap_password": "MyPassword",
  "ap_channel": 11,
  "ap_hidden": true
}

GPIO Endpoints

GET /api/gpio

Get GPIO pin configuration and available pins.

Response:

{
  "count": 16,
  "maxRelays": 16,
  "pins": [15, 2, 4, 5, 18, 3, 1, 23, 13, 12, 14, 27, 26, 25, 33, 32],
  "activeLow": [true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true],
  "availablePins": [0, 2, 3, 4, 5, 12, 13, 14, 15, 18, 23, 25, 26, 27, 32, 33]
}

POST /api/gpio/save

Set all GPIO pins at once (reset defaults).

Request:

{
  "pins": [15, 2, 4, 5, 18, 3, 1, 23, 13, 12, 14, 27, 26, 25, 33, 32]
}

POST /api/gpio/add

Add a single relay pin.

Request:

{
  "pin": 19
}

Error conditions: Maximum relays reached, pin already in use.

POST /api/gpio/delete

Remove a relay at specified index.

Request:

{
  "index": 5
}

Behavior: Shifts all subsequent relays to fill gap.

POST /api/gpio/toggle-active-low

Toggle active-low setting for a relay.

Request:

{
  "index": 0
}

Response: {"success": true, "activeLow": false}

mDNS Endpoints

GET /api/mdns

Get mDNS status.

Response:

{
  "hostname": "esp32-16ch-relay",
  "started": true,
  "url": "http://esp32-16ch-relay.local"
}

POST /api/mdns

Set mDNS hostname.

Request:

{
  "hostname": "my-relay-controller"
}

POST /api/mdns/restart

Force mDNS service restart.

Response: {"success": true}

System Endpoints

GET /api/system

Get comprehensive system status.

Response:

{
  "ip": "192.168.1.100",
  "ap_ip": "192.168.4.1",
  "uptime": 3600,
  "freeHeap": 200000,
  "utcEpoch": 1700000000,
  "timeSource": "NTP",
  "ntpServer": "ph.pool.ntp.org",
  "ntpSyncAge": 120,
  "browserSyncAge": -1,
  "wifiConnected": true,
  "wifiSSID": "MyNetwork",
  "rssi": -45,
  "version": 6,
  "chipModel": "ESP32-38P",
  "mdnsHostname": "esp32-16ch-relay",
  "mdnsStarted": true,
  "relayCount": 16,
  "maxRelays": 16,
  "gmtOffset": 28800
}

POST /api/reset

Software restart device.

Response: {"success": true} (then device restarts)

POST /api/factory-reset

Clear all NVS storage and restart.

Response: {"success": true} (then device restarts with defaults)

Captive Portal Endpoints

Endpoint Method Response
/generate_204 GET 302 Redirect to AP IP
/hotspot-detect.html GET 200 HTML "Success"
/library/test/success.html GET 200 HTML "Success"
/success.txt GET 200 Text "success\n"
/canonical.html GET 302 Redirect
/connecttest.txt GET 200 Text "Microsoft Connect Test"
/ncsi.txt GET 200 Text "Microsoft NCSI"
/redirect GET 302 Redirect
* (catch-all) GET 302 Redirect to AP IP

---

mDNS Service Discovery

Service Advertisement

Property Value
Service Type _http._tcp
Port 80
TXT: model ESP32-16CH-Relay
TXT: version v8
TXT: channels Dynamic (current relay count)

Hostname Generation

· Default: esp32-16ch-relay
· Auto-generated: Sanitized version of AP SSID on first mDNS start
· Sanitization: Lowercase, spaces/hyphens → hyphens, non-alphanumeric removed
· Length limit: 31 characters
· Persistence: Stored in mdnsHostname variable (in-memory, not NVS)

mDNS Lifecycle

1. Start: On boot after WiFi initialization
2. Restart: 2-second delayed restart after AP configuration changes
3. Stop: Clean MDNS.end() before restart
4. Service update: TXT record updated with current relay count

---

Persistent Storage

NVS (Non-Volatile Storage) Organization

Namespace: "relay16"
├── sysConfig (SystemConfig)     — WiFi, NTP, AP, RTC settings
├── relayConfigs (RelayConfig[16]) — All relay schedules and names
├── gpioConfig (GPIOPinConfig)   — Pin mappings and active levels
└── extConfig (ExtConfig)        — Extended configuration

System Configuration Structure

struct SystemConfig {
    uint16_t magic;              // Validation: 0x1234
    uint8_t  version;            // Current: 6
    char     sta_ssid[32];       // WiFi station SSID
    char     sta_password[64];   // WiFi station password
    char     ap_ssid[32];        // Access point SSID
    char     ap_password[32];    // Access point password
    char     ntp_server[48];     // NTP server hostname
    int32_t  gmt_offset;         // GMT offset in seconds
    int32_t  daylight_offset;    // DST offset in seconds
    time_t   last_rtc_epoch;     // Saved RTC epoch
    float    rtc_drift;          // Drift compensation factor
    char     hostname[32];       // Device hostname
} __attribute__((packed));

Extended Configuration Structure

struct ExtConfig {
    uint8_t magic;               // Validation: 0xEC
    uint8_t ap_channel;          // AP WiFi channel (1-13)
    uint8_t ntp_sync_hours;      // NTP sync interval (1-24)
    uint8_t ap_hidden;           // AP SSID hidden flag
    uint8_t reserved[28];        // Future expansion
} __attribute__((packed));

GPIO Configuration Structure

struct GPIOPinConfig {
    uint8_t pins[16];            // GPIO pin assignments
    uint8_t count;               // Active relay count
    uint16_t magic;              // Validation: 0xD002
    bool activeLow[16];          // Per-relay active-low flags
};

Data Validation

· Magic number check on every load
· Corrupt data detection — auto-initializes defaults if invalid
· Version migration — handles upgrades from v3/v4/v5 to v6
· Size validation — checks if stored data matches expected struct size

Save Triggers

Event What's Saved
Schedule change relayConfigs
Relay rename relayConfigs
Manual override relayConfigs
WiFi settings change sysConfig
NTP settings change sysConfig, extConfig
AP settings change sysConfig, extConfig
GPIO config change gpioConfig, relayConfigs
NTP sync sysConfig (RTC state)
Browser time sync sysConfig (RTC state)

---

Factory Reset Mechanisms

Software Factory Reset

1. Trigger: System page → "Factory Reset" button
2. Confirmation: Browser confirm() dialog
3. Action: Clears entire relay16 NVS namespace
4. Result: ESP32 restarts with all defaults

Hardware Factory Reset

1. Trigger: Hold BOOT button (GPIO 0) for 5+ seconds
2. Detection: Continuous polling in loop() via checkBootButton()
3. Debounce: Rising/falling edge detection
4. Action: Clears NVS and restarts
5. Flag: factoryResetTriggered prevents multiple triggers

void checkBootButton() {
    bool buttonState = !digitalRead(BOOT_BUTTON_PIN);  // Active low
    
    if (buttonState && !bootButtonPressed) {
        bootButtonPressed = true;
        bootButtonPressStart = millis();
    }
    else if (!buttonState && bootButtonPressed) {
        bootButtonPressed = false;  // Released before timeout
    }
    
    if (bootButtonPressed && !factoryResetTriggered && 
        (millis() - bootButtonPressStart >= 5000)) {
        factoryResetTriggered = true;
        preferences.begin(NVS_NAMESPACE, false);
        preferences.clear();
        preferences.end();
        ESP.restart();
    }
}

Default Values After Reset

Setting Default Value
AP SSID ESP32_16CH_Timer_Switch
AP Password ESP32-admin
AP Channel 6
AP Hidden false
NTP Server ph.pool.ntp.org
GMT Offset 28800 (UTC+8)
DST Offset 0
NTP Sync Interval 1 hour
WiFi STA SSID (empty)
WiFi STA Password (empty)
Relay Names "Relay 1" through "Relay 16"
Relay Schedules All disabled, all days
GPIO Pins Default 16-pin mapping
Active Level All active-LOW
RTC Epoch 0 (uninitialized)
RTC Drift 1.0 (no compensation)
Time Source None

---

Performance Optimizations

Memory Optimization

Technique Implementation
PROGMEM storage HTML pages, CSS stored in flash, not RAM
StaticJsonDocument Bounded size (256–24576 bytes depending on endpoint)
Inline functions getRelayPin(), setRelayOutput(), isActiveLow()
Packed structs __attribute__((packed)) on SystemConfig
Fixed-size buffers No dynamic allocation in hot paths

CPU Optimization

Technique Implementation
Schedule caching Only recalculated every 1000ms
Relay state caching GPIO only written on actual state change
Non-blocking WiFi State machine, no delay() calls
Async WiFi scan Background scan with polling
Yield calls yield() in loops to prevent watchdog triggers
RTC microsecond precision Avoids frequent NTP calls

Timing Constants

NTP_RETRY_INTERVAL         = 30000ms   // 30 seconds between retries
WIFI_CHECK_INTERVAL        = 10000ms   // 10 seconds status polling
WIFI_CONNECT_TIMEOUT       = 20000ms   // 20 seconds per attempt
RTC_UPDATE_INTERVAL        = 100ms     // (defined but not enforced)
SCHEDULE_PROCESS_INTERVAL  = 250ms     // Relay state evaluation
RELAY_UPDATE_INTERVAL      = 500ms     // (defined but not enforced)
SCHEDULE_CACHE_INTERVAL    = 1000ms    // Day/date cache refresh
MDNS_RESTART_DELAY         = 2000ms    // After AP changes

---

Security Features

Input Validation

Input Validation
SSID Length 1–31 characters
Password Length 0 or 8–63 characters
NTP Server Length 1–47 characters
AP SSID Length 1–31 characters
AP Password Length 0 or 8–31 characters
Relay Index 0 ≤ index < gpioConfig.count
GPIO Pin Must be in available pins list
Relay Name Truncated to 15 characters
Epoch Time 1577836800 ≤ epoch ≤ 4102444800
NTP Sync Hours 1–24
AP Channel 1–13
JSON Parse DeserializationError checked on all POST

WiFi Security

· Password masking — Password input type in UI
· AP password in response — Included in API response (for user convenience; displayed as masked field)
· Open network support — AP can run without password (blank field)

---

Diagnostics & Monitoring

System Page Metrics

Metric Update Frequency Source
Time display 1 second getCurrentEpoch()
System info 5 seconds /api/system polling
Free heap 5 seconds ESP.getFreeHeap()
Uptime 5 seconds millis() / 1000
WiFi RSSI 5 seconds WiFi.RSSI()
NTP sync age 5 seconds (millis() - lastNTPSync) / 1000
Browser sync age 5 seconds (millis() - lastBrowserSync) / 1000

Header Status Dots

Dot Color Meaning
WiFi (left) Green Connected to station
WiFi (left) Red Disconnected
Time (right) Green NTP synced
Time (right) Blue Browser synced
Time (right) Yellow No time source

Uptime Formatting

< 1 hour:    "Xm Ys"
1-24 hours:  "Xh Ym Zs"
> 24 hours:  "Xd Xh Ym"

Sync Age Formatting

< 0:          "Never"
< 60s:        "Just now"
1-59 min:     "X min ago"
> 60 min:     "Xh Ym ago"

RSSI Quality Labels

RSSI Range Quality
≥ -50 dBm Excellent
-60 to -51 Good
-70 to -61 Fair
< -70 Weak

---

Configuration Migration

Version History

Version Changes
v3 Original format (no monthDays, no separate days field)
v4 Added days-of-week field
v5 Added relay names
v6 Added monthDays (day-of-month scheduling)

Migration Logic

// In loadConfiguration():
if (len == v4Size) {
    // Upgrade from v4: initialize monthDays to 0
    for (int i = 0; i < 16; i++)
        for (int s = 0; s < 8; s++)
            relayConfigs[i].schedule.monthDays[s] = 0;
}
else if (len == v3Size) {
    // Upgrade from v3: initialize days and monthDays
    for (int i = 0; i < 16; i++)
        for (int s = 0; s < 8; s++) {
            relayConfigs[i].schedule.days[s] = DAY_ALL;
            relayConfigs[i].schedule.monthDays[s] = 0;
        }
}

GPIO Config Migration

// In loadGPIOConfig():
if (len == sizeof(GPIOPinConfig) - (16 * sizeof(bool)) && magic == 0xD001) {
    // Upgrade from v1 (no activeLow): set all to true
    for (int i = 0; i < count; i++)
        gpioConfig.activeLow[i] = true;
    gpioConfig.magic = GPIO_CONFIG_MAGIC;  // Update to 0xD002
}

---

Quick Reference Card

Default IP Addresses

Interface IP Address
Access Point 192.168.4.1
Station DHCP-assigned

Default Credentials

Setting Value
AP SSID ESP32_16CH_Timer_Switch
AP Password ESP32-admin
mDNS esp32-16ch-relay.local

Button Functions

Button Duration Action
BOOT (GPIO 0) < 5 seconds No action
BOOT (GPIO 0) ≥ 5 seconds Factory reset

LED Indicators

Condition Indication
ESP32 built-in LED Platform-dependent (not controlled by sketch)

First-Time Setup Flow

1. Power on device
2. Connect to ESP32_16CH_Timer_Switch WiFi (password: ESP32-admin)
3. Open 192.168.4.1 in browser
4. Configure WiFi station (WiFi tab)
5. Set timezone and NTP (Time tab)
6. (Optional) Configure AP settings (AP tab)
7. (Optional) Adjust GPIO pins (GPIO tab)
8. Create relay schedules (Relays tab)
9. Access via station IP or mDNS hostname

---
```

</details>
