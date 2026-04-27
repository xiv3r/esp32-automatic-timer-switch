# Requirements
- 5V 1-16 Channel Relay
- ESP32-C3 Dev Module
- 2pcs Dupont Wires
- 5V 5A Power Supply

`Optional`
- 5V Battery (Maintain Time)

# Libraries
- ArduinoJson
- NTPClient

# Installation
- Download the Firmware and Flash
```
bootloader: 0x0
partitions: 0x8000
firmware  : 0x10000
```
# Wifi Key
- Wifi Name:`ESP32_C3_16CH_Timer_Switch`

- Password:`ESP32-C3-admin`

# Setup
- Go to `192.168.4.1/wifi` then connect your ESP32-S3 to your Home Wifi

# Access
° Direct Access
- mDNS:`esp32-c3-16ch-timer.local`
- Captive Portal: Auto redirect
- Gateway:`192.168.4.1`
- WAN:`192.168.1.123`
  
° Global:`Enable esp32-c3 Port Forwarding on your router to access anywhere`

# GPIO Connections
```
16CH | ESP32-S3
VCC  => 5V
IN1  => 0
IN2  => 1
IN3  => 2
IN4  => 3
IN5  => 4
IN6  => 5
IN7  => 6
IN8  => 7
IN9  => 8
IN10 => 9
IN11 => 10
IN12 => 11
IN13 => 18
IN14 => 19
IN15 => 20
IN16 => 21
GND  => GND
```

<details><summary>

# Full Features
  
</summary>

# 1. HARDWARE CONFIGURATION

## 1.1 Relay Outputs
```
Parameter Value
Number of Channels 16
GPIO Pins 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 18, 19, 20, 21
Active Logic Configurable via relayActiveLow (default: true = active-LOW)
Boot State All OFF (HIGH if active-low, LOW if active-high)
Strapping Pin Caution GPIOs 2, 8, 9 are strapping pins; active-low relays naturally satisfy HIGH requirement at boot
```
---

# 2 GPIO Usage
```
GPIO Function Notes
0 Relay 1 Output
1 Relay 2 Output
2 Relay 3 Strapping pin (must be HIGH at boot)
3 Relay 4 Output
4 Relay 5 Output
5 Relay 6 Output
6 Relay 7 Output
7 Relay 8 Output
8 Relay 9 Strapping pin (must be HIGH at boot)
9 Relay 10 Strapping pin (must be HIGH at boot)
10 Relay 11 Output
11 Relay 12 Output
18 Relay 13 Output
19 Relay 14 Output
20 Relay 15 Output
21 Relay 16 Output
```
---

# 3. WIRELESS CONNECTIVITY

## 3.1 Wi-Fi Modes
```
Mode Support Description
STA (Station) Yes Connects to an existing network
AP (Access Point) Yes Creates its own network
Simultaneous AP+STA Yes WIFI_AP_STA mode active at all times
```
## 3.2 Access Point Defaults & Configuration
```
Setting Default Range / Constraint
SSID ESP32-C3_16CH_Timer_Switch Max 31 characters
Password ESP32-C3-admin 0 (open) or 8+ characters
Channel 6 1–13
Hidden SSID No (visible) true or false
Max Simultaneous Clients Platform default (typically 4 for ESP32-C3) —
AP IP Address Platform default (typically 192.168.4.1) Available via WiFi.softAPIP()
```
## 3.3 Station Mode Behavior
```
Behavior Detail
Connection Timeout 15 seconds (WIFI_CONNECT_TIMEOUT)
Max Reconnect Attempts 10 (MAX_RECONNECT)
Backoff After Max Attempts 5 minutes (300000UL ms)
Wi-Fi Health Check Interval Every 5 seconds (WIFI_CHECK_INTERVAL)
Credentials Storage NVS (non-volatile)
SSID Field Size 32 chars (31 + null)
Password Field Size 64 chars (63 + null)
```
## 3.4 Wi-Fi Scan
```
Attribute Detail
Mode Asynchronous (WiFi.scanNetworks(true, true))
Timeout 10 seconds (WIFI_SCAN_TIMEOUT)
Hidden Networks Included in scan
Max Reported Results 30
API POST /api/wifi/scan starts; GET /api/wifi/scan polls
```
---

