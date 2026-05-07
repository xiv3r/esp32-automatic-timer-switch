# Requirements
- ESP32 38P Pins
- 1 -> 16 Channel 5V Relay
- Female Dupont Wire
- Home Wifi for NTP/RTC sync
- 5v 5-8a Power supply
  
`Optional`
- 5v50Ah DC Battery (Maintain Power and Timer)

# Arduino Libraries
- ArduinoJson
- Preferences
- NTPClient

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
---

🚀 Core Features

Relay Management

· 16 Independent Relay Channels - Control up to 16 relays simultaneously
· Individual Manual Override - Temporarily override automatic schedules for each relay
· Real-time Status Monitoring - Visual indicators for relay states (ON/OFF/AUTO)
· Custom Relay Naming - Assign friendly names to each relay (double-click to edit)
· Active Level Configuration - Choose between ACTIVE LOW or ACTIVE HIGH per relay
· Instant Control - Physical pin state updates with debounced control

Scheduling System

· 8 Schedules per Relay - Each relay supports up to 8 independent schedules
· Flexible Time Selection - Precise hour/minute/second configuration
· Day-of-Week Selection - Individual day selection (Sun through Sat)
· Day-of-Month Selection - Specific date targeting (1-31 day-of-month)
· Preset Day Masks - Quick selection options:
  · Everyday (All days)
  · Weekdays (Mon-Fri)
  · Weekends (Sat-Sun)
· Overnight Support - Schedules that span past midnight
· 24/7 Operation - Start == Stop means always ON during active days
· Real-time Schedule Cache - Optimized processing for performance

---

🌐 Web Interface

Dashboard

· Responsive Design - Mobile-friendly layout
· Live Clock Display - Real-time time with source indicator
· Network Status Indicators - WiFi and time source visual cues
· Card-based Relay View - Clean, organized layout for all relays
· Inline Name Editing - Double-click relay names to edit

Relay Control Panel

· Manual ON/OFF Buttons - Immediate relay control
· Auto Mode Reset - Return to automatic scheduling
· Schedule Editor - Complete schedule management per relay
· Visual Schedule Cards - Collapsible schedule sections
· Night Mode Badges - Visual indicators for overnight schedules

---

🕐 Time Management

NTP Time Synchronization

· Multiple NTP Servers - Automatic fallback between 4 servers:
  · ph.pool.ntp.org
  · pool.ntp.org
  · time.nist.gov
  · time.google.com
· Configurable Sync Interval - 1-24 hours (default: 1 hour)
· Smart Failover - Automatic server rotation on failure
· Drift Compensation - Maintains accurate time between NTP syncs
· Internal RTC Backup - Persists time across reboots

Browser Time Sync

· Fallback Sync Method - Use browser's system time when NTP unavailable
· Manual Sync Button - Force time synchronization
· Time Source Tracking - Shows current time source (NTP/Browser/None)
· GMT Offset Configuration - Support for any timezone (±12 hours)
· Daylight Saving Support - Configurable DST offset

---

📡 Networking

WiFi Station Mode

· Secure Credential Storage - SSID/password saved in NVS
· Non-blocking Connection - Async connection state machine
· Auto-reconnect - Automatic reconnection with exponential backoff
· Network Scanning - Scan and select nearby networks
· Signal Strength Display - RSSI bars and dBm readings
· Connection Timeout - 20-second connection timeout protection

Access Point Mode

· Configurable SSID - Customizable AP network name
· Password Protection - Optional 8+ character password
· Channel Selection - Choose from channels 1-13 (default: 6)
· Hidden SSID Option - Hide network from scans
· Default AP Credentials - ESP32_16CH_Timer_Switch / ESP32-admin

mDNS Support

· Automatic Discovery - Find device as devicename.local
· Custom Hostname - Configurable mDNS hostname
· Service Announcement - HTTP service with device info
· Bonjour/Avahi Compatible - Works with macOS, Linux, Windows

Dual-mode Operation

· AP+STA Simultaneous - Both networks active at once
· Captive Portal Detection - Handles common hotspot detection URLs
· Automatic Redirect - Unknown requests redirected to main page

---

🔧 GPIO Configuration

Dynamic Pin Assignment

· Flexible Pin Mapping - Assign any compatible GPIO to any relay
· Maximum 16 Relays - Configurable channel count
· Default Pin Mapping:
  · Relay 1: GPIO15 | Relay 2: GPIO2 | Relay 3: GPIO4 | Relay 4: GPIO5
  · Relay 5: GPIO18 | Relay 6: GPIO3 | Relay 7: GPIO1 | Relay 8: GPIO23
  · Relay 9: GPIO13 | Relay 10: GPIO12 | Relay 11: GPIO14 | Relay 12: GPIO27
  · Relay 13: GPIO26 | Relay 14: GPIO25 | Relay 15: GPIO33 | Relay 16: GPIO32

Active Level Configuration

· Toggle Active State - Per-relay HIGH/LOW active configuration
· Relay Type Support - Works with both NO and NC relay modules
· Visual Indication - Shows current active level in web UI

Pin Validation

· Collision Detection - Prevents duplicate pin assignment
· Available Pins List - Shows only usable GPIOs
· Maximum Limit Enforcement - Prevents exceeding 16 relays

---

💾 Data Persistence

NVS (Non-Volatile Storage)

