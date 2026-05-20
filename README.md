## ESP8266
👉 https://github.com/xiv3r/esp8266-automatic-timer-switch

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

# GMT offset (your country time)
- ⚠️ Search your country `gmt offsets in seconds` and paste it on the Time -> GMT Offset

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

---

Overview

The ESP32 16-Channel Relay Smart Switch is a professional-grade automation controller designed for home, business, and farm automation applications. It features a robust web-based control interface, programmable schedules with day-of-week and day-of-month support, automatic time synchronization via NTP, and a self-healing architecture that ensures reliable operation.

Key Capabilities

· Control up to 16 independent relays
· 8 programmable schedules per relay (128 total schedules)
· Day-of-week (Sun-Sat) and day-of-month (1-31) scheduling
· Automatic time sync via NTP with browser fallback
· Self-healing watchdog system
· mDNS support for easy discovery (http://esp32-16ch-relay.local)
· Persistent storage using ESP32 Preferences (NVS)

---

Key Features

🎛️ Relay Control

Feature Description
Manual Override Force relays ON/OFF independent of schedules
Auto Mode Relay follows programmed schedules
Real-time Status Visual indication of relay states
Double-click Rename Customizable relay names (max 15 chars)
Active Level Toggle Support for both HIGH-active and LOW-active relays

⏰ Advanced Scheduling

Feature Description
8 Schedules/Relay Independent timing rules per relay
Day-of-Week Select any combination of days
Day-of-Month Granular control for monthly schedules
Overnight Support Schedules that cross midnight
Always ON Schedule with identical start/stop times
Visual Badges Quick identification of schedule type

🌐 Network Features

Feature Description
WiFi Station Mode Connect to existing networks
Access Point Mode Built-in configuration AP
Captive Portal Automatic redirect on connection
mDNS Discovery .local hostname resolution
WiFi Scanning Visual network selection

⏱️ Time Management

Feature Description
NTP Synchronization Automatic time updates
Multiple NTP Servers Fallback chain (4 servers)
Browser Time Sync Manual fallback when NTP unavailable
GMT Offset Configurable timezone support
DST Support Daylight saving time offset
Drift Compensation Auto-adjusts internal clock drift

🛡️ Self-Healing System

Feature Description
WiFi Recovery Automatic reconnection attempts
mDNS Restart Detects and restarts mDNS service
DNS Server Recovery Restarts captive portal DNS
Web Server Recovery Resets HTTP server if needed
Relay State Verification Periodic state confirmation
Critical State Backup Saves relay states for recovery
Memory Monitoring Heap cleanup at thresholds

---

Hardware Specifications

Default Pin Mapping (16-Channel)

Relay GPIO Pin Relay GPIO Pin
IN1 15 IN9 13
IN2 2 IN10 12
IN3 4 IN11 14
IN4 5 IN12 27
IN5 18 IN13 26
IN6 3 (RX) IN14 25
IN7 1 (TX) IN15 33
IN8 23 IN16 32

Active Level

· Default: Active LOW (relay triggers when pin is LOW)
· Configurable: Per-relay toggle via web interface

Boot Button Factory Reset

Parameter Value
Pin GPIO 0 (BOOT button)
Hold Duration 5 seconds
Action Clears all settings, factory reset

System Limits

Parameter Value
Maximum Relays 16
Schedules per Relay 8
Max Schedules Total 128
Relay Name Length 15 characters
WiFi SSID Length 31 characters
WiFi Password Length 63 characters
AP SSID Length 31 characters
AP Password Length 31 characters

---

Pin Configuration

Default Pin List (16 Relays)

[15, 2, 4, 5, 18, 3, 1, 23, 13, 12, 14, 27, 26, 25, 33, 32]


Valid GPIO Pins for ESP32

0, 2, 3, 4, 5, 12, 13, 14, 15, 18, 23, 25, 26, 27, 32, 33


Active Level Explained


Active LOW  (default): relay ON  → pin LOW
                         relay OFF → pin HIGH

Active HIGH (toggle):   relay ON  → pin HIGH
                         relay OFF → pin LOW


---

Web Interface Guide

Navigation Structure


┌─────────────────────────────────────────────────────────────┐
│  ⚡ ESP32 16-CH    [Relays] [WiFi] [Time] [AP] [GPIO] [System]         │
├─────────────────────────────────────────────────────────────┤
│                                                                        │
│  [Status Indicators]                           [Clock: HH:MM:SS]       │
│                                                                        │
└─────────────────────────────────────────────────────────────┘


Page Descriptions

Page URL Purpose
Relays / Control relays, edit schedules, rename relays
WiFi /wifi Configure station mode, scan networks
Time /ntp NTP settings, time sync, browser sync
AP /ap Access Point configuration
GPIO /gpio Pin mapping, add/remove relays, active level
System /system System info, restart, factory reset

Status Indicators

Indicator Color Meaning
WiFi Dot 🟢 Green Connected to WiFi
         🔴 Red Not connected
Time Dot 🟢 Green NTP time source
         🔵 Blue Browser time source
         🟡 Yellow No time source

---

Schedule System

Schedule Structure

Each schedule contains:

javascript
{
  "enabled": true/false,
  "startHour": 0-23,
  "startMinute": 0-59,
  "startSecond": 0-59,
  "stopHour": 0-23,
  "stopMinute": 0-59,
  "stopSecond": 0-59,
  "days": 0x00-0x7F,    // Bitmask for Sun-Sat
  "monthDays": 0x00000000-0x7FFFFFFF  // Bitmask for days 1-31
}


Day-of-Week Constants

Constant Value Days
DAY_SUNDAY 0x01 Sunday only
DAY_MONDAY 0x02 Monday only
DAY_TUESDAY 0x04 Tuesday only
DAY_WEDNESDAY 0x08 Wednesday only
DAY_THURSDAY 0x10 Thursday only
DAY_FRIDAY 0x20 Friday only
DAY_SATURDAY 0x40 Saturday only
DAY_ALL 0x7F Every day
DAY_WEEKDAYS 0x3E Mon-Fri
DAY_WEEKENDS 0x41 Sat+Sun

Schedule Logic

Normal Schedule (start < stop)


Relay ON:  [startTime] ────────────[stopTime)
Relay OFF: [─────────────────)


Overnight Schedule (start > stop)


Relay ON:  [startTime) ───────────── [stopTime)
Relay OFF: [stopTime) ───── [startTime)


Always ON Schedule (start == stop)

· Relay remains ON whenever schedule is active
· Useful for schedules without time constraints

Day-of-Month Filtering

· monthDays = 0: No filtering (all days allowed)
· monthDays = 0x00002000: Active only on 14th of month
· monthDays = 0x7FFFFFFF: All month days allowed

Priority Rules

1. Manual Override > Schedules
2. Schedules evaluated in order (0-7)
3. First matching schedule determines state
4. If no schedule matches → OFF

---

Time Management

Time Source Priority


┌─────────────────────────────────────────┐
│  1. NTP Server (highest priority)              │
│     ↓ (if unavailable)                         │
│  2. Browser Sync (manual fallback)             │
│     ↓ (if unavailable)                         │
│  3. None (time not reliable)                   │
└─────────────────────────────────────────┘


NTP Server Fallback Chain


ph.pool.ntp.org  →  pool.ntp.org  →  time.nist.gov  →  time.google.com
      ↓                   ↓                 ↓                  ↓
   [Primary]         [Secondary]       [Tertiary]        [Quaternary]


Configuration Parameters

Parameter Description Default
GMT Offset Seconds from UTC (e.g., UTC+8 = 28800) 28800
DST Offset Daylight saving adjustment (seconds) 0
Sync Interval Hours between NTP sync attempts 1 hour
Drift Compensation Auto-adjusted internal clock drift 1.0

Drift Compensation

The system tracks clock drift using NTP samples:

· Maintains 4-sample history
· Exponential moving average (80% old, 20% new)
· Clamped between 0.95 and 1.05
· Automatically applied to internal RTC

RTC Rebase Interval

· Interval: 35 minutes (2,100,000 ms)
· Prevents accumulated microsecond counter overflow
· Recalculates internal epoch based on drift

---

Network Configuration

WiFi Station Mode

Connection Process:


1. Load saved credentials from NVS
2. Attempt connection with timeout (20 seconds)
3. Max retries: 10 attempts
4. Cooldown period: 5 minutes after max retries
5. Automatic retry after cooldown


Connection States:

State Description
wifiConnecting Actively attempting connection
wifiConnected Successfully connected
wifiGiveUpUntil Cooldown period active

Access Point Mode

Default Settings:

Parameter Value
SSID ESP32_16CH_Timer_Switch
Password ESP32-admin
Channel 6
IP Address 192.168.4.1
Hidden No

Configuration Options:

· Custom SSID (max 31 chars)
· Password (min 8 chars or empty for open)
· Channel selection (1-13)
· SSID visibility (broadcast/hidden)

Captive Portal

The system includes a captive portal that intercepts:

· /hotspot-detect.html
· /library/test/success.html
· /generate_204
· /connecttest.txt
· /ncsi.txt

All unrecognized URLs redirect to the main interface.

mDNS Discovery

Default Hostname: esp32-16ch-relay.local

Access Methods:


http://esp32-16ch-relay.local
http://[custom-hostname].local


mDNS Services:

Service Details
Service Type _http._tcp
Port 80
TXT Records model: ESP32-16CH-Relay
 version: v8
 channels: [relay count]

---

Self-Healing System

Component Recovery

cpp
// Recovery thresholds
WiFi Recovery:     3 failures → force reconnect
mDNS Recovery:     3 failures → restart service
DNS Recovery:      3 failures → restart DNS server
Web Server:        3 failures → restart server


Health Monitoring

Metric Description
wifiFailures Consecutive WiFi failures
ntpFailures NTP sync failures
mdnsFailures mDNS service failures
dnsFailures DNS server failures
webServerFailures HTTP server failures
inRecoveryMode Active recovery flag

Critical State Backup

Backup Contents:

· All relay states (ON/OFF)
· Manual override flags
· Timestamp of backup
· Checksum for validation

Backup Frequency: Every 5 minutes (300 seconds)

Restore Triggers:

· System startup
· After recovery operations
· On demand from API

Memory Management

Memory Thresholds:

Condition Action
Free heap < 20KB Immediate cleanup
Every 1 hour Periodic cleanup
Every 5 minutes Heap check

Cleanup Operations:

· Delete WiFi scan results
· Yield operations for task scheduler
· Reopen Preferences gracefully

Relay State Verification

· Verification Interval: 1 minute
· Action: Compares expected vs actual pin states
· Correction: Forces correct state if mismatch detected

---

GPIO Management

Dynamic Pin Configuration

Operations Available:

1. Add Relay - Append new relay with next available GPIO
2. Delete Relay - Remove relay, shift remaining relays up
3. Reset to Default - Restore original 16-pin mapping
4. Toggle Active Low - Per-relay active level switch

Active Level Toggle

Use Cases:

· Active LOW (default): Common relay modules (JD-VCC)
· Active HIGH: Some solid-state relays (SSR)

Pin Availability Check

The system validates pins against a list of safe GPIOs:


{0, 2, 3, 4, 5, 12, 13, 14, 15, 18, 23, 25, 26, 27, 32, 33}


Excluded Pins: Flash pins (6-11), PSRAM pins (16-17), etc.

Pin Removal Impact

When a relay is deleted:

· All schedules for that relay are lost
· Remaining relays shift up (index changes)
· GPIO pin is not disabled (can be reassigned)
· Manual overrides are cleared

---

API Reference

Relay Endpoints

Endpoint Method Description
/api/relays GET Get all relay states and schedules
/api/relay/manual POST Set manual override
/api/relay/reset POST Return to auto mode
/api/relay/save POST Save schedule configuration
/api/relay/name POST Rename relay

POST /api/relay/manual

json
{
  "relay": 0,
  "state": true
}


POST /api/relay/save

json
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
      "days": 62,
      "monthDays": 0
    }
  ]
}