# 4. TIMEKEEPING & NTP

## 4.1 NTP Client Configuration
```
Setting Default Range
Primary Server ph.pool.ntp.org Up to 48 chars
Fallback Servers pool.ntp.org → time.nist.gov → time.google.com 4 total in rotation
GMT Offset 28800 seconds (UTC+8) long
Daylight Saving Offset 0 seconds long
Total Offset gmt_offset + daylight_offset —
Sync Interval 1 hour 1–24 hours
Retry Interval on Failure 30 seconds (NTP_RETRY_INTERVAL) —
Update Interval 3600000 ms (1 hour, updated by getNTPInterval()) —
```
## 4.2 NTP Server Rotation Logic
```
Attempt Server
0 Current ntpServerIndex (default: ph.pool.ntp.org)
1 Next (pool.ntp.org)
2 Next (time.nist.gov)
3 Last (time.google.com)

On success, the working server index is remembered. On total failure, ntpFailCount increments and ntpServerIndex advances.
```
## 4.3 Internal RTC
```
Attribute Detail
Basis millis() with epoch timestamp stored in NVS
Drift Compensation EWMA filter (α = 0.25, clamped to 0.90–1.10)
Update Tick Every 100 ms (RTC_UPDATE_INTERVAL)
Validity Range 1,000,000,000 to 2,000,000,000 epoch (Sep 2001 – May 2033)
Persistence last_rtc_epoch and rtc_drift in SystemConfig
Sync Trigger NTP sync succeeds → syncInternalRTC()
```
---

# 5. SCHEDULE ENGINE

## 5.1 Per-Relay Schedule Structure
```
Field Count Range
Schedules per Relay 8 (NS = 8 in JS) Index 0–7
Start Time Hour (0–23), Minute (0–59), Second (0–59) —
Stop Time Hour (0–23), Minute (0–59), Second (0–59) —
Enabled Flag Boolean per schedule —
Evaluation Resolution 1 second (RTC tick at 100 ms, evaluated every loop iteration) —
```
## 5.2 Schedule Modes
```
Mode Condition Behavior
Normal (Same Day) start < stop ON when current >= start AND current < stop
Overnight start > stop ON when current >= start OR current < stop
Always ON start == stop Permanently ON (overrides all other schedules)
```
## 5.3 Priority
```
1. Manual Override – If manualOverride == true, schedule is ignored entirely.
2. Always ON Schedule – If any enabled schedule has start == stop, relay is ON.
3. Time-Based ON – First matching normal or overnight schedule wins.
4. Default – Relay is OFF.
```
## 5.4 Relay Name
```
Field Max Length Default
Custom Name 15 characters + null "Relay N" (where N = 1–16)
```
---

# 6. mDNS (Multicast DNS)
```
Attribute Detail
Default Hostname esp32c3-16ch-relay
Dynamic Hostname Derived from AP SSID if default unchanged (sanitized: lowercase, spaces → hyphens, alphanumeric + hyphen only)
Max Hostname Length 31 characters
Service Advertised http._tcp on port 80
TXT Records model=ESP32-C3-16CH-Relay, version=v4.1, channels=16
Restart Delay 2 seconds after AP change (MDNS_RESTART_DELAY)
Retry on Start Up to 3 attempts with 100 ms delay between
```
---

# 7. CAPTIVE PORTAL

## 7.1 DNS Server
```
Attribute Detail
Port 53 (DNS_PORT)
Behavior All DNS queries resolved to WiFi.softAPIP()
```
## 7.2 Captive Portal Detection Paths Served
```
Path Purpose
/hotspot-detect.html Apple Captive Network Assistant
/library/test/success.html Apple Captive Network Assistant
/generate_204 Android/ChromeOS captive portal check (302 redirect)
/success.txt Firefox captive portal check
/canonical.html Generic (302 redirect)
/connecttest.txt Microsoft Windows NCSI
/ncsi.txt Microsoft Windows NCSI
/redirect Generic (302 redirect)
Catch-all (onNotFound) All other requests → 302 redirect to AP home page
```
---