· All Settings Saved - WiFi, NTP, GPIO, schedules, relay states
· Configuration Versioning - Handles configuration upgrades
· Factory Reset Support - Clear all settings via button or web
· RTC State Preservation - Maintains time across power cycles

Stored Data Types

· System Configuration - WiFi credentials, NTP servers, time offsets
· Extended Configuration - AP channel, sync intervals, hidden SSID
· Relay Configurations - 8 schedules per relay, names, manual overrides
· GPIO Pin Configuration - Pin assignments and active low states
· RTC State - Last known time and drift compensation

---

🛡️ Safety & Reliability

Boot Button Factory Reset

· Hold for 5 Seconds - Hardware factory reset during normal operation
· Visual Confirmation - LED/status indicators (when implemented)
· Complete Wipe - Clear all NVS data and restart

Web Interface Safety

· Confirmation Dialogs - Confirm destructive actions
· Input Validation - Sanitizes all user inputs
· Error Handling - Graceful error messages
· JSON Validation - Proper JSON parsing with error checking

Schedule Engine

· Conflict Resolution - Multiple schedules handled gracefully
· Cache Optimization - Prevents redundant calculations
· Overlap Handling - Later schedules override earlier ones (per update cycle)

---

📊 Monitoring & Diagnostics

System Information Page

· Network Status - STA IP, AP IP, WiFi RSSI
· System Health - Free heap memory, uptime
· Time Information - Current UTC epoch, time source
· Sync Status - Age of last NTP and browser sync
· Hardware Info - Chip model, relay count
· mDNS Status - Current hostname and running status

Real-time Indicators

· Network Dot - Green (connected) / Red (disconnected)
· Time Source Dot - Green (NTP) / Blue (Browser) / Yellow (None)
· Relay Status Badges - AUTO/MANUAL with ON/OFF states

---

🔄 API Endpoints

Relay Management

Endpoint Method Description
/api/relays GET Get all relay states and schedules
/api/relay/manual POST Set manual override
/api/relay/reset POST Return to auto mode
/api/relay/save POST Save schedule configuration
/api/relay/name POST Change relay name

Time Management

Endpoint Method Description
/api/time GET Get current time and source
/api/time/browser-sync POST Sync time from browser
/api/ntp GET/POST Get/Set NTP settings
/api/ntp/sync POST Force NTP sync

Network Configuration

Endpoint Method Description
/api/wifi GET/POST Get/Set WiFi credentials
/api/wifi/scan POST/GET Start/get scan results
/api/ap GET/POST Get/Set AP settings
/api/mdns GET/POST Get/Set mDNS hostname

GPIO Configuration

Endpoint Method Description
/api/gpio GET Get GPIO configuration
/api/gpio/save POST Save GPIO pin mapping
/api/gpio/add POST Add a relay pin
/api/gpio/delete POST Remove a relay pin
/api/gpio/toggle-active-low POST Toggle active level

System Control

Endpoint Method Description
/api/system GET Get system information
/api/reset POST Restart device
/api/factory-reset POST Factory reset

---

🎨 Web Pages

Page Path Features
Relay Dashboard / All relay controls and schedule editing
WiFi Settings /wifi Network scan, credential management
Time Settings /ntp NTP server, offsets, sync controls
AP Settings /ap Access point configuration
GPIO Manager /gpio Pin assignment and active level
System Info /system Diagnostics and device control

---

⚡ Performance Optimizations

· Non-blocking Operations - All network operations async
· Schedule Cache - 1-second cache for schedule evaluation
· Drift Compensation - Continuous time accuracy improvement
· Selective Relay Updates - Only update changed relay states
· Microsecond Precision - High-resolution time tracking
· Memory Efficient - PROGMEM for static web content
· Yielding Loop - Proper task scheduling prevents watchdog issues

---

🔌 Hardware Requirements

· ESP32 Development Board (38-pin recommended)
· 16-Channel Relay Module (or multiple smaller modules)
· Power Supply (5V/2A minimum for 16 relays)
· Jumper Wires for GPIO connections
· BOOT Button (GPIO0) for factory reset

---

📦 Default Settings

Setting Default Value
AP SSID ESP32_16CH_Timer_Switch
AP Password ESP32-admin
AP Channel 6
NTP Server ph.pool.ntp.org
GMT Offset 28800 (UTC+8)
NTP Sync Interval 1 hour
mDNS Hostname esp32-16ch-relay
Relay Count 16
Active Low True (all relays)
Schedule Days Default Everyday (0x7F)

---

🎯 Use Cases

1. Home Automation - Lighting, fans, appliances scheduling
2. Aquarium Control - Lights, pumps, heaters timed operation
3. Greenhouse Automation - Irrigation, fans, lighting schedules
4. Industrial Timers - Equipment run-time management
5. Security Systems - Automated gate/door controls
6. Energy Management - Load shedding, timer-based shutdown
7. DIY Projects - Any timed relay switching application

---

📝 Notes

· All schedules support overnight operations (start > stop)
· Month-day scheduling allows precise date-based activation
· Browser time sync provides fallback when NTP unavailable
· Manual override persists until manually reset or device reboot
· GPIO changes preserve existing schedules where possible
· Factory reset clears ALL settings including WiFi, schedules, and GPIO config

---

🔄 Version Information

· Configuration Version: 6
· Supports Graceful Upgrades - Old configs migrated automatically
· Backward Compatible - Works with existing schedule data
```

</details>
