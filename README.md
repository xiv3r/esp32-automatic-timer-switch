# About
This is ESP32 smart relay controller, designed to manage 20 independent relay channels with advanced scheduling capabilities. The system operates as a standalone device with its own Wi-Fi access point and web-based control interface, making it suitable for industrial automation, home automation, greenhouse control, lighting systems, and other applications requiring precise timed control of multiple electrical circuits.


# Requirements
- ESP32 38P Pins
- 1 -> 20 Channel Relay
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
firmware: 0x0
```

# WiFi Key
- WiFi SSID: `ESP32_20CH_Timer_Switch`
- Password: `ESP32-admin`

# Activation
- Go to `wifi settings` and connect to your home wifi after the NTP is synchronized everything will work

# Relay Name
- Double click relay name to edit

# Access
° Direct Access
- mDNS:`esp32-20ch-timer.local`
- Captive Portal: Auto redirect
- Gateway:`192.168.4.1`
- WAN:`192.168.1.123`
  
° Global:`Enable Port Forwarding on your router to access anywhere`

# 20 Channel GPIO Connection 
```
RELAY    ESP32 38P
VCC  _____ 5VIN 
IN1  _____ 15
IN2  _____ 2
IN3  _____ 4
IN4  _____ 16
IN5  _____ 17
IN6  _____ 5
IN7  _____ 18
IN8  _____ 19
IN9  _____ 21
IN10 _____ RX 
IN11 _____ TX
IN12 _____ 22
IN13 _____ 23
IN14 _____ 13
IN15 _____ 14
IN16 _____ 27
IN17 _____ 26
IN18 _____ 25
IN19 _____ 33
IN20 _____ 32
GND  _____ GND
```

<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/libraries/20ch.png">
<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/libraries/20ch-1.png">

<details><summary>
  
# Full Features
</summary>

## 🔌 Hardware Features

### Relay Configuration
- **20 Independent Channels** controlling 20 relays via dedicated GPIO pins
- **Active-Low Operation** support (configurable)
- **Pin Mapping**: GPIOs 15, 2, 4, 16, 17, 5, 18, 19, 21, 3, 1, 22, 23, 13, 14, 27, 26, 25, 33, 32
- **Safe Boot State**: All relays default to OFF during initialization

---

## ⏰ Scheduling & Timer Features

### Per-Relay Scheduling
- **8 Independent Schedules** per relay channel
- **Time-of-Day Control**: Precise start/stop times with second-level granularity
- **Day-of-Week Selection**: Individual day toggles (Sun-Sat) with presets:
  - Everyday (0x7F)
  - Weekdays (0x3E) 
  - Weekends (0x41)
- **Day-of-Month Selection**: 31-day bitmap for monthly scheduling patterns
- **Overnight Support**: Automatic handling of schedules spanning midnight (start > stop)

### Schedule States
- **Standard Schedule**: Start time < Stop time (same-day operation)
- **Overnight Schedule**: Start time > Stop time (spans midnight)
- **24/7 Always ON**: Start time = Stop time with schedule enabled

---

## 🌐 Network Features

### WiFi Connectivity
- **Dual-Mode Operation**: Simultaneous AP + Station (AP_STA mode)
- **Station Mode**: Connect to existing WiFi networks
- **Automatic Reconnection**: Non-blocking reconnection with backoff strategy
  - Maximum 10 reconnection attempts
  - 5-minute cooldown after failed attempts
- **WiFi Network Scanner**: Async scanning with timeout protection
- **Signal Strength Display**: RSSI visualization in web interface

### Access Point (AP)
- **Built-in Hotspot**: ESP32_20CH_Timer_Switch (default)
- **Customizable**: Configurable SSID, password, channel (1-13)
- **Hidden SSID Support**: Option to hide network broadcast
- **Open Network Support**: Passwordless operation available
- **Captive Portal**: Automatic redirection for easy configuration
  - Supports Android, iOS, Windows captive portal detection
  - DNS-based redirection (port 53)

### mDNS (Bonjour/Avahi)
- **Local Network Discovery**: `esp32-20ch-relay.local` (default)
- **Dynamic Hostname**: Auto-generated or custom hostnames
- **Service Advertisement**: HTTP service with metadata
- **Hot Restart**: mDNS service restart without device reboot

---

## 🕐 Time Synchronization

### NTP Client
- **Primary Server**: `ph.pool.ntp.org` (default, configurable)
- **Fallback Chain**: 
  1. Custom/primary server
  2. `pool.ntp.org`
  3. `time.nist.gov`
  4. `time.google.com`
- **Automatic Failover**: Rotates through servers on failure
- **Configurable Sync Interval**: 1-24 hours
- **Manual Sync Trigger**: Force synchronization via web UI
- **GMT Offset**: Configurable timezone offset (default: UTC+8/28800s)
- **Daylight Saving**: Secondary offset for DST adjustments

### Internal RTC
- **Drift Compensation**: Adaptive algorithm with ±10% bounds
- **Persistent Storage**: Epoch and drift saved to NVS
- **Millisecond Precision**: Internal timekeeping between NTP syncs
- **Graceful Degradation**: Continues operation during network outages

---

## 💾 Data Management

### Preferences/NVS Storage
- **Configuration Persistence**: All settings survive power cycles
- **Version Migration**: Automatic schema updates (currently v5)
- **Backward Compatibility**: Loads older configuration formats
- **Factory Reset**: Complete NVS clearing option

### Stored Configurations
- **SystemConfig**: WiFi credentials, NTP settings, RTC state
- **ExtConfig**: AP channel, sync interval, hidden SSID flag
- **RelayConfigs**: 20× schedules with names and states
- **Magic Number Validation**: Prevents corrupted data loading

---

## 🎛️ Control Features

### Relay Control Modes
- **Automatic Mode**: Schedule-based operation (default)
- **Manual Override**: Direct ON/OFF control via web interface
- **Named Relays**: Custom 15-character names (double-click to edit)

### Web Interface Controls
- **Per-Relay Buttons**: ON/OFF/Auto for each channel
- **Bulk Schedule Editor**: Visual day/month toggles
- **Real-time Status**: Live relay state indicators
- **Toast Notifications**: Operation feedback messages

---

## 🌍 Web Interface

### Responsive Design
- **Mobile-First Layout**: Optimized for phones and tablets
- **Adaptive Grid**: Auto-fill relay cards (340px minimum)
- **Sticky Header**: Navigation always accessible
- **Dark-Friendly**: High contrast color scheme

### Pages & Sections
1. **Relays Dashboard** (`/`)
   - 20 relay cards with status badges
   - Schedule editor per relay
   - Manual control buttons
   
2. **WiFi Settings** (`/wifi`)
   - Network scanner with RSSI bars
   - Connection status display
   - Credential management

3. **Time Settings** (`/ntp`)
   - NTP server configuration
   - Timezone/GMT offset
   - Sync interval control
   - Manual sync trigger

4. **AP Settings** (`/ap`)
   - Hotspot SSID/password
   - Channel selection
   - Visibility toggle

5. **System Info** (`/system`)
   - Network diagnostics
   - Resource monitoring
   - Device restart/reset

### Real-Time Updates
- **1-Second Clock**: Live time display in header
- **Status Indicators**: WiFi (green/red), NTP (green/yellow) dots
- **Auto-Refresh**: Relay states update every 60 seconds

---

## 🔧 API Endpoints

### Relay Management
| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/relays` | GET | Get all relay states and schedules |
| `/api/relay/manual` | POST | Set manual ON/OFF state |
| `/api/relay/reset` | POST | Return to automatic mode |
| `/api/relay/save` | POST | Save schedule configuration |
| `/api/relay/name` | POST | Update relay name |

