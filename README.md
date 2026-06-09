# Requirements
- ESP32 38P Pins
- DS3231 RTC Module (recommended)
- 1-16 Channel 5V Relay
- Female to Female Dupont Wire
- Stable Wifi Connection for NTP/RTC sync (without ds3231)
- 5v 3-5a Power supply
  
`Optional`
- 5v UPS (Maintain RTC Time without DS3231)
- Solid State Relay (SSR DC-AC) (High Load Setup)
- mini cooling fan (stay esp32 cool)

# Libraries
- ArduinoJson
- NTPClient
- RTClib 1.14.1

# Installation
> ⚠️ Download and install 
### ESP32 Win/Linux Drivers
- CH340G: https://sparks.gogo.co.nz/ch340.html
- CP2102: https://www.silabs.com/software-and-tools/usb-to-uart-bridge-vcp-drivers?tab=downloads
## Flasher
### Android (otg)
- https://play.google.com/store/apps/details?id=io.serialflow.espflash
### Windows
- https://dl.espressif.com/public/flash_download_tool.zip
### Linux
```sh
esptool --port <PORT> write_flash 0x0 esp32-firmware-0x0.bin
```
### Win/Linux Browser
- https://g3gg0.github.io/esp32_flasher/flasher.html
### Flash firmware 
- Download the [Firmware](https://github.com/xiv3r/esp32-automatic-timer-switch/releases/tag/esp32) and flash.
- Offset address
```
esp32-dump-0x0.bin: 0x0
```

# WiFi Key
- WiFi SSID: `ESP32_16CH_Timer_Switch`
- Password: `ESP32-admin`
  
# Activation
> First time setup

° Online
- Go to `Wifi settings` and connect to your home wifi then everything will work

° Offline
- Go to `Time settings` and click `Sync Browser ` then everything will work

# Relay Naming 
- Double click relay name to edit

# Set Time (country)
> ⚠️ Set to your country time e.g for PH (UTC+8.0) 28800 seconds
- Search your country `gmt offsets in seconds` and paste to the Time -> GMT Offset

# Access
- mDNS:`esp32-16ch-timer-switch.local`
- Captive Portal: `Auto redirect`
- Gateway:`192.168.4.1`
- WAN:`192.168.1.123`
° Global:`Enable Port Forwarding on your router to access anywhere`

# ⚠️ Note
- Avoid connecting to a wrong open wifi network SSID to prevent hang issue. Solution turn off wifi station mode.

# Reset
- Hold BOOT button for 5 seconds to factory reset 

# Restart
- Press EN button to restart

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
- Extra gpio pins
> Max 16 relays, Reserves only for gpio pin reassignment 
```
IN17 _____ 16  Relay 17
IN18 _____ 17  Relay 18
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
<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/libraries/shot1.png">
<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/libraries/shot2.jpg">

<details><summary>
  
# Full Features
</summary>

# ESP32 16-Channel Relay Smart Switch - Complete Documentation

## 📋 Table of Contents
- [Overview](#overview)
- [Features](#features)
- [Hardware Requirements](#hardware-requirements)
- [Pin Configuration](#pin-configuration)
- [Installation & Setup](#installation--setup)
- [Web Interface Guide](#web-interface-guide)
- [API Reference](#api-reference)
- [Configuration System](#configuration-system)
- [Time Management](#time-management)
- [Schedule Engine](#schedule-engine)
- [Self-Healing System](#self-healing-system)
- [GPIO Management](#gpio-management)
- [Factory Reset](#factory-reset)
- [Troubleshooting](#troubleshooting)
- [Technical Specifications](#technical-specifications)

---

## Overview

The **ESP32 16-Channel Relay Smart Switch** is a production-grade automation system designed for home, business, and farm automation. It features:

- **16 independent relay channels** with individual scheduling
- **Web-based configuration interface** accessible via WiFi
- **DS3231 RTC** for timekeeping across power cycles
- **NTP time synchronization** with fallback servers
- **Browser-based time sync** as a last resort
- **Self-healing capabilities** for reliable operation
- **GPIO reconfiguration** without firmware changes

---

## Features

### Core Features
| Feature | Description |
|---------|-------------|
| **16 Relays** | Independent control with 8 schedules per relay |
| **Web Interface** | Full configuration via browser |
| **mDNS Support** | Access via `http://esp32.local` |
| **NTP Sync** | Automatic time synchronization |
| **DS3231 RTC** | Battery-backed real-time clock |
| **Manual Override** | Temporary control without schedule changes |
| **Persistent Storage** | All settings survive power loss |

### Advanced Features
- **Active Low/High Configuration** - Per-relay or global level control
- **Flexible Scheduling** - Daily, weekday, weekend, specific days of month
- **Overnight Schedules** - Support for schedules spanning midnight
- **WiFi Scanning** - Network discovery with automatic pausing of connections
- **Browser Time Sync** - Fallback when NTP unavailable
- **Self-Healing** - Automatic recovery of WiFi, mDNS, DNS, WebServer
- **Critical State Recovery** - Relay states restored after power loss
- **Factory Reset** - Via button or web interface

---

## Hardware Requirements

### Required Components

| Component | Specification | Quantity |
|-----------|--------------|----------|
| **ESP32 Board** | ESP32-WROOM-32 (38-pin) | 1 |
| **Relay Module** | 16-Channel, 5V/12V | 1 |
| **DS3231 RTC** | I2C interface | 1 |
| **Power Supply** | 5V @ 2A (min) | 1 |
| **Jumper Wires** | Female-Female | 20+ |

### Optional Components
- **Capacitors** - 100µF on relay power lines for stability
- **Push Button** - For hardware factory reset (BOOT button built-in)
- **LED Indicators** - For status visualization

### Pin Connections

| ESP32 Pin | Connection |
|-----------|------------|
| GPIO21 | DS3231 SDA |
| GPIO22 | DS3231 SCL |
| GPIO0 | BOOT Button (factory reset) |
| GPIO1-33 | Relays 1-16 (see pin table) |

> **Note**: GPIOs 6-11 are used for internal flash and unavailable.

---

## Pin Configuration

### Default Relay Pins

| Relay | GPIO | Relay | GPIO |
|-------|------|-------|------|
| 1 | 15 | 9 | 23 |
| 2 | 2 | 10 | 13 |
| 3 | 4 | 11 | 14 |
| 4 | 5 | 12 | 27 |
| 5 | 18 | 13 | 26 |
| 6 | 19 | 14 | 25 |
| 7 | 3 | 15 | 33 |
| 8 | 1 | 16 | 32 |

### Customizing Relay Pins

The system supports dynamic GPIO reassignment without recompiling:

1. Navigate to **GPIO Configuration** page
2. Add/remove relays using the dropdown
3. Toggle **Active Level** (LOW/HIGH) per relay
4. Enable **Global Active Mode** for uniform behavior

> **Active Level Explanation**:
> - **Active LOW**: Relay activates when pin is LOW (0V)
> - **Active HIGH**: Relay activates when pin is HIGH (3.3V/5V)

---

## Installation & Setup

### 1. Upload the Firmware

```bash
# Using Arduino IDE
1. Install ESP32 board support
2. Install required libraries:
   - WiFi.h (built-in)
   - WebServer.h (built-in)
   - DNSServer.h (built-in)
   - Preferences.h (built-in)
   - ArduinoJson (version 6)
   - NTPClient (by Fabrice Weinberg)
   - RTClib (by Adafruit)
3. Select ESP32 board (e.g., ESP32 Dev Module)
4. Compile and upload
```

### 2. First Boot

The ESP32 creates a WiFi Access Point:

| Setting | Value |
|---------|-------|
| **SSID** | `ESP32_16CH_Timer_Switch` |
| **Password** | `ESP32-admin` |
| **IP Address** | `192.168.4.1` |

### 3. Initial Configuration

1. Connect to the ESP32's WiFi network
2. Open browser to `http://192.168.4.1`
3. Navigate to **WiFi** page
4. Scan for networks and connect to your WiFi
5. Set your timezone and NTP server
6. Configure relays as needed

### 4. Accessing the Device

After connecting to your network:
- **Local Access**: `http://esp32.local` (mDNS)
- **IP Access**: `http://[ESP32-IP-Address]`

---

## Web Interface Guide

### Navigation Structure

```
┌─────────────────────────────────────────────────────────────┐
│  [Relays]  [WiFi]  [Time]  [AP]  [GPIO]  [System]                      │
├─────────────────────────────────────────────────────────────┤
│                                                                        │
│  Status Indicators:  ● WiFi Status   ● Time Source                     │
│                      Current Time: HH:MM:SS                            │
└─────────────────────────────────────────────────────────────┘
```

### 1. Relays Page

**Features:**
- **Real-time relay status** (ON/OFF/Manual)
- **Manual override buttons** (ON/OFF/Auto)
- **Schedule management** (8 schedules per relay)
- **Double-click relay name to rename**
- **Visual schedule editor**

**Schedule Options:**
- Start/Stop times (down to seconds)
- Days of week selection
- Days of month selection (1-31)
- Enable/disable toggle
- Overnight schedule support

### 2. WiFi Page

**Features:**
- View current connection status
- Scan for available networks
- Enter SSID and password
- RSSI signal strength indicator
- Auto-reconnect on settings change

### 3. Time Page

**Features:**
- NTP server configuration
- GMT offset (seconds)
- Daylight saving offset
- Sync interval (1-24 hours)
- **Manual NTP sync** button
- **Browser time sync** (fallback)

### 4. AP Page

**Features:**
- Change Access Point SSID
- Set AP password (min 8 chars)
- Select WiFi channel (1-13)
- Toggle SSID visibility (hidden mode)

### 5. GPIO Page

**Features:**
- View current relay pins
- Add/remove relay channels
- Toggle Active LOW/HIGH per relay
- Global active mode override
- Reset to default pins

### 6. System Page

**Features:**
- System information display
  - IP addresses (STA & AP)
  - Free heap memory
  - Uptime
  - WiFi RSSI
  - Time source status
  - NTP sync age
  - Drift compensation
  - RTC presence
- Service verification button
- Factory reset button

---

## API Reference

### REST API Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/relays` | Get all relay states and schedules |
| POST | `/api/relay/manual` | Set manual relay state |
| POST | `/api/relay/reset` | Return relay to auto mode |
| POST | `/api/relay/save` | Save schedule configuration |
| POST | `/api/relay/name` | Set relay name |
| GET | `/api/time` | Get current time and status |
| POST | `/api/time/browser-sync` | Sync time from browser |
| GET | `/api/wifi` | Get WiFi configuration |
| POST | `/api/wifi` | Save WiFi settings |
| POST | `/api/wifi/scan` | Start WiFi scan |
| GET | `/api/wifi/scan` | Get scan results |
| GET | `/api/ntp` | Get NTP settings |
| POST | `/api/ntp` | Save NTP settings |
| POST | `/api/ntp/sync` | Trigger NTP sync |
| GET | `/api/ap` | Get AP settings |
| POST | `/api/ap` | Save AP settings |
| GET | `/api/gpio` | Get GPIO configuration |
| POST | `/api/gpio/save` | Save GPIO configuration |
| POST | `/api/gpio/add` | Add relay pin |
| POST | `/api/gpio/delete` | Delete relay pin |
| POST | `/api/gpio/toggle-active-low` | Toggle active level |
| GET | `/api/gpio/global-mode` | Get global active mode |
| POST | `/api/gpio/global-mode` | Set global active mode |
| GET | `/api/system` | Get system information |
| POST | `/api/reset` | Verify services |
| POST | `/api/factory-reset` | Factory reset device |

### Example API Calls

#### Set Relay Manual State
```bash
curl -X POST http://esp32.local/api/relay/manual \
  -H "Content-Type: application/json" \
  -d '{"relay":0,"state":true}'
```

#### Sync Time from Browser
```bash
curl -X POST http://esp32.local/api/time/browser-sync \
  -H "Content-Type: application/json" \
  -d '{"utc_epoch":1700000000}'
```

#### Get Relay Status
```bash
curl http://esp32.local/api/relays
```

#### Response Example
```json
[
  {
    "state": true,
    "manual": false,
    "name": "Living Room Light",
    "pin": 15,
    "schedules": [
      {
        "startHour": 6,
        "startMinute": 0,
        "startSecond": 0,
        "stopHour": 22,
        "stopMinute": 0,
        "stopSecond": 0,
        "enabled": true,
        "days": 62,
        "monthDays": 0
      }
    ]
  }
]
```

---

## Configuration System

### NVS Storage (Preferences)

All settings are stored in ESP32's Non-Volatile Storage (NVS):

| Configuration | Size | Description |
|---------------|------|-------------|
| `sysConfig` | ~150 bytes | System settings (WiFi, NTP, RTC) |
| `extConfig` | ~40 bytes | Extended settings (channel, sync interval) |
| `relayConfigs` | ~3KB | 16 relay configurations with schedules |
| `gpioConfig` | ~68 bytes | GPIO pin mapping |
| `criticalState` | ~44 bytes | Last known relay states |

### System Configuration Structure

```cpp
struct SystemConfig {
    uint16_t magic;           // Validation (0x1234)
    uint8_t version;          // Config version (9)
    char sta_ssid[32];        // WiFi SSID
    char sta_password[64];    // WiFi password
    char ap_ssid[32];         // AP SSID
    char ap_password[32];     // AP password
    char ntp_server[48];      // NTP server
    int32_t gmt_offset;       // GMT offset (seconds)
    int32_t daylight_offset;  // DST offset (seconds)
    time_t last_rtc_epoch;    // Last RTC epoch
    float rtc_drift;          // Drift compensation
    char hostname[32];        // mDNS hostname
};
```

### Extended Configuration

```cpp
struct ExtConfig {
    uint8_t magic;            // Validation (0xEC)
    uint8_t ap_channel;       // AP channel (1-13)
    uint8_t ntp_sync_hours;   // NTP sync interval
    uint8_t ap_hidden;        // Hidden SSID flag
    uint8_t global_active_mode; // 0=per-relay, 1=global LOW, 2=global HIGH
    uint8_t reserved[27];     // Future use
};
```

---

## Time Management

### Time Source Hierarchy

```
┌─────────────────────────────────────────────────────────────┐
│                    TIME SOURCE PRIORITY                     │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   1. NTP Server     ←── Highest accuracy                    │
│         ↓                                                   │
│   2. DS3231 RTC     ←── Battery-backed, survives reboot    │
│         ↓                                                   │
│   3. Browser Sync   ←── Manual fallback                     │
│         ↓                                                   │
│   4. Internal RTC   ←── Least accurate                     │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Drift Compensation

The system automatically calculates and applies drift compensation:

- Compares NTP time against internal RTC over time
- Maintains running average of drift (0.95-1.05 range)
- Adjusts time calculations dynamically
- Saves drift value to NVS for persistence

### RTC Functions

| Function | Description |
|----------|-------------|
| `initRTC()` | Initialize DS3231 on I2C (GPIO21/22) |
| `syncInternalRTC()` | Set internal time from external source |
| `getCurrentEpoch()` | Get current UTC epoch with drift |
| `immediateDS3231Sync()` | Force DS3231 update |
| `performRTCReabase()` | Rebase internal RTC for drift correction |

### NTP Fallback Servers

```cpp
static const char* NTP_SERVERS[] = {
    "time.google.com",
    "time.windows.com", 
    "time.cloudflare.com",
    "time.facebook.com"
};
```

---

## Schedule Engine

### Schedule Structure

Each relay has **8 independent schedules** with:

```cpp
struct TimerSchedule {
    uint8_t startHour[8];     // Start hour (0-23)
    uint8_t startMinute[8];   // Start minute (0-59)
    uint8_t startSecond[8];   // Start second (0-59)
    uint8_t stopHour[8];      // Stop hour (0-23)
    uint8_t stopMinute[8];    // Stop minute (0-59)
    uint8_t stopSecond[8];    // Stop second (0-59)
    bool enabled[8];          // Schedule enabled flag
    uint8_t days[8];          // Day of week bitmask (0-127)
    uint32_t monthDays[8];    // Day of month bitmask (0-0x7FFFFFFF)
};
```

### Day of Week Bitmask

| Day | Bit | Value | Macro |
|-----|-----|-------|-------|
| Sunday | 0 | 1 << 0 | `DAY_SUNDAY` |
| Monday | 1 | 1 << 1 | `DAY_MONDAY` |
| Tuesday | 2 | 1 << 2 | `DAY_TUESDAY` |
| Wednesday | 3 | 1 << 3 | `DAY_WEDNESDAY` |
| Thursday | 4 | 1 << 4 | `DAY_THURSDAY` |
| Friday | 5 | 1 << 5 | `DAY_FRIDAY` |
| Saturday | 6 | 1 << 6 | `DAY_SATURDAY` |

**Predefined Masks:**
- `DAY_ALL = 0x7F` (Every day)
- `DAY_WEEKDAYS = 0x3E` (Monday-Friday)
- `DAY_WEEKENDS = 0x41` (Saturday-Sunday)

### Month Day Bitmask

- Bit 0 = Day 1
- Bit 1 = Day 2
- ...
- Bit 30 = Day 31

**Example**: `0x00000001` = Only 1st day of month

### Schedule Logic

```
IF schedule enabled AND day matches AND month day matches THEN
    IF start_time == stop_time:
        → ALWAYS ON during matching days
    ELSE IF start_time < stop_time:
        → ON between start and stop (same day)
    ELSE:
        → ON overnight (start → midnight → stop)
```

### Overnight Schedule Example

| Setting | Value |
|---------|-------|
| Start Time | 22:00:00 |
| Stop Time | 06:00:00 |
| Days | Monday-Friday |
| Result | ON from 10 PM to 6 AM next day |

### Schedule Processing

- **Cache Interval**: 1 second (lightweight time checks)
- **Process Interval**: 250ms (state evaluation)
- **Debounce Time**: 500ms (prevents relay chatter)

---

## Self-Healing System

### Monitored Components

| Component | Recovery Action | Interval |
|-----------|----------------|----------|
| **WiFi** | Auto-reconnect with backoff | 10 seconds |
| **mDNS** | Service re-announcement | 60 seconds |
| **DNS** | Live reconfiguration | 60 seconds |
| **WebServer** | Health verification | 30 seconds |
| **AP** | Restart if IP is 0.0.0.0 | As needed |
| **NTP** | Server rotation on failure | 30 seconds |
| **RTC** | Re-initialization | As needed |

### Critical State Protection

The system saves relay states and manual overrides to NVS:

- **Magic Value**: `0xDEADBEEF` (validation)
- **Checksum**: XOR of states with timestamp
- **Save Frequency**: Every 5 minutes if changed
- **Restoration**: On power-up or after recovery

### Recovery Logic

```cpp
// Simplified recovery flow
if (WiFi disconnected && !connecting && !scanning) {
    health.wifiFailures++;
    if (health.wifiFailures >= 3) {
        WiFi.begin(ssid, password);  // Reconnect
    }
}

if (mDNS not announced in 5 minutes) {
    MDNS.addService("http", "tcp", 80);  // Re-announce
}

// Full health check every 30 minutes
if (now - lastFullHealthCheck > 30 minutes) {
    verifyRelayStates();
    saveCriticalState();
}
```

---

## GPIO Management

### Valid GPIO Pins

The ESP32 has specific pins suitable for relay control:

| Available | Unavailable |
|-----------|-------------|
| 1, 2, 3, 4, 5 | 6, 7, 8, 9, 10, 11 |
| 12, 13, 14, 15 | 24 (Crystal) |
| 18, 19, 21, 22 | 28, 29, 30, 31 (ADC2) |
| 23, 25, 26, 27 | (ESP32-WROOM-32 specific) |
| 32, 33 | |

> **Note**: GPIO21 and GPIO22 are reserved for DS3231 RTC I2C.

### Global Active Mode

| Mode | Value | Behavior |
|------|-------|----------|
| Per-Relay | 0 | Each relay uses its own activeLow setting |
| Global LOW | 1 | All relays active when pin LOW |
| Global HIGH | 2 | All relays active when pin HIGH |

### Active Level Examples

```
Active LOW (Default):
- To turn ON relay: digitalWrite(pin, LOW)
- To turn OFF relay: digitalWrite(pin, HIGH)

Active HIGH:
- To turn ON relay: digitalWrite(pin, HIGH)
- To turn OFF relay: digitalWrite(pin, LOW)
```

### Adding/Removing Relays

1. **Add Relay**: New channel appended with default settings
2. **Remove Relay**: Shifts higher-numbered relays down
3. **Schedule Preservation**: Schedules stay with their original relay number
4. **Maximum Relays**: 16 (hardware limit)

---

## Factory Reset

### Method 1: Hardware Button (BOOT/GPIO0)

1. **Press and hold** the BOOT button
2. **Continue holding for 5 seconds**
3. **Release** when you see serial output confirming reset
4. Device reboots with factory defaults

### Method 2: Web Interface

1. Navigate to **System** page
2. Click **Factory Reset** button
3. Confirm the action
4. Device resets without restarting

### What Gets Reset

| Item | Reset Behavior |
|------|----------------|
| WiFi Credentials | Cleared (AP mode enabled) |
| AP Settings | Restored to defaults |
| NTP Settings | Restored to defaults |
| Relay Configurations | All schedules cleared |
| GPIO Pins | Restored to default mapping |
| mDNS Hostname | Reset to "esp32" |
| Relay Names | Reset to "Relay X" |
| Manual Overrides | Cleared |
| RTC Drift | Reset to 1.0 |

### What Is Preserved

> **Nothing** - Factory reset is a complete wipe of all user settings.

---

## Troubleshooting

### Common Issues and Solutions

#### WiFi Connection Fails

| Symptom | Solution |
|---------|----------|
| Wrong password | Use **Scan** feature to verify network exists |
| Hidden SSID | Enter SSID manually in text field |
| 2.4GHz only | ESP32 doesn't support 5GHz networks |
| Signal too weak | Check RSSI value (should be > -80dBm) |
| DHCP issues | Static IP not supported (DHCP only) |

#### Time Won't Sync

| Symptom | Solution |
|---------|----------|
| NTP fails | Use **Sync Browser** button as fallback |
| Wrong timezone | Verify GMT offset (seconds, not hours) |
| DST not applying | Set daylight_offset manually |
| RTC not detected | Check I2C wiring (GPIO21=SDA, GPIO22=SCL) |

#### Relays Not Responding

| Symptom | Solution |
|---------|----------|
| Wrong active level | Check GPIO page for Active LOW/HIGH setting |
| Manual override active | Click **Auto** button to release |
| Schedule not enabled | Verify checkbox is checked |
| Wrong day/month | Check day of week and month day selections |
| Pin conflict | Ensure GPIO not used by other hardware |

#### Web Interface Issues

| Symptom | Solution |
|---------|----------|
| Cannot connect | Use AP mode: Connect to ESP32_16CH_Timer_Switch |
| mDNS not working | Access via IP address directly |
| Slow response | Check free heap on System page (<20KB = issue) |
| API timeout | Large schedule data takes time to serialize |

### LED Status Indicators

| Indicator | Color | Meaning |
|-----------|-------|---------|
| WiFi Dot | Green | Connected to WiFi |
| WiFi Dot | Red | WiFi disconnected |
| Time Source Dot | Green | Time from NTP |
| Time Source Dot | Blue | Time from Browser/RTC |
| Time Source Dot | Yellow | No time source |

### Serial Debug Output

```cpp
// Enable serial debugging (115200 baud)
// Monitor output includes:
- Boot sequence information
- WiFi connection status
- NTP sync attempts
- RTC detection
- GPIO configuration
- Memory warnings (<20KB free)
- Recovery actions
```

---

## Technical Specifications

### System Specifications

| Parameter | Value |
|-----------|-------|
| **Max Relays** | 16 |
| **Schedules per Relay** | 8 |
| **Schedule Granularity** | Seconds |
| **NTP Sync Interval** | 1-24 hours (configurable) |
| **Internal Time Drift** | 0.95 - 1.05 (auto-compensated) |
| **Schedule Cache Interval** | 1 second |
| **Relay Debounce Time** | 500ms |
| **Web Server Port** | 80 |
| **DNS Server Port** | 53 |

### Memory Usage

| Component | Approximate Size |
|-----------|------------------|
| Program Flash | ~250 KB |
| Global Variables | ~15 KB |
| NVS Storage | ~3.5 KB |
| Heap (idle) | ~180 KB |
| Minimum Free Heap | Configurable warning (20KB) |

### Network Specifications

| Protocol | Details |
|----------|---------|
| **WiFi Modes** | STA + AP simultaneously |
| **AP Channel** | 1-13 (default: 6) |
| **AP IP** | 192.168.4.1 |
| **mDNS Service** | `http://esp32.local` |
| **TCP Timeout** | 10 seconds |
| **Max WiFi Clients** | 4 |

### Timing Constants

| Operation | Timeout/Interval |
|-----------|------------------|
| WiFi Connection | 20 seconds |
| NTP Request | 5 seconds |
| NTP Retry | 30 seconds |
| WiFi Check | 10 seconds |
| RTC Sync | 60 seconds |
| RTC Rebase | 5 minutes |
| NTP Sync Interval | 1 hour (default) |
| Memory Cleanup | 30 seconds |
| Critical State Save | 5 minutes (if dirty) |

### Pin Constraints

| GPIO | Constraint |
|------|------------|
| 0 | Boot button (factory reset) |
| 1 | UART0 TX (debug) |
| 2 | Onboard LED |
| 3 | UART0 RX |
| 6-11 | Internal flash |
| 12 | Boot mode (pull-down) |
| 21 | I2C SDA (DS3231) |
| 22 | I2C SCL (DS3231) |

---

## Version History

| Version | Changes |
|---------|---------|
| **v9** | Current release |
| | - Global active mode support |
| | - Browser time sync fallback |
| | - Self-healing system enhancements |
| | - Critical state protection |
| | - Dynamic GPIO configuration |
| | - Month-day schedule support |
| | - Overnight schedule support |
| | - WiFi scan pause system |

---

## Credits

- **Author**: Raff Alds
- **GitHub**: [https://github.com/xiv3r](https://github.com/xiv3r)
- **License**: Open source

---

</details>

# Customize GPIO Pins
> Find this line inside sketch.ino
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
> You can add more gpio pins like `16, 17` in int validPins.
```
// Change GPIO PIN
int validPins[] = {15, 2, 4, 16, 17, 5, 18, 19, 3, 1, 23, 13, 14, 27, 26, 25, 33, 32};
```

# Build Firmware
>  Auto Build firmware binaries using github action
- https://github.com/xiv3r/arduino