WiFi Endpoints

Endpoint Method Description
/api/wifi GET Get WiFi status
/api/wifi POST Save credentials
/api/wifi/scan POST Start scan
/api/wifi/scan GET Get scan results

Time Endpoints

Endpoint Method Description
/api/time GET Current time status
/api/time/browser-sync POST Sync from browser
/api/ntp GET NTP configuration
/api/ntp POST Save NTP settings
/api/ntp/sync POST Force NTP sync

POST /api/time/browser-sync

json
{
  "utc_epoch": 1700000000
}


GPIO Endpoints

Endpoint Method Description
/api/gpio GET Get pin configuration
/api/gpio/save POST Save full pin configuration
/api/gpio/add POST Add new relay pin
/api/gpio/delete POST Remove relay pin
/api/gpio/toggle-active-low POST Toggle active level

System Endpoints

Endpoint Method Description
/api/system GET System information
/api/reset POST Soft restart
/api/factory-reset POST Factory reset
/api/mdns GET mDNS status
/api/mdns POST Set mDNS hostname
/api/mdns/restart POST Force mDNS restart

System Information Response

json
{
  "ip": "192.168.1.100",
  "ap_ip": "192.168.4.1",
  "uptime": 86400,
  "freeHeap": 180000,
  "utcEpoch": 1700000000,
  "timeSource": "NTP",
  "ntpServer": "ph.pool.ntp.org",
  "ntpSyncAge": 300,
  "browserSyncAge": -1,
  "wifiConnected": true,
  "wifiSSID": "HomeNetwork",
  "rssi": -55,
  "version": 6,
  "chipModel": "ESP32-38P",
  "mdnsHostname": "esp32-16ch-relay",
  "mdnsStarted": true,
  "relayCount": 16,
  "maxRelays": 16,
  "gmtOffset": 28800
}


