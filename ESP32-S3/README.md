# Requirements
- 5V 1-16 Channel Relay
- ESP32-S3 N16R8 Wroom-1 Dev Module
- Dupont Wires
- 5V 5-10A Power Supply

`Optional`
- 5V Battery (Maintain Time)

# Libraries
- ArduinoJson
- Preferences 
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

- Password:`ESP32-admin`

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
26CH | ESP32-S3
VCC  => 5V
IN1  => 1
IN2  => 2
IN3  => 3
IN4  => 4
IN5  => 5
IN6  => 6
IN7  => 7
IN8  => 8
IN9  => 9
IN10 => 10
IN11 => 11
IN12 => 12
IN13 => 13
IN14 => 14
IN15 => 15
IN16 => 16
GND  => GND
```

# Full Features

## 🧠 Core Hardware Support

- **26 Independent Relay Channels**
  - Configurable GPIO pins (see pin mapping below)
  - Active-low or active-high logic (global setting)
  - Default OFF state on boot for safety
- **ESP32-S3 MCU** with WiFi & Bluetooth capability
- **Non-Volatile Storage (NVS)** via Preferences library for all settings
- **Internal RTC Emulation** — drift-compensated software clock

### Relay GPIO Pin Mapping
| Relay | GPIO Pin |
|-------|----------|
| 1     | 4        |
| 2     | 5        |
| 3     | 6        |
| 4     | 7        |
| 5     | 15       |
| 6     | 16       |
| 7     | 17       |
| 8     | 18       |
| 9     | 8        |
| 10    | 9        |
| 11    | 10       |
| 12    | 11       |
| 13    | 12       |
| 14    | 13       |
| 15    | 14       |
| 16    | 1        |
| 17    | 2        |
| 18    | 42       |
| 19    | 41       |
| 20    | 40       |
| 21    | 39       |
| 22    | 38       |
| 23    | 45       |
| 24    | 48       |
| 25    | 47       |
| 26    | 21       |

---

## 🌐 Networking & Connectivity

- **WiFi Station (STA) Mode** — connect to existing WiFi networks
  - Non-blocking connection state machine (no delays in `loop()`)
  - Automatic periodic connection checks
  - Configurable retry logic (up to 10 attempts, then 5-min cooldown)
  - Connection timeout: 15 seconds per attempt
- **WiFi Access Point (AP) Mode** — standalone network
  - Configurable SSID & password (WPA2 or open)
  - Selectable channel (1–13)
  - Optional hidden SSID (no broadcast)
  - **Captive Portal** — auto-redirect for iOS, Android, Windows, macOS
    - `/hotspot-detect.html`, `/generate_204`, `/ncsi.txt`, `/success.txt`, etc.
  - DNS server on port 53 (captive portal detection)
- **WiFi Network Scanner** — async scan (non-blocking)
  - Returns SSID, signal strength (RSSI), encryption type
  - Sorted by signal strength in web UI

---

## ⏱️ Time & Scheduling

### NTP Time Synchronization
- Configurable **primary NTP server** with 4 fallback pools:
  1. `ph.pool.ntp.org`
  2. `pool.ntp.org`
  3. `time.nist.gov`
  4. `time.google.com`
- Automatic failover between servers on failure
- Configurable sync interval (1–24 hours)
- Manual "Sync Now" button from web UI
- GMT offset & daylight saving offset (in seconds)

### Internal RTC Emulator
- Software-based clock with **drift compensation**
  - Drift measured vs. NTP over time (exponential moving average)
  - Clamped between 0.90x–1.10x
- Persisted to NVS across reboots (last known epoch + drift factor)
- Updates every 100ms

---

## 📋 Per-Relay Schedule Engine (8 Schedules × 26 Relays = 208 Schedules)

Each relay has **8 independent timer schedules**. Each schedule supports:

### Time Settings
- **Start Time**: hour, minute, second
- **Stop Time**: hour, minute, second
- Behavior modes:
  - `Start < Stop` → Normal daily window
  - `Start > Stop` → **Overnight window** (crosses midnight)
  - `Start == Stop` → **Always ON** during active days

### Day-of-Week Filter
- Individual day selection: Sunday through Saturday
- Predefined constants: `DAY_ALL` (everyday), `DAY_WEEKDAYS`, `DAY_WEEKENDS`
- Stored as **bitmask** (`uint8_t`)

### Day-of-Month Filter
- Select specific calendar days (1–31)
- Stored as **32-bit bitmask** (`uint32_t`)
- `0` = not used (ignored, schedule only uses day-of-week)
- `0xFFFFFFFF` = all month days

### Priority Logic
1. Manual override (highest priority)
2. Schedule engine (evaluates all 8 schedules, first match wins)
3. Default OFF

---

## 🖥️ Web Interface — Responsive Single-Page Design

The web UI is entirely self-hosted (no external CDN), served as `PROGMEM` strings. It features:

### Pages
| Page    | Path       | Purpose                              |
|---------|-----------|--------------------------------------|
| Relays  | `/`       | Main control & schedule management   |
| WiFi    | `/wifi`   | STA connection settings & scan       |
| Time    | `/ntp`    | NTP configuration & manual sync      |
| AP      | `/ap`     | Access point configuration           |
| System  | `/system` | Device info, uptime, controls        |

### UI Features
- **Live clock** in header (updates every second)
- **WiFi/NTP status indicators** (green/yellow/red dots)
- **Toast notifications** for success/error feedback
- **Auto-refresh** relay states every 60 seconds
- Mobile-responsive CSS (grid layout adapts to screen size)

### Relay Controls Page
- **Manual ON / OFF / Auto** buttons per relay
- **State badges**: `ON` (green), `OFF` (red), `MANUAL` (orange)
- **Inline relay renaming** — double-click name to edit
- **8 schedule slots** per relay, each with:
  - Enable/disable checkbox
  - Start/Stop time pickers (with seconds)
  - Interactive day-of-week buttons (7 clickable day chips)
  - Interactive day-of-month buttons (31 clickable number chips)
  - Overnight / Always-ON badge with textual summary
- **Individual Save** button per relay (writes to NVS)

### WiFi Page
- Current connection status display
- Signal strength bars (visual RSSI indicator)
- **Network scanner** with async polling
- Click-to-select networks from scan results

### System Page
- STA IP, AP IP, Free Heap, Uptime
- WiFi RSSI (with quality descriptor: Excellent/Good/Fair/Weak)
- NTP last sync time (relative: "X min ago" or "Never")
- NTP server currently in use
- ESP32-S3 chip model display
- mDNS hostname & status
- **Device Restart** button
- **Factory Reset** button (clears all NVS data)

---

## 🔌 REST API Endpoints

### Relay Management
| Method | Endpoint              | Description                     |
|--------|-----------------------|---------------------------------|
| GET    | `/api/relays`         | Get all relay states & schedules |
| POST   | `/api/relay/manual`   | Set manual override state       |
| POST   | `/api/relay/reset`    | Clear manual override (auto mode) |
| POST   | `/api/relay/save`     | Save schedules for one relay    |
| POST   | `/api/relay/name`     | Rename a relay                  |

### System & Status
| Method | Endpoint              | Description                     |
|--------|-----------------------|---------------------------------|
| GET    | `/api/time`           | Current time, WiFi/NTP status   |
| GET    | `/api/system`         | Full system info & stats        |
| POST   | `/api/reset`          | Restart device                  |
| POST   | `/api/factory-reset`  | Clear NVS and restart           |

### WiFi
| Method | Endpoint              | Description                     |
|--------|-----------------------|---------------------------------|
| GET    | `/api/wifi`           | STA status, IP, RSSI            |
| POST   | `/api/wifi`           | Save STA credentials & restart  |
| POST   | `/api/wifi/scan`      | Start async WiFi scan           |
| GET    | `/api/wifi/scan`      | Poll scan results               |

### NTP / Time
| Method | Endpoint              | Description                     |
|--------|-----------------------|---------------------------------|
| GET    | `/api/ntp`            | Current NTP settings            |
| POST   | `/api/ntp`            | Save NTP settings               |
| POST   | `/api/ntp/sync`       | Force immediate NTP sync        |

### AP (Access Point)
| Method | Endpoint              | Description                     |
|--------|-----------------------|---------------------------------|
| GET    | `/api/ap`             | Current AP settings             |
| POST   | `/api/ap`             | Save AP settings & restart AP   |

### mDNS
| Method | Endpoint              | Description                     |
|--------|-----------------------|---------------------------------|
| GET    | `/api/mdns`           | mDNS status & hostname          |
| POST   | `/api/mdns`           | Set mDNS hostname               |
| POST   | `/api/mdns/restart`   | Restart mDNS service            |

---

## 🔧 Configuration Persistence

All settings are stored in **NVS (Non-Volatile Storage)** using the Preferences library:

### `SystemConfig` (version 5)
- Magic number validation (`0x1234`)
- Version tracking (supports migration from v3 → v4 → v5)
- STA SSID & password
- AP SSID & password
- NTP server, GMT offset, DST offset
- Last known RTC epoch & drift coefficient
- mDNS hostname

### `ExtConfig`
- AP channel (1–13)
- NTP sync interval hours (1–24)
- AP hidden flag
- 28 reserved bytes for future use

### Per-Relay Config (26 × `RelayConfig`)
- Manual override flag & state
- Custom name (up to 15 chars)
- 8 schedule slots with full time/day/month-day settings

### Backward Compatibility
- Automatically detects and upgrades configs from older firmware versions
- Missing fields initialized to safe defaults

---

## 🔐 Over-the-Air (OTA) & Service Discovery

- **mDNS (Bonjour/Avahi)**:
  - Advertises as `http` service on port 80
  - TXT records: model, version, channel count
  - Configurable hostname (auto-derived from AP SSID or custom)
  - Accessible via `hostname.local`

---

## 🛡️ Captive Portal Support

The device acts as a **captive portal** when in AP mode. Supported detection probes:
- Apple iOS/macOS: `/hotspot-detect.html`, `/library/test/success.html`
- Android: `/generate_204`
- Windows: `/ncsi.txt`, `/connecttest.txt`
- Generic: `/redirect`, `/success.txt`, `/canonical.html`
- Fallback: all unhandled paths redirect to home page

---

## 🔄 Automatic Recovery & Watchdog Features

- **WiFi auto-reconnect** with exponential-style backoff
- **NTP server failover** — automatically cycles through 4 servers
- **AP always available** — even when STA fails, softAP remains up
- **Safe boot state** — all relays forced OFF at startup
- **NVS corruption detection** — magic number validation, falls back to defaults
- **Software reset API** — `/api/reset` and `/api/factory-reset`

---

## 📐 Technical Specifications

| Parameter             | Value                        |
|-----------------------|------------------------------|
| Max Relays            | 26                           |
| Schedules per Relay   | 8                            |
| Total Schedules       | 208                          |
| Schedule Granularity  | 1 second                     |
| Day-of-Week Filter    | Bitmask (7 bits)             |
| Day-of-Month Filter   | Bitmask (32 bits, days 1–31) |
| NTP Sync Interval     | 1–24 hours (configurable)    |
| RTC Drift Compensation| Adaptive (0.90x–1.10x)       |
| Web UI Size           | ~10KB compressed (PROGMEM)   |
| JSON Document Buffer  | 32KB (relay state endpoint)  |
| Configuration Storage | ESP32-S3 NVS (Preferences)   |
| WiFi Reconnect Max    | 10 attempts / 5-min cooldown |
| NTP Retry Interval    | 30 seconds                   |
| WiFi Scan Timeout     | 10 seconds                   |

---

## 🚀 Use Cases

- Home automation lighting control
- Garden irrigation scheduling
- Aquarium pump & light timers
- Industrial equipment duty cycling
- Hydroponic system control
- Energy management (load shedding)
- Holiday light displays
- UV/heat lamp sterilization cycles
