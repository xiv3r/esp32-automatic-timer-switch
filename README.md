# Requirements
- ESP32 30P/38P Pins
- 1 -> 16 Channel Relay
- Female Dupont Wire
- Home Wifi for NTP/RTC sync
- 5vDC Battery (Maintain Power and Timer)(optional)

# Arduino Libraries
- ArduinoJson
- NTPClient

# Installation
- Download the [flasher](https://github.com/xiv3r/esp32-automatic-timer-switch/releases/tag/flasher) and [firmware](https://github.com/xiv3r/esp32-automatic-timer-switch/raw/refs/heads/main/esp32-8ch-timer-switch-firmware-0x0.bin) then flash using `0x0` offset

# WiFi Key
`Note: Take note your wifi credentials reset doesn't supported (anti-reset protection)` 
- WiFi SSID: `ESP32_16CH_Timer_Switch`
- Password: `ESP32-admin`

# Activation
- Go to `wifi settings` and connect to your home wifi after the NTP is synchronized everything will works.

# GPIO Connection 
```
RELAY     ESP32 30P/38P
VCC _____ 5VIN 
IN1 _____ 16
IN2 _____ 17
IN3 _____ 18
IN4 _____ 19
IN5 _____ 21
IN6 _____ 22
IN7 _____ 23
IN8 _____ 25
- Extra Relay
IN1 _____ 26
IN2 _____ 27
IN3 _____ 32
GND _____ GND
```

<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/img/esp32_dashboard.jpg">

<details><summary>
  
# Full Features
</summary>

## 🔌 Relay Control System
· 8 Independent Relay Channels - GPIO pin configurable

· Active LOW/HIGH Support - Configurable relay trigger logic

· Manual Override Mode - Direct ON/OFF control bypassing schedules

· Auto Mode - Returns to scheduled operation

· Real-time Status Display - Live relay state monitoring

## ⏰ Timer & Scheduling
· 4 Independent Schedules per Relay - Up to 64 total schedules system-wide

· Precise Time Control - Hour, minute, and second granularity

· Overnight Scheduling - Supports schedules crossing midnight

. Per-Schedule Enable/Disable - Individual schedule activation

· Automatic Schedule Processing - Runs continuously in background

## 🌐 Network & Connectivity
· WiFi Station Mode - Connects to existing WiFi networks

· Access Point Mode - Creates its own WiFi network for configuration

· Dual-Mode Operation - Runs both Station and AP simultaneously

· Captive Portal - Automatic redirect to configuration page

· DNS Server - Handles all DNS requests for easy access

## 🕐 Time Synchronization (NTP/RTC)
· NTP Client - Syncs time from internet time servers· Customizable NTP Server - Configure any NTP server

· Timezone Support - GMT offset and daylight saving configuration

· RTC Emulation - Stores last known time in EEPROM

· Cold Boot Time Recovery - Restores time without internet connection

· Automatic Time Sync - Regular NTP updates when connected

## 💾 Persistent Storage (EEPROM)
. Configuration Backup - All settings survive power cycles

· Version Migration - Automatic upgrades from older config versions

· Magic Number Validation - Detects corrupted/invalid configurations

. Stored Settings Include:
  · WiFi credentials (SSID & password)
  · AP credentials (SSID & password)
  · NTP server configuration
  · Timezone offsets
  · Last successful NTP sync timestamp
  · All 8 relay configurations and schedules

## 🌍 Web Interface
· Responsive Design - Works on desktop, tablet, and mobile

· 4 Main Pages:
  1. Relay Control Dashboard - Main interface for all 16 relays
  2. WiFi Settings - Configure station network connection
  3. AP Settings - Configure access point credentials
  4. NTP/RTC Settings - Configure time synchronization

## 📱 Web Interface Features
. Real-time Clock Display - Live updating time in header

· Visual Status Indicators - Color-coded ON/OFF states

· Manual Control Buttons - Direct relay control

· Schedule Configuration - Intuitive time input for each schedule

· Checkbox Enable/Disable - Quick schedule activation

· Save per Relay - Individual relay configuration saving

· Auto-Refresh - Periodic status updates (every 60 seconds)

· Notification System - Success/error feedback messages

· Input Validation - Client-side time range validation

## 🔧 API Endpoints
Endpoint Method Purpose
```
/api/relays GET Retrieve all relay states and schedules
/api/relay/manual POST Set manual override state
/api/relay/reset POST Cancel manual override
/api/relay/save POST Save schedule configuration
/api/time GET Get current system time
/api/wifi GET/POST Get/Set WiFi station settings
/api/ap GET/POST Get/Set Access Point settings
/api/ntp GET/POST Get/Set NTP configuration
/api/ntp/sync POST Force immediate NTP sync
```

## 🛡️ Security Features
· Password Protection - AP password with minimum 8 characters

· Open Network Option - Blank password for open AP

· Password Not Exposed - API doesn't return stored passwords

· Input Validation - Server-side validation for all settings

## ⚙️ Configuration Management
· Factory Defaults - Automatic initialization on first boot

· Configuration Migration - Handles version upgrades gracefully

· Automatic Restart - After WiFi/AP configuration changes

· Serial Debug Output - Comprehensive logging for troubleshooting

## 🔄 System Features
· Non-blocking Operation - Schedules process without interrupting web server

· Dual-core ESP32 Support - Efficient multitasking

· Graceful Degradation - Continues operation without WiFi

· Watchdog Friendly - Short delay() calls prevent resets

## 📊 Technical Specifications
· 8 Relays with 4 schedules each = 32 total schedules

· EEPROM Storage: 2048 bytes

· Configuration Version: 2 (with migration from v1)

· Supported NTP Servers: Any standard NTP pool/server

· Default Timezone: Philippines GMT+8 (28800 seconds)

· Default AP: ESP32_8CH_Smart_Switch / ESP32-admin

## 🎯 Use Cases
· Home automation lighting control

· Irrigation/sprinkler systems

· Aquarium/terrarium lighting schedules

· Industrial equipment timing

· Holiday decoration timing

· Greenhouse environmental control

· Security lighting schedules

· Energy management systems
</details>