---

Factory Reset & Recovery

Trigger Methods

1. Web Interface

Navigate to System → Factory Reset button

2. Hardware Button

· Hold BOOT button (GPIO 0) for 5 seconds
· System clears all NVS data
· Automatic restart with default settings

Reset Behavior

Cleared Data:

· WiFi credentials
· All relay schedules
· Relay names
· AP configuration
· GPIO pin assignments
· NTP settings
· Time offsets
· mDNS hostname

Restored Defaults:

· 16 default GPIO pins
· Active LOW for all relays
· AP: ESP32_16CH_Timer_Switch
· GMT Offset: 28800 (UTC+8)
· NTP: ph.pool.ntp.org
· All schedules disabled

Recovery After Reset

1. Device restarts in AP mode
2. Connect to ESP32_16CH_Timer_Switch
3. Navigate to 192.168.4.1
4. Configure WiFi credentials
5. Set timezone/NTP
6. Reconfigure schedules as needed

---

Technical Architecture

Data Persistence (NVS)

Namespaces:

Namespace Data Stored
relay16 System config, relay configs, GPIO config, critical state

Structures:

cpp
struct SystemConfig {
    uint16_t magic;           // Validation: 0x1234
    uint8_t version;          // Config version: 6
    char sta_ssid[32];        // WiFi SSID
    char sta_password[64];    // WiFi password
    char ap_ssid[32];         // AP SSID
    char ap_password[32];     // AP password
    char ntp_server[48];      // NTP server
    int32_t gmt_offset;       // GMT offset (seconds)
    int32_t daylight_offset;  // DST offset
    time_t last_rtc_epoch;    // Last saved epoch
    float rtc_drift;          // Drift compensation
    char hostname[32];        // mDNS hostname
};

