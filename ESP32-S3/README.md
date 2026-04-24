# Requirements
- 5V 1-16 Channel Relay
- ESP32-S3 Dev Module
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
- Wifi Name:`ESP32_S3_16CH_Timer_Switch`

- Password:`ESP32-S3-admin`

# Setup
- Go to `192.168.4.1/wifi` then connect your ESP32-S3 to your Home Wifi

# Access
° Direct Access
- mDNS:`esp32-s3-16ch-timer.local`
- Captive Portal: Auto redirect
- Gateway:`192.168.4.1`
- WAN:`192.168.1.123`
  
° Global:`Enable esp32 s3 Port Forwarding on your router to access anywhere`

# GPIO Connections
```
16CH | ESP32-S3
VCC  => 5V
IN1  => 4
IN2  => 5
IN3  => 6
IN4  => 7
IN5  => 8
IN6  => 9
IN7  => 10
IN8  => 11
IN9  => 12
IN10 => 13
IN11 => 14
IN12 => 15
IN13 => 16
IN14 => 17
IN15 => 18
IN16 => 21
GND  => GND
```

<details><summary>

# Full Features
  
</summary>

```
---

🧠 Core System Features

Relay Control

· 16 independent relay channels with configurable GPIO pins
· Active-low relay support (configurable via relayActiveLow flag)
· Manual override: Force any relay ON/OFF regardless of schedule
· Automatic mode: Returns relay to schedule-based control
· Custom naming: Each relay can be named (up to 15 characters)
· Safe startup: All relays forced OFF at boot to prevent unexpected activation

Scheduling Engine

· 8 independent schedules per relay (128 total scheduling slots)
· Time-of-day scheduling: Start/stop times with second precision
· Three schedule types:
  · Normal (start < stop): Runs within a single day
  · Overnight (start > stop): Spans across midnight
  · Always ON (start = stop): Relay permanently active
· Schedule resolution: Hour:Minute:Second
· Real-time enforcement based on synchronized time

---

🌐 WiFi & Connectivity

Dual WiFi Mode

· Station (STA) mode: Connects to existing WiFi network
· Access Point (AP) mode: Creates its own WiFi network simultaneously
· Configurable AP SSID, password, channel (1-13), and hidden SSID option
· Non-blocking WiFi reconnection with state machine
· Backoff mechanism: After 10 failed reconnect attempts, waits 5 minutes
· RSSI monitoring with visual bars in web UI

WiFi Scanning

· Asynchronous WiFi scanning (non-blocking, 10-second timeout)
· Discovers available networks and displays in web UI
· Shows SSID, signal strength (RSSI), and encryption status
· Click-to-select networks from scan results

---

⏰ Time Synchronization (NTP)

NTP Client

· Multi-server fallback chain:
  1. ph.pool.ntp.org (Philippines NTP pool)
  2. pool.ntp.org (Global pool)
  3. time.nist.gov (NIST)
  4. time.google.com (Google)
· Automatic server rotation on failure
· Configurable GMT offset and daylight saving offset
· Adjustable sync interval (1–24 hours)
· Manual sync trigger via web UI
· Consecutive failure tracking with retry logic

Internal RTC

· Drift-compensated software RTC (no hardware RTC chip needed)
· EWMA filter for drift calculation (0.90–1.10 bounds)
· Survives reboots via NVS (non-volatile storage)
· Sub-second precision timing

---

🌍 Web Interface

Complete Web Application (SPA with AJAX)

· 5-page responsive web UI:
  1. Relays: Main dashboard with all relay controls and schedules
  2. WiFi: Network settings, connection status, WiFi scanner
  3. Time (NTP): Time sync configuration and manual sync
  4. AP: Access point settings (SSID, password, channel, visibility)
  5. System: Device info, mDNS status, restart/factory reset controls

UI Features

· Real-time clock display in header (updates every second)
· Status indicators: WiFi (green/red), NTP sync (green/yellow)
· Toast notifications for user feedback (success/error)
· Bulk relay control: ON/OFF/Auto buttons per relay
· Inline name editing (double-click relay name)
· Overnight badge indicators and "Always ON" labels
· Responsive design: Works on mobile and desktop
· Generated via ArduinoJson for all API responses

API Endpoints (RESTful JSON API)

Method Endpoint Function
GET /api/relays Get all relay states, manual status, and schedules
POST /api/relay/manual Set relay manual override ON/OFF
POST /api/relay/reset Return relay to automatic schedule mode
POST /api/relay/save Save relay schedules
POST /api/relay/name Save relay custom name
GET /api/time Get current time, WiFi/NTP status
GET /api/wifi Get WiFi connection status and SSID
POST /api/wifi Save WiFi credentials (triggers restart)
POST /api/wifi/scan Start async WiFi scan
GET /api/wifi/scan Get scan results (polling)
GET /api/ntp Get NTP configuration
POST /api/ntp Save NTP settings
POST /api/ntp/sync Force immediate NTP sync
GET /api/ap Get AP configuration
POST /api/ap Save AP settings (restarts AP)
GET/POST /api/mdns Get/set mDNS hostname
POST /api/mdns/restart Restart mDNS service
GET /api/system System info (IP, heap, uptime, etc.)
POST /api/reset Software restart
POST /api/factory-reset Clear all NVS and restart

---

📡 mDNS (Bonjour/Zeroconf)

· Automatic hostname derived from AP SSID (sanitized)
· Configurable hostname via API
· Advertises HTTP service with metadata (model, version, channel count)
· Retry logic for startup failures (3 attempts)
· Scheduled restart mechanism to avoid race conditions
· Accessible via http://[hostname].local

---

🖥️ Captive Portal

· DNS server running on port 53 redirects all requests
· Platform-specific probe paths for automatic portal detection:
  · Apple: /hotspot-detect.html, /library/test/success.html
  · Android: /generate_204
  · Microsoft: /ncsi.txt, /connecttest.txt
  · Firefox: /success.txt
· Catch-all redirect to device homepage
· Smooth onboarding for WiFi-only device setup

---

💾 Persistent Storage (NVS/Preferences)

Two-tier configuration system

1. v3 SystemConfig: Main configuration (backward compatible)
   · WiFi STA credentials
   · AP credentials
   · NTP server and timezone offsets
   · RTC state (epoch + drift)
2. v4 ExtConfig: Extended configuration (independent of main layout)
   · AP channel selection
   · NTP sync interval
   · AP hidden mode
   · 28 reserved bytes for future expansion

Data Integrity

· Magic number validation (0x1234 for main config, 0xEC for ext config)
· Version checking for migration path preservation
· Corruption recovery: Auto-resets configs if corrupted
· Boundary validation on all critical values

---

🛡️ Error Handling & Robustness

Watchdog & Self-Healing

· WiFi health checks every 5 seconds
· Connection timeout detection (15 seconds per attempt)
· mDNS restart scheduling to prevent crash loops
· Scan timeout enforcement (10 seconds max)
· Heap monitoring visible in system page

Edge Case Handling

· RTC drift bounds (0.90–1.10) to prevent extreme values
· Channel validation (1–13) for AP configuration
· NTP epoch validation (1,000,000,000–2,000,000,000)
· JSON parse error handling for all API endpoints
· Overnight schedule crossing midnight properly handled

---

🔧 Hardware-Specific Optimizations

ESP32-S3 Safe GPIO Selection

Avoids problematic pins:

· Strapping pins: GPIO 0, 3, 45, 46
· USB pins: GPIO 19, 20
· Internal SPI/PSRAM: GPIO 26–33
· Reserved pins: GPIO 48

Used Pins: 4–18, 21 (16 channels)

---

📊 System Information Dashboard

· STA IP address and AP IP address
· Free heap memory (KB)
· System uptime (formatted as hours/minutes/seconds)
· WiFi RSSI with human-readable quality indicator
· NTP last sync time with relative age
· NTP server currently in use
· Chip model identification
· mDNS hostname and operational status

---

🔄 Reset & Recovery

· Soft restart via API (preserves configuration)
· Factory reset clears all NVS data and reboots to default AP
· Default AP: ESP32_S3_16CH_Timer_Switch with password ESP32-S3-admin
· Default NTP: Philippines pool, UTC+8 timezone (28800 offset)

---

 📦 Memory Efficiency

· Program memory: Uses PROGMEM for HTML/CSS content
· JSON parsing: Uses StaticJsonDocument for small payloads, DynamicJsonDocument for variable-sized responses
· Non-blocking design: No delay() calls in main loop
· Selective NVS writes: Only saves when configuration changes
---
```
</details>
