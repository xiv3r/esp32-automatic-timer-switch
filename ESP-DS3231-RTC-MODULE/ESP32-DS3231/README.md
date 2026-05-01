# Requirements
- ESP32 38P Pins
- 1 -> 18 Channel Relay
- DS3232 RTC Module
- Female Dupont Wire
- Home Wifi for NTP/RTC sync
- 5v 5-8a Power supply

# Arduino Libraries
- ArduinoJson
- Preferences 
- NTPClient
- RTClib 1.14.1

# Installation
- Download the [flasher](https://github.com/xiv3r/esp32-automatic-timer-switch/releases/tag/flasher) and firmware and then flash.

- offset address
```
firmware  : 0x0
```

# WiFi Key
- WiFi SSID: `ESP32_18CH_Timer_Switch`
- Password: `ESP32-admin`

# Activation
- Go to `wifi settings` and connect to your home wifi after the NTP is synchronized everything will work

# Relay Name
- Double click relay name to edit

# Access
° Direct Access
- mDNS:`esp32-18ch-timer.local`
- Captive Portal: Auto redirect
- Gateway:`192.168.4.1`
- WAN:`192.168.1.123`
  
° Global:`Enable Port Forwarding on your router to access anywhere`

# 18-Channel GPIO Connection 
```
RELAY     ESP32 38P
VCC  _____ 5VIN 
IN1  _____ 15
IN2  _____ 2
IN3  _____ 4
IN4  _____ 16
IN5  _____ 17
IN6  _____ 5
IN7  _____ 18
IN8  _____ 19
IN9  _____ RX
IN10 _____ TX
IN11 _____ 23
IN12 _____ 13
IN13 _____ 14
IN14 _____ 27
IN15 _____ 26
IN16 _____ 25
IN17 _____ 33
IN18 _____ 32
GND  _____ GND
```

# DS3231 RTC GPIO
```
RTC | ESP32 30/38P
SDA → 21
SCL → 22
VCC → 3.3V
GND → GND
```

# Full Features 

## 🔌 Hardware Features

### Relay Configuration
- **18 Independent Channels** controlling 18 relays via dedicated GPIO pins
- **Active-Low Operation** support (configurable)
- **Pin Mapping**: GPIOs 15, 2, 4, 16, 17, 5, 18, 19, 3, 1, 23, 13, 14, 27, 26, 25, 33, 32
- **I2C Reserved Pins**: GPIO21 (SDA) and GPIO22 (SCL) dedicated to DS3231
- **Safe Boot State**: All relays default to OFF during initialization

### DS3231 Hardware RTC
- **High Precision**: Temperature-compensated crystal oscillator (±2ppm accuracy)
- **Battery Backup**: Maintains time during power outages (CR2032 battery)
- **Automatic Power-Loss Detection**: Sets compile time if RTC lost power
- **I2C Interface**: Connected via GPIO21 (SDA) and GPIO22 (SCL)
- **Automatic Temperature Compensation**: ±0.432 sec/day typical accuracy

---

## ⏰ Time Synchronization & Scheduling

### Multi-Source Time Architecture
- **Priority 1**: DS3231 Hardware RTC (battery-backed, always available)
- **Priority 2**: Internal Software RTC (NTP-synced with drift compensation)
- **Failure Mode**: Graceful degradation through priority hierarchy

### DS3231 Integration
- **Periodic Sync**: Software RTC synced from DS3231 every 60 seconds
- **NTP → DS3231 Update**: Hardware RTC updated with NTP time for maximum accuracy
- **Power-Loss Recovery**: Automatic compile-time setting if RTC lost power
- **Seamless Operation**: No internet required for accurate timekeeping

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
- **Millisecond Precision**: Internal timekeeping between syncs
- **Dual-Source Validation**: Cross-reference between NTP and DS3231

---

## 📅 Per-Relay Scheduling

### Time-Based Control
- **8 Independent Schedules** per relay channel
- **Time-of-Day Precision**: Second-level granularity for start/stop times
- **Day-of-Week Selection**: Individual day toggles (Sun-Sat) with presets:
  - Everyday (0x7F)
  - Weekdays (0x3E) 
  - Weekends (0x41)
- **Day-of-Month Selection**: 31-day bitmap for monthly scheduling patterns
- **Overnight Support**: Automatic handling of schedules spanning midnight

### Schedule States
- **Standard Schedule**: Start time < Stop time (same-day operation)
- **Overnight Schedule**: Start time > Stop time (spans midnight)
- **24/7 Always ON**: Start time = Stop time with schedule enabled
- **Combined Logic**: Day-of-week AND day-of-month filtering

---

## 🌐 Network Features

### WiFi Connectivity
- **Dual-Mode Operation**: Simultaneous AP + Station (AP_STA mode)
- **Station Mode**: Connect to existing WiFi networks
- **Automatic Reconnection**: Non-blocking reconnection with backoff strategy
  - Maximum 10 reconnection attempts
  - 5-minute cooldown after failed attempts
- **WiFi Network Scanner**: Async scanning with 10-second timeout
- **Signal Strength Display**: RSSI visualization in web interface
- **Connection Status**: Real-time indicators (Green/Red dots)

### Access Point (AP)
- **Built-in Hotspot**: `ESP32_18CH_Timer_Switch` (default)
- **Customizable**: Configurable SSID, password, channel (1-13)
- **Hidden SSID Support**: Option to hide network broadcast
- **Open Network Support**: Passwordless operation available
- **Captive Portal**: Automatic redirection for easy configuration
  - Supports Android, iOS, Windows captive portal detection
  - DNS-based redirection (port 53)
  - Multiple detection endpoints
- **Channel Selection**: 13 channels available, default channel 6

### mDNS (Bonjour/Avahi)
- **Local Network Discovery**: `esp32-18ch-relay.local` (default)
- **Dynamic Hostname**: Auto-generated from AP SSID or custom
- **Service Advertisement**: HTTP service with metadata
- **Version Tagging**: v5.1 with channel count in TXT records
- **Hot Restart**: mDNS service restart without device reboot
- **Sanitization**: Automatic hostname cleanup (spaces→dashes, lowercase)

---

## 🎛️ Control Features

### Relay Control Modes
- **Automatic Mode**: DS3231-backed schedule-based operation
- **Manual Override**: Direct ON/OFF control via web interface
- **Named Relays**: Custom 15-character names (double-click to edit)
- **Visual Status**: Color-coded badges (Green=ON, Red=OFF, Orange=MANUAL)

### Web Interface Controls
- **Per-Relay Buttons**: ON/OFF/Auto for each channel
- **Bulk Schedule Editor**: Visual day/month toggles with real-time preview
- **Schedule Badges**: 
  - 🌙 Overnight indicator for midnight-spanning schedules
  - ● Always ON indicator for 24/7 schedules
  - Day-of-week and day-of-month summary
- **Inline Name Editing**: Double-click relay name to rename
- **Toast Notifications**: Operation feedback with color coding

---

## 💾 Data Management

### Preferences/NVS Storage
- **Configuration Persistence**: All settings survive power cycles
- **Version Migration**: Automatic schema updates (currently v5)
- **Backward Compatibility**: Loads v3, v4, and v5 configurations
- **Factory Reset**: Complete NVS clearing option
- **Magic Number Validation**: Prevents corrupted data loading

### Stored Configurations
- **SystemConfig**: 
  - WiFi credentials (STA SSID/Password)
  - AP configuration (SSID/Password/Hostname)
  - NTP server settings
  - GMT/DST offsets
  - RTC state (last epoch, drift compensation)
- **ExtConfig**:
  - AP channel (1-13)
  - NTP sync interval (1-24 hours)
  - Hidden SSID flag
  - Future expansion (28 reserved bytes)
- **RelayConfigs**: 18× (8 schedules + names + manual states)
- **Configuration Migration Paths**:
  - v3 → v5: Adds day-of-week and month-day support
  - v4 → v5: Adds month-day scheduling

---

## 🌍 Web Interface

### Responsive Design
- **Mobile-First Layout**: Optimized for phones and tablets
- **Adaptive Grid**: Auto-fill relay cards (340px minimum)
- **Sticky Header**: Navigation always accessible
- **Material Design Inspired**: Clean, modern aesthetic
- **Dark-Friendly**: High contrast color scheme

### Pages & Sections

1. **Relays Dashboard** (`/`)
   - 18 relay cards with status badges
   - Schedule editor per relay (8 schedules each)
   - Manual control buttons
   - Inline relay renaming
   - Auto-refresh every 60 seconds
   - Real-time clock in header

2. **WiFi Settings** (`/wifi`)
   - Network scanner with RSSI visualization
   - Signal strength bars (1-4 bars)
   - Connection status with IP display
   - Credential management
   - Lock indicator for encrypted networks
   - Save & reconnect functionality

3. **Time Settings** (`/ntp`)
   - NTP server configuration
   - Timezone/GMT offset adjustment
   - Daylight saving offset
   - Sync interval control (1-24 hours)
   - Manual sync trigger
   - Server fallback information

4. **AP Settings** (`/ap`)
   - Hotspot SSID/password configuration
   - Channel selection with recommendation
   - Visibility toggle (hidden/broadcast)
   - Warning about client disconnection
   - Live reload after save

5. **System Info** (`/system`)
   - Network diagnostics (STA IP, AP IP)
   - Resource monitoring (Free Heap, Uptime)
   - WiFi RSSI with quality description
   - NTP sync status with age
   - Chip model identification
   - mDNS hostname status
   - Device restart/factory reset

### Real-Time Updates
- **1-Second Clock**: Live time display in header
- **Status Indicators**: 
  - WiFi: Green (connected) / Red (disconnected)
  - NTP: Green (synced) / Yellow (not synced)
- **Auto-Refresh**: 
  - Clock: Every 1 second
  - System info: Every 5 seconds
  - Relay states: Every 60 seconds (when not editing)

---

## 🔧 API Endpoints

### Relay Management
| Endpoint | Method | Data Size | Description |
|----------|--------|-----------|-------------|
| `/api/relays` | GET | 24KB JSON | Get all 18 relay states and schedules |
| `/api/relay/manual` | POST | 128B | Set manual ON/OFF state |
| `/api/relay/reset` | POST | 64B | Return to automatic schedule mode |
| `/api/relay/save` | POST | 4KB | Save schedule configuration |
| `/api/relay/name` | POST | 128B | Update relay display name |

### Time & Network
| Endpoint | Method | Data Size | Description |
|----------|--------|-----------|-------------|
| `/api/time` | GET | Small | Current time, WiFi/NTP status |
| `/api/wifi` | GET/POST | 128B | WiFi configuration & status |
| `/api/wifi/scan` | POST | - | Start async WiFi scan |
| `/api/wifi/scan` | GET | 8KB | Poll scan results (30 max) |
| `/api/ntp` | GET/POST | 256B | NTP settings management |
| `/api/ntp/sync` | POST | - | Force NTP synchronization |
| `/api/ap` | GET/POST | 256B | Access point configuration |
| `/api/mdns` | GET/POST | 128B | mDNS hostname management |
| `/api/mdns/restart` | POST | - | Restart mDNS service |

### System Management
| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/system` | GET | System diagnostics (1KB JSON) |
| `/api/reset` | POST | Restart device (600ms delay) |
| `/api/factory-reset` | POST | Clear NVS & restart |

### Captive Portal Endpoints
| Endpoint | Purpose |
|----------|---------|
| `/hotspot-detect.html` | Android captive portal detection |
| `/library/test/success.html` | Apple CNA detection |
| `/generate_204` | Android/Chrome detection |
| `/success.txt` | Firefox detection |
| `/canonical.html` | Apple detection |
| `/connecttest.txt` | Windows detection |
| `/ncsi.txt` | Windows NCSI detection |
| `/redirect` | Generic redirect |
| `/*` (catch-all) | Any unknown path → AP home |

---

## 🛡️ Safety & Reliability

### Hardware Protection
- **Safe Initialization**: All 18 relay pins set to OFF state before setup
- **Watchdog Friendly**: Non-blocking operations throughout
- **Timeout Protection**: 
  - WiFi scan: 10 seconds
  - WiFi connection: 15 seconds
  - NTP retry: 30 seconds
  - mDNS restart: 2 seconds

### Time Reliability
- **Triple Redundancy**: DS3231 HW RTC → Software RTC → NTP
- **Battery Backup**: DS3231 maintains time during power loss
- **Graceful Degradation**: 
  - WiFi down → DS3231 keeps time
  - DS3231 failure → Software RTC continues
  - Complete RTC failure → Schedule engine disabled (epoch check)
- **Drift Bounding**: ±10% compensation limits

### Software Resilience
- **Configuration Validation**: Magic number (0x1234) and extended config (0xEC) checking
- **Graceful Fallbacks**: Default values when configs are invalid
- **Memory-Safe JSON**: StaticJsonDocument with appropriate sizes
- **Non-Blocking Architecture**: 
  - Async WiFi scanning
  - State machine-based STA reconnection
  - Scheduled mDNS restart (2s delay)
- **Backward Compatibility**: Loads v3+ configuration formats

---

## 📊 Monitoring & Diagnostics

### System Metrics
- **Free Heap Memory**: Real-time RAM usage (KB)
- **Uptime Counter**: Device runtime (hours:minutes:seconds)
- **WiFi RSSI**: Signal strength with quality descriptions:
  - ≥ -50 dBm: Excellent
  - -60 to -50 dBm: Good
  - -70 to -60 dBm: Fair
  - < -70 dBm: Weak
- **NTP Sync Age**: Time since last synchronization
- **Chip Information**: ESP32 model identification
- **mDNS Status**: Active/Not running with hostname

### Time Sources Status
- **DS3231**: Available/Unavailable
- **Software RTC**: Initialized/Uninitialized
- **NTP**: Synced/Not synced with server info
- **Drift Factor**: Current compensation value (0.90-1.10)

---

## 🔄 Advanced Features

### Power Management
- **WiFi Power Save**: Implicit through non-blocking operations
- **Connection Backoff**: 10 attempts with 5-minute cooldown
- **Resource Optimization**: 
  - 100ms RTC update interval (software)
  - 60-second DS3231 sync interval
  - 5-second WiFi check interval
  - Configurable NTP sync interval

### Security
- **Password Protection**: Configurable AP authentication (WPA2)
- **Hidden SSID**: Reduce network visibility
- **No Hardcoded Credentials**: All settings user-configurable
- **Captive Portal Isolation**: Limited attack surface
- **NVS Encryption**: ESP32 hardware encryption for stored credentials

### Extensibility
- **JSON API**: Easy integration with home automation systems
- **mDNS Discovery**: Zero-configuration network finding
- **Version Tracking**: Forward-compatible configurations (v5+)
- **Modular Design**: Clear separation of concerns in code
- **Extended Config**: 28 reserved bytes for future features

### I2C Bus
- **Dedicated Pins**: GPIO21 (SDA), GPIO22 (SCL)
- **DS3231 Primary**: Hardware RTC at address 0x68
- **Expandable**: Additional I2C devices can share the bus
- **Automatic Detection**: `rtcAvailable` flag for graceful handling

---

## 📱 User Experience

### Initial Setup Flow
1. Power on ESP32 → Default AP activates
2. Connect to `ESP32_18CH_Timer_Switch` network
3. Captive portal automatically redirects to configuration
4. Configure WiFi station settings
5. Set timezone and NTP preferences
6. (Optional) Customize AP settings
7. Create relay schedules via web interface
8. Device connects to network and syncs time
9. DS3231 maintains time across reboots

### Daily Operation
- Schedules run automatically using DS3231 time
- Manual overrides via web/mobile browser
- Status monitoring at a glance
- Zero-touch after initial configuration
- Battery-backed RTC survives power outages

### Recovery Options
- Factory reset via web interface
- Automatic reconnection after WiFi drops
- Fallback AP always available for configuration
- DS3231 continues timekeeping during network outages
- NTP resync after extended offline periods

### Error Handling
- **Invalid Time**: Schedule engine disabled if epoch < 1,000,000,000
- **DS3231 Failure**: Falls back to software RTC
- **WiFi Disconnection**: Non-blocking reconnection with backoff
- **NTP Failure**: Server rotation (4 fallback servers)
- **Configuration Corruption**: Magic number validation with default reset