struct ExtConfig {
    uint8_t magic;            // Validation: 0xEC
    uint8_t ap_channel;       // AP channel (1-13)
    uint8_t ntp_sync_hours;   // NTP sync interval
    uint8_t ap_hidden;        // AP hidden flag
    uint8_t reserved[28];     // Future use
};

struct GPIOPinConfig {
    uint8_t pins[MAX_RELAYS];
    uint8_t count;
    uint16_t magic;           // Validation: 0xD002
    bool activeLow[MAX_RELAYS];
};


Timing Constants

Constant Value Description
NTP_RETRY_INTERVAL 30,000 ms NTP retry on failure
WIFI_CHECK_INTERVAL 10,000 ms WiFi status check
WIFI_CONNECT_TIMEOUT 20,000 ms Connection timeout
RTC_UPDATE_INTERVAL 100 ms RTC read interval
SCHEDULE_PROCESS_INTERVAL 250 ms Schedule evaluation
RELAY_UPDATE_INTERVAL 500 ms Relay update interval
RTC_REBASE_INTERVAL 35 min RTC recalibration
SCHEDULE_CACHE_INTERVAL 1,000 ms Schedule cache refresh

Millis-Safe Time Comparison

cpp
inline bool timeHasElapsed(unsigned long current, 
                           unsigned long previous, 
                           unsigned long interval) {
    return (current - previous) >= interval;
}


