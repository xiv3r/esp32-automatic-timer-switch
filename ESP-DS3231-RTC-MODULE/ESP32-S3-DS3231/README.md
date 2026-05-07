# Requirements
- 5V 1-16 Channel Relay
- ESP32-S3 N16R8 Wroom-1 Dev Module
- DS3231 RTC Module
- 2pcs Dupont Wires
- 5V 5-8A Power Supply

# Libraries
- ArduinoJson
- Preferences 
- NTPClient
- [RTCLib 1.14.1](https://codeload.github.com/adafruit/RTClib/zip/refs/tags/1.14.1)

# Installation
- Download the Firmware and Flash
```
bootloader: 0x0
partitions: 0x8000
firmware  : 0x10000
```
# Wifi Key
- Wifi Name:`ESP32_S3_16CH_Timer_Switch`

- Password:`ESP32-admin`

# Setup
- Go to `192.168.4.1 –> wifi` then connect your ESP32-S3 to your Home Wifi

# Access
° Direct Access
- mDNS:`esp32-s3-16ch-timer.local`
- Captive Portal: Auto redirect
- Gateway:`192.168.4.1`
- WAN:`192.168.1.123`
  
° Global:`Enable esp32 s3 Port Forwarding on your router to access anywhere`

# GPIO Connections
```
16CH | ESP32-S3 N16R8
VCC  => 5V
IN1  => 1
IN2  => 2
IN3  => 3
IN4  => 4
IN5  => 5
IN6  => 6
IN7  => 7
IN8  => 10
IN9  => 11
IN10 => 12
IN11 => 13
IN12 => 14
IN13 => 15
IN14 => 16
IN15 => 17
IN16 => 18
GND  => GND
```

# DS3231 RTC Module GPIO
```
RTC  |  ESP32-S3 N16R8 
SDA  → 8
SCL  → 9
VCC → 3.3V
GND → GND
```

# Full Features

```
Core Features

1. Relay Control & Scheduling

· Manual Override — Force any relay ON/OFF with automatic detection
· Auto Mode — Revert to schedule-driven operation
· 8 Independent Schedules per Relay — Each with start/stop times to the second
· Day-of-Week Filtering — Select any combination of days (Sunday–Saturday)
  · Presets: Everyday, Weekdays (Mon–Fri), Weekends (Sat–Sun)
· Day-of-Month Filtering — Select specific days of the month (1–31)
· Overnight Scheduling — Supports schedules crossing midnight (start > stop)
· Always-ON Detection — Identifies schedules where start = stop (continuous ON)
· Custom Relay Naming — Rename each relay (up to 15 characters) via double-click

2. Time Management

· DS3231 RTC — Battery-backed real-time clock with automatic drift compensation (0.90–1.10 range)
· NTP Synchronization — Multi-server fallback chain:
  1. ph.pool.ntp.org (primary, configurable)
  2. pool.ntp.org
  3. time.nist.gov
  4. time.google.com
· Configurable Sync Interval — 1–24 hours
· GMT Offset — Customizable (e.g., UTC+8 = 28800)
· Daylight Saving Offset — Adjustable
· RTC Drift Tracking — Kalman-like weighted averaging (75% history, 25% new measurement)
· Graceful Fallback — Uses RTC when NTP unavailable; manual sync trigger

3. WiFi Connectivity

· Dual Mode — Simultaneous Station (STA) + Access Point (AP)
· STA Features:
  · WiFi network scanning with RSSI bars and encryption icons
  · Non-blocking reconnect with exponential backoff (up to 10 attempts)
  · 5-minute cooldown after max reconnection failures
  · 15-second connection timeout per attempt
· AP Features:
  · Configurable SSID, password, channel (1–13), and visibility (hidden/visible)
  · Captive portal support for iOS, Android, Windows, and macOS
  · Default credentials: ESP32_S3_24CH_Timer_Switch / ESP32S3-admin

4. mDNS / Bonjour

· Automatic mDNS hostname generation from AP SSID
· Service advertisement: http._tcp with metadata (model, version, channels)
· Access via http://[hostname].local
· Configurable via API with restart capability

5. Web Interface

· Responsive Single-Page Application (no frameworks)
· Pages:
  · Relays — Full schedule editor with time pickers, day/month toggles, ON/OFF/Auto buttons
  · WiFi — Network scanner with signal strength visualization, connect/save
  · Time (NTP) — Server configuration, sync now, GMT/DST settings
  · AP — SSID, password, channel, hidden SSID toggle
  · System — Live stats (IP, heap, uptime, RSSI, NTP status, chip model, mDNS)
· Live Clock — Updates every second with WiFi/NTP status indicators
· Toast Notifications — Success/error feedback
· Auto-refresh — Relay list refreshes every 60 seconds

6. Configuration Management

· NVS Storage — All settings survive power cycles
· Version Migration — Handles upgrades from v3, v4, and v5 config formats
· Backward Compatibility — New monthDays field defaults to 0 (all days)
· Magic Number Validation — 0x1234 for system config, 0xEC for extended config

7. System Resilience

· Safe Boot State — All relays OFF on startup before loading configuration
· Watchdog-friendly — Non-blocking WiFi reconnection state machine
· Async WiFi Scanning — Background scan with 10-second timeout
· Factory Reset — Clears all NVS and restarts to default configuration
· Soft Restart — API-triggered reboot

---

REST API Endpoints

Method Endpoint Description
GET /api/relays Get all relay states, schedules, and names
POST /api/relay/manual Set manual ON/OFF override
POST /api/relay/reset Clear manual override (return to auto)
POST /api/relay/save Save all 8 schedules for a relay
POST /api/relay/name Rename a relay
GET /api/time Current time, WiFi/NTP status
GET /api/wifi STA SSID, connection status, IP, RSSI
POST /api/wifi Save STA credentials & reconnect
POST /api/wifi/scan Start async WiFi scan
GET /api/wifi/scan Poll scan results
GET /api/ntp NTP server, offsets, sync interval
POST /api/ntp Save NTP settings
POST /api/ntp/sync Force immediate NTP sync
GET /api/ap AP configuration
POST /api/ap Save AP config & restart AP
GET /api/mdns mDNS hostname and status
POST /api/mdns Set mDNS hostname
POST /api/mdns/restart Restart mDNS service
GET /api/system System diagnostics
POST /api/reset Soft restart device
POST /api/factory-reset Factory reset + restart

---

Visual Design Features

· Material-inspired card-based UI with gradient header
· Dark blue professional color scheme (#1565C0 / #0D47A1)
· Color-coded status badges: Green (ON), Red (OFF), Orange (Manual)
· WiFi signal strength visualization with animated bars
· Day/Month toggle chips with purple accent for month-day selection
· Sticky navigation header with live clock
· Fully responsive — mobile-optimized with breakpoint at 500px
· Overnight badge (🌙) with contextual schedule info
· Always-ON badge (●) for continuous schedules

---

Default Settings

Setting Default Value
AP SSID ESP32_S3_24CH_Relay
AP Password ESP32S3-admin
AP Channel 6
AP Visibility Visible
NTP Server ph.pool.ntp.org
GMT Offset 28800 (UTC+8)
NTP Sync Interval 1 hour
mDNS Hostname esp32-s3-24ch-relay
Relay Names Relay 1 through Relay 24

---

Captive Portal Support

Automatically redirects clients on the following detection URLs:

· /hotspot-detect.html (iOS)
· /library/test/success.html (iOS)
· /generate_204 (Android/Chrome)
· /success.txt (Firefox)
· /canonical.html (Windows)
· /connecttest.txt / /ncsi.txt (Windows NCSI)
· /redirect

---

Technical Highlights

· ArduinoJson for API serialization (StaticJsonDocument for fixed payloads, DynamicJsonDocument for variable arrays)
· RTClib for DS3231 communication
· NTPClient with configurable pool server switching
· Preferences (NVS) for persistent storage across reboots
· Non-blocking design — no delay() in main loop, state machines for WiFi reconnection
· Drift-compensated software RTC for sub-second accuracy between syncs
```