# 8. WEB INTERFACE

## 8.1 Served Pages
```
Route File/Handler Content Type
/ index_html text/html – Relay Controls & Schedules
/wifi wifi_html text/html – Wi-Fi Station Settings
/ntp ntp_html text/html – NTP & Time Settings
/ap ap_html text/html – Access Point Settings
/system system_html text/html – System Info & Controls
/style.css style_css text/css – Embedded stylesheet
```
## 8.2 Client-Side Features
```
Feature Detail
Live Clock Updates every 1 second via GET /api/time
Connection Indicators Green (Wi-Fi OK), Red (Wi-Fi down), Yellow (NTP unsynced)
Network Scanner Asynchronous scan with RSSI bars and lock icons for encrypted networks
Toast Notifications Success (green) / Error (red) auto-dismiss after 3 seconds
Inline Relay Naming Double-click relay name to edit in-place
Auto-Refresh Relay list refreshes every 60 seconds if idle
```
---

# 9. REST API — COMPLETE REFERENCE

## 9.1 Relay Endpoints
```
Method Endpoint Request Body Response (Success)
GET /api/relays — JSON array of 16 relay objects with state, manual flag, name, 8 schedules
POST /api/relay/manual {"relay": <0-15>, "state": <bool>} {"success": true}
POST /api/relay/reset {"relay": <0-15>} {"success": true}
POST /api/relay/save {"relay": <0-15>, "schedules": [...]} {"success": true}
POST /api/relay/name {"relay": <0-15>, "name": "<string>"} {"success": true}
```
## 9.2 Time Endpoints
```
Method Endpoint Request Body Response Fields
GET /api/time — time, wifi, ntp
GET /api/ntp — ntpServer, gmtOffset, daylightOffset, syncHours
POST /api/ntp {"ntpServer", "gmtOffset", "daylightOffset", "syncHours"} {"success": true}
POST /api/ntp/sync — {"success": true} if synced within 5 s
```
## 9.3 Wi-Fi Endpoints
```
Method Endpoint Request Body Response Fields
GET /api/wifi — ssid, connected, ip, rssi
POST /api/wifi {"ssid", "password"} {"success": true} → Restarts ESP
POST /api/wifi/scan — {"scanning": true} (202)
GET /api/wifi/scan — {"scanning": false, "networks": [...]}
```
## 9.4 AP Endpoints
```
Method Endpoint Request Body Response Fields
GET /api/ap — ap_ssid, ap_password, ap_channel, ap_hidden
POST /api/ap {"ap_ssid", "ap_password", "ap_channel", "ap_hidden"} {"success": true} → Restarts AP
```
## 9.5 mDNS Endpoints
 ```
Method Endpoint Request Body Response Fields
GET /api/mdns — hostname, started, url
POST /api/mdns {"hostname": "<string>"} {"success": true, "hostname": "..."}
POST /api/mdns/restart — {"success": true}
```
## 9.6 System Endpoints
```
Method Endpoint Request Body Response Fields
GET /api/system — ip, ap_ip, uptime, freeHeap, ntpSynced, ntpServer, ntpSyncAge, wifiConnected, wifiSSID, rssi, version, chipModel, mdnsHostname, mdnsStarted
POST /api/reset — {"success": true} → Restarts ESP
POST /api/factory-reset — {"success": true} → Clears NVS & restarts
```
---

# 10. NON-VOLATILE STORAGE (NVS)

