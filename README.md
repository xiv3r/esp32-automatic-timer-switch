# Requirements
- ESP32 30P/38P Pins
- 1 -> 16 Channel Relay
- Female Dupont Wire
- Home Wifi for NTP/RTC sync
- 5v 3a Power supply 
- 5vDC Battery (Maintain Power and Timer)(optional)

# Arduino Libraries
- ArduinoJson
- NTPClient

# Installation
- Download the [flasher](https://github.com/xiv3r/esp32-automatic-timer-switch/releases/tag/flasher) and [firmware](https://github.com/xiv3r/esp32-automatic-timer-switch/raw/refs/heads/main/esp32-16ch-firmware-0x0.bin) then flash using `0x0` offset

# WiFi Key
- WiFi SSID: `ESP32_16CH_Timer_Switch`
- Password: `ESP32-admin`

# Activation
- Go to `wifi settings` and connect to your home wifi after the NTP is synchronized everything will work

# Relay Name
- Double click relay name to edit

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
IN9 _____ 26
IN10 _____ 27
IN11 _____ 32
IN12 _____ 33
IN13 _____ 13
IN14 _____ 14
IN15 _____ 4
IN16 _____ 5
GND _____ GND
```

<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/img/esp32-16ch.jpg">

<details><summary>
  
# Full Features
</summary>

## Core Features
```
· 16 independent relays with configurable GPIO pins
· Manual override mode (ON/OFF/Auto) via web interface
· 8 schedules per relay for automated control
· Flexible scheduling:
  · Normal schedules (start < stop within same day)
  · Overnight schedules (start > stop, spans midnight)
  · Always-ON mode (start = stop)
· Custom naming for each relay (up to 15 characters)
· Active-low relay support (configurable)
```

## WiFi Connectivity
```
· Dual-mode operation: Simultaneous STA (client) + AP (access point)
· Non-blocking WiFi reconnection with exponential backoff
· Automatic reconnect (up to 10 attempts, then 5-minute cooldown)
· WiFi network scanner (async, non-blocking)
· Captive portal for easy initial setup
```

## Time Management
```
· NTP synchronization with multiple fallback servers:
  · ph.pool.ntp.org → pool.ntp.org → time.nist.gov → time.google.com
· Internal RTC with drift compensation (EWMA filter)
· Configurable sync interval (1-24 hours)
· Timezone support with GMT and DST offsets
· Persistent time storage across reboots
```

## Web Interface
```
· Responsive HTML5/CSS3 interface (mobile-friendly)
· Five main pages:
  1. Relays - Control and schedule management
  2. WiFi - Network configuration and scanning
  3. Time - NTP and timezone settings
  4. AP - Access point configuration
  5. System - Device info and maintenance
· Real-time status indicators (WiFi, NTP, time)
· Toast notifications for user feedback
· Double-click editing for relay names
```

## Network Services
```
· mDNS/Bonjour support (hostname.local access)
· DNS server for captive portal functionality
· RESTful JSON API for all operations
· Configurable AP with:
  · Channel selection (1-13)
  · Hidden SSID option
  · Password protection (8+ chars or open)
```

## Configuration Storage
```
· Preferences/NVS for persistent storage
· Versioned configuration (v3 main config + v4 extensions)
· Separate extended config for advanced settings
· Factory reset capability
· Automatic corruption recovery
```

## API Endpoints
```
Endpoint Method Purpose
/api/relays GET List all relays and schedules
/api/relay/manual POST Set manual override
/api/relay/reset POST Clear manual override
/api/relay/save POST Save schedules
/api/relay/name POST Update relay name
/api/time GET Current time and status
/api/wifi GET/POST WiFi config and status
/api/wifi/scan POST/GET Network scanning
/api/ntp GET/POST NTP configuration
/api/ntp/sync POST Force NTP sync
/api/ap GET/POST AP configuration
/api/system GET System information
/api/reset POST Restart device
/api/factory-reset POST Factory reset
```

## Safety Features
```
· Safe relay initialization (all OFF at boot)
· Pin validation (ESP32-safe GPIOs only)
· Configuration validation with magic numbers
· Drift compensation bounds (0.90-1.10)
· NTP epoch validation (reject invalid timestamps)
```

## Performance Optimizations
```
· Non-blocking state machines for WiFi and scans
· Efficient JSON parsing with ArduinoJson
· Memory-conscious design (~16KB JSON documents)
· EWMA filtering for RTC drift compensation
· Async WiFi scanning (doesn't block main loop)
```

## System Information Display
```
· STA IP address and connection status
· AP IP address
· Free heap memory
· Uptime counter
· WiFi RSSI with quality indicator
· NTP sync status and age
· Chip model and firmware version
· mDNS hostname and status
```

## Special Features
```
· Overnight schedule detection with visual indicators
· RSSI bars for WiFi signal strength
· Automatic AP restart on configuration changes
· mDNS service advertisement with metadata
· Multiple captive portal probe paths (Apple, Microsoft, Android)
· Schedule conflict resolution (first active schedule wins)
```
</details>