### Network & System
| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/time` | GET | Current time and sync status |
| `/api/wifi` | GET/POST | WiFi configuration |
| `/api/wifi/scan` | POST/GET | WiFi network scanning |
| `/api/ntp` | GET/POST | NTP settings management |
| `/api/ntp/sync` | POST | Force NTP synchronization |
| `/api/ap` | GET/POST | Access point configuration |
| `/api/mdns` | GET/POST | mDNS hostname management |
| `/api/mdns/restart` | POST | Restart mDNS service |
| `/api/system` | GET | System diagnostics |
| `/api/reset` | POST | Restart device |
| `/api/factory-reset` | POST | Full configuration erase |

---

## 🛡️ Safety & Reliability

### Hardware Protection
- **Safe Initialization**: All pins set to off state before setup
- **Watchdog Friendly**: Non-blocking operations throughout
- **Timeout Protection**: WiFi scan (10s), connection (15s), NTP retry intervals

### Software Resilience
- **Configuration Validation**: Magic number checking prevents corruption
- **Graceful Fallbacks**: Default values when configs are invalid
- **Memory-Safe JSON**: StaticJsonDocument with appropriate sizes
- **Error Handling**: Comprehensive try-catch across API handlers

---

## 📊 Monitoring & Diagnostics

### System Metrics
- **Free Heap Memory**: Real-time RAM usage
- **Uptime Counter**: Device runtime tracking
- **WiFi RSSI**: Signal strength with quality descriptions
- **NTP Sync Age**: Time since last synchronization
- **Chip Information**: ESP32 model identification

### Debug Features
- **Structured Logging**: Clear magic numbers and version tracking
- **State Machine States**: WifiConnState for connection debugging
- **Scan Status**: Async scan progress reporting
- **Drift Monitoring**: RTC compensation factor tracking

---

## 🔄 Advanced Features

### Power Management
- **WiFi Power Save**: Implicit through non-blocking operations
- **Connection Backoff**: Exponential-like retry delays
- **Resource Optimization**: Minimal polling intervals (100ms RTC, 5s WiFi check)

### Security
- **Password Protection**: Configurable AP authentication (WPA2)
- **Hidden SSID**: Reduce network visibility
- **No Hardcoded Credentials**: All settings user-configurable
- **Captive Portal Isolation**: Limited attack surface

### Extensibility
- **JSON API**: Easy integration with home automation systems
- **mDNS Discovery**: Zero-configuration network finding
- **Version Tracking**: Forward-compatible configurations
- **Modular Design**: Clear separation of concerns in code

---

## 📱 User Experience

### Setup Flow
1. Power on → Default AP activates
2. Connect to `ESP32_20CH_Timer_Switch`
3. Captive portal redirects to configuration
4. Configure WiFi, timezone, schedules
5. Device connects to network and syncs time

### Daily Operation
- Schedules run automatically
- Manual overrides via web/mobile browser
- Status monitoring at a glance
- Zero-touch after initial configuration

### Recovery Options
- Factory reset via web interface
- Automatic reconnection after WiFi drops
- Fallback AP always available
- RTC continues during network outages

</details>

## ESP32 All Board Support (Models & Variations)
- Find and change from [esp32_generic_sketch.ino](https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/esp32-generic-sketch.ino) before compiling
```
#define NVS_NAMESPACE "relay20"  // changed 20 to the number of relays supported by your esp32 board 
```
```
#define NUM_RELAYS 20 // Change 20 to the number of relays supported by your esp32 board 
const int relayPins[NUM_RELAYS] = { // Reduce or Add GPIO based on your esp32 board
 15, // IN1
  2,  // IN2
  4,  // IN3
 16, // IN4
 17, // IN5
  5,  // IN6
 18, // IN7
 19, // IN8
 21, // IN9
  3,  // IN10 (RX)
  1,  // IN11 (TX)
 22, // IN12
 23, // IN13
 13, // IN14
 14, // IN15
 27, // IN16
 26, // IN17
 25, // IN18
 33, // IN19
 32  // IN20
};
```
```
MDNS.addServiceTxt("http", "tcp", "channels", "20");  // changed 20 to the number of relays supported by your esp32 board 
```