## 10.1 Namespace
```
relay16
```
## 10.2 Data Structures
```
Sysconfig

Field Type Size (bytes) Default
magic uint16_t 2 0x1234
version uint8_t 1 3
sta_ssid char[32] 32 ""
sta_password char[64] 64 ""
ap_ssid char[32] 32 "ESP32-C3_16CH_Timer_Switch"
ap_password char[32] 32 "ESP32-C3-admin"
ntp_server char[48] 48 "ph.pool.ntp.org"
gmt_offset long 4 28800
daylight_offset int 4 0
last_rtc_epoch time_t 4 0
rtc_drift float 4 1.0
hostname char[32] 32 "esp32relay"
Total  ~256 

ExtConfig

Field Type Size (bytes) Default
magic uint8_t 1 0xEC
ap_channel uint8_t 1 6
ntp_sync_hours uint8_t 1 1
ap_hidden uint8_t 1 0
reserved uint8_t[28] 28 0
Total  32 

RelayConfig (×16)

Field Type Size (bytes)
schedule.startHour[8] uint8_t[8] 8
schedule.startMinute[8] uint8_t[8] 8
schedule.startSecond[8] uint8_t[8] 8
schedule.stopHour[8] uint8_t[8] 8
schedule.stopMinute[8] uint8_t[8] 8
schedule.stopSecond[8] uint8_t[8] 8
schedule.enabled[8] bool[8] 8
manualOverride bool 1
manualState bool 1
name char[16] 16
Per-Relay Total  ~74 bytes
All 16 Relays  ~1184 bytes
```
## 10.3 NVS Keys
```
Key Data
sysConfig Serialized SystemConfig
relayConfigs Serialized RelayConfig[16]
extConfig Serialized ExtConfig
```
## 10.4 Validation & Recovery
```
Condition Action
Magic mismatch (0x1234 or 0xEC) Reset to defaults
Size mismatch when reading Reset to defaults
Version mismatch (EEPROM_VERSION != 3) Full initDefaults()
ap_channel out of range 1–13 Clamp to 6
ntp_sync_hours out of range 1–24 Clamp to 1
Drift compensation out of range 0.90–1.10 Reset to 1.0
RTC epoch out of range (1e9–2e9) Ignore, wait for NTP sync
```
---

# 11. TIMING CONSTANTS
```
Constant Value Description
NTP_RETRY_INTERVAL 30,000 ms Wait between NTP retries on failure
WIFI_CHECK_INTERVAL 5,000 ms Wi-Fi health check period
WIFI_CONNECT_TIMEOUT 15,000 ms STA connection timeout window
RTC_UPDATE_INTERVAL 100 ms RTC tick period
WIFI_SCAN_TIMEOUT 10,000 ms Scan abortion timeout
MAX_RECONNECT 10 Wi-Fi reconnect attempts before 5-min backoff
MDNS_RESTART_DELAY 2,000 ms Delay after AP change before mDNS restart
```
---

# 12. SAFETY & RELIABILITY
```
Feature Implementation
Power-On Relay State All relays explicitly set OFF before any initialization
Strapping Pin Protection Active-low logic ensures GPIOs 2,8,9 are HIGH at boot
NVS Corruption Recovery Magic number validation + automatic re-initialization
NTP Data Validation Epoch must be between 1,000,000,000 and 2,000,000,000
Drift Compensation Clamping Limited to 0.90–1.10 to prevent runaway
Schedule Validation All time components bounded by array indexing (0–59/0–23)
JSON Overflow Protection Static/Dynamic JSON documents with bounded sizes
ESP.restart() Safety Delay 600–800 ms delay before restart after critical saves
```
---

# 13. DEPENDENCIES
```
Library Purpose
WiFi.h ESP32 Wi-Fi AP/STA
NTPClient.h NTP time synchronization
WiFiUdp.h UDP transport for NTP
WebServer.h HTTP server on port 80
DNSServer.h Captive portal DNS on port 53
Preferences.h NVS read/write for persistent config
ArduinoJson.h JSON serialization/deserialization
ESPmDNS.h mDNS .local hostname resolution
```
---

# 14. MEMORY & PERFORMANCE
```
Metric Estimate
JSON Document (Relays GET) Dynamic, up to ~16 KB
JSON Document (Wi-Fi Scan Results) Dynamic, up to ~8 KB
Static JSON (API handlers) 64–256 bytes each
HTML Pages (PROGMEM) Stored in flash via PROGMEM and R"raw(...)raw" raw literals
CSS (PROGMEM) ~5 KB compressed
Heap Free Reported in System API (freeHeap)
Loop Execution Non-blocking; DNS, HTTP, Wi-Fi state, NTP, RTC, and relays on every iteration
```
</details>
