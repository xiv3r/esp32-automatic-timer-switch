# 📘 ESP32 16-Channel Automatic Relay Timer Switch

> **Author:** Raff Alds — [github.com/xiv3r](https://www.github.com/xiv3r)
> **License:** GPLv3

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Complete Feature List](#2-complete-feature-list)
3. [File Header & License Block](#3-file-header--license-block)
4. [Library Dependencies](#4-library-dependencies)
5. [Compile-Time Constants & Macros](#5-compile-time-constants--macros)
6. [Data Structures](#6-data-structures)
7. [Class: `SelfHealingSystem`](#7-class-selfhealingsystem)
8. [Global Variables & Objects](#8-global-variables--objects)
9. [Forward Declarations](#9-forward-declarations)
10. [Function Reference](#10-function-reference)
    - [10.1 Millis/Timing Helpers](#101-millis--timing-helpers)
    - [10.2 64-bit Time Conversion Helpers](#102-64-bit-time-conversion-helpers)
    - [10.3 Relay Control Helpers](#103-relay-control-helpers)
    - [10.4 WiFi Station Control](#104-wifi-station-control)
    - [10.5 DS3231 RTC Subsystem](#105-ds3231-rtc-subsystem)
    - [10.6 Internal (Software) RTC Subsystem](#106-internal-software-rtc-subsystem)
    - [10.7 WiFi Scan Pause](#107-wifi-scan-pause)
    - [10.8 NTP Subsystem](#108-ntp-subsystem)
    - [10.9 Browser Time Sync](#109-browser-time-sync)
    - [10.10 WiFi Connection Manager](#1010-wifi-connection-manager)
    - [10.11 Relay & Schedule Engine](#1011-relay--schedule-engine)
    - [10.12 mDNS Subsystem](#1012-mdns-subsystem)
    - [10.13 Self-Healing Methods](#1013-self-healing-methods)
    - [10.14 Memory Management](#1014-memory-management)
    - [10.15 Boot-Button Factory Reset](#1015-boot-button-factory-reset)
    - [10.16 GPIO Configuration Persistence](#1016-gpio-configuration-persistence)
    - [10.17 Configuration Persistence (NVS)](#1017-configuration-persistence-nvs)
    - [10.18 `setup()` and `loop()`](#1018-setup-and-loop)
    - [10.19 Web Server Route Registration](#1019-web-server-route-registration)
    - [10.20 HTTP API Handlers](#1020-http-api-handlers)
11. [Embedded Web Assets (PROGMEM)](#11-embedded-web-assets-progmem)
12. [Complete HTTP Route & REST API Reference](#12-complete-http-route--rest-api-reference)
13. [NVS (Flash) Storage Map](#13-nvs-flash-storage-map)
14. [Default GPIO Pin Map](#14-default-gpio-pin-map)
15. [Timing / Interval Reference](#15-timing--interval-reference)
16. [Execution Flow](#16-execution-flow)
17. [Bitmask Encoding Reference](#17-bitmask-encoding-reference)
18. [Notes, Invariants & Known Idiosyncrasies](#18-notes-invariants--known-idiosyncrasies)

---

## 1. Project Overview

This firmware turns an ESP32 into a **network-configurable 16-channel automatic relay timer switch**. A built-in web application (served from flash memory on port 80) lets a user:

* switch any relay manually (override) or return it to automatic schedule mode,
* program **up to 8 independent ON/OFF schedules per relay** with second resolution,
* filter each schedule by **day-of-week, day-of-month, and month-of-year** bitmasks (holiday/seasonal switching),
* remap which GPIO pin drives which relay **at runtime** (no re-flash needed), including per-relay or global **active-low / active-high** logic,
* keep accurate time via **NTP (64-bit, year-2106-safe)**, a **DS3231 hardware RTC**, or a **browser-sync fallback**, with an internal software RTC as the always-available backbone,
* operate via **WiFi station + Access Point simultaneously** (`WIFI_AP_STA`), with a **captive portal**, **mDNS** (`<hostname>.local`), and the ability to **fully disable station mode** for offline AP-only operation,
* survive faults unattended through a **self-healing subsystem** (WiFi/mDNS/DNS/web-server/AP reconfiguration, relay-state verification, persistent crash-safe critical state),
* be factory-reset either from the web UI or by holding the **BOOT button (GPIO 0) for 5 s**.

All settings persist in ESP32 **NVS (Non-Volatile Storage)** under the namespace `relay16`, versioned with magic numbers and migrating older layouts automatically.

---

## 2. Complete Feature List

### 2.1 Relay Control
| # | Feature |
|---|---------|
| F1 | Up to **16 relays**, dynamically add/remove pins at runtime (1–16 active at once) |
| F2 | **8 schedules per relay**, each with start & stop time (hour/minute/**second**) |
| F3 | Per-schedule **enabled/disabled** toggle |
| F4 | **Day-of-week mask** (Sun–Sat), **day-of-month mask** (1–31), **month-of-year mask** (Jan–Dec) per schedule |
| F5 | **Overnight schedules** (start > stop wraps past midnight, e.g. 22:00 → 06:00) |
| F6 | **Always-ON schedule** mode (start == stop keeps relay on for the whole matching day) |
| F7 | **Manual override** (force ON / force OFF) with persistent storage |
| F8 | One-touch **return to Auto** (clears override) |
| F9 | **Custom relay names** (up to 15 chars, double-click inline rename in UI) |
| F10 | **Active-low / active-high per relay**, plus a global override mode (all-LOW / all-HIGH / per-relay) |
| F11 | All relays driven safely to OFF on boot, config change, delete, and active-level toggle |
| F12 | **Debounced** schedule switching (500 ms) to avoid relay chatter |

### 2.2 Time & Scheduling Core
| # | Feature |
|---|---------|
| F13 | **64-bit Unix epoch** pipeline — safe beyond year 2106 (32-bit NTP wraparound) |
| F14 | Custom **`gmtime64()`** civil-date conversion (Howard Hinnant algorithm) — no reliance on 32-bit libc |
| F15 | **Internal software RTC** based on `micros()` with fractional-second rounding and drift field |
| F16 | Periodic **RTC rebase** (every 5 min) — immune to `micros()` 71.6-minute overflow |
| F17 | **DS3231** hardware RTC over I²C (SDA 21 / SCL 22); primary time authority when present |
| F18 | DS3231 power-loss detection (`lostPower()`); validity year window 2020–2100 |
| F19 | **NTP sync** with 4-server fallback pool (Google → Windows → Cloudflare → Facebook) |
| F20 | **Browser-sync time fallback** (posts browser epoch to the device) |
| F21 | **Time-source tracking & priority** display (NTP / Browser / RTC / None), NTP overrides browser time |
| F22 | Hourly **internal RTC auto-save to NVS** (skipped when NTP/DS3231 available to save flash wear) |
| F23 | Configurable **GMT offset + DST offset** (seconds) and **NTP resync interval (1–24 h)** |
| F24 | 1-second **schedule cache** refresh; schedule engine runs every 250 ms |

### 2.3 Networking
| # | Feature |
|---|---------|
| F25 | Simultaneous **AP + STA** mode; AP always available for configuration |
| F26 | AP settings: SSID, WPA2 password (or open), **channel 1–13**, **hidden SSID** |
| F27 | **Captive portal** — DNS hijack + Apple/Android/Windows connectivity-check endpoints, catch-all redirect |
| F28 | **mDNS responder** with sanitized hostname from AP SSID; TXT records (model/version/channels/features) |
| F29 | Live **mDNS hostname change** with deferred (2 s) restart |
| F30 | WiFi **station enable/disable switch** — full offline AP-only mode with NVS persistence |
| F31 | **Async WiFi network scan** (up to 30 SSIDs, RSSI, encryption flag, hidden networks) |
| F32 | Scan-aware **connection pausing** so scans can't starve a pending STA connect (15 s pause, auto-resume) |
| F33 | Reconnect state machine: 20 s connect timeout, **10 retries → 5-min give-up backoff** |
| F34 | WiFi status indicators in UI (green/red dot, RSSI bars with quality text) |

### 2.4 Reliability / Self-Healing
| # | Feature |
|---|---------|
| F35 | **SelfHealingSystem class**: live reconfiguration of WiFi, mDNS, DNS, web server, AP (rate-limited) |
| F36 | **Targeted recovery** sequence (web → DNS → mDNS → WiFi → AP → RTC → relays) |
| F37 | **Smart recovery** tick (every 10 s): WiFi failure counting, reconnect after 3 failures |
| F38 | **Critical relay state persistence** (magic + checksum + timestamp in NVS) restores manual overrides after unexpected reboot |
| F39 | **Relay state verification** — actual GPIO readback compared with expected state every 30 s, corrected on mismatch |
| F40 | **Heap watchdog** — tracks minimum free heap; cleanup if < 20 KB free; periodic cleanup hourly |
| F41 | **Stale-resource cleanup** every 30 s (abandoned WiFi scans, expired response cache) and 60 s stale-client purge |
| F42 | **Software factory reset** (web API) and **hardware factory reset** (BOOT button 5 s hold) — both non-restarting |
| F43 | **NVs magic/version validation** with automatic defaults + v10→v11 config migration (adds month masks) |
| F44 | `millis()`-overflow-safe timing comparisons everywhere (subtraction-based) |
| F45 | Deferred critical-state save (5-min throttle) to minimize flash wear |

### 2.5 Web Application
| # | Feature |
|---|---------|
| F46 | Six-page single-server web UI: **Relays / WiFi / Time / AP / GPIO / System** |
| F47 | Shared PROGMEM stylesheet, responsive (mobile breakpoint 500 px), card-based design |
| F48 | Live header clock (1 s poll) + WiFi & time-source status dots |
| F49 | Toast notifications (success/error) across all pages |
| F50 | Schedule UI: time pickers, clickable day/month grids, overnight & always-ON badges |
| F51 | WiFi page: station-mode toggle button, scan picker with signal bars and lock icons |
| F52 | Time page: NTP settings + sync now + browser sync + RTC presence/status panel |
| F53 | System dashboard: 20 info cards (IP, heap, uptime, RSSI, time source, epoch, sync ages, drift, GPIO mode, DS3231 status…) |
| F54 | Chunked (streamed) JSON for relay list + short-lived response cache infrastructure |
| F55 | Full JSON **REST API** (≈ 25 endpoints) — see [§12](#12-complete-http-route--rest-api-reference) |

---

## 3. File Header & License Block

**Lines 1–9.** A block comment identifying the project as **"ESP32 16-Channel Automatic Relay Timer Switch"**, author **Raff Alds**, repository `https://www.github.com/xiv3r`, licensed under **GPLv3**. This is metadata only; it produces no code.

---

## 4. Library Dependencies

**Lines 11–19.** All includes are part of the Arduino-ESP32 core except `ArduinoJson` (3rd-party) and `RTClib` (Adafruit).

| Header | Source | Used For |
|---|---|---|
| `WiFi.h` | Arduino-ESP32 core | STA/AP management, scanning, RSSI, modes (`WIFI_AP`, `WIFI_AP_STA`) |
| `WiFiUdp.h` | Arduino-ESP32 core | UDP socket for the SNTP client |
| `WebServer.h` | Arduino-ESP32 core | HTTP/1.1 server on port 80, route registration, chunked responses |
| `DNSServer.h` | Arduino-ESP32 core | Captive-portal DNS (answers every query with the AP IP) |
| `Preferences.h` | Arduino-ESP32 core | NVS key/value persistence (all configuration & critical state) |
| `ArduinoJson.h` | 3rd-party (bblanchon) | Request/response (de)serialization for the REST API |
| `ESPmDNS.h` | Arduino-ESP32 core | Multicast DNS (`hostname.local`), service TXT records |
| `Wire.h` | Arduino-ESP32 core | I²C bus for the DS3231 RTC |
| `RTClib.h` | Adafruit | `RTC_DS3231` driver, `DateTime` objects |

---

## 5. Compile-Time Constants & Macros

### 5.1 Preferences / NVS Identity  *(L23–28)*

| Constant | Value | Meaning |
|---|---|---|
| `NVS_NAMESPACE` | `"relay16"` | NVS namespace containing every persisted key |
| `EEPROM_MAGIC` | `0x1234` | Validity magic for the `SystemConfig` blob (legacy name — stored in NVS, not EEPROM) |
| `EEPROM_VERSION` | `11` | Config layout version; triggers migration when older blob found |
| `EXT_CFG_MAGIC` | `0xEC` | Validity magic for the `ExtConfig` blob |

Also: `Preferences preferences;` (L23) — the single shared NVS accessor object.

### 5.2 Year-2106+ 64-bit Epoch Support  *(L31–35)*

| Constant | Value | Meaning |
|---|---|---|
| `MAX_UNIX_TIME_64` | `18446744073709551615ULL` (2⁶⁴−1) | Upper bound of representable epoch |
| `MIN_UNIX_TIME_64` | `1000000000ULL` (2001-09-09) | Values below are treated as "no valid time" |
| `VALID_UNIX_TIME_64(epoch)` | macro | `true` if epoch is inside `(MIN, MAX)` — the global sanity gate for any timestamp before use |

### 5.3 Day-of-Week Bitmask  *(L38–48)*

Bit i = weekday i (starting Sunday). Used in `TimerSchedule.days[8]`.

| Constant | Value | Bit |
|---|---|---|
| `DAY_SUNDAY` … `DAY_SATURDAY` | `1<<0` … `1<<6` | individual days |
| `DAY_ALL` | `0x7F` | every day |
| `DAY_WEEKDAYS` | `0x3E` | Mon–Fri |
| `DAY_WEEKENDS` | `0x41` | Sat + Sun |

### 5.4 Month-of-Year Bitmask  *(L51–66)*

Bit i = month (i+1). Used in `TimerSchedule.monthMask[8]` (16-bit).

| Constant | Value | Meaning |
|---|---|---|
| `MONTH_JANUARY` … `MONTH_DECEMBER` | `1<<0` … `1<<11` | individual months |
| `MONTH_ALL` | `0x0FFF` | all 12 months (schedule default) |

### 5.5 Timing Constants  *(L69–79)*

All `unsigned long` milliseconds, used with the overflow-safe helpers of §10.1.

| Constant | Value | Every | Purpose |
|---|---|---|---|
| `NTP_RETRY_INTERVAL` | 30 000 | 30 s | min gap between NTP attempts |
| `WIFI_CHECK_INTERVAL` | 10 000 | 10 s | WiFi link state evaluation in `loop()` |
| `WIFI_CONNECT_TIMEOUT` | 20 000 | 20 s | per-attempt STA connect timeout |
| `RTC_UPDATE_INTERVAL` | 100 | 100 ms | declared RTC tick quantum (reference) |
| `SCHEDULE_PROCESS_INTERVAL` | 250 | 250 ms | `processRelaySchedules()` cadence |
| `RELAY_UPDATE_INTERVAL` | 500 | 500 ms | reference output refresh quantum |
| `RTC_REBASE_INTERVAL` | 300 000 | 5 min | internal-RTC rebase (≪ `micros()` overflow) |
| `RTC_SYNC_INTERVAL` | 3 600 000 | 1 h | generic RTC resync quantum |
| `DS3231_SYNC_INTERVAL` | 3 600 000 | 1 h | DS3231 → internal RTC resync in `loop()` |

### 5.6 NTP Async State-Machine Constants  *(L83–87)*

| Constant | Value | Meaning |
|---|---|---|
| `NTP_SERVER_TIMEOUT` | 5 000 ms | per-server wait |
| `NTP_STATE_IDLE` / `NTP_STATE_CONNECTING` / `NTP_STATE_WAITING` | 0 / 1 / 2 | async NTP phases (state var exists; current implementation synchronizes via the blocking-style `tryNTPSync()`, the async states are reset bookkeeping) |

### 5.7 NTP Protocol Constants (64-bit client)  *(L90–95)*

| Constant | Value | Meaning |
|---|---|---|
| `NTP_PACKET_SIZE` | 48 | standard SNTP packet length |
| `NTP_PORT` | 123 | server port |
| `NTP_TIMEOUT` | 5 000 ms | response wait per retry |
| `NTP_RETRY_COUNT` | 3 | retries per server |
| `NTP_EPOCH_OFFSET` | 2 208 988 800 | seconds between 1900 (NTP epoch) and 1970 (Unix epoch) |

### 5.8 Memory-Management Constants  *(L98–101)*

| Constant | Value | Purpose |
|---|---|---|
| `MEMORY_CLEANUP_INTERVAL` | 30 000 ms | `cleanupStaleResources()` cadence in `loop()` |
| `MEMORY_CHECK_INTERVAL` | 60 000 ms | declared heap-check quantum (reference) |
| `CONNECTION_TIMEOUT` | 10 000 ms | declared stale-connection quantum (reference) |

### 5.9 Boot-Button Factory Reset  *(L104–108)*

| Symbol | Value | Purpose |
|---|---|---|
| `BOOT_BUTTON_PIN` | `0` | on-board BOOT button GPIO (active-low, internal pull-up) |
| `FACTORY_RESET_HOLD` | 5 000 ms | hold time to trigger reset |
| `bootButtonPressStart` | `0` | timestamp of press start |
| `bootButtonPressed` | `false` | button currently held |
| `factoryResetTriggered` | `false` | reset already executed this press |

### 5.10 mDNS Settings  *(L112–114)*

| Constant | Value | Meaning |
|---|---|---|
| `MDNS_HOSTNAME_DEFAULT` | `"esp32"` | fallback/base hostname |
| `MDNS_RESTART_DELAY` | 2 000 ms | deferred restart after hostname change |

### 5.11 NTP Fallback Pool  *(L118–125)*

```cpp
static const char* NTP_SERVERS[] = {
    "time.google.com", "time.windows.com",
    "time.cloudflare.com", "time.facebook.com" };
static const uint8_t NUM_NTP_SERVERS = 4;
```
Iterated round-robin by `ntpServerIndex`; each sync rotates the starting point.

### 5.12 Time-Source Tracking  *(L129–136)*

| Constant | Value | Meaning |
|---|---|---|
| `TIME_SOURCE_NONE` | 0 | no valid time yet |
| `TIME_SOURCE_NTP` | 1 | last sync from NTP (highest priority when online) |
| `TIME_SOURCE_BROWSER` | 2 | last sync from browser post |
| `TIME_SOURCE_RTC` | 3 | time from DS3231 |
| `timeSource` | global `uint8_t` | current active source |
| `lastBrowserSync` | global ms timestamp | last browser sync (for age reporting) |

### 5.13 Relay & GPIO Constants  *(L179–212)*

| Symbol | Value | Meaning |
|---|---|---|
| `MAX_RELAYS` | `16` | hard channel ceiling |
| `DEFAULT_RELAY_PINS[]` | `{15,2,4,5,18,19,3,1,23,13,14,27,26,25,33,32}` | factory IN1…IN16 mapping (see §14) |
| `GPIO_CONFIG_MAGIC` | `0xD002` | validity magic for `GPIOPinConfig` blob |

---

## 6. Data Structures

### 6.1 `struct GPIOPinConfig` — Runtime pin map *(L204)*
```cpp
struct GPIOPinConfig {
    uint8_t  pins[MAX_RELAYS];      // GPIO driving relay i
    uint8_t  count;                 // active relay count (0–16)
    uint16_t magic;                 // GPIO_CONFIG_MAGIC validity tag
    bool     activeLow[MAX_RELAYS]; // true = relay energizes when pin LOW
};
```
Persisted in NVS key `gpioConfig`. Global instance: `gpioConfig`. Invalid/missing blob → rebuilt from `DEFAULT_RELAY_PINS` with `count = 16`, all `activeLow = true`.

### 6.2 `struct TimerSchedule` — 8-slot schedule per relay *(L216)*
```cpp
struct TimerSchedule {
    uint8_t  startHour[8], startMinute[8], startSecond[8];
    uint8_t  stopHour[8],  stopMinute[8],  stopSecond[8];
    bool     enabled[8];    // slot on/off
    uint8_t  days[8];       // day-of-week bitmask (§5.3)
    uint32_t monthDays[8];  // day-of-month bitmask, bit i = day i+1; 0 = unrestricted
    uint16_t monthMask[8];  // month bitmask (§5.4); 0 also treated as unrestricted
};
```

### 6.3 `struct RelayConfig` — full per-relay state *(L226)*
```cpp
struct RelayConfig {
    TimerSchedule schedule;     // 8 schedule slots
    bool          manualOverride; // true = schedule ignored
    bool          manualState;    // forced ON/OFF while overridden
    char          name[16];       // display name (15 chars + NUL)
};
```
Global array: `relayConfigs[MAX_RELAYS]` (16 × sizeof(RelayConfig), persisted as one NVS blob `relayConfigs`).

### 6.4 `struct SystemConfig` — main persisted config *(L234, `__attribute__((packed))`)*
| Field | Type | Purpose |
|---|---|---|
| `magic` | `uint16_t` | must equal `EEPROM_MAGIC` (0x1234) |
| `version` | `uint8_t` | layout version (`EEPROM_VERSION` = 11) |
| `sta_ssid[32]` / `sta_password[64]` | char | station (uplink) credentials |
| `ap_ssid[32]` / `ap_password[32]` | char | access-point credentials |
| `ntp_server[48]` | char | preferred NTP host (informational; the pool in §5.11 is what is actually dialed) |
| `gmt_offset` | `int32_t` | timezone offset, seconds (default 28800 = UTC+8) |
| `daylight_offset` | `int32_t` | DST offset, seconds (default 0) |
| `last_rtc_epoch` | `uint64_t` | last saved internal-RTC epoch (cold-start restore) |
| `rtc_drift` | `float` | drift factor slot (currently stored as 1.0) |
| `hostname[32]` | char | device hostname seed (default `"esp32"`) |

### 6.5 `struct ExtConfig` — extended config *(L250, packed)*
| Field | Type | Default | Purpose |
|---|---|---|---|
| `magic` | `uint8_t` | `0xEC` | validity tag |
| `ap_channel` | `uint8_t` | 6 | AP WiFi channel (valid 1–13) |
| `ntp_sync_hours` | `uint8_t` | 1 | NTP resync interval in hours (1–24) |
| `ap_hidden` | `uint8_t` | 0 | broadcast SSID hidden flag |
| `global_active_mode` | `uint8_t` | 0 | 0 = per-relay level, 1 = all active-LOW, 2 = all active-HIGH |
| `sta_enabled` | `uint8_t` | 1 | station mode master switch |
| `reserved[26]` | uint8_t | — | future expansion |

### 6.6 `struct HealthMetrics` — self-healing counters *(L263)*
| Field | Purpose |
|---|---|
| `wifiFailures` / `ntpFailures` / `mdnsFailures` / `dnsFailures` / `webServerFailures` | rolling failure counters per subsystem |
| `lastRecoveryAttempt` | ms timestamp, rate-limits `smartRecovery()` |
| `inRecoveryMode` | flag reserved for degraded-mode logic |

### 6.7 `struct CriticalRelayState` — crash-safe snapshot *(L272)*
```cpp
struct CriticalRelayState {
    uint32_t magic;                     // 0xDEADBEEF
    bool     relayStates[MAX_RELAYS];   // last physical outputs
    bool     manualOverrides[MAX_RELAYS];
    uint32_t timestamp;                 // millis() at save
    uint32_t checksum;                  // see calculateCriticalChecksum()
};
```
Saved to NVS key `criticalState`; restored on boot so manual overrides survive unexpected reboots/crashes.

### 6.8 `struct ResponseCache` — JSON response cache *(L367)*
```cpp
struct ResponseCache {
    String relaysJson, systemJson, timeJson;
    unsigned long lastUpdate = 0;
    bool   valid = false;
};
```
Infrastructure for caching API payloads (see §18 note N4 about current utilization).

---

## 7. Class: `SelfHealingSystem`

**Declaration: L379–401 · Global instance: `SelfHealingSystem healer;` (L410)**

A service-watchdog class. All heavy recovery is **rate-limited** via private timestamps so recovery attempts never hammer the hardware.

### 7.1 Public interface

| Method | Summary |
|---|---|
| `liveReconfigureWiFi()` | re-issue `WiFi.begin()` when credentials exist but link is down/wrong SSID (≤ 1×/30 s) |
| `liveReconfigureMDNS()` | (re)start mDNS and refresh service/TXT records (≤ 1×/60 s) |
| `liveReconfigureDNS()` | captive-DNS keepalive placeholder (≤ 1×/60 s) |
| `liveReconfigureWebServer()` | web-server keepalive placeholder (≤ 1×/30 s) |
| `liveReconfigureAP()` | restart AP if its IP collapsed to `0.0.0.0` |
| `restartAPIfNeeded(bool forceRestart = false)` | full AP teardown + rebuild when `forceRestart` is true |
| `recoverWiFi()` `recoverMDNS()` `recoverDNS()` `recoverWebServer()` `recoverNTP()` `recoverRTC()` | per-subsystem recovery entry points |
| `smartRecovery()` | 10-second watchdog tick (failure counting, reconnects, mDNS re-announce) |
| `verifyRelayStates()` | GPIO read-back audit vs. expected state (≤ 1×/30 s) |
| `saveCriticalState()` / `restoreCriticalState()` | NVS persistence of relay+override snapshot with checksum |
| `performTargetedRecovery()` | ordered multi-subsystem recovery sweep |

### 7.2 Private rate-limit state

| Member | Guards |
|---|---|
| `lastMDNSAnnounce` | mDNS announcement spacing |
| `lastWebServerCheck` | web-server check spacing |
| `lastFullHealthCheck` | 30-min full health sweep |
| `lastWiFiReconfigure` | WiFi reconfigure/recover spacing |
| `lastMDNSReconfigure` | mDNS reconfigure spacing |
| `lastDNSReconfigure` | DNS reconfigure spacing |

Method-by-method details are in [§10.13](#1013-self-healing-methods).

---

## 8. Global Variables & Objects

### 8.1 Core objects
| Symbol | Line | Type | Purpose |
|---|---|---|---|
| `preferences` | 23 | `Preferences` | NVS accessor for all persistence |
| `rtc` | 143 | `RTC_DS3231` | hardware RTC driver instance |
| `dnsServer` | 174 | `DNSServer` | captive-portal DNS |
| `server` | 175 | `WebServer(80)` | HTTP server |
| `DNS_PORT` | 176 | `const byte = 53` | DNS listen port |
| `healer` | 410 | `SelfHealingSystem` | recovery subsystem |

### 8.2 Configuration blobs (persisted)
| Symbol | Line | Type | NVS key |
|---|---|---|---|
| `sysConfig` | 283 | `SystemConfig` | `sysConfig` |
| `extConfig` | 284 | `ExtConfig` | `extConfig` |
| `relayConfigs[MAX_RELAYS]` | 285 | `RelayConfig[16]` | `relayConfigs` |
| `gpioConfig` | 209 | `GPIOPinConfig` | `gpioConfig` |
| `criticalState` | 287 | `CriticalRelayState` | `criticalState` |
| `criticalStateInitialized` | 288 | `bool` | — (RAM flag: snapshot ever saved) |
| `criticalStateDirty` | 289 | `bool` | — (RAM flag: snapshot needs saving) |

### 8.3 Internal RTC state *(L292–303)*
| Symbol | Type | Purpose |
|---|---|---|
| `internalEpoch` | `uint64_t` | authoritative Unix epoch base (UTC) |
| `internalMillisAtLastSync` | `unsigned long` | `millis()` snapshot at last epoch set |
| `driftCompensation` | `float = 1.0` | multiplicative drift factor (structural; saved/loadable) |
| `rtcInitialized` | `bool` | internal RTC holds valid time |
| `lastRTCUpdate` | `unsigned long` | last rebase tick |
| `rtcMicrosAtLastSync` | `unsigned long` | `micros()` snapshot at last rebase |
| `lastRTCRebase` | `unsigned long` | `millis()` of last rebase |
| `lastInternalRTCSave` | `unsigned long` | last NVS auto-save tick |
| `INTERNAL_RTC_SAVE_INTERVAL` | `3 600 000 ms` | auto-save cadence (1 h) |

### 8.4 DS3231 state *(L143–147, 305–309)*
| Symbol | Purpose |
|---|---|
| `rtcPresent` | chip answered on I²C |
| `rtcTimeValid` | chip time passed validity window |
| `lastRTCDSync` | ms of last DS3231 adjust/read sync |

### 8.5 NTP state *(L305–314)*
| Symbol | Purpose |
|---|---|
| `ntpServerIndex` | current server in the fallback pool (round-robin) |
| `ntpFailCount` | consecutive sync failures |
| `lastNTPSync` / `lastNTPAttempt` | ms timestamps (success / any attempt) |
| `ntpAsyncState` / `ntpAsyncCurrentServer` / `ntpAsyncPhaseStart` | async state-machine bookkeeping (see note N1) |

### 8.6 WiFi state *(L316–333, 151–155)*
| Symbol | Line | Purpose |
|---|---|---|
| `wifiConnected` | 316 | STA link currently up |
| `lastWiFiCheck` | 317 | last 10 s evaluation tick |
| `wifiReconnectAttempts` | 318 | retries in current episode |
| `wifiGiveUpUntil` | 319 | ms before which no reconnect is allowed (backoff) |
| `MAX_RECONNECT` | 320 | `10` attempts before 5-min give-up |
| `wifiConnecting` / `wifiConnectStart` | 322–323 | in-progress attempt + start time |
| `wifiFirstAttempt` | 324 | distinguish first boot attempt for backoff logic |
| `scanInProgress` | 326 | `volatile` async scan active |
| `scanResultCount` | 327 | `volatile` scan results ready (`-1` = none) |
| `scanStartTime` | 328 | scan watchdog start (10 s timeout in `loop()`) |
| `wifiPausedForScan` | 151 | STA connect deliberately paused for scanning |
| `wifiPauseUntil` / `WIFI_PAUSE_DURATION` | 152–153 | pause deadline (15 s) |
| `lastScanAttempt` | 154 | pause-safety fallback timer |

### 8.7 AP / mDNS state
| Symbol | Line | Purpose |
|---|---|---|
| `ap_ssid[32]`, `ap_password[32]` | 331–332 | legacy mirrors of `sysConfig.ap_*` (kept in sync on load/save) |
| `mdnsStarted` | 335 | responder running |
| `mdnsHostname[32]` | 336 | active hostname |
| `mdnsRestartPending` / `mdnsRestartScheduled` | 337–338 | deferred restart bookkeeping |

### 8.8 Health & misc
| Symbol | Purpose |
|---|---|
| `health` (`HealthMetrics`) | failure counters & recovery timestamps |
| `minFreeHeap`, `lastHeapCheck`, `lastMemoryCleanup`, `lastConnectionActivity`, `lastServerRestart` | memory/connection watchdog state (L358–362) |
| `responseCache` (L374) | API payload cache struct (see note N4) |

### 8.9 Schedule engine state *(L343–355)*
| Symbol | Type | Purpose |
|---|---|---|
| `cachedTodayBit` | `uint8_t` | precomputed day bit for "now" |
| `cachedMonthDay` / `cachedMonth` | `int` | day-of-month / month of "now" |
| `lastScheduleEpoch` | `uint64_t` | epoch used for the cache |
| `scheduleActiveCache[MAX_RELAYS]` | `bool[16]` | per-relay "any schedule active now" |
| `lastScheduleCacheUpdate` | ms | cache refresh tick (`SCHEDULE_CACHE_INTERVAL` = 1 s) |
| `lastScheduleProcess` | ms | 250 ms engine cadence tick |
| `lastRelayUpdate` | ms | reference output tick (see note N2) |
| `lastRelayOutputs[MAX_RELAYS]` | `bool[16]` | last commanded logical outputs |
| `relayOutputsInitialized` | `bool` | first-write done after boot |

---

## 9. Forward Declarations

**L404–455.** Prototypes for every configuration, RTC, schedule, web-server, API-handler, memory, and WiFi-scan function + the `healer` instance, allowing the sketch's cross-calling structure to compile as a single translation unit. Also includes `initScheduleDefaults(int)` and `getNTP64Time(const char*)`. (Arduino .ino auto-prototyping is explicitly **not** relied upon.)

---

## 10. Function Reference

> Function locations are given as **L\<line\>** of the source sketch. "Side effects" lists hardware, flash (NVS), and global mutations.

### 10.1 Millis / Timing Helpers

#### `inline bool timeHasElapsed(unsigned long current, unsigned long previous, unsigned long interval)` — L159
Overflow-safe elapsed-time test using unsigned wraparound arithmetic: `(current - previous) >= interval`. Correct across the 49.7-day `millis()` rollover.
**Returns:** `true` when at least `interval` ms have passed since `previous`.
**Used by:** virtually every periodic task in the firmware.

#### `inline bool isTimeReached(unsigned long current, unsigned long target)` — L166
Overflow-safe "deadline reached" test using signed wraparound: `(long)(current - target) >= 0`.
**Returns:** `true` when `current` has reached/passed `target`.
**Used by:** WiFi give-up backoff, mDNS deferred restart, scan-pause expiry.

---

### 10.2 64-bit Time Conversion Helpers

#### `inline uint64_t rtcDateTimeToUint64(DateTime dt)` — L458
Widens `DateTime::unixtime()` (32-bit) to a 64-bit epoch.
**Returns:** `uint64_t` Unix time.

#### `inline DateTime uint64ToRtcDateTime(uint64_t t64)` — L461
Converts a 64-bit epoch back to an RTClib `DateTime` by masking to the low 32 bits (`t64 & 0xFFFFFFFF`). Valid for dates inside the DS3231's practical 2000–2100 window.
**Returns:** `DateTime` suitable for `rtc.adjust()`.

#### `struct tm* gmtime64(uint64_t* timep)` — L468
64-bit replacement for libc `gmtime()`. Breaks a 64-bit epoch into civil calendar fields using **Howard Hinnant's `civil_from_days` era/day-of-era math** (handles leap rules without a year loop), so it stays correct past 2106 where 32-bit `gmtime()` fails.

* **Special case:** epoch `0` → 1970-01-01 00:00:00, Thursday (`tm_wday = 4`).
* Fills: `tm_sec, tm_min, tm_hour` from the day remainder; `tm_mday, tm_mon (0-based), tm_year (since 1900), tm_wday` (`(days+4)%7`, Sunday=0), `tm_yday`, `tm_isdst = 0` (UTC).
* **Returns:** pointer to a function-`static struct tm` (not thread-safe — safe here because the sketch is single-threaded cooperative).
* **Used by:** schedule engine, time API, browser-sync response.

#### `struct tm* localtime64(uint64_t* timep)` — L511
Applies `gmt_offset + daylight_offset` (seconds) to the epoch, then delegates to `gmtime64()`.
**Returns:** broken-down **local** time in the same static `tm`.

#### `inline uint64_t getLocalEpoch(uint64_t utcEpoch)` — L519
**Returns:** `utcEpoch + gmt_offset + daylight_offset` as 64-bit. The arithmetic core of local-time display and schedule matching.

---

### 10.3 Relay Control Helpers

#### `inline bool isActiveLow(uint8_t index)` — L526
Resolves the electrical polarity of relay `index`:
1. out-of-range index → `true` (fail-safe for common active-low boards);
2. `global_active_mode == 1` → always active-LOW;
3. `global_active_mode == 2` → always active-HIGH;
4. otherwise → per-relay `gpioConfig.activeLow[index]`.

#### `inline unsigned long getNTPInterval()` — L541
**Returns:** `extConfig.ntp_sync_hours` (clamped into 1–24) in milliseconds — the NTP resync period.

#### `inline int getRelayPin(uint8_t index)` — L546
**Returns:** GPIO number for relay `index`, or `-1` when `index >= gpioConfig.count`. All GPIO access goes through this guard.

#### `inline uint8_t getActiveRelayCount()` — L552
**Returns:** `gpioConfig.count` — live relay count (1–16).

#### `inline void setRelayOutput(uint8_t index, bool state)` — L559
The **only** function that ever writes a relay GPIO.
* Looks up the pin via `getRelayPin()` (no-op on `-1`).
* Inverts for active-low: `digitalWrite(pin, isActiveLow(index) ? !state : state)`.
* `state` is always the **logical** ON/OFF, electrical level is derived here.
**Side effects:** GPIO write.

#### `void initScheduleDefaults(int relayIndex)` — L569
Zeroes the whole `RelayConfig` for `relayIndex` (`memset`), then seeds each of the 8 slots with `days = DAY_ALL`, `monthDays = 0` (unrestricted), `monthMask = MONTH_ALL`, and names the relay `"Relay <n>"` (`snprintf`, 16-byte buffer).
**Side effects:** mutates `relayConfigs[relayIndex]` (RAM; persistence happens via the caller).

---

### 10.4 WiFi Station Control

#### `void setWiFiStationEnabled(bool enabled)` — L582
Master switch for STA (uplink) mode; persists immediately via `saveExtConfig()`.

**Disabling (`enabled == false`):**
* `WiFi.disconnect(true)`, clears `wifiConnected`, `wifiConnecting`, `wifiPausedForScan`, `wifiReconnectAttempts`, `wifiGiveUpUntil`.
* Downgrades radio mode to `WIFI_AP` if needed (AP stays up — UI remains reachable).
* Resets NTP async state to `NTP_STATE_IDLE`, clears `lastNTPSync`.

**Enabling (`true`):**
* Upgrades radio to `WIFI_AP_STA`.
* If an SSID is configured, immediately `WiFi.begin()` and arms the connect state machine (`wifiConnecting`, `wifiConnectStart`, `wifiFirstAttempt`, cleared backoff).

**Side effects:** NVS write (`extConfig`), radio mode change, invalidates `responseCache`.
**Called by:** `handleSaveWiFi()` (`sta_enabled` toggle path).

---

### 10.5 DS3231 RTC Subsystem

#### `void initRTC()` — L616
Boot-time DS3231 bring-up.
1. `Wire.begin(21, 22)` (SDA 21, SCL 22).
2. `rtc.begin()` failure → `rtcPresent = rtcTimeValid = false`, return (system continues RTC-less).
3. `rtc.lostPower()` → marks time invalid (oscillator stopped / battery event), returns.
4. Reads chip time; accepts only years **2020–2100**; passes through `VALID_UNIX_TIME_64`.
5. On success seeds the **internal** RTC (`internalEpoch`, `driftCompensation = 1.0`, `millis()/micros()` anchors, `rtcInitialized = true`) and sets `timeSource = TIME_SOURCE_RTC`.
**Side effects:** I²C init, internal-RTC seeding.

#### `void immediateDS3231Sync()` — L645
Pushes the current internal epoch **into** the DS3231 right now (used after NTP/browser sync so hardware time follows network time). Guards on `rtcPresent && rtcInitialized && internalEpoch > 0`; re-validates the bus with `rtc.begin()`, then `rtc.adjust(uint64ToRtcDateTime(internalEpoch))`, sets `rtcTimeValid`, stamps `lastRTCDSync`.
**Side effects:** I²C write to DS3231.

#### `void syncDS3231FromInternalRTC()` — L656
Same intent as `immediateDS3231Sync()` without the bus re-check: if RTC present and internal time valid → `rtc.adjust(...)` + validity flags. Extracted as a reusable helper (kept for external/future call sites; normal sync flow uses `immediateDS3231Sync()`).

#### `bool loadRTCFromDS3231()` — L770
Cold-start path: if the chip is present and its time is valid (2020–2100 window + `VALID_UNIX_TIME_64`), copy it into the internal RTC exactly like `initRTC()` does, set `timeSource = TIME_SOURCE_RTC`.
**Returns:** `true` when internal time was (re)initialized from hardware.

---

### 10.6 Internal (Software) RTC Subsystem

#### `void performRTCReabase()` — L668  *(sic — "Rebase")*
Folds elapsed `micros()` into `internalEpoch` and re-anchors the software clock.
* Computes `elapsedMicros` since `rtcMicrosAtLastSync`, **explicitly handling `micros()` overflow** (`0xFFFFFFFF - old + now + 1`).
* Applies `driftCompensation` to elapsed seconds; splits into whole seconds + fraction; **rounds up the epoch when the fraction ≥ 0.5 s**.
* Re-anchors `rtcMicrosAtLastSync`, `lastRTCRebase`, `lastRTCUpdate`.
Guarantees the epoch never depends on an un-rebased span longer than the 5-minute `RTC_REBASE_INTERVAL` — far below the ~71.6-minute `micros()` wrap.

#### `uint64_t getCurrentEpoch()` — L691
The single authoritative "what time is it?" read.
1. If the internal RTC is initialized: rebase first when due (`lastRTCRebase == 0` or 5 min elapsed). On detection of `micros()` wrap mid-call, performs an immediate rebase and returns the rebased epoch.
2. Otherwise computes `internalEpoch + elapsed` with drift + 0.5 s rounding (math identical to the rebase path, non-mutating).
3. If the internal RTC isn't initialized but a valid DS3231 exists → reads the chip directly.
4. Else returns `0` ("no time").
**Returns:** current UTC epoch (64-bit) or `0`.

#### `void saveRTCState()` — L728
Rebases, then persists `sysConfig.last_rtc_epoch = internalEpoch` and `rtc_drift` (currently the constant `1.0f`), followed by a full `saveConfiguration()`.
**Side effects:** NVS write.

#### `void loadRTCState()` — L735
Cold-start restore: if `sysConfig.last_rtc_epoch` passes `VALID_UNIX_TIME_64`, seed the internal RTC from it (anchors all millis/micros references, `rtcInitialized = true`). Gives the device approximate time after a clean reboot even with no NTP/DS3231.

#### `void autoSaveInternalRTC()` — L749
Called every `loop()`. **Flash-wear guard:** when NTP is the current source *or* a valid DS3231 exists, the function just refreshes its tick and returns (better authorities will restore time anyway). Otherwise, every `INTERNAL_RTC_SAVE_INTERVAL` (1 h) it rebases and stores `last_rtc_epoch` + drift via `saveConfiguration()`, so a power cycle loses ≤ 1 h.
**Side effects:** conditional NVS write.

---

### 10.7 WiFi Scan Pause

#### `void pauseWiFiForScan()` — L793
Called before a scan while a STA connection attempt is in flight (scanning and connecting on one radio can starve each other).
* Only acts when `wifiConnecting && !wifiConnected && !wifiPausedForScan && sta_enabled`.
* Sets `wifiPausedForScan`, computes `wifiPauseUntil = millis() + 15 s`, `WiFi.disconnect(true)`, waits 100 ms, clears `wifiConnecting`, stamps `lastScanAttempt`.
The `loop()` clears the pause at its deadline (or after 30 s safety) and zeroes the backoff so the reconnect resumes immediately.

---

### 10.8 NTP Subsystem

#### `uint64_t getNTP64Time(const char* server)` — L2387
Self-contained SNTP client returning a **64-bit** Unix epoch.

**Protocol flow (per retry, up to `NTP_RETRY_COUNT` = 3):**
1. Open UDP on local port **2390**; failure → 100 ms, next retry.
2. Build the 48-byte packet: byte0 `0b11100011` (LI=3, **Version 4**, Mode=3 client), stratum 0, poll 6, precision `0xEC`; writes the device's own current epoch + `NTP_EPOCH_OFFSET` into the transmit-timestamp seconds field (bytes 40–43; fraction zeroed).
3. `beginPacket(server, 123)` + write + `endPacket()`; send failure → cleanup, 500 ms, next retry.
4. Poll `parsePacket()` up to `NTP_TIMEOUT` (5 s, 10 ms cadence with `yield()`).
5. On a ≥48-byte reply: **validate** Mode == 4 (server) and **LI ≠ 3** (reject "clock unsynchronized"); extract seconds field, subtract `NTP_EPOCH_OFFSET`; reject epochs < 1 577 836 800 (2020-01-01) as garbage.

**Returns:** UTC epoch, or `0` on failure. Note the epoch is *wider* than the returned 32-bit NTP field — the API is future-proofed for 2106+ handling (note N5).
**Side effects:** UDP socket open/close; blocking waits bounded ≈ 3 × (5 s + 0.5 s).

#### `void tryNTPSync()` — L2449
Top-level sync orchestrator, safe to call anytime.
* Aborts (async state → IDLE) when offline or STA disabled; throttles attempts to `NTP_RETRY_INTERVAL` (30 s).
* Dials the current pooled server via `getNTP64Time()`; on success (`> 1577836800`): `syncInternalRTC()`, resets failure counters, **rotates** `ntpServerIndex` round-robin.
* On failure walks the remaining three servers; only after all four fail increments `ntpFailCount` and `health.ntpFailures`.

#### `void syncInternalRTC(uint64_t rawUtcEpoch)` — L2359
The funnel through which **any** external time becomes device time.
1. Rejects invalid epochs (`VALID_UNIX_TIME_64`).
2. Re-anchors `internalEpoch` + all millis/micros references, marks `rtcInitialized`.
3. Bookkeeping: browser-sourced syncs update `lastBrowserSync`; everything else records `lastNTPSync` and sets `timeSource = TIME_SOURCE_NTP` (NTP override semantics) — note `handleBrowserTimeSync()` restores `TIME_SOURCE_BROWSER` afterwards.
4. `ntpFailCount = 0`; pushes the new time into the DS3231 if present (`immediateDS3231Sync()`); persists via `saveRTCState()`; marks `criticalStateDirty`; refreshes `updateScheduleCache()` so schedules react to the (possibly jumped) time immediately.
**Side effects:** NVS write, DS3231 adjust, cache invalidation.

---

### 10.9 Browser Time Sync

#### `void handleBrowserTimeSync()` — L2485
`POST /api/time/browser-sync` — last-resort time source when NTP is unreachable.
* Requires JSON body `{"utc_epoch": <seconds>}`; 400 on missing/malformed JSON.
* Validates epoch ∈ [1 577 836 800 (2020), `MAX_UNIX_TIME_64`].
* Sets `timeSource = TIME_SOURCE_BROWSER` (both before and after `syncInternalRTC()` so NTP's override inside the funnel doesn't mislabel it), records `lastBrowserSync`, clears `lastNTPSync`.
* Adjusts the DS3231 (re-validating the bus), `saveRTCState()`, `updateScheduleCache()`, `criticalStateDirty = true`.
* **200 response:**
```json
{ "success": true, "utc_epoch": 1754550000, "local_time": "YYYY-MM-DD HH:MM:SS",
  "gmt_offset": 28800, "time_source": "browser",
  "rtc_present": true, "rtc_synced": true, "drift": 1.0 }
```
(`local_time` is produced by `gmtime64()` on the offset epoch; `utc_epoch` echo is 32-bit-truncated, note N5.)

---

### 10.10 WiFi Connection Manager

#### `void beginWiFiConnect()` — L2547
Guarded connection kick used by the reconnect logic. No-ops when: STA disabled, no SSID configured, give-up backoff still pending (`isTimeReached(millis(), wifiGiveUpUntil)`), already connecting, or paused for a scan. Otherwise: `WiFi.disconnect(true)` + 100 ms settle, increments `wifiReconnectAttempts`, ensures `WIFI_AP_STA` mode, `WiFi.begin(ssid, password)`, arms `wifiConnecting`/`wifiConnectStart`.

---

### 10.11 Relay & Schedule Engine

#### `void updateRelayOutputs()` — L2567
Immediate reconciler: for every active relay, pick state from `manualOverride ? manualState : scheduleActiveCache[i]` and write the GPIO only when it differs from `lastRelayOutputs[i]` (or on the first run), then sets `relayOutputsInitialized`. Retained as a utility — the main `loop()` drives outputs via `processRelaySchedules()` instead (note N2).

#### `void updateScheduleCache()` — L2586
Recomputes the per-relay "schedule active now" booleans against current time.
1. Reads `getCurrentEpoch()`; ignores absurd times (< `MIN_UNIX_TIME_64`).
2. Breaks down the **local** epoch (`getLocalEpoch` → `gmtime64`) and caches `cachedTodayBit`, `cachedMonthDay`, `cachedMonth`.
3. For each active relay (skipping manual-overridden ones, whose cache entry is forced `false`): ORs together all 8 slots. A slot counts when **all** filters pass:
   * `enabled[s]`;
   * month filter: if `monthMask != 0`, current month bit must be set;
   * weekday bit set in `days[s]`;
   * if `monthDays != 0`, bit `(day-1)` must be set;
   * time window matches with three semantics — `start == stop` → **always ON**; `start < stop` → `start ≤ now < stop`; `start > stop` → **overnight**: `now ≥ start OR now < stop`.
4. Stores results in `scheduleActiveCache[]`, stamps `lastScheduleCacheUpdate`.
**Called by:** the engine every 1 s, plus every mutation path (save, reset, time sync, factory reset, GPIO changes).

#### `void processRelaySchedules()` — L2641
The 250 ms heartbeat that physically drives outputs.
1. Refreshes the cache when older than `SCHEDULE_CACHE_INTERVAL` (1 s).
2. Re-derives local time (epoch-validated) using the cached date fields.
3. **Manual override branch:** drive to `manualState` immediately; update `lastRelayOutputs`, debounce mirror, `criticalStateDirty = true`.
4. **Automatic branch:** re-evaluates all 8 slots with the same filter algebra as `updateScheduleCache()` — deliberately computed independently so outputs don't depend on cache freshness.
5. **Debounce:** a change is applied only if the target state stayed stable for **500 ms** (`lastStateChange[]` / `lastDebouncedState[]` static arrays), then `setRelayOutput()` + bookkeeping + `criticalStateDirty`.
6. Sets `relayOutputsInitialized = true`.
**Side effects:** GPIO writes.

#### `void restartAP()` — L2711
Thin wrapper: `healer.restartAPIfNeeded(false)`. With the (default) `false` the healer only acts when the AP is actually down — see note N6. Kept for call-site compatibility.

---

### 10.12 mDNS Subsystem

#### `void startMDNS()` — L2718
Starts the responder once (`mdnsStarted` guard).
* If `mdnsHostname` is empty or still the default, derives it from the **AP SSID**: lowercase, spaces/underscores → `-`, strips non-alphanumerics, falls back to `"esp32"`, truncates to 31 chars.
* `MDNS.begin(hostname)`; advertises service `http/tcp:80` with TXT `model=ESP32`, `version=v11`, `channels=<count>`, `features=monthmask,64bit`.
**Side effects:** multicast responder, sets `mdnsStarted`.

#### `void stopMDNS()` — L2750
`MDNS.end()` and clears `mdnsStarted` if running.

#### `void restartMDNS()` — L2757
Delegates to `healer.liveReconfigureMDNS()` (rate-limited restart/refresh of the responder + TXT refresh).

#### `void scheduleMDNSRestart()` — L2761
Arms a deferred restart: `mdnsRestartPending = millis() + MDNS_RESTART_DELAY (2 s)`; consumed by `loop()` → `restartMDNS()`. Lets API responses return before the responder drops.

#### `String getMDNSHostname()` — L2766
**Returns:** current `mdnsHostname` as `String` (used by `/api/mdns` and `/api/system`).

#### `void setMDNSHostname(const char* hostname)` — L2770
Sanitizes a user hostname (lowercase; alnum + `-` kept; space/underscore → `-`; ≤ 31 chars), stores it, and if the responder is live schedules the deferred restart so the new name propagates. No-op on empty/oversized input.

---

### 10.13 Self-Healing Methods

All members of `SelfHealingSystem`; instance: `healer`.

#### `uint32_t calculateCriticalChecksum()` — L807 *(free function used by the class)*
Integrity checksum for `CriticalRelayState`: accumulates bit `i` for each ON relay and bit `(i+16) % 32` for each active override, then XORs with `timestamp`. Cheap corruption/noise detector for the NVS snapshot.

#### `void SelfHealingSystem::saveCriticalState()` — L816
Snapshots current relay truth into `criticalState`: active relays get `lastRelayOutputs[]` + `manualOverride[]`; idle slots zeroed; stamps `millis()`, `magic = 0xDEADBEEF`, checksum; writes NVS key `criticalState`. Sets `criticalStateInitialized`.
**Called by:** `loop()` (dirty + 5 min throttle) and `smartRecovery()` 30-min sweep.

#### `bool SelfHealingSystem::restoreCriticalState()` — L834
Boot path (from `setup()`). Reads the blob; requires exact size + magic + checksum match. Restores only **manual-override** relays: re-applies their GPIO level, `lastRelayOutputs`, `manualOverride`, `manualState`. Schedule-controlled relays re-derive naturally from the schedule engine.
**Returns:** `true` on valid restore.

#### `bool SelfHealingSystem::liveReconfigureWiFi()` — L857
Rate-limited to 1×/30 s; skipped when STA disabled. If credentials exist and the link is down **or the connected SSID differs from the configured one**, re-issues `WiFi.begin()` and re-arms the connect state machine. **Returns** `true` (best-effort).

#### `bool SelfHealingSystem::liveReconfigureMDNS()` — L878
Rate-limited to 1×/60 s. Restarts mDNS when stopped (full service + TXT set) or refreshes the dynamic TXT fields (`channels`, `features`) when running. **Returns** `mdnsStarted`.

#### `bool SelfHealingSystem::liveReconfigureDNS()` — L899 / **`liveReconfigureWebServer()`** — L906
Rate-limited keepalive checkpoints for the captive DNS (60 s) and web server (30 s). Both services are self-recovering in the Arduino core, so these currently just stamp their timers and return `true` — explicit hooks for future hardening.

#### `bool SelfHealingSystem::liveReconfigureAP()` — L913
Detects a collapsed AP (`softAPIP() == "0.0.0.0"`) and rebuilds it with the configured SSID/password (open network if password blank), channel (validated 1–13, default 6) and hidden flag; then kicks DNS + mDNS reconfiguration. **Returns** `true`.

#### `void SelfHealingSystem::restartAPIfNeeded(bool forceRestart = false)` — L930
Heavy AP rebuild used by factory reset and AP-settings changes — only when `forceRestart` is true: `softAPdisconnect(true)` + 500 ms, re-validate channel/hidden, ensure AP-capable radio mode, `softAP(...)` (secured or open), then on success DNS + mDNS reconfigure. With `false` it is a no-op (note N6).

#### `bool SelfHealingSystem::recoverWiFi()` — L957
One-shot recovery (30 s spacing): reconnects when credentials exist and link is down; **returns** link state. Honors the STA master switch (returns `true` = nothing to do when disabled).

#### `bool SelfHealingSystem::recoverMDNS()` — L971 / **`recoverDNS()`** — L975 / **`recoverWebServer()`** — L979
Thin pass-throughs to the corresponding `liveReconfigure*()` methods.

#### `bool SelfHealingSystem::recoverNTP()` — L983
Emergency time recovery: if WiFi is available, walks the entire fallback pool once (100 ms + `yield()` between servers) until `getNTP64Time()` yields a sane epoch; adopts it (`timeSource = NTP`, `syncInternalRTC`, pool index updated). **Returns** success.

#### `bool SelfHealingSystem::recoverRTC()` — L1001
DS3231 resuscitation: re-inits I²C + chip; if internal time is known, writes it to the chip; otherwise if the chip holds valid time, pulls it into the internal RTC. **Returns** whether valid RTC time exists afterwards.

#### `void SelfHealingSystem::performTargetedRecovery()` — L1022
Ordered best-effort sweep with 50 ms breathing room between steps: **web server → DNS → mDNS → WiFi → AP → RTC → relay verification**, then clears the web/mDNS/DNS/WiFi failure counters. Invoked by `POST /api/reset` ("Verify Services" button) and both factory-reset paths.

#### `void SelfHealingSystem::verifyRelayStates()` — L1041
**GPIO read-back audit**, ≤ 1×/30 s. For each active relay computes the expected logical output (manual state or schedule cache), converts through `isActiveLow()`, `digitalRead()`s the pin, and re-writes + re-logs on mismatch. Catches external interference or missed writes.

#### `void SelfHealingSystem::smartRecovery()` — L1061
The 10-second watchdog tick driven from `loop()`:
* **WiFi:** if STA should be up but isn't (and no scan pause/connect in flight) count a failure; on the **3rd** consecutive failure re-issue `WiFi.begin()` and reset the counter. Counts reset on success.
* **mDNS:** re-announce the HTTP service every 5 min.
* **30-min full sweep:** re-announce mDNS; flush a dirty critical state to NVS.
* Always ends with `verifyRelayStates()` + `liveReconfigureAP()`.

---

### 10.14 Memory Management

#### `void performMemoryCleanup()` — L1097
Reclaims fragmented resources: deletes leftover WiFi scan results (unless a scan is live), yields 10×, closes the NVS handle, then performs 5 × (`malloc(512)`/`free`) — a heap-trim nudge to coalesce free blocks. Deliberately gentle: everything remains running.

#### `void cleanupStaleResources()` — L1114
30 s cadence from `loop()`: kills a scan stuck in `WIFI_SCAN_RUNNING` outside an intentional scan; expires the response cache after 5 s (clears the three `String` payloads); finishes with `performMemoryCleanup()`.

#### `void checkWebServerHealth()` — L1128
If no HTTP client activity for 5 minutes (but there once was), triggers the healer's web-server reconfigure and re-arms the activity timestamp.

#### `void checkAndCleanMemory()` — L1136
Called every `loop()`:
* every 5 min: sample `ESP.getFreeHeap()`, track the historical minimum in `minFreeHeap`, run a cleanup when free heap < **20 KB**;
* every 1 h: unconditional `performMemoryCleanup()`.

---

### 10.15 Boot-Button Factory Reset

#### `void checkBootButton()` — L2287
Called first in every `loop()`. Debounced, non-blocking state machine on `BOOT_BUTTON_PIN` (GPIO 0, `INPUT_PULLUP`, active-low):
* **Press edge:** records `bootButtonPressStart`, sets `bootButtonPressed`.
* **Release edge:** clears the flag (a short press does nothing).
* **Held ≥ `FACTORY_RESET_HOLD` (5 s):** executes the full reset once per press:
  1. `preferences.clear()` — wipes the entire `relay16` NVS namespace;
  2. `initDefaults()` + reload `GPIO`/`System`/`Ext` configs (now factory values);
  3. all active relays forced OFF, overrides cleared, outputs marked initialized;
  4. if STA should be up: disconnect + fresh `WiFi.begin()` with full state-machine reset;
  5. `healer.restartAPIfNeeded(true)` (AP rebuild) + `healer.performTargetedRecovery()`;
  6. `updateScheduleCache()`; clears `factoryResetTriggered` ready for the next hold.
The device **does not reboot** — it self-reconfigures live.

---

### 10.16 GPIO Configuration Persistence

#### `void loadGPIOConfig()` — L2334
Reads NVS blob `gpioConfig`. Rejects it (size mismatch / bad magic / `count` 0 or > 16) and rebuilds factory state: 16 channels from `DEFAULT_RELAY_PINS`, all active-low, then immediately `saveGPIOConfig()`.

#### `void saveGPIOConfig()` — L2350
Writes the whole `GPIOPinConfig` struct to NVS under `gpioConfig`.

---

### 10.17 Configuration Persistence (NVS)

#### `void initDefaults()` — L3019
Builds factory configuration and persists it.
* `sysConfig`: magic + version 11, AP `ESP32_16CH_Timer_Switch` / `ESP32-admin`, NTP host `time.google.com`, GMT **+28800 s (UTC+8)**, DST 0, epoch 0, drift 1.0, hostname `esp32`.
* All 16 relays → `initScheduleDefaults()` (8 empty slots each, everyday/all-month masks, names `Relay n`).
* GPIO factory map (16 pins, active-low); `timeSource = NONE`; clears browser-sync stamp and drift.
* Writes back via `saveConfiguration()` + `saveGPIOConfig()`.

#### `void loadConfiguration()` — L3047
1. Reads `sysConfig`; size/magic failure → `initDefaults()` and re-read.
2. **Migration:** `version < 11` → for `version < 10` fills every slot's `monthMask` with `MONTH_ALL` (feature added in v10/v11 era), bumps `version` to 11, saves.
3. Reads the 16-element `relayConfigs` blob; size mismatch → re-initialize all defaults.
4. Mirrors `ap_ssid`/`ap_password` into the legacy globals.

#### `void saveConfiguration()` — L3082
Atomically-ish writes both blobs: `sysConfig` and the full `relayConfigs` array. The single persist point for schedule/manual/name/system changes.

#### `void loadExtConfig()` — L3089
Reads `extConfig`. Invalid (size/magic) → zero, seed defaults (magic, channel 6, sync 1 h, visible AP, per-relay active mode 0, **STA enabled**), save, re-open. Valid → clamp every field into range (channel 1–13, hours 1–24, mode ≤ 2, sta_enabled ≤ 1) defensively.

#### `void saveExtConfig()` — L3120
Writes `extConfig` to NVS.

---

### 10.18 `setup()` and `loop()`

#### `void setup()` — L2794
Boot sequence, in order:

| Step | Action |
|---|---|
| 1 | `initRTC()` — I²C + DS3231 probe/seeding (before WiFi so time exists ASAP) |
| 2 | `loadGPIOConfig()`; `pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP)` |
| 3 | Every active relay pin → `OUTPUT` + logical OFF; `relayOutputsInitialized = true` (safe power-on) |
| 4 | `initScheduleDefaults(i)` × 16 (RAM baseline before NVS load) |
| 5 | `loadConfiguration()` + `loadExtConfig()` |
| 6 | Time restore ladder: **DS3231** → **saved NVS epoch** → time source NONE (drift 1.0, uninitialized) |
| 7 | Radio to `WIFI_AP_STA`; if STA enabled with SSID → immediate `WiFi.begin()`; if STA disabled → downgrade to `WIFI_AP` |
| 8 | Validate AP channel (1–13, else 6 + persist), hidden flag; `softAPdisconnect` + fresh `softAP()` (secured/open) |
| 9 | `startMDNS()` |
| 10 | `dnsServer.start(53, "*", softAPIP())` — captive-portal wildcard DNS |
| 11 | `setupWebServer()` — all routes + `server.begin()` |
| 12 | `updateScheduleCache()` (harmless no-op until time is valid) |
| 13 | `healer.restoreCriticalState()` — re-apply manual overrides that survived a crash |
| 14 | Arm watchdog timestamps (memory, heap, activity, server, RTC auto-save) |

#### `void loop()` — L2865
Cooperative super-loop; nothing blocks for long. Per iteration, in order:

| Task | Cadence |
|---|---|
| `checkBootButton()` | every pass |
| `server.handleClient()` + `dnsServer.processNextRequest()` | every pass |
| track `lastConnectionActivity` when a client is present | every pass |
| **Stale-client purge** (stop up to 5 half-open clients) + scan-result cleanup | 60 s |
| `cleanupStaleResources()` (memory + cache expiry) | 30 s |
| `checkWebServerHealth()` | 60 s |
| `healer.smartRecovery()` | 10 s |
| `checkAndCleanMemory()` | internal (5 min / 1 h) |
| **DS3231 → internal RTC resync** (read chip, re-anchor, mark source RTC unless NTP) | `DS3231_SYNC_INTERVAL` = 1 h |
| `autoSaveInternalRTC()` | internal 1 h (skipped when NTP/RTC authoritative) |
| deferred **mDNS restart** when scheduled | at deadline |
| **scan watchdog**: force-complete scans older than 10 s | — |
| **connect state machine**: on `WL_CONNECTED` → clear flags, `tryNTPSync()`; on 20 s timeout → count failure; ≥ 10 failures (not first attempt) → 5-min give-up backoff | while connecting |
| clear scan pause at deadline / 30 s safety | — |
| **WiFi watchdog** (`WIFI_CHECK_INTERVAL` 10 s): detect dropped link, trigger `beginWiFiConnect()`; detect unsolicited links (e.g. healer) → adopt + NTP sync | 10 s |
| **NTP scheduler**: sync when never synced / failure retry due / `getNTPInterval()` elapsed | event-driven |
| `processRelaySchedules()` | 250 ms |
| `healer.saveCriticalState()` when `criticalStateDirty` | throttled 5 min |
| `yield()` | every pass |

---

### 10.19 Web Server Route Registration

#### `void setupWebServer()` — L3129
Registers every route, then `server.begin()`. Four route families:

**A. Page & asset routes (GET, served from PROGMEM):**

| Path | Serves |
|---|---|
| `/` | `index_html` (Relays) |
| `/wifi` | `wifi_html` |
| `/ntp` | `ntp_html` (Time) |
| `/ap` | `ap_html` |
| `/gpio` | `gpio_html` |
| `/system` | `system_html` |
| `/style.css` | `style_css` |

**B. JSON API routes** — delegated to the named handlers of §10.20:

| Method | Path | Handler |
|---|---|---|
| GET | `/api/relays` | `handleGetRelays` |
| POST | `/api/relay/manual` | `handleManualControl` |
| POST | `/api/relay/reset` | `handleResetManual` |
| POST | `/api/relay/save` | `handleSaveRelay` |
| POST | `/api/relay/name` | `handleRelayName` |
| GET | `/api/time` | `handleGetTime` |
| POST | `/api/time/browser-sync` | `handleBrowserTimeSync` |
| GET / POST | `/api/wifi` | `handleGetWiFi` / `handleSaveWiFi` |
| POST / GET | `/api/wifi/scan` | `handleWiFiScanStart` / `handleWiFiScanResults` |
| GET / POST | `/api/ntp` | `handleGetNTP` / `handleSaveNTP` |
| POST | `/api/ntp/sync` | `handleSyncNTP` |
| GET / POST | `/api/ap` | `handleGetAP` / `handleSaveAP` |
| GET | `/api/gpio` | `handleGetGPIOConfig` |
| POST | `/api/gpio/save` | `handleSaveGPIOConfig` |
| POST | `/api/gpio/add` | `handleAddGPIO` |
| POST | `/api/gpio/delete` | `handleDeleteGPIO` |
| POST | `/api/gpio/toggle-active-low` | `handleToggleActiveLow` |
| GET / POST | `/api/gpio/global-mode` | inline lambda / `handleGlobalActiveMode` |
| GET / POST | `/api/mdns` | inline lambdas (get/set hostname) |
| POST | `/api/mdns/restart` | inline lambda → `restartMDNS()` |
| GET | `/api/system` | `handleGetSystem` |
| POST | `/api/reset` | `handleReset` |
| POST | `/api/factory-reset` | `handleFactoryReset` |

**Inline mDNS lambdas:**
* `GET /api/mdns` → `{"hostname":..., "started":bool, "url":"http://<host>.local"}`.
* `POST /api/mdns` — body `{"hostname":"<1–31 chars>"}`; sanitized via `setMDNSHostname()` (deferred restart); 400 on bad JSON/invalid hostname.
* `POST /api/mdns/restart` → `restartMDNS()`, `{"success":true}`.
* `GET /api/gpio/global-mode` → `{"mode":0|1|2}`.

**C. Captive-portal detection endpoints** — satisfy OS connectivity probes so phones pop the portal up:

| Path | Behavior |
|---|---|
| `/hotspot-detect.html`, `/library/test/success.html` (Apple) | plain "Success" HTML 200 |
| `/generate_204` (Android), `/canonical.html`, `/redirect` | 302 → `http://<AP-IP>/` |
| `/success.txt` | `success\n` 200 |
| `/connecttest.txt` | `Microsoft Connect Test` |
| `/ncsi.txt` | `Microsoft NCSI` |

**D. Catch-all:** `server.onNotFound(...)` → 302 redirect to the AP IP homepage (the captive-portal trap) + `server.begin()`.

---

### 10.20 HTTP API Handlers

Every handler bumps `lastConnectionActivity` and replies JSON `{"success":...}`-style. Error paths use HTTP 400.

#### `void handleGetRelays()` — L3232 · `GET /api/relays`
* Honors the response cache (valid, < 2 s old, non-empty) — see note N4.
* Otherwise **streams** the array chunked (`CONTENT_LENGTH_UNKNOWN`): per relay a 512-byte `StaticJsonDocument` →
```json
[{ "state": false, "manual": false, "name": "Relay 1", "pin": 15,
   "schedules": [ { "startHour":0,"startMinute":0,"startSecond":0,
                    "stopHour":0,"stopMinute":0,"stopSecond":0,
                    "enabled":false,"days":127,"monthDays":0,"monthMask":4095 } ×8 ] } ×count]
```
* `yield()` between relays; marks cache invalid after streaming.

#### `void handleManualControl()` — L3273 · `POST /api/relay/manual`
Body `{"relay":i,"state":bool}`. Validates range → sets `manualOverride = true`, `manualState = state`, clears the schedule-cache entry, **immediately** drives the GPIO via `setRelayOutput()` + `lastRelayOutputs[]`, persists (`saveConfiguration()`), `criticalStateDirty`, cache invalidate → `{"success":true}`. Out-of-range → 400 `Invalid relay`.

#### `void handleResetManual()` — L3302 · `POST /api/relay/reset`
Body `{"relay":i}`. Clears `manualOverride`, recomputes `updateScheduleCache()` (relay reverts to schedule rule at the next engine tick), persists, cache invalidate → success. 400 on bad index/JSON.

#### `void handleSaveRelay()` — L3326 · `POST /api/relay/save`
Body `{"relay":i,"schedules":[{...} ×8]}` (5 120-byte document). Copies up to 8 slots field-by-field; defaults `days|0`, `monthDays|0`, `monthMask|MONTH_ALL` for missing keys; then `saveConfiguration()` + `updateScheduleCache()` + cache invalidate. 400 on invalid relay/JSON.

#### `void handleRelayName()` — L3365 · `POST /api/relay/name`
Body `{"relay":i,"name":"..."}`. `strncpy(..., 15)` + forced NUL, persist, cache invalidate. 400 on invalid data.

#### `void handleGetTime()` — L3390 · `GET /api/time`
Header clock/status feed:
```json
{ "time": "HH:MM:SS"|"--:--:--", "wifi": true, "ntp": true,
  "timeSource": "none|ntp|browser|rtc",
  "rtcPresent": true, "rtcSynced": true, "rtcSyncAge": 123 }
```
`time` comes from `gmtime64()` on the local epoch; `rtcSyncAge` = seconds since last DS3231 sync (0 if never).

#### `void handleGetWiFi()` — L3418 · `GET /api/wifi`
```json
{ "ssid": "...", "connected": true, "ip": "192.168.1.7", "rssi": -58, "sta_enabled": true }
```
(`snprintf` into a 384-byte buffer; RSSI 0 when disconnected.)

#### `void handleSaveWiFi()` — L3432 · `POST /api/wifi`
Two-mode handler:
* **Station toggle:** `{"sta_enabled":bool}` → `setWiFiStationEnabled()`, echoes new state.
* **Credentials:** `{"ssid":"... (<32)", "password":"..."}` — updates `sta_ssid`/`sta_password` (blank password = clears it), persists; if STA enabled and either changed → clears scan pause, `disconnect` + 500 ms, full connect state-machine reset, `WiFi.begin()` with new credentials. Replies success; **does not wait** for the link (UI redirects after 3 s).
400 on missing data / invalid SSID.

#### `void handleWiFiScanStart()` — L3487 · `POST /api/wifi/scan`
Rejects (400) when STA is disabled. Pauses an in-flight connect (`pauseWiFiForScan()`), arms scan state (`scanInProgress`, count −1, 10 s watchdog), ensures `WIFI_AP_STA`, starts **async, show-hidden** scan (`WiFi.scanNetworks(true, true)`). Replies **202** `{"scanning":true}`.

#### `void handleWiFiScanResults()` — L3508 · `GET /api/wifi/scan`
Polling endpoint. While running → `{"scanning":true}`; on completion records count, clears the flag, and extends the scan pause by 5 s so the reconnect doesn't race result retrieval. Builds (8 KB doc):
```json
{ "scanning": false, "networks": [ { "ssid": "...", "rssi": -62, "enc": true } ≤30 ] }
```
Skips empty (hidden-stub) SSIDs, frees driver results (`scanDelete`), resets count.

#### `void handleGetNTP()` — L3544 · `GET /api/ntp`
```json
{ "ntpServer":"time.google.com", "gmtOffset":28800, "daylightOffset":0,
  "syncHours":1, "globalActiveMode":0 }
```

#### `void handleSaveNTP()` — L3558 · `POST /api/ntp`
Body `{"ntpServer":"... (<48)", "gmtOffset":s, "daylightOffset":s, "syncHours":1–24}`; saves offsets + server to `saveConfiguration()`, hours (when present & in range) to `saveExtConfig()`. 400 on invalid server/JSON.

#### `void handleSyncNTP()` — L3590 · `POST /api/ntp/sync`
Non-blocking kick: rejects (400) when WiFi down/STA disabled; resets `ntpAsyncState`, `lastNTPSync`, `lastNTPAttempt` so the `loop()` NTP scheduler fires immediately. Replies `{"success":true,"message":"NTP sync initiated"}` — actual sync completion is observed via `/api/time`.

#### `void handleGetAP()` — L3603 · `GET /api/ap`
```json
{ "ap_ssid":"...", "ap_password":"...", "ap_channel":6, "ap_hidden":false }
```

#### `void handleSaveAP()` — L3616 · `POST /api/ap`
Body `{"ap_ssid","ap_password","ap_channel","ap_hidden"}` — every field optional:
* SSID (1–31) → update + legacy mirror.
* Password: ≥ 8 chars to set; blank/absent → **open AP**; tracks `passChanged`.
* Channel 1–13 and hidden bool with change detection.
* Persists both blobs. If **anything changed** → `healer.restartAPIfNeeded(true)` (all clients drop) and reply `{"success":true,"restarted":true}`; otherwise `restarted:false`.

#### `void handleGetSystem()` — L3677 · `GET /api/system`
Dashboard feed (2 KB doc):

| Field | Notes |
|---|---|
| `ip`, `ap_ip` | STA / AP addresses |
| `uptime` | seconds |
| `freeHeap` | bytes |
| `utcEpoch` | **string** (64-bit safe format `%llu`) |
| `timeSource` | `None/NTP/Browser/RTC` |
| `ntpServer`, `gmtOffset`, `driftComp` | config echoes |
| `ntpSyncAge`, `browserSyncAge`, `rtcSyncAge` | seconds; `0xFFFFFFFF` (4294967295) = never |
| `wifiConnected`, `wifiSSID`, `rssi`, `staEnabled` | link state |
| `version` | 11 |
| `chipModel` | `"ESP32-38P"` |
| `mdnsHostname`, `mdnsStarted` | responder state |
| `relayCount`, `maxRelays` | e.g. 16 / 16 |
| `globalActiveMode` | 0/1/2 |
| `rtcPresent` | DS3231 detected |

#### `void handleReset()` — L3715 · `POST /api/reset`
"Verify Services" — replies success immediately, then runs `healer.performTargetedRecovery()` (live check of web/DNS/mDNS/WiFi/AP/RTC/relays; **no reboot**).

#### `void handleFactoryReset()` — L3721 · `POST /api/factory-reset`
Software twin of the BOOT-button path: clears NVS → `initDefaults()` → reload all blobs → force every relay OFF + clear overrides → (fresh `WiFi.begin()` when STA configured) → AP force-restart → `performTargetedRecovery()` → `updateScheduleCache()` → `{"success":true,"message":"Factory reset complete..."}` (UI navigates home after ~7 s).

#### `void handleGetGPIOConfig()` — L3757 · `GET /api/gpio`
```json
{ "count": 16, "maxRelays": 16, "pins": [15,2,...], "activeLow": [true,...],
  "availablePins": [15,2,4,5,18,19,3,1,23,13,14,27,26,25,33,32] }
```
`availablePins` is the fixed candidate pool offered to "add relay" (same set as factory defaults).

#### `void handleSaveGPIOConfig()` — L3779 · `POST /api/gpio/save`
Whole-map replace, body `{"pins":[...]}` (≤ 16, used by "Reset to Default"):
1. Snapshots old overrides/states/names/schedules/activeLow into locals.
2. Rewrites `gpioConfig` (count + pins), restoring per-index settings so configuration survives the remap.
3. Slots ≥ new count → `initScheduleDefaults()` + active-low.
4. Every active pin → `OUTPUT` + OFF; persist both blobs; `updateScheduleCache()`; dirty + cache flags.
`{"success":true,"count":n}`; 400 on > 16 pins / bad JSON.

#### `void handleAddGPIO()` — L3840 · `POST /api/gpio/add`
Body `{"pin":n}`. Guards: capacity (16) and **duplicate pin** (400). Appends pin (default active-low), `initScheduleDefaults()` for the fresh index, increments count, `pinMode OUTPUT` + OFF, persists + cache maintenance → `{"success":true,"count":n}`.

#### `void handleDeleteGPIO()` — L3878 · `POST /api/gpio/delete`
Body `{"index":i}` (range-checked). Drives the removed relay OFF, **compacts** `pins`/`activeLow`/`relayConfigs`/`lastRelayOutputs` down one slot (relay numbering shifts — the UI warns), re-initializes the vacated tail slot, decrements count, persists + cache maintenance → success.

#### `void handleToggleActiveLow()` — L3913 · `POST /api/gpio/toggle-active-low`
Body `{"index":i}`. Flips `gpioConfig.activeLow[i]`, saves GPIO blob, safely re-drives the pin OFF (prevents a stuck-energized relay across the polarity change), dirty + cache flags → `{"success":true,"activeLow":bool}`. (UI disables this while a global mode overrides individual levels.)

#### `void handleGlobalActiveMode()` — L3945 · `POST /api/gpio/global-mode`
Body `{"mode":0|1|2}` (0 = per-relay, 1 = all LOW, 2 = all HIGH; else 400). Stores to `extConfig` (+NVS), then **re-applies every output** through `setRelayOutput()` so the new polarity takes effect without changing logical states; dirty + cache flags → `{"success":true,"mode":m}`.

#### `void handleBrowserTimeSync()` — L2485 · `POST /api/time/browser-sync`
Documented in [§10.9](#109-browser-time-sync).

---

## 11. Embedded Web Assets (PROGMEM)

All front-end code lives in flash as raw-string literals (`R"css(...)css"` / `R"raw(...)raw"`), served with `server.send_P()`. No SPIFFS/LittleFS, no external CDN — the UI works fully offline from the AP.

### 11.1 `const char style_css[]` — L1157 · `GET /style.css`
Shared stylesheet for all six pages. Design tokens:

| Group | Selectors | Purpose |
|---|---|---|
| Reset/base | `*`, `body` | border-box, system font stack, `#EEF2F7` background |
| Header/nav | `header`, `.logo`, `nav a(.cur)`, `.hdr-r` | sticky gradient blue app bar; `.cur` marks the active page |
| Status | `.dot` + `.g/.r/.y/.b` | WiFi & time-source indicator LEDs (green/red/yellow/blue) |
| Layout | `main`, `.ptitle`, `.grid` | max-width 1200 px, auto-fill card grid (≥ 340 px) |
| Cards | `.card`, `.card-hdr`, `.ctitle`, `.badge` + `.bon/.boff/.bman` | relay cards; ON/OFF/MANUAL pill colors |
| Buttons | `.btn` + `.bon-b/.boff-b/.baut/.bsave/.bsync/.bdanger/.bwarn/.bscan` | color-coded actions |
| Schedule editor | `.slist`, `.si(.act)`, `.shdr`, `.trow`, `.days/.day(.on)`, `.mdays/.mday(.on)`, `.months/.month(.on)`, `.sched-section`, `.night(.always)` | scrollable slot list, active-slot highlight, 7/31/12 toggle grids (blue/purple/cyan), overnight & always-on badges |
| Inputs | `input[type=time]`, `.fg input/select` | monospace time pickers, focus rings |
| Info | `.ibar`, `.ibox .l/.v` | System-page stat cards |
| Forms | `.fcrd`, `.fg`, `.input-row`, `.alert(.aw/.ai)` | 600 px form cards, warning/info callouts |
| WiFi scan | `.netlist`, `.netitem .ns/.nr`, `.bars/.bar(.on)` | network picker + 4-bar RSSI glyph |
| Toast | `#toast(.show)(.ok/.er)` | fixed bottom slide-up notifications |
| Responsive | `@media(max-width:500px)` | single column, shrunken toggle grids, stacked input rows |

### 11.2 `const char index_html[]` — L1243 · `GET /` (Relays page)
Main control surface: sticky nav header, live clock + status dots, card grid of all relays, per-card ON/OFF/Auto buttons, the 8-slot schedule editor (time pickers + 7 weekday + 31 month-day + 12 month toggles), save button, empty-state hint linking to `/gpio`.

**Module state:** `D[]`/`M[]` (day/month labels), `NS = 8`, `relays[]`, `busy` (save lock), `editingRelay`, `editingInput`.

| JS Function | Purpose |
|---|---|
| `toast(m, ok=true)` | 3 s success(green)/error(red) toast |
| `tick()` (1 s interval) | polls `/api/time`; updates clock, WiFi dot (`g`/`r`), time dot (`ntp→g`, `browser`/`rtc→b`, else `y`) |
| `escapeHtml(text)` | XSS-safe text injection for relay names |
| `load()` | GETs `/api/relays` (skipped while `busy`) and re-renders |
| `toTS(h,m,s)` / `fromTS(v)` | (h,m,s) ⇄ `"HH:MM:SS"` for `<input type=time step=1>` |
| `dayMaskToStr(d)` | mask → `"Everyday"`, day list, or `"None"` |
| `monthDayMaskToStr(md)` | month-day mask → `""`, `"All month days"`, or `1,15,31` list |
| `monthMaskToStr(mm)` | month mask → `"All months"` (0/0x0FFF) or month list |
| `nightBadge(sc)` | renders 🌙 Overnight / ● Always-ON badge with day/month summary for a slot |
| `startEditName(relayIdx)` | double-click inline rename: swaps title for a styled input (maxlength 15) |
| `cancelEdit()` | Escape — restores the title, drops the input |
| `saveNameEdit(relayIdx, newName)` | trims (empty → `Relay n`), optimistic UI update, POSTs `/api/relay/name` |
| `render()` | rebuilds the whole card grid DOM from `relays[]` (checkbox, two time inputs, 7+31+12 toggle cells per slot, save button) |
| `toggleDay(ri,si,dayIdx)` | XORs weekday bit; updates cell + badge |
| `toggleMonthDay(ri,si,dayIdx)` | XORs month-day bit (lazy-inits mask) |
| `toggleMonth(ri,si,mIdx)` | XORs month bit (lazy-init 0x0FFF) |
| `uf(ri,si,field,val)` | field updater: `en` (enabled + card highlight), `start`, `stop` (parse + badge refresh) |
| `mc(ri,state)` | manual ON/OFF → POST `/api/relay/manual` → toast + reload |
| `ra(ri)` | return-to-Auto → POST `/api/relay/reset` |
| `save(ri)` | POSTs the full 8-slot set to `/api/relay/save` under the `busy` lock |
| `load()` (trailing call) | initial page populate |

### 11.3 `const char wifi_html[]` — L1537 · `GET /wifi` (Station settings)
Station-mode master toggle (blue ON / red OFF button styles via an inline `<style>` block), SSID field + async **Scan** picker, password field, Save & Connect; status banner adapts to connected / not-connected / station-disabled; whole config section dims & disables when STA is off.
**State:** `scanTimer`, `scanning`.

| JS Function | Purpose |
|---|---|
| `toast`, `tick` | shared header clock/status pattern (as 11.2) |
| `updateStaButton(enabled)` | restyles the toggle (`✓ ON` / `✗ OFF`) |
| `loadWiFiStatus()` | GET `/api/wifi`; fills SSID, toggle, section enable/disable, and status banner (with RSSI bars + dBm when connected) |
| `rssiBar(rssi)` | RSSI → 1–4 lit bars (≥ −50/−60/−70 thresholds) |
| `toggleStation()` | POSTs `{sta_enabled:!current}` to `/api/wifi`; busy-state on the button; warns that NTP is lost when disabling; refreshes banner |
| `startScan()` | guards STA-enabled + re-entry; spinner UI; POST `/api/wifi/scan`, then polls every 2.5 s (`scanTimer`) |
| `pollScan()` | GET results; renders list when `scanning:false` |
| `endScan()` | restores the Scan button |
| `renderNets(nets)` | sorted (strongest first) network rows: name, dBm, bars, 🔒 when encrypted; click = fill SSID + focus password |
| `saveWiFi()` | validates non-empty SSID; busy-state; POSTs credentials; on success redirects to `/` after 3 s |
| `loadWiFiStatus()` (trailing call) | initial populate |

### 11.4 `const char ntp_html[]` — L1770 · `GET /ntp` (Time page)
Live time-source banner (NTP ✅ / browser 🌐 / DS3231 💾 / none ⚠, plus RTC presence + sync age), NTP server field (fallback order documented), GMT offset (s), DST offset (s), auto-sync hours (1–24), **Save / Sync NTP / Sync Browser** buttons, and guidance text on source priority.

| JS Function | Purpose |
|---|---|
| `toast`, `tick` | shared pattern + feeds `updateTimeStatus()` |
| `updateTimeStatus(d)` | builds the colored source banner + DS3231 detected/sync-age sub-line |
| anonymous loader | GET `/api/ntp` → pre-fills the four inputs (defaults: server google, GMT 28800, DST 0, 1 h) |
| `save()` | validates 1–24 h; POSTs all four fields to `/api/ntp` |
| `sync()` | busy-state; POST `/api/ntp/sync` (kick only — result observed via the banner) |
| `syncFromBrowser()` | computes `Date.now()/1000`, POSTs `{utc_epoch}` to `/api/time/browser-sync`; on success updates the clock, notes "DS3231 updated" when `rtc_synced`, re-ticks |

### 11.5 `const char ap_html[]` — L1894 · `GET /ap` (Access Point)
Warning callout (AP restart disconnects clients), SSID (≤ 31), password (≥ 8 or blank = open), channel dropdown (1–13, 6 = default), visibility dropdown (visible/hidden), Save.

| JS Function | Purpose |
|---|---|
| `toast`, `tick` | shared pattern |
| anonymous loader | GET `/api/ap` → pre-fills SSID/channel/visibility |
| `save()` | validates password length (0 or ≥ 8); POSTs all four fields; toasts whether an AP restart happened; reloads the page after 4 s |

### 11.6 `const char gpio_html[]` — L1961 · `GET /gpio` (Pin mapper)
Global active-mode dropdown (per-relay / global-LOW / global-HIGH), add-relay dropdown (unallocated pins only) + button, list of configured relays (name, GPIO, level, per-relay Set HIGH/LOW toggle — disabled under global mode, Remove), active-mode override banner, Reset-to-Default (factory 16-pin map), count/max footers.
**State:** `DEFAULT_PINS` (mirror of firmware defaults), `gpioData`.

| JS Function | Purpose |
|---|---|
| `toast`, `tick` | shared pattern |
| `saveGlobalMode()` | POSTs `{mode}` to `/api/gpio/global-mode`, reloads |
| `loadGPIO()` | GET `/api/gpio` (+ `/api/gpio/global-mode`); renders override banner, relay rows, add-pin dropdown minus used pins, "all pins in use" / "maximum 16" edge states |
| `toggleActiveLow(index)` | POST `/api/gpio/toggle-active-low`; echoes new level |
| `addPin()` | POSTs selected pin to `/api/gpio/add` |
| `deletePin(index)` | confirm dialog (warns schedules are lost & numbering shifts) → POST `/api/gpio/delete` |
| `resetDefaults()` | confirm dialog → POSTs the 16 factory pins to `/api/gpio/save` (per-index schedules preserved where possible) |
| `loadGPIO()` (trailing call) | initial populate |

### 11.7 `const char system_html[]` — L2168 · `GET /system` (Dashboard)
20 stat cards — STA IP, AP IP, Free Heap (KB), Uptime, WiFi RSSI (quality text), Time Source (color-coded), UTC Epoch, NTP Last Sync, Last Browser Sync, NTP Server, Chip Model, mDNS Hostname, Relay Count, GMT Offset, Drift Comp, GPIO Mode, DS3231 present, DS3231 Last Sync, Sync Direction, WiFi Station state — plus **Verify Services** and **Factory Reset** buttons and BOOT-button hint.

| JS Function | Purpose |
|---|---|
| `toast`, `tick` | shared pattern |
| `fmtUp(s)` | seconds → `Hh Mm Ss` |
| `fmtAge(s)` | seconds → `Never` (0xFFFFFFFF sentinel) / `Just now` / `N min ago` / `Hh Mm ago` |
| `rssiDesc(r)` | dBm → `Excellent/Good/Fair/Weak` text |
| `loadSys()` (5 s interval) | GET `/api/system` and fills every card, coloring time-source, DS3231, and station status |
| `rst()` | confirm → POST `/api/reset` (live service verification, no reboot) |
| `fct()` | confirm → POST `/api/factory-reset`; navigates home after 7 s |

---

## 12. Complete HTTP Route & REST API Reference

Quick-reference index of every endpoint (details in §10.19–10.20). All APIs are unauthenticated (the trust boundary is the AP password).

| Method | Endpoint | Request Body | Success Response | Purpose |
|---|---|---|---|---|
| GET | `/` | — | HTML | Relays page |
| GET | `/wifi` | — | HTML | WiFi page |
| GET | `/ntp` | — | HTML | Time page |
| GET | `/ap` | — | HTML | AP page |
| GET | `/gpio` | — | HTML | GPIO page |
| GET | `/system` | — | HTML | System page |
| GET | `/style.css` | — | CSS | shared stylesheet |
| GET | `/api/relays` | — | `[{state,manual,name,pin,schedules[8]}]` | all relays + schedules (chunked) |
| POST | `/api/relay/manual` | `{relay,state}` | `{success:true}` | force relay ON/OFF (override) |
| POST | `/api/relay/reset` | `{relay}` | `{success:true}` | clear override → auto |
| POST | `/api/relay/save` | `{relay,schedules[8]}` | `{success:true}` | replace 8 schedule slots |
| POST | `/api/relay/name` | `{relay,name}` | `{success:true}` | rename (≤ 15 chars) |
| GET | `/api/time` | — | `{time,wifi,ntp,timeSource,rtcPresent,rtcSynced,rtcSyncAge}` | header clock feed |
| POST | `/api/time/browser-sync` | `{utc_epoch}` | `{success,local_time,gmt_offset,time_source,rtc_present,rtc_synced}` | adopt browser time |
| GET | `/api/wifi` | — | `{ssid,connected,ip,rssi,sta_enabled}` | STA state |
| POST | `/api/wifi` | `{sta_enabled}` **or** `{ssid,password}` | `{success,...}` | toggle STA / save credentials & reconnect |
| POST | `/api/wifi/scan` | — | 202 `{scanning:true}` | start async scan |
| GET | `/api/wifi/scan` | — | `{scanning,networks[]}` | poll scan results |
| GET | `/api/ntp` | — | `{ntpServer,gmtOffset,daylightOffset,syncHours,globalActiveMode}` | NTP config |
| POST | `/api/ntp` | `{ntpServer,gmtOffset,daylightOffset,syncHours}` | `{success:true}` | save NTP config |
| POST | `/api/ntp/sync` | — | `{success,message}` | trigger sync now |
| GET | `/api/ap` | — | `{ap_ssid,ap_password,ap_channel,ap_hidden}` | AP config |
| POST | `/api/ap` | `{ap_ssid?,ap_password?,ap_channel?,ap_hidden?}` | `{success,restarted}` | save AP (restarts on change) |
| GET | `/api/gpio` | — | `{count,maxRelays,pins[],activeLow[],availablePins[]}` | pin map |
| POST | `/api/gpio/save` | `{pins[]}` | `{success,count}` | replace whole map |
| POST | `/api/gpio/add` | `{pin}` | `{success,count}` | append relay |
| POST | `/api/gpio/delete` | `{index}` | `{success,count}` | remove relay (compacts) |
| POST | `/api/gpio/toggle-active-low` | `{index}` | `{success,activeLow}` | flip per-relay polarity |
| GET | `/api/gpio/global-mode` | — | `{mode}` | read global polarity mode |
| POST | `/api/gpio/global-mode` | `{mode}` | `{success,mode}` | set 0=per-relay/1=LOW/2=HIGH |
| GET | `/api/mdns` | — | `{hostname,started,url}` | mDNS state |
| POST | `/api/mdns` | `{hostname}` | `{success,hostname}` | set hostname (sanitized) |
| POST | `/api/mdns/restart` | — | `{success:true}` | restart responder |
| GET | `/api/system` | — | 21-field status object | dashboard feed |
| POST | `/api/reset` | — | `{success,message}` | live verify services |
| POST | `/api/factory-reset` | — | `{success,message}` | wipe NVS, restore defaults |
| GET | `/hotspot-detect.html` · `/library/test/success.html` | — | `Success` | Apple CNA probe |
| GET | `/generate_204` · `/canonical.html` · `/redirect` | — | 302 → AP IP | Android probe |
| GET | `/success.txt` | — | `success` | Firefox probe |
| GET | `/connecttest.txt` · `/ncsi.txt` | — | text | Windows NCSI probe |
| *any* | *(unknown path)* | — | 302 → AP IP | captive-portal catch-all |

Common error shape: HTTP 400 `{"success":false,"error":"<reason>"}` (`No data`, `Bad JSON`, `Invalid relay`, `Invalid SSID`, `Pin already in use`, `Maximum relays reached`, `Invalid mode (0-2)`, …).

---

## 13. NVS (Flash) Storage Map

Namespace **`relay16`** — one `Preferences` instance, always wrapped in `begin()`/`end()`.

| Key | Type | Size | Content | Written By |
|---|---|---|---|---|
| `sysConfig` | bytes | sizeof(SystemConfig) | §6.4 blob (magic 0x1234, version 11) | `saveConfiguration()` (schedules, names, WiFi, NTP, RTC save…) |
| `relayConfigs` | bytes | 16 × sizeof(RelayConfig) | §6.2/6.3 blobs | same |
| `extConfig` | bytes | sizeof(ExtConfig) | §6.5 blob (magic 0xEC) | `saveExtConfig()` |
| `gpioConfig` | bytes | sizeof(GPIOPinConfig) | §6.1 blob (magic 0xD002) | `saveGPIOConfig()` |
| `criticalState` | bytes | sizeof(CriticalRelayState) | §6.7 snapshot (magic 0xDEADBEEF + checksum) | `saveCriticalState()` |

**Erase-all paths:** `preferences.clear()` in `checkBootButton()` (5 s hold) and `handleFactoryReset()`.
**Wear strategy:** hourly epoch auto-save skipped while NTP/DS3231 is authoritative; critical state flush throttled to 5 min; everything else only writes on user action.

---

## 14. Default GPIO Pin Map

`DEFAULT_RELAY_PINS` (L181) maps relay-module inputs IN1…IN16:

| Relay | GPIO | Relay | GPIO | Relay | GPIO | Relay | GPIO |
|---|---|---|---|---|---|---|---|
| 1 | 15 | 5 | 18 | 9 | 23 | 13 | 26 |
| 2 | 2 | 6 | 19 | 10 | 13 | 14 | 25 |
| 3 | 4 | 7 | **3 (RX0)** | 11 | 14 | 15 | 33 |
| 4 | 5 | 8 | **1 (TX0)** | 12 | 27 | 16 | 32 |

All relays ship **active-LOW** (`activeLow[i] = true`).
⚠ **Hardware caution:** GPIO 1/3 are the USB-serial UART and GPIO 2/15 are strapping pins — remappable via the GPIO page if they conflict with your board. DS3231 reserves GPIO 21 (SDA) / 22 (SCL); GPIO 0 is the BOOT/factory-reset button.

---

## 15. Timing / Interval Reference

| Interval | Value | Where |
|---|---|---|
| Schedule engine tick | 250 ms | `processRelaySchedules()` in `loop()` |
| Schedule cache refresh | 1 s | `SCHEDULE_CACHE_INTERVAL` |
| Relay debounce | 500 ms | inside engine |
| NTP resync | 1–24 h (default 1 h) | `getNTPInterval()` |
| NTP attempt spacing / per-server timeout / retries | 30 s / 5 s / ×3 | NTP subsystem |
| WiFi link check | 10 s | `WIFI_CHECK_INTERVAL` |
| WiFi connect timeout / max retries / give-up | 20 s / 10 / 5 min | connect state machine |
| Scan pause for connect / watchdog | 15 s (+5 s after results) / 10 s | scan subsystem |
| DS3231 resync into internal RTC | 1 h | `DS3231_SYNC_INTERVAL` |
| Internal-RTC rebase / NVS auto-save | 5 min / 1 h (conditional) | RTC subsystem |
| Smart recovery tick / WiFi failure threshold | 10 s / 3 strikes | healer |
| Relay read-back verification | 30 s | `verifyRelayStates()` |
| Full health sweep (mDNS re-announce + critical flush) | 30 min | `smartRecovery()` |
| Stale-resource / connection / heap housekeeping | 30 s / 60 s / 5 min | memory mgmt |
| Emergency heap cleanup threshold | < 20 KB free | `checkAndCleanMemory()` |
| Critical-state save throttle | 5 min (when dirty) | `loop()` |
| BOOT-button factory-reset hold | 5 s | `FACTORY_RESET_HOLD` |
| mDNS deferred restart | 2 s | `MDNS_RESTART_DELAY` |

---

## 16. Execution Flow

**Boot:** RTC probe → GPIO load & all-off → defaults → configs → time restore ladder (DS3231 → NVS epoch → none) → radio mode + STA attempt → AP up → mDNS → captive DNS → HTTP routes → schedule cache → restore crash snapshot → arm timers.

**Steady state:** HTTP/DNS servicing dominates; 250 ms schedule engine writes relays only on debounced change; 10 s WiFi & healing watchdogs; 30 s memory/verification; hourly DS3231 & (conditional) epoch persistence; event-driven NTP.

**Time authority precedence (effective):** NTP while online (overrides browser) → DS3231 as hardware backbone (hourly refresh and cold-start) → browser sync when user-triggered → NVS-restored epoch as approximation → NONE (schedules hold off until `MIN_UNIX_TIME_64` is exceeded).

---

## 17. Bitmask Encoding Reference

| Mask | Type | Bit i meaning | Special values |
|---|---|---|---|
| `days[8]` | `uint8_t` | weekday i (0 = Sun … 6 = Sat) | `0x7F` all, `0x3E` weekdays, `0x41` weekends |
| `monthDays[8]` | `uint32_t` | day-of-month (i+1) | `0` = **unrestricted**, `0xFFFFFFFF` = all 31 |
| `monthMask[8]` | `uint16_t` | month (i+1) | `0x0FFF` = all; `0` also treated as unrestricted by the engine |

**Schedule slot truth table** (all filters passed, current time `cur` in seconds-of-day):

| Condition | Relay is ON when |
|---|---|
| `start == stop` | always (whole matching day) |
| `start < stop` | `start ≤ cur < stop` |
| `start > stop` (overnight) | `cur ≥ start` OR `cur < stop` |

---

## 18. Notes, Invariants & Known Idiosyncrasies

* **N1 — Async NTP scaffolding:** `ntpAsyncState/CurrentServer/PhaseStart` and `NTP_STATE_*` describe an async state machine, but synchronization is performed by the bounded `getNTP64Time()`/`tryNTPSync()` path; the async variables currently serve as reset bookkeeping only.
* **N2 — Retained utilities:** `updateRelayOutputs()` and `lastRelayUpdate`/`RELAY_UPDATE_INTERVAL` are kept helpers; the live loop drives outputs exclusively through `processRelaySchedules()`. `recoverMDNS/DNS/WebServer` are thin forwards to the `liveReconfigure*` twins.
* **N3 — RTC drift field:** `driftCompensation`/`rtc_drift` are fully plumbed (apply in rebase math, persist, restore) but always `1.0` in practice — a clean seam for future measured-drift calibration.
* **N4 — Response cache:** `ResponseCache` exists and `handleGetRelays()` consults it, but it is invalidated (never repopulated) after each stream, and `cleanupStaleResources()` expires it every 5 s — in the current build responses are effectively always fresh; the struct & checks are forward-looking infrastructure.
* **N5 — 64-bit readiness vs. NTP wire format:** the epoch pipeline (storage, `gmtime64`, JSON `%llu`) is 64-bit-clean; the NTP seconds field itself is 32-bit (wraps in 2036/2106 era semantics), and the browser-sync echo truncates `utc_epoch` to 32-bit in the response only. For the device's 2020–2100 validity window behavior is exact.
* **N6 — `restartAP()` wrapper:** delegates with `forceRestart = false`, so it performs no teardown by itself; actual AP rebuilds happen via `liveReconfigureAP()` (on collapse) or `restartAPIfNeeded(true)` (explicit paths).
* **N7 — Naming:** `performRTCReabase()` and the NVS `EEPROM_*` identifiers are legacy spellings kept for compatibility; behavior is as documented.
* **N8 — Single writer rule:** every relay GPIO change flows through `setRelayOutput()` + `lastRelayOutputs[]`, which is what makes the 30 s read-back verification and crash-restore trustworthy.
* **N9 — Heap-safety:** JSON documents are size-bounded (64 B – 8 KB), relay responses stream in 512 B chunks, and `yield()` is interleaved — the UI stays usable on a busy single-core loop.
* **N10 — Security boundary:** no HTTP authentication is implemented; the AP password is the only access control — deploy on trusted networks or extend the handlers.

---