Handles millis() rollover (every 49.7 days).

---

Troubleshooting

Common Issues & Solutions

WiFi Connection Problems

Symptom Solution
Won't connect to network Check SSID/password, ensure 2.4GHz network
Frequent disconnects Reduce distance to router, check interference
Cannot scan networks Wait for ongoing connection to complete
AP mode not visible Check channel selection (use 1,6, or 11)

Time Sync Issues

Symptom Solution
Time not updating Check WiFi connectivity, try browser sync
Wrong timezone Verify GMT offset value
DST not applying Configure daylight_offset correctly
Never syncs NTP servers blocked? Try browser sync

Relay Issues

Symptom Solution
Relay not responding Check GPIO pin connection, verify active level
Wrong relay activates Double-check pin mapping in GPIO page
Schedule not running Verify schedule enabled, day-of-week selected
Schedule triggering wrong time Check time source status (NTP/Browser)

Web Interface Issues

Symptom Solution
Page won't load Check IP address, try mDNS hostname
Slow response Reduce number of active schedules
JSON parse error Clear browser cache, try incognito mode
Captive portal not working Check DNS server status in System page

Diagnostic Steps

1. Check System Page
   · Verify WiFi connected
   · Check time source
   · Review free heap (>20KB required)
   · Confirm NTP sync age
2. Verify GPIO Configuration
   · Navigate to GPIO page
   · Confirm correct pins assigned
   · Check active level setting
3. Test Relay Manually
   · Use ON/OFF buttons
   · Verify physical relay clicks
   · Check LED indicators
4. Validate Schedules
   · Ensure schedules are enabled
   · Verify day-of-week selection
   · Check start/stop times
   · Review month-day filters

Log Messages (Serial Monitor)

Connect via Serial (115200 baud) to monitor:

· WiFi connection attempts
· NTP sync success/failure
· Schedule evaluation
· Recovery actions
· Memory warnings

Recovery Procedures

Scenario Recovery Steps
Lost WiFi credentials Connect to AP, reconfigure WiFi
Web interface unresponsive Power cycle device
Schedules not working Verify time source, re-save schedules
Device in boot loop Hold BOOT for factory reset
Can't find AP Check power, wait 30 seconds after boot

---

```

</details>
