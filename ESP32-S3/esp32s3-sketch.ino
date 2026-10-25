
/*
===============================================================================
 *  ESP32S3 16-Channel Automatic Relay Timer Switch
 *  Author: Raff Alds
 *  Github: https://www.github.com/xiv3r
 *  License: GPLv3
===============================================================================
*/

#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include <RTClib.h>

// =============================================================================
//  Preferences (NVS)
// =============================================================================
Preferences preferences;
#define NVS_NAMESPACE "relay16"
#define EEPROM_MAGIC   0x1234
#define EEPROM_VERSION 14
#define EXT_CFG_MAGIC  0xEC
#define RELAY_CONFIG_MAGIC 0x1CE5

// =============================================================================
//  Year 2106+ Support
// =============================================================================
#define MIN_UNIX_TIME_64 1577836800ULL
#define MAX_UNIX_TIME_64 4294967295ULL 
#define VALID_UNIX_TIME_64(epoch) ((epoch) >= MIN_UNIX_TIME_64 && (epoch) <= MAX_UNIX_TIME_64)

// =============================================================================
//  Day-of-Week / Month-of-Year Masks
// =============================================================================
#define DAY_ALL   0x7F
#define MONTH_ALL 0x0FFF

// =============================================================================
//  Timing Constants
// =============================================================================
static const unsigned long NTP_RETRY_INTERVAL   =   30000UL;
static const unsigned long WIFI_CHECK_INTERVAL  =   10000UL;
static const unsigned long WIFI_CONNECT_TIMEOUT =   20000UL;
static const unsigned long SCHEDULE_PROCESS_INTERVAL = 250UL;
static const unsigned long RTC_REBASE_INTERVAL  =  300000UL;
static const unsigned long DS3231_SYNC_INTERVAL =  3600000UL;

// =============================================================================
//  DST Cache Timing
// =============================================================================
static const unsigned long DST_CACHE_TTL_SECONDS = 5ULL;
static const unsigned long DST_CACHE_INVALIDATE_INTERVAL = 5000UL;

// =============================================================================
//  NTP Async State Machine Constants
// =============================================================================
#define NTP_STATE_IDLE       0
#define NTP_STATE_CONNECTING 1

// =============================================================================
//  64-bit NTP Constants
// =============================================================================
#define NTP_PACKET_SIZE 48
#define NTP_PORT 123
#define NTP_TIMEOUT 3000
#define NTP_EPOCH_OFFSET 2208988800ULL

// =============================================================================
//  Memory Management
// =============================================================================
static const unsigned long MEMORY_CLEANUP_INTERVAL = 30000UL;

// =============================================================================
//  Boot Button Factory Reset
// =============================================================================
#define BOOT_BUTTON_PIN      0
#define FACTORY_RESET_HOLD   5000
static unsigned long bootButtonPressStart = 0;
static bool bootButtonPressed = false;
static bool factoryResetTriggered = false;

// =============================================================================
//  mDNS Settings
// =============================================================================
#define MDNS_HOSTNAME_DEFAULT "esp32s3"

// =============================================================================
//  NTP Fallback Pool
// =============================================================================
static const char* NTP_SERVERS[] = {
    "time.google.com",
    "time.windows.com",
    "time.cloudflare.com",
    "time.facebook.com"
};
static const uint8_t NUM_NTP_SERVERS = 4;

// =============================================================================
//  Time Source Tracking
// =============================================================================
#define TIME_SOURCE_NONE    0
#define TIME_SOURCE_NTP     1
#define TIME_SOURCE_BROWSER 2
#define TIME_SOURCE_RTC     3
static uint8_t timeSource = TIME_SOURCE_NONE;
static unsigned long lastBrowserSync = 0;

// =============================================================================
//  DS3231 RTC
// =============================================================================
RTC_DS3231 rtc;
bool rtcPresent = false;
unsigned long lastRTCDSync = 0;
bool rtcTimeValid = false;
static uint8_t rtcFailCount = 0;
static const uint8_t RTC_FAIL_THRESHOLD = 3;
static float         cachedRtcTemp          = NAN;
static unsigned long lastRtcTempRead        = 0;
static const unsigned long RTC_TEMP_READ_INTERVAL = 10000UL;

// =============================================================================
//  WiFi Scan Pause System
// =============================================================================
static bool wifiPausedForScan = false;
static unsigned long wifiPauseUntil = 0;
static const unsigned long WIFI_PAUSE_DURATION = 15000UL;
static unsigned long lastScanAttempt = 0;

// =============================================================================
//  Millis-Safe Time Comparison Macros
// =============================================================================
inline bool timeHasElapsed(unsigned long current, unsigned long previous, unsigned long interval) {
    return (current - previous) >= interval;
}
inline bool isTimeReached(unsigned long current, unsigned long target) {
    return (int32_t)(current - target) >= 0;
}

// =============================================================================
//  DNS / Web Server
// =============================================================================
DNSServer        dnsServer;
WebServer        server(80);
const byte       DNS_PORT = 53;

// =============================================================================
//  Relay Config
// =============================================================================
#define MAX_RELAYS 16
// change gpio
const uint8_t DEFAULT_RELAY_PINS[] = {4, 5, 6, 7, 11, 12, 13, 14, 1, 2, 42, 41, 47, 21, 20, 19};

// =============================================================================
//  Dynamic GPIO Config
// =============================================================================
struct GPIOPinConfig {
    uint8_t pins[MAX_RELAYS];
    uint8_t count;
    uint16_t magic;
    bool activeLow[MAX_RELAYS];
};
GPIOPinConfig gpioConfig;
#define GPIO_CONFIG_MAGIC 0xD002

// =============================================================================
//  Data Structures
// =============================================================================
struct TimerSchedule {
    uint8_t  startHour[8], startMinute[8], startSecond[8];
    uint8_t  stopHour[8],  stopMinute[8],  stopSecond[8];
    bool     enabled[8];
    uint8_t  days[8];
    uint32_t monthDays[8];
    uint16_t monthMask[8];
};

struct RelayConfig {
    TimerSchedule schedule;
    bool          manualOverride;
    bool          manualState;
    char          name[16];
    uint16_t      magic;
};

struct SystemConfig {
    uint16_t magic;
    uint8_t  version;
    char     sta_ssid[32];
    char     sta_password[64];
    char     ap_ssid[32];
    char     ap_password[32];
    char     ntp_server[48];
    int32_t  gmt_offset;
    int32_t  daylight_offset;
    uint64_t last_rtc_epoch;
    char     tz_name[32];
    uint8_t  tz_rule_id;
    uint8_t  tz_auto_dst;
    uint8_t  tz_reserved[2];
} __attribute__((packed));

struct ExtConfig {
    uint8_t magic;
    uint8_t ap_channel;
    uint8_t ntp_sync_hours;
    uint8_t ap_hidden;
    uint8_t global_active_mode;
    uint8_t sta_enabled;
    uint8_t reserved[26];
} __attribute__((packed));

// =============================================================================
//  Timezone Rule Table
// =============================================================================
struct TZRule {
    const char* name;
    int32_t     std_offset;
    int32_t     dst_offset;
    uint8_t     dst_start_mon;
    uint8_t     dst_start_wday;
    uint8_t     dst_start_nth;
    uint8_t     dst_start_mday;
    uint8_t     dst_start_hour;
    uint8_t     dst_start_min;
    uint8_t     dst_end_mon;
    uint8_t     dst_end_wday;
    uint8_t     dst_end_nth;
    uint8_t     dst_end_mday;
    uint8_t     dst_end_hour;
    uint8_t     dst_end_min;
};

static const TZRule TZ_TABLE[] = {
    {"UTC",                       0,       0,     0,  0, 0xFF,   0,  0, 0,    0,  0, 0xFF,   0,  0, 0},
    {"Asia/Manila",           28800,       0,     0,  0, 0xFF,   0,  0, 0,    0,  0, 0xFF,   0,  0, 0},
    {"Asia/Tokyo",            32400,       0,     0,  0, 0xFF,   0,  0, 0,    0,  0, 0xFF,   0,  0, 0},
    {"Asia/Shanghai",         28800,       0,     0,  0, 0xFF,   0,  0, 0,    0,  0, 0xFF,   0,  0, 0},
    {"Asia/Kolkata",          19800,       0,     0,  0, 0xFF,   0,  0, 0,    0,  0, 0xFF,   0,  0, 0},
    {"Asia/Dubai",            14400,       0,     0,  0, 0xFF,   0,  0, 0,    0,  0, 0xFF,   0,  0, 0},
    {"Asia/Singapore",        28800,       0,     0,  0, 0xFF,   0,  0, 0,    0,  0, 0xFF,   0,  0, 0},
    {"Asia/Hong_Kong",        28800,       0,     0,  0, 0xFF,   0,  0, 0,    0,  0, 0xFF,   0,  0, 0},
    {"Asia/Seoul",            32400,       0,     0,  0, 0xFF,   0,  0, 0,    0,  0, 0xFF,   0,  0, 0},
    {"Asia/Bangkok",          25200,       0,     0,  0, 0xFF,   0,  0, 0,    0,  0, 0xFF,   0,  0, 0},
    {"Australia/Sydney",      36000,   39600,    10,  0,    1,   0,  2, 0,    4,  0,    1,   0,  3, 0},
    {"Australia/Perth",       28800,       0,     0,  0, 0xFF,   0,  0, 0,    0,  0, 0xFF,   0,  0, 0},
    {"Australia/Brisbane",    36000,       0,     0,  0, 0xFF,   0,  0, 0,    0,  0, 0xFF,   0,  0, 0},
    {"Europe/London",             0,    3600,     3,  0, 0xFE,   0,  1, 0,   10,  0, 0xFE,   0,  2, 0},
    {"Europe/Paris",           3600,    7200,     3,  0, 0xFE,   0,  2, 0,   10,  0, 0xFE,   0,  3, 0},
    {"Europe/Berlin",          3600,    7200,     3,  0, 0xFE,   0,  2, 0,   10,  0, 0xFE,   0,  3, 0},
    {"Europe/Moscow",         10800,       0,     0,  0, 0xFF,   0,  0, 0,    0,  0, 0xFF,   0,  0, 0},
    {"America/New_York",     -18000,  -14400,     3,  0,    2,   0,  2, 0,   11,  0,    1,   0,  2, 0},
    {"America/Chicago",      -21600,  -18000,     3,  0,    2,   0,  2, 0,   11,  0,    1,   0,  2, 0},
    {"America/Denver",       -25200,  -21600,     3,  0,    2,   0,  2, 0,   11,  0,    1,   0,  2, 0},
    {"America/Los_Angeles",  -28800,  -25200,     3,  0,    2,   0,  2, 0,   11,  0,    1,   0,  2, 0},
    {"America/Sao_Paulo",    -10800,       0,     0,  0, 0xFF,   0,  0, 0,    0,  0, 0xFF,   0,  0, 0},
    {"Pacific/Auckland",      43200,   46800,     9,  0, 0xFE,   0,  2, 0,    4,  0,    1,   0,  3, 0},
    {"Pacific/Honolulu",     -36000,       0,     0,  0, 0xFF,   0,  0, 0,    0,  0, 0xFF,   0,  0, 0},
    {"Africa/Cairo",           7200,   10800,     4,  5, 0xFE,   0,  0, 0,   10,  4, 0xFE,   0, 24, 0},
    {"Africa/Johannesburg",    7200,       0,     0,  0, 0xFF,   0,  0, 0,    0,  0, 0xFF,   0,  0, 0},
};
static const uint8_t TZ_TABLE_SIZE = sizeof(TZ_TABLE) / sizeof(TZ_TABLE[0]);

// =============================================================================
//  Self-Healing
// =============================================================================
struct HealthMetrics {
    uint32_t wifiFailures = 0;
    unsigned long lastRecoveryAttempt = 0;
};

// =============================================================================
//  Globals
// =============================================================================
SystemConfig sysConfig;
ExtConfig    extConfig;
RelayConfig  relayConfigs[MAX_RELAYS];
HealthMetrics health;

// RTC
uint64_t      internalEpoch            = 0;
float         driftCompensation        = 1.0f;
bool          rtcInitialized           = false;
unsigned long rtcMicrosAtLastSync      = 0;
unsigned long lastRTCRebase            = 0;

// RTC Auto-Save
static unsigned long lastInternalRTCSave = 0;
static const unsigned long INTERNAL_RTC_SAVE_INTERVAL = 3600000UL;

// NTP
uint8_t       ntpFailCount    = 0;
unsigned long lastNTPSync     = 0;
unsigned long lastNTPAttempt  = 0;

// NTP Async
static uint8_t  ntpAsyncState = NTP_STATE_IDLE;
static unsigned long ntpAsyncPhaseStart = 0;
static WiFiUDP  ntpUDP;
static uint8_t  ntpPacketBuffer[NTP_PACKET_SIZE];
static bool     ntpReceived = false;
static uint64_t ntpResult = 0;
static uint8_t  ntpRetryCount = 0;
static uint8_t  ntpAsyncStage = 0;

// WiFi
bool          wifiConnected         = false;
unsigned long lastWiFiCheck         = 0;
uint8_t       wifiReconnectAttempts = 0;
unsigned long wifiGiveUpUntil       = 0;
static const uint8_t MAX_RECONNECT  = 10;
bool          wifiConnecting        = false;
unsigned long wifiConnectStart      = 0;

// WiFi Scan
volatile bool scanInProgress  = false;
volatile int  scanResultCount = -1;
unsigned long scanStartTime   = 0;

// mDNS
bool          mdnsStarted           = false;
char          mdnsHostname[32]      = MDNS_HOSTNAME_DEFAULT;

// =============================================================================
//  Schedule Engine
// =============================================================================
static bool     scheduleActiveCache[MAX_RELAYS] = {false};
static unsigned long lastScheduleCacheUpdate = 0;
static const unsigned long SCHEDULE_CACHE_INTERVAL = 250UL;
static unsigned long lastScheduleProcess = 0;
static bool lastRelayOutputs[MAX_RELAYS] = {false};
static unsigned long lastStateChangeGlobal[MAX_RELAYS] = {0};
static bool lastDebouncedStateGlobal[MAX_RELAYS] = {false};

// =============================================================================
//  Memory Management
// =============================================================================
static unsigned long lastMemoryCleanup = 0;
static unsigned long lastHeapCheck = 0;

// =============================================================================
//  DST Cache
// =============================================================================
static int32_t  cachedDstOffset = 0;
static uint64_t cachedDstForEpoch = 0;
static bool     cachedDstValid = false;

// =============================================================================
//  Self-Healing Class
// =============================================================================
class SelfHealingSystem {
public:
    bool liveReconfigureWiFi();
    bool liveReconfigureMDNS();
    bool liveReconfigureAP();
    bool recoverRTC();
    void smartRecovery();
    void verifyRelayStates(bool force = false);
    void performTargetedRecovery();
    void restartAPIfNeeded(bool forceRestart);
private:
    unsigned long lastFullHealthCheck = 0;
    unsigned long lastWiFiReconfigure = 0;
    unsigned long lastMDNSReconfigure = 0;
};

// =============================================================================
//  Forward Declarations
// =============================================================================
SelfHealingSystem healer;
void initDefaults();
void loadGPIOConfig();
void saveGPIOConfig();
void loadConfiguration();
void saveConfiguration();
void loadExtConfig();
void saveExtConfig();
void syncInternalRTC(uint64_t rawUtcEpoch, uint8_t source);
void updateScheduleCache();
void autoSaveInternalRTC();
void setupWebServer();
void handleGetRelays();
void handleManualControl();
void handleResetManual();
void handleSaveRelay();
void handleRelayName();
void handleGetTime();
void handleBrowserTimeSync();
void handleGetWiFi();
void handleSaveWiFi();
void handleWiFiScanStart();
void handleWiFiScanResults();
void handleGetNTP();
void handleSaveNTP();
void handleSyncNTP();
void handleGetAP();
void handleSaveAP();
void handleGetSystem();
void handleReset();
void handleFactoryReset();
void handleGetGPIOConfig();
void handleSaveGPIOConfig();
void handleAddGPIO();
void handleDeleteGPIO();
void handleToggleActiveLow();
void handleGlobalActiveMode();
void performMemoryCleanup();
void cleanupStaleResources();
void immediateDS3231Sync();
void pauseWiFiForScan();
void setWiFiStationEnabled(bool enabled);
void initScheduleDefaults(int relayIndex);
void startNTPRequest(const char* server);
void processNTPResponse();
void updateNTPSync();
float getRTCTemperature();
void invalidateDstCache();
int32_t currentDstOffset(uint64_t utcEpoch);
uint8_t findTzByName(const char* name);

// =============================================================================
//  64-bit Conversion Helpers
// =============================================================================
inline uint64_t rtcDateTimeToUint64(DateTime dt) {
    return (uint64_t)dt.unixtime();
}
inline DateTime uint64ToRtcDateTime(uint64_t t64) {
    return DateTime((uint32_t)(t64 & 0xFFFFFFFFULL));
}

// =============================================================================
//  64-bit GMT Time Implementation
// =============================================================================
struct tm* gmtime64(uint64_t* timep) {
    static struct tm tm;
    uint64_t t = *timep;
    if (t == 0) {
        tm.tm_sec = 0;
        tm.tm_min = 0;
        tm.tm_hour = 0;
        tm.tm_mday = 1;
        tm.tm_mon = 0;
        tm.tm_year = 70;
        tm.tm_wday = 4;
        tm.tm_yday = 0;
        tm.tm_isdst = 0;
        return &tm;
    }
    uint64_t days = t / 86400ULL;
    uint64_t rem = t % 86400ULL;
    tm.tm_sec = rem % 60ULL;
    rem /= 60ULL;
    tm.tm_min = rem % 60ULL;
    tm.tm_hour = rem / 60ULL;
    uint64_t z = days + 719468ULL;
    uint64_t era = z / 146097ULL;
    uint64_t doe = z - era * 146097ULL;
    uint64_t yoe = (doe - doe / 1460ULL + doe / 36524ULL - doe / 146096ULL) / 365ULL;
    uint64_t y = yoe + era * 400ULL;
    uint64_t doy = doe - (365ULL * yoe + yoe / 4ULL - yoe / 100ULL);
    uint64_t mp = (5ULL * doy + 2ULL) / 153ULL;
    uint64_t d = doy - (153ULL * mp + 2ULL) / 5ULL + 1ULL;
    uint64_t m = mp + (mp < 10ULL ? 3ULL : -9ULL);
    y += (m <= 2ULL ? 1ULL : 0ULL);
    tm.tm_mday = (int)d;
    tm.tm_mon = (int)(m - 1);
    tm.tm_year = (int)(y - 1900ULL);
    tm.tm_wday = (int)((days + 4ULL) % 7ULL);
    {
        uint64_t isLeap = ((y % 4ULL == 0) && (y % 100ULL != 0)) || (y % 400ULL == 0);
        tm.tm_yday = (m <= 2ULL) ? (int)(doy - 306ULL)
                                 : (int)(doy + (isLeap ? 60ULL : 59ULL));
    }
    tm.tm_isdst = 0;
    return &tm;
}

// =============================================================================
//  Timezone Helpers
// =============================================================================
static uint64_t daysFromYMD(uint16_t y, uint8_t m, uint8_t d) {
    if (m <= 2) { y -= 1; m += 12; }
    uint64_t era = y / 400;
    uint64_t yoe = y - era * 400;
    uint64_t doy = (153ULL * (m - 3) + 2) / 5 + d - 1;
    uint64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

static uint8_t nthWeekdayOfMonth(uint16_t year, uint8_t mon0, uint8_t wday, uint8_t nth) {
    static const uint8_t t[] = {0,3,2,5,0,3,5,1,4,6,2,4};
    uint16_t y = year;
    if (mon0 < 2) y -= 1;
    uint8_t wday1 = (uint8_t)((y + y/4 - y/100 + y/400 + t[mon0] + 1) % 7);
    uint8_t first = (uint8_t)(((wday - wday1 + 7) % 7) + 1);
    if (nth == 0xFE) {
        static const uint8_t mdays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
        uint8_t dim = mdays[mon0];
        bool leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
        if (mon0 == 1 && leap) dim = 29;
        uint8_t d = first;
        while (d + 7 <= dim) d += 7;
        return d;
    }
    uint8_t d = first + (nth - 1) * 7;
    static const uint8_t mdays2[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    uint8_t dim2 = mdays2[mon0];
    bool leap2 = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    if (mon0 == 1 && leap2) dim2 = 29;
    if (d > dim2) d -= 7;
    return d;
}

static int32_t computeDstOffset(uint64_t utcEpoch, uint8_t ruleId) {
    if (ruleId >= TZ_TABLE_SIZE) return 0;
    const TZRule& r = TZ_TABLE[ruleId];
    if (r.dst_offset == 0) return 0;

    uint64_t localForStd = utcEpoch + (uint64_t)(int64_t)r.std_offset;
    struct tm* p = gmtime64(&localForStd);
    if (!p) return 0;
    uint16_t year = (uint16_t)(p->tm_year + 1900);

    uint8_t startDay = (r.dst_start_nth == 0xFF)
        ? r.dst_start_mday
        : nthWeekdayOfMonth(year, r.dst_start_mon - 1, r.dst_start_wday, r.dst_start_nth);
    uint64_t startLocal = daysFromYMD(year, r.dst_start_mon, startDay) * 86400ULL
                        + (uint64_t)r.dst_start_hour * 3600ULL
                        + (uint64_t)r.dst_start_min * 60ULL;
    uint64_t startUtc = startLocal - (uint64_t)(int64_t)r.std_offset;

    uint8_t endDay = (r.dst_end_nth == 0xFF)
        ? r.dst_end_mday
        : nthWeekdayOfMonth(year, r.dst_end_mon - 1, r.dst_end_wday, r.dst_end_nth);
    uint64_t endLocal = daysFromYMD(year, r.dst_end_mon, endDay) * 86400ULL
                      + (uint64_t)r.dst_end_hour * 3600ULL
                      + (uint64_t)r.dst_end_min * 60ULL;
    uint64_t endUtc = endLocal - (uint64_t)(int64_t)r.dst_offset;

    bool inDst;
    if (startUtc < endUtc) {
        inDst = (utcEpoch >= startUtc && utcEpoch < endUtc);
    } else {
        inDst = (utcEpoch >= startUtc || utcEpoch < endUtc);
    }
    return inDst ? (r.dst_offset - r.std_offset) : 0;
}

uint8_t findTzByName(const char* name) {
    if (!name) return 0;
    for (uint8_t i = 0; i < TZ_TABLE_SIZE; i++) {
        if (strcmp(TZ_TABLE[i].name, name) == 0) return i;
    }
    return TZ_TABLE_SIZE;
}

void invalidateDstCache() {
    cachedDstValid = false;
}

int32_t currentDstOffset(uint64_t utcEpoch) {
    if (cachedDstValid && utcEpoch >= cachedDstForEpoch
        && (utcEpoch - cachedDstForEpoch) < DST_CACHE_TTL_SECONDS) {
        return cachedDstOffset;
    }
    int32_t off = 0;
    if (sysConfig.tz_auto_dst && sysConfig.tz_rule_id < TZ_TABLE_SIZE) {
        off = computeDstOffset(utcEpoch, sysConfig.tz_rule_id);
    } else {
        off = sysConfig.daylight_offset;
    }
    cachedDstOffset = off;
    cachedDstForEpoch = utcEpoch;
    cachedDstValid = true;
    return off;
}

// =============================================================================
//  Get Local Epoch
// =============================================================================
inline uint64_t getLocalEpoch(uint64_t utcEpoch) {
    int64_t offset;
    if (sysConfig.tz_auto_dst && sysConfig.tz_rule_id < TZ_TABLE_SIZE) {
        const TZRule& r = TZ_TABLE[sysConfig.tz_rule_id];
        offset = (int64_t)r.std_offset + (int64_t)currentDstOffset(utcEpoch);
    } else {
        offset = (int64_t)sysConfig.gmt_offset + (int64_t)sysConfig.daylight_offset;
    }
    if (offset > 50400) offset = 50400;
    if (offset < -50400) offset = -50400;
    return utcEpoch + (uint64_t)offset;
}

// =============================================================================
//  Get Active Low Setting For Relays
// =============================================================================
inline bool isActiveLow(uint8_t index) {
    if (index < gpioConfig.count) {
        if (extConfig.global_active_mode == 1) {
            return true;
        } else if (extConfig.global_active_mode == 2) {
            return false;
        }
        return gpioConfig.activeLow[index];
    }
    return true;
}

// =============================================================================
//  Inline Helpers
// =============================================================================
inline unsigned long getNTPInterval() {
    uint8_t h = extConfig.ntp_sync_hours;
    if (h < 1 || h > 24) h = 1;
    return (unsigned long)h * 3600000UL;
}
inline int getRelayPin(uint8_t index) {
    if (index < gpioConfig.count) {
        return gpioConfig.pins[index];
    }
    return -1;
}

// =============================================================================
//  Set Relay Output with Active Low Consideration
// =============================================================================
inline void setRelayOutput(uint8_t index, bool state) {
    int pin = getRelayPin(index);
    if (pin >= 0) {
        digitalWrite(pin, isActiveLow(index) ? !state : state);
    }
}

// =============================================================================
//  Initialize Schedule Defaults
// =============================================================================
void initScheduleDefaults(int relayIndex) {
    memset(&relayConfigs[relayIndex], 0, sizeof(RelayConfig));
    for (int s = 0; s < 8; s++) {
        relayConfigs[relayIndex].schedule.days[s] = DAY_ALL;
        relayConfigs[relayIndex].schedule.monthDays[s] = 0x7FFFFFFFUL;
        relayConfigs[relayIndex].schedule.monthMask[s] = MONTH_ALL;
    }
    snprintf(relayConfigs[relayIndex].name, 16, "Relay %d", relayIndex + 1);
    relayConfigs[relayIndex].magic = RELAY_CONFIG_MAGIC;
}

// =============================================================================
//  WiFi Station Control
// =============================================================================
void setWiFiStationEnabled(bool enabled) {
    extConfig.sta_enabled = enabled ? 1 : 0;
    saveExtConfig();
    if (!enabled) {
        ntpUDP.stop();
        WiFi.disconnect(false, false);
        wifiConnected = false;
        wifiConnecting = false;
        wifiPausedForScan = false;
        wifiReconnectAttempts = 0;
        wifiGiveUpUntil = 0;
        if (WiFi.getMode() != WIFI_AP_STA && WiFi.getMode() != WIFI_AP) {
            WiFi.mode(WIFI_AP_STA);
        }
        ntpAsyncState = NTP_STATE_IDLE;
        ntpAsyncStage = 0;
        lastNTPSync = 0;
    } else {
        if (WiFi.getMode() != WIFI_AP_STA) {
            WiFi.mode(WIFI_AP_STA);
        }
        if (strlen(sysConfig.sta_ssid) > 0) {
            WiFi.begin(sysConfig.sta_ssid, sysConfig.sta_password);
            wifiConnecting = true;
            wifiConnectStart = millis();
            wifiReconnectAttempts = 0;
            wifiGiveUpUntil = 0;
        }
    }
}

// =============================================================================
//  DS3231 RTC Functions
// =============================================================================
void initRTC() {
    Wire.begin(8, 9);
    Wire.setTimeOut(50);
    if (!rtc.begin()) {
        rtcPresent = false;
        rtcTimeValid = false;
        return;
    }
    rtcPresent = true;
    rtcTimeValid = false;
    rtcFailCount = 0;
    if (rtc.lostPower()) {
        return;
    }
    DateTime now = rtc.now();
    if (now.year() >= 2020 && now.year() <= 2100) {
        uint64_t rtcEpoch = rtcDateTimeToUint64(now);
        if (VALID_UNIX_TIME_64(rtcEpoch)) {
            rtcTimeValid = true;
            internalEpoch = rtcEpoch;
            driftCompensation = 1.0f;
            rtcMicrosAtLastSync = micros();
            lastRTCRebase = millis();
            lastRTCDSync = millis();
            rtcInitialized = true;
            timeSource = TIME_SOURCE_RTC;
        }
    }
}

void immediateDS3231Sync() {
    if (rtcPresent && rtcInitialized && internalEpoch > 0) {
        if (rtc.begin()) {
            DateTime dt = uint64ToRtcDateTime(internalEpoch);
            rtc.adjust(dt);
            rtcTimeValid = true;
            lastRTCDSync = millis();
            rtcFailCount = 0;
        } else {
            rtcPresent = false;
            rtcTimeValid = false;
        }
    }
}

// =============================================================================
//  DS3231 Temperature
// =============================================================================
float getRTCTemperature() {
    if (!rtcPresent) return NAN;
    unsigned long now = millis();
    if (lastRtcTempRead != 0 && (now - lastRtcTempRead) < RTC_TEMP_READ_INTERVAL) {
        return cachedRtcTemp;
    }
    lastRtcTempRead = now;
    Wire.beginTransmission(0x68);
    Wire.write(0x11);
    if (Wire.endTransmission() != 0) {
        if (rtcFailCount < 255) rtcFailCount++;
        if (rtcFailCount >= RTC_FAIL_THRESHOLD) {
            rtcPresent = false;
            rtcTimeValid = false;
        }
        return cachedRtcTemp;
    }
    if (Wire.requestFrom((uint8_t)0x68, (uint8_t)2) != 2) {
        if (rtcFailCount < 255) rtcFailCount++;
        if (rtcFailCount >= RTC_FAIL_THRESHOLD) {
            rtcPresent = false;
            rtcTimeValid = false;
        }
        return cachedRtcTemp;
    }
    rtcFailCount = 0;
    int8_t  msb = (int8_t)Wire.read();
    uint8_t lsb = Wire.read();
    cachedRtcTemp = (float)msb + ((lsb >> 6) * 0.25f);
    return cachedRtcTemp;
}

// =============================================================================
//  Enhanced RTC Functions
// =============================================================================
void performRTCReabase() {
    unsigned long currentMicros = micros();
    unsigned long currentMillis = millis();
    if (rtcInitialized && internalEpoch > 0) {
        unsigned long elapsedMicros;
        if (currentMicros >= rtcMicrosAtLastSync) {
            elapsedMicros = currentMicros - rtcMicrosAtLastSync;
        } else {
            elapsedMicros = (0xFFFFFFFF - rtcMicrosAtLastSync) + currentMicros + 1;
        }
        float driftSafe = driftCompensation;
        if (driftSafe < 0.9f) driftSafe = 0.9f;
        if (driftSafe > 1.1f) driftSafe = 1.1f;
        double elapsedSeconds = (double)elapsedMicros / 1000000.0;
        double adjustedSeconds = elapsedSeconds * (double)driftSafe;
        uint64_t secondsToAdd = (uint64_t)adjustedSeconds;
        internalEpoch += secondsToAdd;
        unsigned long committedMicros = (unsigned long)(((uint64_t)((double)secondsToAdd * 1000000.0 / (double)driftSafe)) & 0xFFFFFFFFULL);
        rtcMicrosAtLastSync = (rtcMicrosAtLastSync + committedMicros) & 0xFFFFFFFFUL;
        lastRTCRebase = currentMillis;
        return;
    }
    rtcMicrosAtLastSync = currentMicros;
    lastRTCRebase = currentMillis;
}

uint64_t getCurrentEpoch() {
    if (rtcInitialized && internalEpoch > 0) {
        unsigned long currentMicros = micros();
        unsigned long currentMillis = millis();
        if (lastRTCRebase == 0 || timeHasElapsed(currentMillis, lastRTCRebase, RTC_REBASE_INTERVAL)) {
            performRTCReabase();
            currentMicros = micros();
        }
        unsigned long elapsedMicros;
        if (currentMicros >= rtcMicrosAtLastSync) {
            elapsedMicros = currentMicros - rtcMicrosAtLastSync;
        } else {
            performRTCReabase();
            return internalEpoch;
        }
        float driftSafe = driftCompensation;
        if (driftSafe < 0.9f) driftSafe = 0.9f;
        if (driftSafe > 1.1f) driftSafe = 1.1f;
        double elapsedSeconds = (double)elapsedMicros / 1000000.0;
        double adjustedSeconds = elapsedSeconds * (double)driftSafe;
        uint64_t secondsToAdd = (uint64_t)adjustedSeconds; 
        uint64_t result = internalEpoch + secondsToAdd;
        return result;
    }
    if (rtcPresent && rtcTimeValid) {
        DateTime now = rtc.now();
        if (now.year() >= 2020 && now.year() <= 2100) {
            uint64_t rtcEpoch = rtcDateTimeToUint64(now);
            if (VALID_UNIX_TIME_64(rtcEpoch)) {
                return rtcEpoch;
            }
        }
    }
    return 0;
}

void saveRTCState() {
    performRTCReabase();
    sysConfig.last_rtc_epoch = internalEpoch;
    saveConfiguration();
}

void loadRTCState() {
    if (VALID_UNIX_TIME_64(sysConfig.last_rtc_epoch)) {
        internalEpoch            = sysConfig.last_rtc_epoch;
        driftCompensation        = 1.0f;
        rtcMicrosAtLastSync      = micros();
        lastRTCRebase            = millis();
        rtcInitialized           = true;
        timeSource               = TIME_SOURCE_RTC;
    }
}

// =============================================================================
//  Auto-Save Internal RTC
// =============================================================================
void autoSaveInternalRTC() {
    unsigned long now = millis();
    if (!rtcInitialized || internalEpoch == 0) return;
    if (rtcPresent && rtcTimeValid) return;
    if (timeHasElapsed(now, lastInternalRTCSave, INTERNAL_RTC_SAVE_INTERVAL)) {
        lastInternalRTCSave = now;
        performRTCReabase();
        sysConfig.last_rtc_epoch = internalEpoch;
        preferences.begin(NVS_NAMESPACE, false);
        preferences.putBytes("sysConfig", &sysConfig, sizeof(SystemConfig));
        preferences.end();
    }
}

// =============================================================================
//  Load RTC from DS3231
// =============================================================================
bool loadRTCFromDS3231() {
    if (rtcPresent && rtcTimeValid) {
        DateTime now = rtc.now();
        if (now.year() >= 2020 && now.year() <= 2100) {
            uint64_t rtcEpoch = rtcDateTimeToUint64(now);
            if (VALID_UNIX_TIME_64(rtcEpoch)) {
                internalEpoch = rtcEpoch;
                driftCompensation = 1.0f;
                rtcMicrosAtLastSync = micros();
                lastRTCRebase = millis();
                lastRTCDSync = millis();
                rtcInitialized = true;
                timeSource = TIME_SOURCE_RTC;
                if (sysConfig.last_rtc_epoch == 0 ||
                    (rtcEpoch > sysConfig.last_rtc_epoch
                        ? rtcEpoch - sysConfig.last_rtc_epoch > 60
                        : sysConfig.last_rtc_epoch - rtcEpoch > 60)) {
                    saveRTCState();
                }
                return true;
            }
        }
    }
    return false;
}

// =============================================================================
//  WiFi Scan Pause Function
// =============================================================================
void pauseWiFiForScan() {
    if (wifiConnecting && !wifiConnected && !wifiPausedForScan && extConfig.sta_enabled) {
        wifiPausedForScan = true;
        wifiPauseUntil = millis() + WIFI_PAUSE_DURATION;
        WiFi.disconnect(false, false);
        delay(100);
        wifiConnecting = false;
        lastScanAttempt = millis();
    }
}

// =============================================================================
//  Live Reconfiguration Functions
// =============================================================================
bool SelfHealingSystem::liveReconfigureWiFi() {
    unsigned long now = millis();
    if (!timeHasElapsed(now, lastWiFiReconfigure, 30000)) return true;
    if (!extConfig.sta_enabled) return true;
    if (strlen(sysConfig.sta_ssid) > 0) {
        if (WiFi.status() != WL_CONNECTED) {
            WiFi.begin(sysConfig.sta_ssid, sysConfig.sta_password);
            wifiConnecting = true;
            wifiConnectStart = now;
        } else {
            if (WiFi.SSID() != String(sysConfig.sta_ssid)) {
                WiFi.begin(sysConfig.sta_ssid, sysConfig.sta_password);
                wifiConnecting = true;
                wifiConnectStart = now;
            }
        }
    }
    lastWiFiReconfigure = now;
    return true;
}

bool SelfHealingSystem::liveReconfigureMDNS() {
    unsigned long now = millis();
    if (!timeHasElapsed(now, lastMDNSReconfigure, 60000)) return mdnsStarted;
    if (!mdnsStarted) {
        if (MDNS.begin(mdnsHostname)) {
            MDNS.addService("http", "tcp", 80);
            MDNS.addServiceTxt("http", "tcp", "model", "ESP32S3");
            MDNS.addServiceTxt("http", "tcp", "version", "v12");
            MDNS.addServiceTxt("http", "tcp", "channels", String(gpioConfig.count).c_str());
            MDNS.addServiceTxt("http", "tcp", "features", "monthmask,64bit,tz");
            mdnsStarted = true;
        }
    } else {
        MDNS.addService("http", "tcp", 80);
        MDNS.addServiceTxt("http", "tcp", "channels", String(gpioConfig.count).c_str());
        MDNS.addServiceTxt("http", "tcp", "features", "monthmask,64bit");
    }
    lastMDNSReconfigure = now;
    return mdnsStarted;
}

bool SelfHealingSystem::liveReconfigureAP() {
    if (WiFi.softAPIP() == IPAddress(0, 0, 0, 0)) {
        uint8_t ch = extConfig.ap_channel;
        if (ch < 1 || ch > 13) ch = 6;
        uint8_t hidden = extConfig.ap_hidden ? 1 : 0;
        bool ok;
        if (strlen(sysConfig.ap_password) > 0) {
            ok = WiFi.softAP(sysConfig.ap_ssid, sysConfig.ap_password, ch, hidden);
        } else {
            ok = WiFi.softAP(sysConfig.ap_ssid, NULL, ch, hidden);
        }
        if (ok) {
            liveReconfigureMDNS();
        }
        return ok;
    }
    return true;
}

void SelfHealingSystem::restartAPIfNeeded(bool forceRestart) {
    if (!forceRestart) return;
    WiFi.softAPdisconnect(true);
    delay(500);
    uint8_t ch = extConfig.ap_channel;
    if (ch < 1 || ch > 13) ch = 6;
    uint8_t hidden = extConfig.ap_hidden ? 1 : 0;
    if (WiFi.getMode() != WIFI_AP_STA && WiFi.getMode() != WIFI_AP) {
        WiFi.mode(WIFI_AP_STA);
    }
    bool apStarted = false;
    if (strlen(sysConfig.ap_password) > 0) {
        apStarted = WiFi.softAP(sysConfig.ap_ssid, sysConfig.ap_password, ch, hidden);
    } else {
        apStarted = WiFi.softAP(sysConfig.ap_ssid, NULL, ch, hidden);
    }
    if (!apStarted) {
        if (strlen(sysConfig.ap_password) > 0) {
            apStarted = WiFi.softAP(sysConfig.ap_ssid, sysConfig.ap_password, 6, hidden);
        } else {
            apStarted = WiFi.softAP(sysConfig.ap_ssid, NULL, 6, hidden);
        }
    }
    if (apStarted) {
        delay(100);
        dnsServer.stop();
        dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
        if (mdnsStarted) {
            MDNS.end();
            mdnsStarted = false;
        }
        delay(50);  
        if (MDNS.begin(mdnsHostname)) {
            MDNS.addService("http", "tcp", 80);
            MDNS.addServiceTxt("http", "tcp", "model", "ESP32S3");
            MDNS.addServiceTxt("http", "tcp", "version", "v12");
            MDNS.addServiceTxt("http", "tcp", "channels", String(gpioConfig.count).c_str());
            MDNS.addServiceTxt("http", "tcp", "features", "monthmask,64bit,tz");
            mdnsStarted = true;
        } else {
            mdnsStarted = false;
        }
        lastMDNSReconfigure = millis();
    } else {
        ESP.restart();
    }
}

// =============================================================================
//  Recovery Functions
// =============================================================================
bool SelfHealingSystem::recoverRTC() {
    if (!rtcPresent) return false;
    Wire.begin(8, 9);
    Wire.setTimeOut(50);
    if (!rtc.begin()) {
        rtcPresent = false;
        rtcTimeValid = false;
        return false;
    }
    if (rtcInitialized && internalEpoch > 0) {
        DateTime dt = uint64ToRtcDateTime(internalEpoch);
        rtc.adjust(dt);
        rtcTimeValid = true;
        lastRTCDSync = millis();
        return true;
    }
    if (rtc.lostPower()) {
        return false;
    }
    DateTime now = rtc.now();
    if (now.year() >= 2020 && now.year() <= 2100) {
        uint64_t rtcEpoch = rtcDateTimeToUint64(now);
        if (VALID_UNIX_TIME_64(rtcEpoch)) {
            rtcTimeValid = true;
            internalEpoch = rtcEpoch;
            driftCompensation = 1.0f;
            rtcMicrosAtLastSync = micros();
            lastRTCRebase = millis();
            rtcInitialized = true;
            timeSource = TIME_SOURCE_RTC;
            return true;
        }
    }
    return false;
}

void SelfHealingSystem::performTargetedRecovery() {
    liveReconfigureMDNS();
    delay(50);
    liveReconfigureWiFi();
    delay(50);
    liveReconfigureAP();
    delay(50);
    recoverRTC();
    verifyRelayStates(true);
    health.wifiFailures = 0;
}

void SelfHealingSystem::verifyRelayStates(bool force) {
    static unsigned long lastVerification = 0;
    unsigned long now = millis();
    if (!force && !timeHasElapsed(now, lastVerification, 30000)) return;
    lastVerification = now;
    updateScheduleCache();
    for (int i = 0; i < gpioConfig.count; i++) {
        int pin = getRelayPin(i);
        if (pin < 0) continue;
        bool expectedState = (relayConfigs[i].manualOverride)
            ? relayConfigs[i].manualState
            : scheduleActiveCache[i];
        bool expectedLowState = isActiveLow(i) ? !expectedState : expectedState;
        bool actualState = digitalRead(pin);
        if (actualState != expectedLowState) {
            setRelayOutput(i, expectedState);
            lastRelayOutputs[i] = expectedState;
        }
    }
}

void SelfHealingSystem::smartRecovery() {
    unsigned long now = millis();
    if (!timeHasElapsed(now, health.lastRecoveryAttempt, 10000)) return;
    health.lastRecoveryAttempt = now;
    if (extConfig.sta_enabled && WiFi.status() != WL_CONNECTED && strlen(sysConfig.sta_ssid) > 0 && !wifiConnecting && !wifiPausedForScan) {
        health.wifiFailures++;
        if (health.wifiFailures >= 3) {
            WiFi.begin(sysConfig.sta_ssid, sysConfig.sta_password);
            wifiConnecting = true;
            wifiConnectStart = now;
            health.wifiFailures = 0;
        }
    } else if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        health.wifiFailures = 0;
    }
    if (timeHasElapsed(now, lastMDNSReconfigure, 300000)) {
        if (mdnsStarted) {
            MDNS.addService("http", "tcp", 80);
            lastMDNSReconfigure = now;
        }
    }
    if (timeHasElapsed(now, lastFullHealthCheck, 1800000)) {
        lastFullHealthCheck = now;
        if (mdnsStarted) {
            MDNS.addService("http", "tcp", 80);
        }
    }
    verifyRelayStates();
    liveReconfigureAP();
}

// =============================================================================
//  Memory Management
// =============================================================================
void performMemoryCleanup() {
    if (!scanInProgress) {
        WiFi.scanDelete();
    }
    for (int i = 0; i < 10; i++) {
        yield();
        delay(1);
    }
}

void cleanupStaleResources() {
    if (!scanInProgress && WiFi.scanComplete() == WIFI_SCAN_RUNNING) {
        WiFi.scanDelete();
    }
    performMemoryCleanup();
}

void checkAndCleanMemory() {
    unsigned long now = millis();
    if (timeHasElapsed(now, lastHeapCheck, 300000)) {
        lastHeapCheck = now;
        size_t freeHeap = ESP.getFreeHeap();
        if (freeHeap < 20000) {
            performMemoryCleanup();
        }
    }
    if (timeHasElapsed(now, lastMemoryCleanup, 3600000)) {
        lastMemoryCleanup = now;
        performMemoryCleanup();
    }
}

// =============================================================================
//  SHARED CSS
// =============================================================================
const char style_css[] PROGMEM = R"css(
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Arial,sans-serif;background:#EEF2F7;color:#1A1A2E;font-size:14px;line-height:1.5}
header{background:linear-gradient(135deg,#1565C0 0%,#0D47A1 100%);color:#fff;padding:10px 16px;display:flex;align-items:center;gap:10px;position:sticky;top:0;z-index:50;box-shadow:0 2px 10px rgba(0,0,0,.3);flex-wrap:wrap}
nav{display:flex;gap:3px;flex-wrap:wrap;flex:1}
nav a{color:rgba(255,255,255,.8);text-decoration:none;padding:5px 8px;border-radius:5px;font-size:12px;transition:.15s}
nav a:hover,nav a.cur{background:rgba(255,255,255,.2);color:#fff}
.hdr-r{display:flex;align-items:center;gap:6px;margin-left:auto;font-size:12px;white-space:nowrap}
.dot{width:8px;height:8px;border-radius:50%;display:inline-block;background:#546E7A;flex-shrink:0}
.g{background:#69F0AE}.r{background:#FF5252}.y{background:#FFD740}.b{background:#42A5F5}
main{max-width:1200px;margin:0 auto;padding:16px}
.ptitle{font-size:17px;font-weight:700;color:#1565C0;margin-bottom:14px}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(340px,1fr));gap:14px}
.card{background:#fff;border-radius:10px;box-shadow:0 2px 8px rgba(0,0,0,.08);padding:16px;transition:box-shadow .2s}
.card:hover{box-shadow:0 4px 18px rgba(0,0,0,.13)}
.card-hdr{display:flex;align-items:center;justify-content:space-between;margin-bottom:10px}
.ctitle{font-weight:700;font-size:15px;cursor:pointer;transition:background .15s;padding:2px 4px;border-radius:4px}
.ctitle:hover{background:#E3F2FD;color:#1565C0}
.badge{padding:3px 9px;border-radius:20px;font-size:11px;font-weight:700}
.bon{background:#E8F5E9;color:#2E7D32}.boff{background:#FFEBEE;color:#C62828}.bman{background:#FFF3E0;color:#E65100}.bman-on{background:#FFF3E0;color:#E65100;border:1px solid #FFB74D}.bman-off{background:#FBE9E7;color:#BF360C;border:1px solid #FF8A65}
.brow{display:flex;gap:6px;flex-wrap:wrap;margin-bottom:10px}
.btn{border:none;padding:7px 12px;border-radius:6px;cursor:pointer;font-size:12px;font-weight:600;transition:.15s}
.btn:hover{filter:brightness(1.1)}.btn:disabled{opacity:.5;cursor:default}
.bon-b{background:#43A047;color:#fff}.boff-b{background:#E53935;color:#fff}.baut{background:#546E7A;color:#fff}
.bsave{background:#1565C0;color:#fff;width:100%;padding:9px;font-size:13px;border-radius:6px;margin-top:8px}
.bsync{background:#FB8C00;color:#fff}.bdanger{background:#B71C1C;color:#fff}.bwarn{background:#F9A825;color:#212121}
.bscan{background:#0288D1;color:#fff}
.slist{display:flex;flex-direction:column;gap:6px;margin-bottom:8px;max-height:500px;overflow-y:auto;padding-right:2px}
.si{border:1px solid #E3E8EF;border-radius:7px;padding:9px;min-height:auto;height:auto;overflow:visible}
.si.act{border-color:#90CAF9;background:#F0F7FF}
.shdr{display:flex;align-items:flex-start;gap:7px;margin-bottom:7px;font-size:11px;font-weight:700;color:#607D8B;text-transform:uppercase;flex-wrap:wrap}
.shdr label{display:flex;align-items:center;gap:4px;cursor:pointer;font-size:12px;font-weight:700;color:#1A1A2E;text-transform:none;white-space:nowrap}
.trow{display:flex;align-items:center;gap:8px;font-size:12px;margin-top:5px}
.trow .l{color:#90A4AE;font-weight:600;width:32px;flex-shrink:0}
.days{display:flex;gap:3px;margin-top:5px;flex-wrap:wrap}
.day{width:36px;height:36px;border-radius:4px;border:1px solid #CFD8DC;display:flex;align-items:center;justify-content:center;font-size:11px;font-weight:600;cursor:pointer;background:#FAFAFA;transition:.15s;user-select:none;padding:0;font-family:inherit;color:#1A1A2E;-webkit-appearance:none;appearance:none}
.day:hover{border-color:#90CAF9;background:#E3F2FD}
.day.on{background:#1565C0;color:#fff;border-color:#1565C0}
.day:focus-visible{outline:2px solid #1565C0;outline-offset:2px}
.mdays{display:flex;gap:2px;margin-top:5px;flex-wrap:wrap}
.mday{width:32px;height:32px;border-radius:3px;border:1px solid #CFD8DC;display:flex;align-items:center;justify-content:center;font-size:10px;font-weight:600;cursor:pointer;background:#FAFAFA;transition:.15s;user-select:none;padding:0;font-family:inherit;color:#1A1A2E;-webkit-appearance:none;appearance:none}
.mday:hover{border-color:#90CAF9;background:#E3F2FD}
.mday.on{background:#1565C0;color:#fff;border-color:#1565C0}
.mday:focus-visible{outline:2px solid #1565C0;outline-offset:2px}
.months{display:flex;gap:3px;margin-top:5px;flex-wrap:wrap}
.month{width:44px;height:36px;border-radius:4px;border:1px solid #CFD8DC;display:flex;align-items:center;justify-content:center;font-size:10px;font-weight:600;cursor:pointer;background:#FAFAFA;transition:.15s;user-select:none;padding:0;font-family:inherit;color:#1A1A2E;-webkit-appearance:none;appearance:none}
.month:hover{border-color:#90CAF9;background:#E3F2FD}
.month.on{background:#1565C0;color:#fff;border-color:#1565C0}
.month:focus-visible{outline:2px solid #1565C0;outline-offset:2px}
.sched-section{margin-top:4px;font-size:10px;font-weight:600;color:#90A4AE;text-transform:uppercase;margin-bottom:2px}
.night{font-size:10px;color:#0D47A1;background:#E3F2FD;padding:2px 6px;border-radius:4px;margin-left:auto;white-space:normal;word-break:break-word;max-width:100%;line-height:1.4;flex:1 1 auto;text-align:right}
.night.always{background:#E8F5E9;color:#2E7D32}
input[type=time]{flex:1;padding:5px 8px;border:1px solid #CFD8DC;border-radius:5px;font-size:13px;font-family:monospace;background:#FAFAFA;cursor:pointer;min-width:0}
input[type=time]:focus{outline:none;border-color:#1565C0;box-shadow:0 0 0 3px rgba(21,101,192,.15);background:#fff}
.ibar{display:grid;grid-template-columns:repeat(auto-fill,minmax(160px,1fr));gap:8px;margin-bottom:16px}
.ibox{background:#fff;border-radius:8px;padding:12px;box-shadow:0 1px 4px rgba(0,0,0,.07)}
.ibox .l{font-size:11px;color:#90A4AE;text-transform:uppercase;font-weight:600}
.ibox .v{font-size:15px;font-weight:700;color:#1A1A2E;margin-top:2px}
.fcrd{max-width:600px}
.fg{margin-bottom:14px}
.fg label{display:block;font-size:11px;font-weight:700;color:#607D8B;margin-bottom:5px;text-transform:uppercase;letter-spacing:.4px}
.fg input,.fg select{width:100%;padding:9px 12px;border:1px solid #CFD8DC;border-radius:7px;font-size:14px;background:#FAFAFA}
.fg input:focus,.fg select:focus{outline:none;border-color:#1565C0;box-shadow:0 0 0 3px rgba(21,101,192,.15);background:#fff}
.fg small{display:block;margin-top:4px;font-size:11px;color:#90A4AE}
.input-row{display:flex;gap:8px}
.input-row input{flex:1;min-width:0}
.alert{border-radius:7px;padding:11px 14px;font-size:13px;margin-bottom:14px}
.aw{background:#FFF8E1;border-left:4px solid #F9A825;color:#5D4037}
.ai{background:#E3F2FD;border-left:4px solid #1565C0;color:#0D47A1}
hr{border:none;border-top:1px solid #ECEFF1;margin:14px 0}
.netlist{margin-top:10px;display:none}
.net-hdr{font-size:11px;font-weight:700;color:#607D8B;text-transform:uppercase;margin-bottom:6px}
.netitem{display:flex;align-items:center;gap:8px;padding:8px 10px;border:1px solid #E3E8EF;border-radius:7px;cursor:pointer;margin-bottom:5px;background:#FAFAFA;transition:.15s}
.netitem:hover{background:#EEF2F7;border-color:#90CAF9}
.netitem .ns{flex:1;font-size:13px;font-weight:600;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.netitem .nr{font-size:11px;color:#90A4AE;white-space:nowrap}
.bars{display:inline-flex;align-items:flex-end;gap:2px;height:14px}
.bar{width:4px;border-radius:1px;background:#CFD8DC}
.bar.on{background:#43A047}
#toast{position:fixed;bottom:22px;left:50%;transform:translateX(-50%) translateY(80px);background:#323232;color:#fff;padding:10px 20px;border-radius:8px;font-size:13px;transition:transform .28s;z-index:999;pointer-events:none;box-shadow:0 4px 16px rgba(0,0,0,.3);min-width:180px;text-align:center}
#toast.show{transform:translateX(-50%) translateY(0)}
#toast.ok{background:#2E7D32}#toast.er{background:#C62828}
@media(max-width:500px){.grid{grid-template-columns:1fr}.ibar{grid-template-columns:1fr}.input-row{flex-direction:column}.day{width:34px;height:34px;font-size:10px}.mday{width:30px;height:30px;font-size:9px}.month{width:40px;height:34px;font-size:9px}}
)css";

// =============================================================================
//  INDEX PAGE
// =============================================================================
const char index_html[] PROGMEM = R"raw(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Relays</title>
<link rel="stylesheet" href="/style.css"></head><body>
<header>
<nav>
<a href="/" class="cur">Relays</a>
<a href="/wifi">WiFi</a>
<a href="/ntp">Time</a>
<a href="/ap">AP</a>
<a href="/gpio">GPIO</a>
<a href="/system">System</a>
</nav>
<div class="hdr-r"><span class="dot wd"></span><span class="dot td"></span>&nbsp;<span id="clk">--:--:--</span><span id="date" style="font-size:11px;opacity:.85;margin-left:6px;white-space:nowrap">--/--/----</span></div>
</header>
<main>
<p class="ptitle">Relay Controls &amp; Schedules</p>
<div class="grid" id="grid"></div>
</main>
<div id="toast"></div>
<script>
const D=['Sun','Mon','Tue','Wed','Thu','Fri','Sat'];
const M=['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'];

function toast(m,ok=true){
  const t=document.getElementById('toast');
  if(!t) return;
  t.textContent=m;
  t.className='show '+(ok?'ok':'er');
  clearTimeout(t._t);
  t._t=setTimeout(()=>t.className='',3000);
}

function fetchWithTimeout(url, options = {}, timeout = 10000) {
  const controller = new AbortController();
  const signal = controller.signal;
  const timer = setTimeout(() => controller.abort(), timeout);
  const opts = Object.assign({}, options, {signal: signal});
  return fetch(url, opts).then(
    response => { clearTimeout(timer); return response; },
    error => { clearTimeout(timer); throw error; }
  );
}

function tick(){
  if(tick._inflight) return;
  tick._inflight = true;
  fetchWithTimeout('/api/time').then(r=>{if(!r.ok) throw new Error('HTTP '+r.status);return r.json();}).then(d=>{
    tick._inflight = false;
    if(!d || typeof d !== 'object') return;
    const clkEl = document.getElementById('clk');
    if(clkEl) clkEl.textContent = (typeof d.time === 'string' && d.time) ? d.time : '--:--:--';
    const de = document.getElementById('date');
    if(de){
      const wk = (typeof d.weekday === 'string' && d.weekday) ? d.weekday + ' ' : '';
      const dt = (typeof d.date === 'string' && d.date) ? d.date : '--/--/----';
      de.textContent = wk + dt;
    }
    const w=document.querySelector('.wd'),t=document.querySelector('.td');
    if(w)w.className='dot '+(d.wifi?'g':'r');
    if(t){
      let tc='y';
      const auth = (typeof d.authority === 'string' && d.authority) ? d.authority : d.timeSource;
      if(auth==='ntp'){
        tc = (d.authorityStale === true) ? 'y' : 'g';
      }
      else if(auth==='browser')tc='y';
      else if(auth==='ds3231')tc='g';
      else if(auth==='internal_rtc')tc='y';
      else if(auth==='rtc')tc=(d.rtcPresent === true) ? 'g' : 'y';
      else tc='y';
      t.className='dot '+tc;
    }
  }).catch(()=>{ tick._inflight = false; });
}
setInterval(tick,1000);tick();

const NS=8;
let relays=[],busyCount=0;
let loadInFlight = false;
let pendingLoad = false;
let editingRelay = -1;
let editingInput = null;
let editSuppressBlur = false;
let savingRelays = {};
let nameEditSeq = {};

function escapeHtml(text) {
  const div = document.createElement('div');
  div.textContent = text;
  return div.innerHTML;
}

function utf8ByteLen(str){
  if(typeof str !== 'string') return 0;
  try {
    if(typeof TextEncoder !== 'undefined') return new TextEncoder().encode(str).length;
  } catch(e) {}
  let n = 0;
  for(let i = 0; i < str.length; i++){
    const c = str.charCodeAt(i);
    if(c < 0x80) n += 1;
    else if(c < 0x800) n += 2;
    else if(c >= 0xD800 && c <= 0xDBFF) { n += 4; i++; }
    else n += 3;
  }
  return n;
}

function clampUtf8Bytes(str, maxBytes){
  if(typeof str !== 'string') return '';
  if(maxBytes <= 0) return '';
  let bytes = 0;
  let out = '';
  for(let i = 0; i < str.length; i++){
    const c = str.charCodeAt(i);
    let step;
    if(c < 0x80){ step = 1; }
    else if(c < 0x800){ step = 2; }
    else if(c >= 0xD800 && c <= 0xDBFF){
      if(i + 1 < str.length){
        const c2 = str.charCodeAt(i + 1);
        if(c2 >= 0xDC00 && c2 <= 0xDFFF){ step = 4; }
        else { step = 3; }
      } else { step = 3; }
    } else { step = 3; }
    if(bytes + step > maxBytes) break;
    bytes += step;
    if(step === 4){ out += str[i] + str[i+1]; i++; }
    else out += str[i];
  }
  return out;
}

function requestLoad(){
  if(busyCount > 0){ pendingLoad = true; return; }
  load();
}

function load(){
  if(busyCount > 0) return;
  if(loadInFlight) return;
  loadInFlight = true;
  const g = document.getElementById('grid');
  fetchWithTimeout('/api/relays').then(r=>r.json()).then(d=>{
    loadInFlight = false;
    if(!Array.isArray(d)){toast('Load error',false);if(g && (!Array.isArray(relays) || relays.length === 0))g.innerHTML='';return;}
    d=d.filter(r=>r&&typeof r==='object');
    d.forEach(r=>{
      if(!Array.isArray(r.schedules))r.schedules=[];
      r.schedules=r.schedules.filter(s=>s&&typeof s==='object');
      if(r.schedules.length > NS) r.schedules = r.schedules.slice(0, NS);
      while(r.schedules.length<NS)r.schedules.push({startHour:0,startMinute:0,startSecond:0,stopHour:0,stopMinute:0,stopSecond:0,enabled:false,days:0x7F,monthDays:0x7FFFFFFF,monthMask:0x0FFF});
    });
    relays=d;render();
  }).catch(()=>{
    loadInFlight = false;
    toast('Load error',false);
    if(g && (!Array.isArray(relays) || relays.length === 0)) g.innerHTML='';
  });
}

function toTS(h,m,s){return String(h).padStart(2,'0')+':'+String(m).padStart(2,'0')+':'+String(s).padStart(2,'0');}
function fromTS(v){const p=(v||'00:00:00').split(':');return{h:parseInt(p[0])||0,m:parseInt(p[1])||0,s:parseInt(p[2])||0};}

function dayMaskToStr(d){
  if(d === undefined || d === null) d = 0x7F;
  d = d & 0x7F;
  if(d===0x7F) return 'Everyday';
  if(d===0) return 'None';
  let s='';
  for(let i=0;i<7;i++) if(d&(1<<i)) s+=D[i]+' ';
  return s.trim()||'None';
}

function monthDayMaskToStr(md){
  let v = (md === undefined || md === null) ? 0x7FFFFFFF : md;
  v = v & 0x7FFFFFFF;
  if(v === 0) return 'None';
  if(v === 0x7FFFFFFF) return 'All month days';
  let s='';
  for(let i=0;i<31;i++) if(v&(1<<i)) s+=(i+1)+',';
  return s.replace(/,$/,'')||'None';
}

function monthMaskToStr(mm) {
  if (mm === undefined || mm === null) return '';
  mm = mm & 0x0FFF;
  if (mm === 0x0FFF) return 'All months';
  if (mm === 0) return 'None';
  let s='';
  for(let i=0;i<12;i++) if(mm&(1<<i)) s+=M[i]+' ';
  return s.trim()||'None';
}

function nightBadge(sc){
  if(!sc) return '';
  if(!sc.enabled) return '';
  const sh = (typeof sc.startHour === 'number') ? sc.startHour : 0;
  const sm = (typeof sc.startMinute === 'number') ? sc.startMinute : 0;
  const ss = (typeof sc.startSecond === 'number') ? sc.startSecond : 0;
  const eh = (typeof sc.stopHour === 'number') ? sc.stopHour : 0;
  const em = (typeof sc.stopMinute === 'number') ? sc.stopMinute : 0;
  const es = (typeof sc.stopSecond === 'number') ? sc.stopSecond : 0;
  const a = sh*3600 + sm*60 + ss;
  const b = eh*3600 + em*60 + es;
  const ds = dayMaskToStr(sc.days);
  const ms = monthDayMaskToStr(sc.monthDays);
  const mm = monthMaskToStr(sc.monthMask);
  let parts=[];
  if(ds === 'None') parts.push('No days');
  else if(ds) parts.push(ds);
  if(ms && ms!=='All month days') parts.push('Days: '+ms);
  if(mm && mm!=='All months') parts.push('Months: '+mm);
  let info = parts.join(' | ');
  if(!info) info = 'Always';
  if(a===b) return '<span class="night always">&#x25CF; Always ON ('+info+')</span>';
  if(a>b) return '<span class="night">&#x1F319; Overnight ('+info+')</span>';
  return '<span class="night">&#x1F319; '+info+'</span>';
}

function startEditName(relayIdx) {
  if(!relays[relayIdx] || typeof relays[relayIdx] !== 'object') return;
  if (editingRelay === relayIdx) {
    if (editingInput) { editingInput.focus(); editingInput.select(); }
    return;
  }
  if (editingRelay !== -1) cancelEdit();
  const nameSpan = document.getElementById('name_' + relayIdx);
  if (!nameSpan) return;
  const currentName = relays[relayIdx].name || ('Relay ' + (relayIdx + 1));
  const input = document.createElement('input');
  input.type = 'text';
  input.value = currentName;
  input.maxLength = 15;
  input.style.cssText = 'font-size:15px;font-weight:700;padding:2px 6px;border:1px solid #1565C0;border-radius:5px;width:120px;background:#fff;color:#1A1A2E;';
  input.onblur = () => {
    if (editSuppressBlur) return;
    saveNameEdit(relayIdx, input.value);
  };
  input.addEventListener('input', () => {
    if(utf8ByteLen(input.value) > 15){
      input.value = clampUtf8Bytes(input.value, 15);
    }
  });
  input.onkeydown = (e) => {
    if (e.key === 'Enter') {
      e.preventDefault();
      editSuppressBlur = true;
      saveNameEdit(relayIdx, input.value);
      editSuppressBlur = false;
    } else if (e.key === 'Escape') {
      e.preventDefault();
      editSuppressBlur = true;
      cancelEdit();
      editSuppressBlur = false;
    }
  };
  nameSpan.style.display = 'none';
  nameSpan.parentNode.insertBefore(input, nameSpan.nextSibling);
  editingRelay = relayIdx;
  editingInput = input;
  input.focus();
  input.select();
}

function cancelEdit() {
  if (editingRelay !== -1) {
    const nameSpan = document.getElementById('name_' + editingRelay);
    if (nameSpan) nameSpan.style.display = '';
    editSuppressBlur = true;
    if (editingInput) editingInput.remove();
    editSuppressBlur = false;
    editingRelay = -1;
    editingInput = null;
  }
}

function saveNameEdit(relayIdx, newName) {
  if(!relays[relayIdx] || typeof relays[relayIdx] !== 'object'){
    if(editingRelay === relayIdx){ editingRelay = -1; editingInput = null; }
    return;
  }
  if(typeof newName !== 'string') newName = '';
  newName = newName.trim();
  if(utf8ByteLen(newName) > 15){
    newName = clampUtf8Bytes(newName, 15).trim();
  }
  while(newName.length > 0 && utf8ByteLen(newName) > 15){
    newName = newName.slice(0, -1).trim();
  }
  if (newName.length === 0) {
    newName = 'Relay ' + (relayIdx + 1);
  }
  const nameSpan = document.getElementById('name_' + relayIdx);
  const oldName = relays[relayIdx].name || ('Relay ' + (relayIdx + 1));
  relays[relayIdx].name = newName;
  if (nameSpan) {
    nameSpan.textContent = newName;
    nameSpan.style.display = '';
  }
  editSuppressBlur = true;
  if (editingInput) editingInput.remove();
  editSuppressBlur = false;
  editingRelay = -1;
  editingInput = null;
  const mySeq = (nameEditSeq[relayIdx] || 0) + 1;
  nameEditSeq[relayIdx] = mySeq;
  fetchWithTimeout('/api/relay/name', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({relay: relayIdx, name: newName})
  })
  .then(r => r.json())
  .then(d => {
    if (nameEditSeq[relayIdx] !== mySeq) return;
    if (d && d.success) {
      toast('Name saved!');
    } else {
      relays[relayIdx].name = oldName;
      const span = document.getElementById('name_' + relayIdx);
      if (span) span.textContent = oldName;
      toast('Failed to save name', false);
    }
  })
  .catch(() => {
    if (nameEditSeq[relayIdx] !== mySeq) return;
    relays[relayIdx].name = oldName;
    const span = document.getElementById('name_' + relayIdx);
    if (span) span.textContent = oldName;
    toast('Error saving name', false);
  });
}

function render(){
  const g = document.getElementById('grid');
  if(!g) return;
  g.innerHTML='';
  if (!Array.isArray(relays) || relays.length === 0) {
    g.innerHTML = '<div class="card" style="text-align:center;padding:40px"><p style="color:#90A4AE">No relays configured. <a href="/gpio" style="color:#1565C0">Configure GPIO pins</a></p></div>';
    return;
  }
  relays.forEach((r,i)=>{
    if(!r || typeof r !== 'object'){
      r = {state:false, manual:false, name:'Relay '+(i+1), schedules:[]};
      relays[i] = r;
    }
    const sl = r.manual ? (r.state ? 'MANUAL ON' : 'MANUAL OFF') : (r.state ? 'ON' : 'OFF');
    const sc = r.manual ? (r.state ? 'bman-on' : 'bman-off') : (r.state ? 'bon' : 'boff');
    const displayName = r.name || ('Relay '+(i+1));
    let html = `<div class="card">
<div class="card-hdr">
<span class="ctitle" id="name_${i}" ondblclick="startEditName(${i})" title="Double-click to rename">${escapeHtml(displayName)}</span>
<span class="badge ${sc}">${sl}</span>
</div>
<div class="brow">
<button type="button" class="btn bon-b" onclick="mc(${i},true)">ON</button>
<button type="button" class="btn boff-b" onclick="mc(${i},false)">OFF</button>
<button type="button" class="btn baut" onclick="ra(${i})">Auto</button>
</div>
<div class="slist">`;
    for(let s=0;s<NS;s++){
      const sc2 = r.schedules[s] || {startHour:0,startMinute:0,startSecond:0,stopHour:0,stopMinute:0,stopSecond:0,enabled:false,days:0x7F,monthDays:0x7FFFFFFF,monthMask:0x0FFF};
      const dayBits = (sc2.days === undefined || sc2.days === null) ? 0x7F : sc2.days;
      const rawMonthDayBits = (sc2.monthDays === undefined || sc2.monthDays === null) ? 0x7FFFFFFF : sc2.monthDays;
      const monthDayBits = rawMonthDayBits & 0x7FFFFFFF;
      const monthMask = (sc2.monthMask === undefined || sc2.monthMask === null) ? 0x0FFF : sc2.monthMask;
      html+=`<div class="si${sc2.enabled?' act':''}" id="si_${i}_${s}">
<div class="shdr">
<label><input type="checkbox" id="en_${i}_${s}" ${sc2.enabled?'checked':''} onchange="uf(${i},${s},'en',this.checked)"> Sched ${s+1}</label>
<span id="nb_${i}_${s}">${nightBadge(sc2)}</span>
</div>
<div class="trow"><span class="l">Start</span>
<input type="time" step="1" id="st_${i}_${s}" value="${toTS(sc2.startHour,sc2.startMinute,sc2.startSecond)}" onchange="uf(${i},${s},'start',this.value)">
</div>
<div class="trow"><span class="l">Stop</span>
<input type="time" step="1" id="et_${i}_${s}" value="${toTS(sc2.stopHour,sc2.stopMinute,sc2.stopSecond)}" onchange="uf(${i},${s},'stop',this.value)">
</div>
<div class="sched-section">Days of Week</div>
<div class="days" id="day_${i}_${s}">`;
      for(let d=0;d<7;d++){
        const mask = 1<<d;
        html+=`<button type="button" class="day${(dayBits&mask)?' on':''}" onclick="toggleDay(${i},${s},${d})" aria-pressed="${(dayBits&mask)?'true':'false'}" aria-label="${D[d]}">${D[d]}</button>`;
      }
      html+=`</div>
<div class="sched-section">Days of Month</div>
<div class="mdays" id="mday_${i}_${s}">`;
      for(let d=0;d<31;d++){
        const mask = 1<<d;
        html+=`<button type="button" class="mday${(monthDayBits&mask)?' on':''}" onclick="toggleMonthDay(${i},${s},${d})" title="Day ${d+1}" aria-pressed="${(monthDayBits&mask)?'true':'false'}" aria-label="Day ${d+1}">${d+1}</button>`;
      }
      html+=`</div>
<div class="sched-section">Months of Year</div>
<div class="months" id="mon_${i}_${s}">`;
      for(let m=0;m<12;m++){
        const mask = 1<<m;
        html+=`<button type="button" class="month${(monthMask&mask)?' on':''}" onclick="toggleMonth(${i},${s},${m})" aria-pressed="${(monthMask&mask)?'true':'false'}" aria-label="${M[m]}">${M[m]}</button>`;
      }
      html+=`</div>
</div>`;
    }
    html+=`</div><button type="button" class="btn bsave" onclick="save(${i})">&#x1F4BE; Save ${escapeHtml(displayName)}</button></div>`;
    const el=document.createElement('div');
    el.innerHTML=html;
    while (el.firstChild) g.appendChild(el.firstChild);
  });
}

function toggleDay(ri,si,dayIdx){
  if(!relays[ri] || !Array.isArray(relays[ri].schedules)) return;
  let sc = relays[ri].schedules[si];
  if(!sc || typeof sc !== 'object') return;
  const mask = 1<<dayIdx;
  let cur = sc.days;
  if(cur === undefined || cur === null) cur = 0x7F;
  cur = (cur & 0x7F) ^ mask;
  sc.days = cur;
  const dayEl = document.getElementById('day_'+ri+'_'+si);
  if(dayEl && dayEl.children[dayIdx]){
    const c = dayEl.children[dayIdx];
    c.className = 'day' + ((cur & mask)?' on':'');
    c.setAttribute('aria-pressed', (cur & mask) ? 'true' : 'false');
  }
  const nb=document.getElementById('nb_'+ri+'_'+si);
  if(nb)nb.innerHTML=nightBadge(sc);
}

function toggleMonthDay(ri,si,dayIdx){
  if(!relays[ri] || !Array.isArray(relays[ri].schedules)) return;
  let sc = relays[ri].schedules[si];
  if(!sc || typeof sc !== 'object') return;
  const mask = 1<<dayIdx;
  let cur = sc.monthDays;
  if(cur === undefined || cur === null) cur = 0x7FFFFFFF;
  cur = cur & 0x7FFFFFFF;
  cur ^= mask;
  sc.monthDays = cur;
  const mdayEl = document.getElementById('mday_'+ri+'_'+si);
  if(mdayEl && mdayEl.children[dayIdx]){
    const c = mdayEl.children[dayIdx];
    c.className = 'mday' + ((cur & mask)?' on':'');
    c.setAttribute('aria-pressed', (cur & mask) ? 'true' : 'false');
  }
  const nb=document.getElementById('nb_'+ri+'_'+si);
  if(nb)nb.innerHTML=nightBadge(sc);
}

function toggleMonth(ri,si,mIdx){
  if(!relays[ri] || !Array.isArray(relays[ri].schedules)) return;
  let sc = relays[ri].schedules[si];
  if(!sc || typeof sc !== 'object') return;
  const mask = 1<<mIdx;
  let cur = sc.monthMask;
  if(cur === undefined || cur === null) cur = 0x0FFF;
  cur = cur & 0x0FFF;
  cur ^= mask;
  sc.monthMask = cur;
  const monEl = document.getElementById('mon_'+ri+'_'+si);
  if(monEl && monEl.children[mIdx]){
    const c = monEl.children[mIdx];
    c.className = 'month' + ((cur & mask)?' on':'');
    c.setAttribute('aria-pressed', (cur & mask) ? 'true' : 'false');
  }
  const nb=document.getElementById('nb_'+ri+'_'+si);
  if(nb)nb.innerHTML=nightBadge(sc);
}

function uf(ri,si,field,val){
  if(!relays[ri] || !Array.isArray(relays[ri].schedules)) return;
  let sc=relays[ri].schedules[si];
  if(!sc || typeof sc !== 'object'){
    sc={startHour:0,startMinute:0,startSecond:0,stopHour:0,stopMinute:0,stopSecond:0,enabled:false,days:0x7F,monthDays:0x7FFFFFFF,monthMask:0x0FFF};
    relays[ri].schedules[si] = sc;
  }
  if(field==='en'){
    sc.enabled = !!val;
    const el = document.getElementById('si_'+ri+'_'+si);
    if(el) el.className = 'si' + (val?' act':'');
  }else if(field==='start'){
    const t=fromTS(val);sc.startHour=t.h;sc.startMinute=t.m;sc.startSecond=t.s;
  }else if(field==='stop'){
    const t=fromTS(val);sc.stopHour=t.h;sc.stopMinute=t.m;sc.stopSecond=t.s;
  }
  const nb=document.getElementById('nb_'+ri+'_'+si);
  if(nb) nb.innerHTML = nightBadge(sc);
}

function mc(ri,state){
  if(!relays[ri]) return;
  fetchWithTimeout('/api/relay/manual',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({relay:ri,state:!!state})})
  .then(r=>r.json()).then(d=>{
    if(d && d.success){toast((relays[ri].name||('Relay '+(ri+1)))+' '+(state?'ON':'OFF'));requestLoad();}
    else toast('Failed',false);
  }).catch(()=>toast('Error',false));
}

function ra(ri){
  if(!relays[ri]) return;
  fetchWithTimeout('/api/relay/reset',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({relay:ri})})
  .then(r=>r.json()).then(d=>{
    if(d && d.success){toast((relays[ri].name||('Relay '+(ri+1)))+' \u2192 Auto');requestLoad();}
    else toast('Failed',false);
  }).catch(()=>toast('Error',false));
}

function save(ri){
  if(!relays[ri]) return;
  if(savingRelays[ri]) return;
  savingRelays[ri] = true;
  busyCount++;
  const btn = document.querySelector(`button[onclick="save(${ri})"]`);
  if(btn){ btn.disabled = true; btn.textContent = 'Saving...'; }
  const safeName = escapeHtml((relays[ri] && relays[ri].name) || ('Relay '+(ri+1)));
  const srcSchedules = (Array.isArray(relays[ri].schedules) ? relays[ri].schedules : []).slice(0, NS);
  const payloadSchedules = srcSchedules.map(function(s){
    s = s || {};
    const clampInt = (v, lo, hi, dflt) => {
      const n = parseInt(v, 10);
      if(isNaN(n)) return dflt;
      return Math.max(lo, Math.min(hi, n));
    };
    return {
      startHour:   clampInt(s.startHour,   0, 23, 0),
      startMinute: clampInt(s.startMinute, 0, 59, 0),
      startSecond: clampInt(s.startSecond, 0, 59, 0),
      stopHour:    clampInt(s.stopHour,    0, 23, 0),
      stopMinute:  clampInt(s.stopMinute,  0, 59, 0),
      stopSecond:  clampInt(s.stopSecond,  0, 59, 0),
      enabled:     !!s.enabled,
      days:        ((typeof s.days === 'number' ? s.days : 0x7F) & 0x7F),
      monthDays:   ((typeof s.monthDays === 'number' ? s.monthDays : 0x7FFFFFFF) & 0x7FFFFFFF),
      monthMask:   ((typeof s.monthMask === 'number' ? s.monthMask : 0x0FFF) & 0x0FFF)
    };
  });
  while(payloadSchedules.length < NS){
    payloadSchedules.push({
      startHour:0, startMinute:0, startSecond:0,
      stopHour:0, stopMinute:0, stopSecond:0,
      enabled:false, days:0x7F, monthDays:0x7FFFFFFF, monthMask:0x0FFF
    });
  }
  fetchWithTimeout('/api/relay/save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({relay:ri,schedules:payloadSchedules})})
  .then(r=>r.json()).then(d=>{
    savingRelays[ri] = false;
    if(busyCount > 0) busyCount--;
    if(btn){
      btn.disabled = false;
      btn.innerHTML = '&#x1F4BE; Save ' + safeName;
    }
    if(busyCount === 0 && pendingLoad){ pendingLoad = false; load(); }
    if(d && d.success) toast((relays[ri].name||('Relay '+(ri+1)))+' saved!');
    else toast('Save failed',false);
  })
  .catch(()=>{
    savingRelays[ri] = false;
    if(busyCount > 0) busyCount--;
    if(btn){
      btn.disabled = false;
      btn.innerHTML = '&#x1F4BE; Save ' + safeName;
    }
    if(busyCount === 0 && pendingLoad){ pendingLoad = false; load(); }
    toast('Error',false);
  });
}

load();
</script></body></html>)raw";

// =============================================================================
//  WIFI PAGE
// =============================================================================
const char wifi_html[] PROGMEM = R"raw(<!DOCTYPE html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>WiFi</title>
<link rel="stylesheet" href="/style.css"></head><body>
<header>
<nav>
<a href="/">Relays</a>
<a href="/wifi" class="cur">WiFi</a>
<a href="/ntp">Time</a>
<a href="/ap">AP</a>
<a href="/gpio">GPIO</a>
<a href="/system">System</a>
</nav>
<div class="hdr-r"><span class="dot wd"></span><span class="dot td"></span>&nbsp;<span id="clk">--:--:--</span><span id="date" style="font-size:11px;opacity:.85;margin-left:6px;white-space:nowrap">--/--/----</span></div>
</header>
<main>
<p class="ptitle">WiFi Station Settings</p>
<div class="card fcrd">
<div id="status" class="alert ai" style="display:none"></div>

<div class="fg">
<label style="display:flex;align-items:center;justify-content:space-between;cursor:pointer;margin-bottom:8px">
<span style="font-size:14px;font-weight:600">WiFi Station Mode</span>
<button id="staToggleBtn" class="btn" style="min-width:90px;padding:8px 16px;font-weight:700;transition:all 0.2s" onclick="toggleStation()"></button>
</label>
<small>When disabled, ESP32S3 runs in AP-only mode (no internet/NTP). Relay schedules using internal RTC/DS3231 will still work. You can still access this UI via the AP.</small>
</div>

<hr>

<div id="stationConfig">
<div class="fg">
<label>Network SSID</label>
<div class="input-row">
<input type="text" id="ssid" placeholder="Enter wifi name or scan" required>
<button class="btn bscan" id="scanBtn" onclick="startScan()" style="white-space:nowrap">&#x1F4F6; Scan</button>
</div>
</div>
<div class="netlist" id="netlist"></div>
<div class="fg"><label>Password</label><input type="password" id="pw" placeholder="Enter password"></div>
<button class="btn bsave" onclick="saveWiFi()">&#x1F4BE; Save &amp; Connect</button>
</div>
</div>
</main>
<div id="toast"></div>
<style>
.btn-sta-on {
    background: linear-gradient(135deg, #1565C0 0%, #0D47A1 100%);
    color: #fff;
    box-shadow: 0 2px 4px rgba(21,101,192,0.3);
}
.btn-sta-on:hover {
    background: linear-gradient(135deg, #1E88E5 0%, #1565C0 100%);
    filter: brightness(1.05);
}
.btn-sta-off {
    background: linear-gradient(135deg, #C62828 0%, #B71C1C 100%);
    color: #fff;
    box-shadow: 0 2px 4px rgba(198,40,40,0.3);
}
.btn-sta-off:hover {
    background: linear-gradient(135deg, #E53935 0%, #C62828 100%);
    filter: brightness(1.05);
}
</style>
<script>
function toast(m,ok=true){
  const t = document.getElementById('toast');
  if(!t) return;
  t.textContent = (typeof m === 'string') ? m : String(m == null ? '' : m);
  t.className = 'show ' + (ok ? 'ok' : 'er');
  clearTimeout(t._t);
  t._t = setTimeout(()=>{ t.className = ''; }, 3000);
}

function fetchWithTimeout(url, options, timeout){
  options = options || {};
  timeout = timeout || 10000;
  const controller = new AbortController();
  const timer = setTimeout(()=>controller.abort(), timeout);
  const opts = Object.assign({}, options, {signal: controller.signal});
  return fetch(url, opts).then(
    r => { clearTimeout(timer); return r; },
    e => { clearTimeout(timer); throw e; }
  );
}

function tick(){
  if(tick._inflight) return;
  tick._inflight = true;
  fetchWithTimeout('/api/time', {}, 8000).then(r=>{
    if(!r.ok) throw new Error('HTTP '+r.status);
    return r.json();
  }).then(d=>{
    tick._inflight = false;
    if(!d || typeof d !== 'object' || Array.isArray(d)) return;
    const clkEl = document.getElementById('clk');
    if(clkEl) clkEl.textContent = d.time || '--:--:--';
    const de = document.getElementById('date');
    if(de) de.textContent = (d.weekday ? d.weekday + ' ' : '') + (d.date || '--/--/----');
    const w = document.querySelector('.wd');
    const t = document.querySelector('.td');
    if(w) w.className = 'dot ' + (d.wifi ? 'g' : 'r');
    if(t){
      let tc = 'y';
      const auth = (typeof d.authority === 'string' && d.authority) ? d.authority : d.timeSource;
      if(auth === 'ntp'){
        tc = (d.authorityStale === true) ? 'y' : 'g';
      }
      else if(auth === 'browser') tc = 'y';
      else if(auth === 'ds3231') tc = 'g';
      else if(auth === 'internal_rtc') tc = 'y';
      else if(auth === 'rtc') tc = (d.rtcPresent === true) ? 'g' : 'y';
      else tc = 'y';
      t.className = 'dot ' + tc;
    }
  }).catch(()=>{ tick._inflight = false; });
}
setInterval(tick,1000);tick();

function updateStaButton(enabled) {
    const btn = document.getElementById('staToggleBtn');
    if(!btn) return;
    const on = !!enabled;
    btn.dataset.enabled = on ? 'true' : 'false';
    if(on) {
        btn.textContent = '✓ ON';
        btn.className = 'btn btn-sta-on';
    } else {
        btn.textContent = '✗ OFF';
        btn.className = 'btn btn-sta-off';
    }
    btn.style.minWidth = '90px';
}

function isStaEnabled() {
    const btn = document.getElementById('staToggleBtn');
    return !!(btn && btn.dataset && btn.dataset.enabled === 'true');
}

function loadWiFiStatus() {
    if(loadWiFiStatus._inflight) return;
    loadWiFiStatus._inflight = true;
    fetchWithTimeout('/api/wifi', {}, 8000).then(r=>{
        if(!r.ok) throw new Error('HTTP '+r.status);
        return r.json();
    }).then(d=>{
        loadWiFiStatus._inflight = false;
        if(!d || typeof d !== 'object' || Array.isArray(d)) return;
        const ssidEl = document.getElementById('ssid');
        const active = document.activeElement;
        const serverSSID = d.ssid || '';
        if (ssidEl && active !== ssidEl && ssidEl.value !== serverSSID) {
            ssidEl.value = serverSSID;
        }
        window._lastSSID = serverSSID;
        const staEnabled = d.sta_enabled !== undefined ? d.sta_enabled : true;
        updateStaButton(staEnabled);
        const stationDiv = document.getElementById('stationConfig');
        if(stationDiv) {
            stationDiv.style.opacity = staEnabled ? '1' : '0.5';
            const inputs = stationDiv.querySelectorAll('input, button');
            inputs.forEach(inp => inp.disabled = !staEnabled);
        }
        const s = document.getElementById('status');
        if(!s) return;
        s.style.display = '';
        s.textContent = '';
        if(staEnabled && d.connected){
            const span1 = document.createElement('span');
            span1.style.fontWeight = '700';
            span1.textContent = '✓ Connected';
            s.appendChild(span1);
            s.appendChild(document.createTextNode(' to: '));
            const strong = document.createElement('strong');
            strong.textContent = d.ssid || '';
            s.appendChild(strong);
            s.appendChild(document.createTextNode(' ('+(d.ip||'')+') '));
            const bars = document.createElement('span');
            bars.innerHTML = rssiBar(d.rssi||0);
            s.appendChild(bars);
            s.appendChild(document.createTextNode(' '+(d.rssi||0)+'dBm'));
            s.className = 'alert ai';
        } else if(staEnabled && !d.connected) {
            const b1 = document.createElement('span');
            b1.style.fontWeight = '700';
            b1.textContent = '⚠ Not Connected';
            s.appendChild(b1);
            s.appendChild(document.createElement('br'));
            s.appendChild(document.createTextNode('Configure SSID/password above to connect to a WiFi network.'));
            s.className = 'alert aw';
        } else {
            const b1 = document.createElement('span');
            b1.style.fontWeight = '700';
            b1.textContent = '⛔ WiFi Station Disabled';
            s.appendChild(b1);
            s.appendChild(document.createElement('br'));
            s.appendChild(document.createTextNode('ESP32S3 running in AP-only mode. Enable above to connect to WiFi networks for NTP time sync.'));
            s.className = 'alert aw';
        }
    }).catch(()=>{
        loadWiFiStatus._inflight = false;
        const s = document.getElementById('status');
        if(s){
            s.style.display = '';
            s.textContent = '';
            const b1 = document.createElement('span');
            b1.style.fontWeight = '700';
            b1.textContent = '⚠ Could not refresh WiFi status';
            s.appendChild(b1);
            s.className = 'alert aw';
        }
    });
}

function rssiBar(rssi){
    const r = (typeof rssi === 'number' && isFinite(rssi)) ? rssi : -100;
    const b = r >= -50 ? 4 : r >= -60 ? 3 : r >= -70 ? 2 : 1;
    let s = '<span class="bars">';
    for(let i = 1; i <= 4; i++){
        s += '<span class="bar' + (i <= b ? ' on' : '') + '" style="height:' + (i*3+2) + 'px"></span>';
    }
    return s + '</span>';
}

function toggleStation() {
    const btn = document.getElementById('staToggleBtn');
    if(!btn || btn.disabled) return;
    const wasEnabled = isStaEnabled();
    const willEnable = !wasEnabled;
    btn.disabled = true;
    btn.textContent = '...';
    fetchWithTimeout('/api/wifi', {
        method:'POST',
        headers:{'Content-Type':'application/json'},
        body:JSON.stringify({sta_enabled: willEnable})
    }, 10000).then(r=>{
        if(!r.ok) throw new Error('HTTP '+r.status);
        return r.json();
    }).then(d=>{
        btn.disabled = false;
        if(!d || typeof d !== 'object' || Array.isArray(d)){
            toast('Failed to toggle WiFi station', false);
            updateStaButton(wasEnabled);
            return;
        }
        if(d.success) {
            updateStaButton(willEnable);
            if(!willEnable) {
                window._staDisabled = true;
                if(scanTimer){ clearInterval(scanTimer); scanTimer=null; }
                if(pollScan){ pollScan._inflight = false; }
                if(scanning) endScan();
                const nlReset = document.getElementById('netlist');
                if(nlReset){ nlReset.innerHTML=''; nlReset.style.display='none'; }
                toast('WiFi Station OFF \u2014 NTP unavailable; use RTC/Browser sync', false);
            } else {
                window._staDisabled = false;
                toast('WiFi Station ON', true);
            }
            const stationDiv = document.getElementById('stationConfig');
            if(stationDiv) {
                stationDiv.style.opacity = willEnable ? '1' : '0.5';
                const inputs = stationDiv.querySelectorAll('input, button');
                inputs.forEach(inp => inp.disabled = !willEnable);
            }
            loadWiFiStatus();
        } else {
            toast('Failed to toggle WiFi station', false);
            updateStaButton(wasEnabled);
        }
    }).catch(()=>{
        btn.disabled = false;
        toast('Error toggling WiFi station', false);
        updateStaButton(wasEnabled);
        setTimeout(loadWiFiStatus, 1000);
    });
}

let scanTimer=null,scanning=false;
function startScan(){
    if(!isStaEnabled()) {
        toast('Please enable WiFi Station Mode first', false);
        return;
    }
    window._staDisabled = false;
    if(scanning)return;
    scanning=true;
    const scanBtn = document.getElementById('scanBtn');
    if(scanBtn){
        scanBtn.textContent = '\u23F3 Scanning\u2026';
        scanBtn.disabled = true;
    }
    const nl = document.getElementById('netlist');
    if(nl){
        nl.style.display = 'block';
        nl.innerHTML = '<div style="text-align:center;color:#90A4AE;padding:12px;font-size:13px">📡 Scanning for networks...</div>';
    }
    fetchWithTimeout('/api/wifi/scan', {method:'POST'}, 10000)
    .then(r => {
        if(!r.ok) throw new Error('HTTP '+r.status);
        return r.json();
    })
    .then(()=>{
        if(scanTimer){ clearInterval(scanTimer); scanTimer = null; }
        pollScan._inflight = false;
        scanTimer = setInterval(pollScan, 2500);
    })
    .catch(()=>{
        endScan();
        toast('Scan failed \u2014 enable WiFi Station', false);
        const nl2 = document.getElementById('netlist');
        if(nl2){ nl2.innerHTML = ''; nl2.style.display = 'none'; }
    });
}
function pollScan(){
    if(pollScan._inflight) return;
    pollScan._inflight = true;
    fetchWithTimeout('/api/wifi/scan', {}, 10000).then(r=>{
        if(!r.ok) throw new Error('HTTP '+r.status);
        return r.json();
    }).then(d=>{
        pollScan._inflight = false;
        if(window._staDisabled){ return; }
        if(!d || typeof d !== 'object' || Array.isArray(d)) return;
        if(!d.scanning){
            if(scanTimer){ clearInterval(scanTimer); scanTimer=null; }
            endScan();
            const nets = Array.isArray(d.networks) ? d.networks : [];
            renderNets(nets);
        }
    }).catch(()=>{
        pollScan._inflight = false;
        if(scanTimer){ clearInterval(scanTimer); scanTimer=null; }
        endScan();
        toast('Scan poll failed', false);
    });
}
function endScan(){
    scanning = false;
    const btn = document.getElementById('scanBtn');
    if(btn){
        btn.textContent = '\uD83D\uDCF6 Scan';
        btn.disabled = false;
    }
}
function renderNets(nets){
    const nl = document.getElementById('netlist');
    if(!nl) return;
    if(!Array.isArray(nets) || nets.length === 0){
        nl.innerHTML = '<div style="color:#90A4AE;text-align:center;padding:8px;font-size:13px">No networks found. Make sure WiFi is enabled.</div>';
        return;
    }
    const frag=document.createDocumentFragment();
    const hdr=document.createElement('div');hdr.className='net-hdr';hdr.textContent='Available Networks';frag.appendChild(hdr);
    const sorted = nets.slice().sort((a,b)=>(b.rssi||0)-(a.rssi||0));
    sorted.forEach(n=>{
        const d=document.createElement('div');d.className='netitem';
        const ns=document.createElement('span');ns.className='ns';ns.textContent=n.ssid;
        const nr=document.createElement('span');nr.className='nr';nr.textContent=(n.rssi||0)+'dBm';
        const bar=document.createElement('span');bar.innerHTML=rssiBar(n.rssi||0);
        const lock=document.createElement('span');lock.style.fontSize='13px';lock.textContent=n.enc?'\uD83D\uDD12':'';
        d.appendChild(ns);d.appendChild(nr);d.appendChild(bar);d.appendChild(lock);
        d.addEventListener('click',()=>{
            const ssidEl=document.getElementById('ssid');
            const pwEl=document.getElementById('pw');
            if(ssidEl && ssidEl.value !== n.ssid && pwEl) pwEl.value='';
            if(ssidEl) ssidEl.value=n.ssid;
            if(pwEl) pwEl.focus();
        });
        frag.appendChild(d);
    });
    nl.innerHTML='';nl.appendChild(frag);
}
function saveWiFi(){
    if(!isStaEnabled()) {
        toast('Enable WiFi Station Mode first to save credentials', false);
        return;
    }
    const ssidEl = document.getElementById('ssid');
    if(!ssidEl){ toast('SSID field not found', false); return; }
    const ssid = ssidEl.value.trim();
    if(!ssid){ toast('SSID required', false); return; }
    try {
        if(new TextEncoder().encode(ssid).length > 31){ toast('SSID must be 31 bytes or less', false); return; }
    } catch(e) {
        if(ssid.length > 31){ toast('SSID must be 31 characters or less', false); return; }
    }
    const btn = document.querySelector('.bsave');
    if(!btn || btn.disabled) return;
    btn.disabled = true;
    btn.textContent = 'Saving...';
    const body = {ssid: ssid};
    const pwEl = document.getElementById('pw');
    const pwVal = pwEl ? pwEl.value : '';
    if (pwVal.length > 0) body.password = pwVal;
    const lastKnownSSID = (typeof window._lastSSID === 'string') ? window._lastSSID : '';
    const ssidChanged = (ssid !== lastKnownSSID);
    fetchWithTimeout('/api/wifi', {
        method:'POST',
        headers:{'Content-Type':'application/json'},
        body:JSON.stringify(body)
    }, 15000)
    .then(r=>{
        if(!r.ok) return r.json().catch(()=>({error:'HTTP '+r.status})).then(e=>Promise.reject(e));
        return r.json();
    }).then(d=>{
        btn.disabled = false;
        btn.innerHTML = '&#x1F4BE; Save &amp; Connect';
        if(!d || typeof d !== 'object' || Array.isArray(d)){
            toast('Error: invalid response', false);
            return;
        }
        if(d.success){
            if(ssidChanged){
                toast('Credentials saved! Reconnecting...');
                setTimeout(()=>{ window.location.href='/'; }, 3000);
            } else {
                toast('Settings saved.');
            }
        } else {
            toast('Failed: '+(d.error||'Unknown'), false);
        }
    }).catch((e)=>{
        btn.disabled = false;
        btn.innerHTML = '&#x1F4BE; Save &amp; Connect';
        toast('Error: '+((e && e.error) || 'request failed'), false);
    });
}
loadWiFiStatus();
</script></body></html>)raw";

// =============================================================================
//  NTP PAGE
// =============================================================================
const char ntp_html[] PROGMEM = R"raw(<!DOCTYPE html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Time</title>
<link rel="stylesheet" href="/style.css"></head><body>
<header>
<nav>
<a href="/">Relays</a>
<a href="/wifi">WiFi</a>
<a href="/ntp" class="cur">Time</a>
<a href="/ap">AP</a>
<a href="/gpio">GPIO</a>
<a href="/system">System</a>
</nav>
<div class="hdr-r"><span class="dot wd"></span><span class="dot td"></span>&nbsp;<span id="clk">--:--:--</span><span id="date" style="font-size:11px;opacity:.85;margin-left:6px;white-space:nowrap">--/--/----</span></div>
</header>
<main>
<p class="ptitle">Time &amp; NTP Settings</p>
<div class="card fcrd">
<div id="timeStatus" class="alert ai" style="display:none;margin-bottom:12px"></div>
<div class="fg">
<label>Timezone</label>
<select id="tz">
<option value="">Loading&hellip;</option>
</select>
<small id="tzInfo">Auto-DST handled by firmware. Transitions detected within 5 seconds.</small>
</div>
<div class="fg">
<label>Manual GMT Offset (seconds)</label>
<input type="number" id="gmt" readonly style="background:#ECEFF1;color:#607D8B">
<small>Read-only when a named timezone is selected. Select &quot;UTC&quot; and enter manual offset to override.</small>
</div>
<div class="fg">
<label>NTP Server</label>
<input type="text" id="srv" placeholder="time.google.com" required>
<small>Fallbacks: time.google.com &rarr; time.windows.com &rarr; time.cloudflare.com &rarr; time.facebook.com</small>
</div>
<div class="fg"><label>Auto Sync (hours, 1&ndash;24)</label><input type="number" id="shi" min="1" max="24" value="1"></div>
<div style="display:flex;gap:8px;flex-wrap:wrap;align-items:stretch">
<button class="btn bsave" style="flex:1;min-width:140px;margin-top:0;height:40px;padding:0 12px;background:#1565C0;color:#fff;border-radius:6px;font-size:13px;font-weight:600" onclick="save()">&#x1F4BE; Save Settings</button>
<button class="btn bsync" id="sbtn" style="flex:1;min-width:120px;margin-top:0;height:40px;padding:0 12px;background:#1565C0;color:#fff;border-radius:6px;font-size:13px;font-weight:600;white-space:nowrap" onclick="sync()">&#x1F504; Sync NTP</button>
<button class="btn bwarn" id="bsbtn" style="flex:1;min-width:130px;margin-top:0;height:40px;padding:0 12px;background:#1565C0;color:#fff;border-radius:6px;font-size:13px;font-weight:600;white-space:nowrap" onclick="syncFromBrowser()">&#x1F310; Sync Browser</button>
</div>
<small style="display:block;margin-top:12px;color:#90A4AE">Use <strong>Sync Browser</strong> when NTP servers are unreachable. <strong>NTP</strong> will automatically override browser time when available. <strong>DS3231 RTC Module</strong> is the primary time authority and maintains time across power cycles.</small>
</div>
</main>
<div id="toast"></div>
<script>
function toast(m,ok=true){const t=document.getElementById('toast');t.textContent=m;t.className='show '+(ok?'ok':'er');clearTimeout(t._t);t._t=setTimeout(()=>t.className='',3000);}
let lastNtpSyncAge = -1;
function fmtOff(s){const h=Math.floor(Math.abs(s)/3600);const m=Math.floor((Math.abs(s)%3600)/60);return (s<0?'-':'+')+h+(m?':'+String(m).padStart(2,'0'):'');}
function tick(){
  fetch('/api/time').then(r=>{if(!r.ok) throw new Error('HTTP '+r.status);return r.json();}).then(d=>{document.getElementById('clk').textContent=d.time||'--:--:--';const de=document.getElementById('date');if(de)de.textContent=(d.weekday?d.weekday+' ':'')+(d.date||'--/--/----');const w=document.querySelector('.wd'),t=document.querySelector('.td');if(w)w.className='dot '+(d.wifi?'g':'r');if(t){let tc='y';const auth=(typeof d.authority==='string'&&d.authority)?d.authority:d.timeSource;if(auth==='ntp'){tc=(d.authorityStale===true)?'y':'g';}else if(auth==='browser')tc='y';else if(auth==='ds3231')tc='g';else if(auth==='internal_rtc')tc='y';else if(auth==='rtc')tc=(d.rtcPresent===true)?'g':'y';else tc='y';t.className='dot '+tc;}if(typeof d.ntpSyncAge === 'number' && d.ntpSyncAge !== 4294967295 && d.ntpSyncAge >= 0)lastNtpSyncAge=d.ntpSyncAge;updateTimeStatus(d);}).catch(()=>{});}
function updateTimeStatus(d){
  const s=document.getElementById('timeStatus');
  s.style.display='block';
  let icon='',msg='',cls='ai';
  const auth = (typeof d.authority === 'string' && d.authority) ? d.authority : d.timeSource;
  const isStale = (d.authorityStale === true);
  if(auth==='ntp'){
    if(isStale){
      icon='&#x26A0;&#xFE0F;';
      msg='Time source: <strong>NTP Server</strong> — Stale, awaiting resync';
      cls='aw';
    }else{
      icon='&#x2705;';msg='Time source: <strong>NTP Server</strong> — High Accuracy';
      cls='ai';
    }
  }else if(auth==='browser'){
    icon='&#x1F310;';msg='Time source: <strong>Browser Sync</strong> — Moderate Accuracy';
    cls='aw';
  }else if(auth==='ds3231'){
    icon='&#x1F4BF;';msg='Time source: <strong>DS3231 Real Time Clock</strong> — High Accuracy';
    cls='ai';
  }else if(auth==='internal_rtc'){
    icon='&#x1F4BF;';msg='Time source: <strong>Internal RTC (NVS)</strong> — Moderate Accuracy';
    cls='aw';
  }else if(auth==='rtc'){
    if(d.rtcPresent){
      icon='&#x1F4BF;';msg='Time source: <strong>DS3231 Real Time Clock</strong> — High Accuracy';
      cls='ai';
    }else{
      icon='&#x1F4BF;';msg='Time source: <strong>Internal RTC (NVS)</strong> — Moderate Accuracy';
      cls='aw';
    }
  }else{
    icon='&#x26A0;&#xFE0F;';msg='Time source: <strong>None</strong> — Please sync time';
    cls='aw';
  }
  let rtcInfo = '';
  if(d.rtcPresent){
    rtcInfo = '<br><small>✅ DS3231 RTC detected on GPIO8/9';
    if(d.rtcSynced){
      const age = (typeof d.rtcSyncAge === 'number') ? d.rtcSyncAge : -1;
      if(age < 0 || age === 4294967295){
        rtcInfo += ' | Last sync: Never';
      } else {
        rtcInfo += ' | Last sync: ' + age + 's ago';
      }
    }
    rtcInfo += '</small>';
  } else {
    rtcInfo = '<br><small>⚠️ DS3231 RTC not detected</small>';
  }
  let dstInfo = '';
  if(d.tzName){
    const tzSafe = String(d.tzName).replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
    dstInfo = '<br><small>🌍 Timezone: <strong>' + tzSafe + '</strong>';
    if(d.dstActive){
      dstInfo += ' — <span style="color:#E65100;font-weight:700">DST ACTIVE</span>';
    } else {
      dstInfo += ' — Standard Time';
    }
    dstInfo += '</small>';
  }
  s.innerHTML=icon+' '+msg+rtcInfo+dstInfo;
  s.className='alert '+cls;
}
setInterval(tick,1000);tick();

let tzList = [];
function loadTZ(){
  fetch('/api/timezones').then(r=>r.json()).then(d=>{
    tzList = d.timezones || [];
    const sel = document.getElementById('tz');
    sel.innerHTML = '';
    tzList.forEach(z=>{
      const o = document.createElement('option');
      o.value = z.name;
      o.textContent = z.name + ' (UTC' + fmtOff(z.offset) + (z.dst ? ' +DST' : '') + ')';
      sel.appendChild(o);
    });
    const manualOpt = document.createElement('option');
    manualOpt.value = 'Manual';
    manualOpt.textContent = 'Manual Offset (custom GMT)';
    sel.appendChild(manualOpt);
    fetch('/api/ntp').then(r=>r.json()).then(cfg=>{
      if (cfg.tzName && cfg.tzName !== 'Manual' && !tzList.some(z => z.name === cfg.tzName)) {
        const o = document.createElement('option');
        o.value = cfg.tzName;
        o.textContent = cfg.tzName + ' (' + fmtOff(cfg.gmtOffset || 0) + ')';
        sel.appendChild(o);
      }
      sel.value = cfg.tzName || 'UTC';
      const gmtEl = document.getElementById('gmt');
      if(gmtEl && typeof cfg.gmtOffset === 'number') gmtEl.value = cfg.gmtOffset;
      const srvEl = document.getElementById('srv');
      if(srvEl) srvEl.value = (typeof cfg.ntpServer === 'string' && cfg.ntpServer) ? cfg.ntpServer : 'time.google.com';
      const shiEl = document.getElementById('shi');
      if(shiEl) shiEl.value = (typeof cfg.syncHours === 'number' && cfg.syncHours >= 1 && cfg.syncHours <= 24) ? cfg.syncHours : 1;
      updateTzInfo(cfg);
      toggleGmtField();
    }).catch(()=>{});
  }).catch(()=>{
    document.getElementById('tz').innerHTML = '<option value="UTC">UTC</option>';
  });
  const selEl = document.getElementById('tz');
  if(selEl && !selEl._hasTzListener) {
    selEl.addEventListener('change', toggleGmtField);
    selEl._hasTzListener = true;
  }
}
function updateTzInfo(cfg){
  const info = document.getElementById('tzInfo');
  if(cfg.tzAutoDst){
    const dstH = (typeof cfg.dstOffset === 'number') ? (cfg.dstOffset/3600) : 0;
    info.textContent = 'Auto-DST enabled. Current DST: ' +
      (cfg.dstActive ? 'ACTIVE (+' + dstH + 'h)' : 'inactive') +
      '. Transitions detected within 5 seconds.';
  } else {
    info.textContent = 'Manual offset mode. DST not applied automatically.';
  }
  const gmtEl = document.getElementById('gmt');
  if(gmtEl && typeof cfg.gmtOffset === 'number') gmtEl.value = cfg.gmtOffset;
}

function toggleGmtField(){
  const tzEl = document.getElementById('tz');
  const gmtEl = document.getElementById('gmt');
  const info = document.getElementById('tzInfo');
  if(!tzEl || !gmtEl) return;
  const tzVal = tzEl.value;
  const isManual = (tzVal === 'Manual' || tzVal === '');
  gmtEl.readOnly = !isManual;
  gmtEl.style.background = isManual ? '#fff' : '#ECEFF1';
  gmtEl.style.color = isManual ? '#1A1A2E' : '#607D8B';
  if(isManual && info){
    info.textContent = 'Manual offset mode. Enter GMT offset in seconds (e.g. 20700 = UTC+5:45). DST not applied automatically.';
  }
}
function save(){
  const h=parseInt(document.getElementById('shi').value);
  if(isNaN(h)||h<1||h>24){toast('Sync interval must be 1\u201324 h',false);return;}
  const srvVal = document.getElementById('srv').value.trim();
  if(srvVal.length === 0){toast('NTP server cannot be empty', false); return;}
  if(srvVal.length >= 48){toast('NTP server must be 47 characters or less', false); return;}
  const body = {
    ntpServer: srvVal,
    syncHours: h
  };
  const tzVal = document.getElementById('tz').value;
  if(tzVal === 'Manual' || tzVal === ''){
    const gmtVal = parseInt(document.getElementById('gmt').value);
    if(isNaN(gmtVal) || gmtVal < -50400 || gmtVal > 50400){
      toast('GMT offset must be between -50400 and 50400 seconds', false);
      return;
    }
    body.gmtOffset = gmtVal;
  } else if(tzVal && tzVal.length > 0){
    body.tzName = tzVal;
  }
  fetch('/api/ntp',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})
    .then(r=>r.json()).then(d=>{
      if(d.success){
        toast('NTP settings saved!');
        loadTZ();
        tick();
      } else {
        toast('Failed: '+d.error,false);
      }
    }).catch(()=>toast('Error',false));
}
function sync(){
  const b=document.getElementById('sbtn');
  b.disabled=true;b.textContent='Syncing\u2026';
  const preAge = lastNtpSyncAge;
  const startTs = Date.now();
  fetch('/api/ntp/sync',{method:'POST'}).then(r=>r.json()).then(d=>{
    if(!d.success){
      b.disabled=false;b.innerHTML='&#x1F504; Sync NTP';
      toast('Sync failed \u2014 ' + (d.error || 'check WiFi/NTP'), false);
      return;
    }
    toast('NTP sync initiated\u2026');
    let attempts = 0;
    const poll = setInterval(()=>{
      attempts++;
      fetch('/api/time').then(r=>r.json()).then(t=>{
        const age = (typeof t.ntpSyncAge !== 'number' || t.ntpSyncAge === 4294967295 || t.ntpSyncAge < 0) ? -1 : t.ntpSyncAge;
        const preAgeSafe = (typeof preAge === 'number' && preAge >= 0) ? preAge : -1;
        const isFresh = (t.timeSource === 'ntp') && (age >= 0) && (age < 15);
        const isNewer = (preAgeSafe < 0) || (age >= 0 && age < preAgeSafe);
        if(isFresh && isNewer){
          clearInterval(poll);
          b.disabled=false;b.innerHTML='&#x1F504; Sync NTP';
          toast('Time synced via NTP!');
          tick();
        } else if(attempts >= 20 || (Date.now() - startTs) > 35000){
          clearInterval(poll);
          b.disabled=false;b.innerHTML='&#x1F504; Sync NTP';
          toast('NTP sync did not complete \u2014 check network', false);
        }
      }).catch(()=>{});
    }, 1000);
  }).catch(()=>{
    b.disabled=false;b.innerHTML='&#x1F504; Sync NTP';
    toast('Error',false);
  });
}
function syncFromBrowser(){
  const b=document.getElementById('bsbtn');
  b.disabled=true;
  b.textContent='Syncing from browser\u2026';
  const utcEpoch = Math.floor(Date.now() / 1000);
  fetch('/api/time/browser-sync', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({utc_epoch: utcEpoch})
  })
  .then(r => r.json())
  .then(d => {
    b.disabled = false;
    b.innerHTML = '&#x1F310; Sync Browser';
    if (d.success) {
      const clkEl = document.getElementById('clk');
      const dateEl = document.getElementById('date');
      const safeTime = (typeof d.local_time === 'string' && d.local_time) ? d.local_time : '--:--:--';
      const safeDate = (typeof d.local_date === 'string' && d.local_date) ? d.local_date : '--/--/----';
      const safeWday = (typeof d.weekday === 'string' && d.weekday) ? d.weekday : '';
      if(clkEl) clkEl.textContent = safeTime;
      if(dateEl) dateEl.textContent = (safeWday ? safeWday + ' ' : '') + safeDate;
      let msg = 'Time synced from browser! Local time: ' + safeTime + (safeDate !== '--/--/----' ? ' ' + safeDate : '');
      if(d.rtc_synced) msg += ' | DS3231 updated';
      toast(msg);
      tick();
    } else {
      toast('Sync failed: ' + (d.error || 'Unknown error'), false);
    }
  })
  .catch(() => {
    b.disabled = false;
    b.innerHTML = '&#x1F310; Sync Browser';
    toast('Error syncing time from browser', false);
  });
}
loadTZ();
</script></body></html>)raw";

// =============================================================================
//  AP PAGE
// =============================================================================
const char ap_html[] PROGMEM = R"raw(<!DOCTYPE html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>AP</title>
<link rel="stylesheet" href="/style.css"></head><body>
<header>
<nav>
<a href="/">Relays</a>
<a href="/wifi">WiFi</a>
<a href="/ntp">Time</a>
<a href="/ap" class="cur">AP</a>
<a href="/gpio">GPIO</a>
<a href="/system">System</a>
</nav>
<div class="hdr-r"><span class="dot wd"></span><span class="dot td"></span>&nbsp;<span id="clk">--:--:--</span><span id="date" style="font-size:11px;opacity:.85;margin-left:6px;white-space:nowrap">--/--/----</span></div>
</header>
<main>
<p class="ptitle">Access Point Settings</p>
<div class="card fcrd">
<form onsubmit="return false" autocomplete="off">
<div class="alert aw">&#x26A0;&#xFE0F; Changing SSID, Password, Channel or Visibility will restart the AP and disconnect all clients. Reconnect to the new SSID afterward.</div>
<div class="fg"><label>AP SSID</label><input type="text" id="ssid" maxlength="31" required></div>
<div class="fg"><label>AP Password</label><input type="password" id="pw" minlength="8" maxlength="31" placeholder="Leave blank to keep current password"></div>
<div class="fg" style="margin-top:-6px"><label style="display:flex;align-items:center;gap:8px;cursor:pointer;text-transform:none;font-size:13px;font-weight:500;color:#1A1A2E"><input type="checkbox" id="clearPw" style="width:auto;margin:0"> Remove password (make AP open)</label></div>
<div class="fg">
<label>Channel (1&ndash;13)</label>
<select id="ch">
<option value="1">1</option><option value="2">2</option><option value="3">3</option>
<option value="4">4</option><option value="5">5</option><option value="6">6 (default)</option>
<option value="7">7</option><option value="8">8</option><option value="9">9</option>
<option value="10">10</option><option value="11">11</option><option value="12">12</option>
<option value="13">13</option>
</select>
<small>Lower interference: pick a channel not used by nearby networks.</small>
</div>
<div class="fg">
<label>SSID Visibility</label>
<select id="hidden">
<option value="0">Visible (broadcast SSID)</option>
<option value="1">Hidden (do not broadcast)</option>
</select>
</div>
<button type="button" class="btn bsave" onclick="save()" id="apSaveBtn">&#x1F4BE; Save Settings</button>
</form>
</div>
</main>
<div id="toast"></div>
<script>
function toast(m,ok=true){
  const t=document.getElementById('toast');
  if(!t) return;
  t.textContent = (typeof m === 'string') ? m : String(m == null ? '' : m);
  t.className='show '+(ok?'ok':'er');
  clearTimeout(t._t);
  t._t=setTimeout(()=>t.className='',3000);
}
function escapeHtml(text){const d=document.createElement('div');d.textContent=String(text==null?'':text);return d.innerHTML;}

function fetchWithTimeout(url, options, timeout){
  options = options || {};
  timeout = timeout || 10000;
  const controller = new AbortController();
  const timer = setTimeout(()=>controller.abort(), timeout);
  const opts = Object.assign({}, options, {signal: controller.signal});
  return fetch(url, opts).then(
    r => { clearTimeout(timer); return r; },
    e => { clearTimeout(timer); throw e; }
  );
}

let savingAP = false;
let tickIntervalId = null;

function tick(){
  if(tick._inflight) return;
  tick._inflight = true;
  fetchWithTimeout('/api/time', {}, 8000).then(r=>{
    if(!r.ok) throw new Error('HTTP '+r.status);
    return r.json();
  }).then(d=>{
    tick._inflight = false;
    if(!d || typeof d !== 'object' || Array.isArray(d)) return;
    const clkEl = document.getElementById('clk');
    if(clkEl) clkEl.textContent = d.time || '--:--:--';
    const de = document.getElementById('date');
    if(de) de.textContent = (d.weekday?d.weekday+' ':'')+(d.date||'--/--/----');
    const w = document.querySelector('.wd'), t = document.querySelector('.td');
    if(w) w.className='dot '+(d.wifi?'g':'r');
    if(t){
      let tc='y';
      const auth = (typeof d.authority === 'string' && d.authority) ? d.authority : d.timeSource;
      if(auth==='ntp'){
        tc = (d.authorityStale === true) ? 'y' : 'g';
      }
      else if(auth==='browser')tc='y';
      else if(auth==='ds3231')tc='g';
      else if(auth==='internal_rtc')tc='y';
      else if(auth==='rtc')tc=(d.rtcPresent === true) ? 'g' : 'y';
      else tc='y';
      t.className='dot '+tc;
    }
  }).catch(()=>{ tick._inflight = false; });
}
tickIntervalId = setInterval(tick,1000);
tick();

fetchWithTimeout('/api/ap', {}, 8000).then(r=>{
  if(!r.ok) throw new Error('HTTP '+r.status);
  return r.json();
}).then(d=>{
  if(!d || typeof d !== 'object' || Array.isArray(d)) return;
  const ssidEl = document.getElementById('ssid');
  if(ssidEl) ssidEl.value = d.ap_ssid || '';
  const chEl = document.getElementById('ch');
  if(chEl) chEl.value = d.ap_channel || 6;
  const hiddenEl = document.getElementById('hidden');
  if(hiddenEl) hiddenEl.value = d.ap_hidden ? '1' : '0';
  const pwEl = document.getElementById('pw');
  const clearEl = document.getElementById('clearPw');
  if(pwEl) pwEl.addEventListener('input', function(){ if(this.value.length > 0 && clearEl) clearEl.checked = false; });
  if(clearEl) clearEl.addEventListener('change', function(){ if(this.checked && pwEl) pwEl.value = ''; });
}).catch(()=>{
  toast('Could not load AP settings \u2014 check connection', false);
});

function save(){
  if(savingAP) return;
  const btn = document.getElementById('apSaveBtn') || document.querySelector('.bsave');

  const ssidEl = document.getElementById('ssid');
  if(!ssidEl){ toast('SSID field not found', false); return; }
  const ssid = (ssidEl.value || '').trim();
  if(!ssid){ toast('SSID cannot be empty', false); return; }
  if(ssid.length > 31){ toast('SSID must be 31 characters or less', false); return; }
  try {
    if(new TextEncoder().encode(ssid).length > 31){ toast('SSID too long (max 31 bytes)', false); return; }
  } catch(e) {}

  const pwEl = document.getElementById('pw');
  const pw = pwEl ? (pwEl.value || '') : '';
  const clearPwEl = document.getElementById('clearPw');
  const shouldClear = !!(clearPwEl && clearPwEl.checked);

  let pwBytes = pw.length;
  try { pwBytes = new TextEncoder().encode(pw).length; } catch(e) {}

  if(!shouldClear && pw.length > 0 && pw.length < 8){
    toast('Password must be 8+ characters or blank', false);
    return;
  }
  if(pwBytes > 31){
    toast('Password must be 31 bytes or less', false);
    return;
  }
  if(shouldClear && pw.length > 0){
    toast('Uncheck "Remove password" or clear the password field', false);
    return;
  }

  const chEl = document.getElementById('ch');
  const hiddenEl = document.getElementById('hidden');
  const body = {
    ap_ssid: ssid,
    ap_channel: chEl ? (parseInt(chEl.value) || 6) : 6,
    ap_hidden: hiddenEl ? (hiddenEl.value === '1') : false
  };

  if(shouldClear){
    body.ap_password = '';
  } else if(pw.length > 0){
    body.ap_password = pw;
  }

  savingAP = true;
  if(btn){ btn.disabled = true; btn.textContent = 'Saving...'; }

  let controller;
  let timeoutId;
  try {
    controller = new AbortController();
    timeoutId = setTimeout(() => controller.abort(), 15000);
  } catch(err) {
    savingAP = false;
    if(btn){ btn.disabled = false; btn.innerHTML = '&#x1F4BE; Save Settings'; }
    toast('Error: browser does not support required features', false);
    return;
  }

  fetch('/api/ap', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(body),
    signal: controller.signal
  })
  .then(r => {
    if(timeoutId !== undefined) clearTimeout(timeoutId);
    if(!r.ok){
      return r.json().then(e => Promise.reject(e)).catch(() => Promise.reject({error:'HTTP '+r.status}));
    }
    return r.json();
  })
  .then(d => {
    savingAP = false;
    if(!d || typeof d !== 'object' || Array.isArray(d)){
      toast('Error: invalid response from device', false);
      if(btn){ btn.disabled = false; btn.innerHTML = '&#x1F4BE; Save Settings'; }
      return;
    }
    if(d.success){
      if(d.restarted){
        if(tickIntervalId !== null){ clearInterval(tickIntervalId); tickIntervalId = null; }
        toast('AP restarted \u2014 reconnect to new network', false);
        document.body.innerHTML = '<div style="text-align:center;padding:60px 20px;font-family:-apple-system,BlinkMacSystemFont,sans-serif;background:#EEF2F7;min-height:100vh;box-sizing:border-box">'
          + '<div style="background:#fff;max-width:420px;margin:0 auto;border-radius:12px;padding:32px 24px;box-shadow:0 4px 20px rgba(0,0,0,.1)">'
          + '<h2 style="color:#1565C0;margin:0 0 16px;font-size:20px">\u2705 AP Settings Saved</h2>'
          + '<p style="font-size:15px;color:#37474F;margin:0 0 20px">The Access Point has been restarted.</p>'
          + '<p style="font-size:14px;color:#607D8B;margin:0 0 8px">Reconnect to:</p>'
          + '<p style="font-size:18px;font-weight:700;color:#1565C0;margin:0 0 20px;word-break:break-word">' + escapeHtml(ssid) + '</p>'
          + '<p style="font-size:14px;color:#607D8B;margin:0 0 8px">Then open:</p>'
          + '<p style="font-size:16px;font-weight:700;margin:0"><a href="http://192.168.4.1/" style="color:#1565C0;text-decoration:none">192.168.4.1</a></p>'
          + '</div></div>';
      } else {
        toast('Settings saved!');
        if(btn){ btn.disabled = false; btn.innerHTML = '&#x1F4BE; Save Settings'; }
      }
    } else {
      toast('Failed: ' + (d.error || 'Unknown error'), false);
      if(btn){ btn.disabled = false; btn.innerHTML = '&#x1F4BE; Save Settings'; }
    }
  })
  .catch((e) => {
    if(timeoutId !== undefined) clearTimeout(timeoutId);
    savingAP = false;
    if(e && e.name === 'AbortError'){
      toast('Request timed out \u2014 AP may have restarted. Check your network.', false);
    } else if(e && (e.error || e.message)){
      const msg = e.error || e.message || 'request failed';
      if(typeof msg === 'string' && (msg.indexOf('Failed to fetch') >= 0 || msg.indexOf('NetworkError') >= 0)){
        toast('Network error \u2014 AP may have restarted. Check your network.', false);
      } else {
        toast('Error: ' + msg, false);
      }
    } else {
      toast('Error: request failed \u2014 AP may have restarted.', false);
    }
    if(btn){ btn.disabled = false; btn.innerHTML = '&#x1F4BE; Save Settings'; }
  });
}
</script></body></html>)raw";

// =============================================================================
//  GPIO PAGE
// =============================================================================
const char gpio_html[] PROGMEM = R"raw(<!DOCTYPE html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>GPIO</title>
<link rel="stylesheet" href="/style.css"></head><body>
<header>
<nav>
<a href="/">Relays</a>
<a href="/wifi">WiFi</a>
<a href="/ntp">Time</a>
<a href="/ap">AP</a>
<a href="/gpio" class="cur">GPIO</a>
<a href="/system">System</a>
</nav>
<div class="hdr-r"><span class="dot wd"></span><span class="dot td"></span>&nbsp;<span id="clk">--:--:--</span><span id="date" style="font-size:11px;opacity:.85;margin-left:6px;white-space:nowrap">--/--/----</span></div>
</header>
<main>
<p class="ptitle">GPIO Pin Configuration</p>

<div class="card fcrd">
<div class="alert ai">Configure which GPIO pins control your relays and set active level. Changes take effect immediately.</div>

<div class="fg">
<label>Global Active Level Mode</label>
<select id="globalMode" onchange="saveGlobalMode()">
<option value="0">Per-Relay Configuration (Individual)</option>
<option value="1">Global: All Relays Active LOW</option>
<option value="2">Global: All Relays Active HIGH</option>
</select>
<small>Global mode overrides individual relay active level settings. Active LOW means relay activates when pin is LOW. Active HIGH means relay activates when pin is HIGH.</small>
</div>

<hr>

<div class="fg">
<label>Add New Relay Pin</label>
<div class="input-row">
<select id="newPin">
<option value="">Select GPIO...</option>
</select>
<button class="btn bsave" onclick="addPin()" style="margin-top:0;width:auto;padding:9px 20px">+ Add Relay</button>
</div>
<small>Only available GPIOs are shown.</small>
</div>

<hr>

<div id="pinList">
<h3 style="margin-bottom:10px">Configured Relays (<span id="relayCount">0</span>)</h3>
<div id="pins"></div>
</div>

<div style="margin-top:16px">
<button class="btn bsave" onclick="resetDefaults()" style="margin-top:0;width:100%;padding:9px 20px;background:#1565C0;color:#fff;font-size:13px;font-weight:600;border-radius:6px">&#x1F504; Reset to Default</button>
</div>
</div>
</main>
<div id="toast"></div>
<script>
function toast(m,ok=true){const t=document.getElementById('toast');t.textContent=m;t.className='show '+(ok?'ok':'er');clearTimeout(t._t);t._t=setTimeout(()=>t.className='',3000);}
function tick(){fetch('/api/time').then(r=>{if(!r.ok) throw new Error('HTTP '+r.status);return r.json();}).then(d=>{document.getElementById('clk').textContent=d.time||'--:--:--';const de=document.getElementById('date');if(de)de.textContent=(d.weekday?d.weekday+' ':'')+(d.date||'--/--/----');const w=document.querySelector('.wd'),t=document.querySelector('.td');if(w)w.className='dot '+(d.wifi?'g':'r');if(t){let tc='y';const auth=(typeof d.authority==='string'&&d.authority)?d.authority:d.timeSource;if(auth==='ntp'){tc=(d.authorityStale===true)?'y':'g';}else if(auth==='browser')tc='y';else if(auth==='ds3231')tc='g';else if(auth==='internal_rtc')tc='y';else if(auth==='rtc')tc=(d.rtcPresent===true)?'g':'y';else tc='y';t.className='dot '+tc;}}).catch(()=>{});}
setInterval(tick,1000);tick();
// change gpio
const DEFAULT_PINS = [4,5,6,7,11,12,13,14,1,2,42,41,47,21,20,19];
let gpioData = null;
function saveGlobalMode() {
    const mode = parseInt(document.getElementById('globalMode').value);
    fetch('/api/gpio/global-mode', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({mode: mode})
    }).then(r=>r.json()).then(d=>{
        if(d.success) {
            toast('Global active mode updated!');
            loadGPIO(); 
        } else {
            toast('Failed: '+d.error, false);
        }
    }).catch(()=>toast('Error', false));
}

function renderGPIOPins(d) {
    const globalMode = parseInt(document.getElementById('globalMode').value);
    const isGlobalMode = (globalMode === 1 || globalMode === 2);
    const globalModeText = (globalMode === 1) ? 'GLOBAL ACTIVE LOW' : (globalMode === 2) ? 'GLOBAL ACTIVE HIGH' : null;
    
    let pinsHtml = '';
    if(isGlobalMode) {
        pinsHtml = `<div style="background:#FFF3E0;padding:10px;border-radius:7px;margin-bottom:12px;border-left:4px solid #F9A825">
            <strong style="color:#E65100">⚠️ Global Mode Active:</strong> ${globalModeText}<br>
            <small style="color:#BF360C">Individual active level settings are currently overridden. Switch to "Per-Relay Configuration" to use individual settings.</small>
        </div>`;
    }
    
    for(let i=0; i<d.count; i++) {
        const activeLow = d.activeLow ? d.activeLow[i] : true;
        pinsHtml += `<div style="display:flex;align-items:center;gap:10px;padding:10px;border:1px solid #E3E8EF;border-radius:7px;margin-bottom:6px;background:#FAFAFA">
            <span style="font-weight:700;min-width:60px">Relay ${i+1}</span>
            <span style="flex:1">GPIO <strong>${d.pins[i]}</strong></span>
            <span style="flex:1;font-size:12px">Active: <strong>${activeLow ? 'LOW' : 'HIGH'}</strong></span>
            <button class="btn ${activeLow ? 'bon-b' : 'boff-b'}" 
                onclick="toggleActiveLow(${i})" 
                style="padding:5px 10px;font-size:11px" 
                title="Toggle active level"
                ${isGlobalMode ? 'disabled' : ''}>
                ${activeLow ? 'Set HIGH' : 'Set LOW'}
            </button>
            <button class="btn boff-b" onclick="deletePin(${i})" style="padding:5px 10px;font-size:11px" title="Remove this relay">&#x1F5D1; Remove</button>
        </div>`;
    }
    if(d.count === 0) pinsHtml += '<div style="color:#90A4AE;text-align:center;padding:20px">No relays configured. Add some using the dropdown above.</div>';
    if(d.count >= 16) pinsHtml += '<div style="color:#E65100;text-align:center;padding:10px;font-size:12px">Maximum 16 relays reached.</div>';
    document.getElementById('pins').innerHTML = pinsHtml;
    
    const select = document.getElementById('newPin');
    select.innerHTML = '<option value="">Select GPIO...</option>';
    const usedPins = new Set(d.pins.slice(0, d.count));
    d.availablePins.forEach(pin => {
        if(!usedPins.has(pin)) {
            select.innerHTML += `<option value="${pin}">GPIO ${pin}</option>`;
        }
    });
    if(select.options.length === 1) {
        select.innerHTML += '<option value="" disabled>All available pins in use</option>';
    }
}

function loadGPIO() {
    fetch('/api/gpio').then(r=>r.json()).then(d=>{
        gpioData = d;
        document.getElementById('relayCount').textContent = d.count;
        
        fetch('/api/gpio/global-mode').then(r=>r.json()).then(data=>{
            if(data.mode !== undefined) {
                document.getElementById('globalMode').value = data.mode;
            }
            renderGPIOPins(d);
        }).catch(()=>{
            renderGPIOPins(d);
        });
    }).catch(()=>toast('Error loading GPIO config',false));
}

function toggleActiveLow(index) {
    fetch('/api/gpio/toggle-active-low', {
        method:'POST',
        headers:{'Content-Type':'application/json'},
        body:JSON.stringify({index:index})
    }).then(r=>r.json()).then(d=>{
        if(d.success) {
            toast('Active level: ' + (d.activeLow ? 'LOW' : 'HIGH'));
            loadGPIO();
        } else {
            toast('Failed: '+d.error, false);
        }
    }).catch(()=>toast('Error',false));
}

function addPin() {
    const rawVal = document.getElementById('newPin').value;
    if(rawVal === '') return;
    const pin = parseInt(rawVal);
    if(isNaN(pin)) return;
    
    fetch('/api/gpio/add', {
        method:'POST',
        headers:{'Content-Type':'application/json'},
        body:JSON.stringify({pin:pin})
    }).then(r=>r.json()).then(d=>{
        if(d.success) {
            toast('Relay added on GPIO '+pin+'!');
            loadGPIO();
        } else {
            toast('Failed: '+d.error, false);
        }
    }).catch(()=>toast('Error',false));
}

function deletePin(index) {
    if(!confirm('Remove Relay ' + (index+1) + ' from GPIO ' + gpioData.pins[index] + '?\n\nAll schedules for this relay will be lost and numbering will shift.')) return;
    
    fetch('/api/gpio/delete', {
        method:'POST',
        headers:{'Content-Type':'application/json'},
        body:JSON.stringify({index:index})
    }).then(r=>r.json()).then(d=>{
        if(d.success) {
            toast('Relay removed!');
            loadGPIO();
        } else {
            toast('Failed: '+d.error, false);
        }
    }).catch(()=>toast('Error',false));
}

function resetDefaults() {
    if(!confirm('Reset to default 16 GPIO pins?\n\nThis will restore all original pin mappings but preserve your schedule settings where possible.')) return;
    
    fetch('/api/gpio/save', {
        method:'POST',
        headers:{'Content-Type':'application/json'},
        body:JSON.stringify({pins:DEFAULT_PINS})
    }).then(r=>r.json()).then(d=>{
        if(d.success) {
            toast('Reset to default 16 pins!');
            loadGPIO();
        } else {
            toast('Failed: '+d.error, false);
        }
    }).catch(()=>toast('Error',false));
}

loadGPIO();
</script></body></html>)raw";

// =============================================================================
//  SYSTEM PAGE
// =============================================================================
const char system_html[] PROGMEM = R"raw(<!DOCTYPE html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>System</title>
<link rel="stylesheet" href="/style.css"></head><body>
<header>
<nav>
<a href="/">Relays</a>
<a href="/wifi">WiFi</a>
<a href="/ntp">Time</a>
<a href="/ap">AP</a>
<a href="/gpio">GPIO</a>
<a href="/system" class="cur">System</a>
</nav>
<div class="hdr-r"><span class="dot wd"></span><span class="dot td"></span>&nbsp;<span id="clk">--:--:--</span><span id="date" style="font-size:11px;opacity:.85;margin-left:6px;white-space:nowrap">--/--/----</span></div>
</header>
<main>
<p class="ptitle">System Information &amp; Settings</p>
<div class="ibar" id="ibar">
<div class="ibox"><div class="l">STA IP</div><div class="v" id="sip">&hellip;</div></div>
<div class="ibox"><div class="l">AP IP</div><div class="v" id="sap">&hellip;</div></div>
<div class="ibox"><div class="l">Free Heap</div><div class="v" id="shp">&hellip;</div></div>
<div class="ibox"><div class="l">Uptime</div><div class="v" id="sup">&hellip;</div></div>
<div class="ibox"><div class="l">WiFi RSSI</div><div class="v" id="srs">&hellip;</div></div>
<div class="ibox"><div class="l">Last Sync Source</div><div class="v" id="stsrc" style="font-size:13px">&hellip;</div></div>
<div class="ibox"><div class="l">Current Authority</div><div class="v" id="sauth" style="font-size:13px">&hellip;</div></div>
<div class="ibox"><div class="l">Timezone</div><div class="v" id="stz" style="font-size:13px;word-break:break-word">&hellip;</div></div>
<div class="ibox"><div class="l">DST Status</div><div class="v" id="sdst" style="font-size:13px">&hellip;</div></div>
<div class="ibox"><div class="l">UTC Epoch</div><div class="v" id="sutc" style="font-size:11px">&hellip;</div></div>
<div class="ibox"><div class="l">NTP Last Sync</div><div class="v" id="snt">&hellip;</div></div>
<div class="ibox"><div class="l">Last Browser Sync</div><div class="v" id="sbs">&hellip;</div></div>
<div class="ibox"><div class="l">NTP Server</div><div class="v" id="sns" style="font-size:12px;word-break:break-all">&hellip;</div></div>
<div class="ibox"><div class="l">Chip Model</div><div class="v" id="sch">&hellip;</div></div>
<div class="ibox"><div class="l">mDNS Hostname</div><div class="v" id="smdns">&hellip;</div></div>
<div class="ibox"><div class="l">Relay Count</div><div class="v" id="src">&hellip;</div></div>
<div class="ibox"><div class="l">GMT Offset</div><div class="v" id="sgmt" style="font-size:13px">&hellip;</div></div>
<div class="ibox"><div class="l">Drift Comp</div><div class="v" id="sdrift" style="font-size:13px">&hellip;</div></div>
<div class="ibox"><div class="l">GPIO Mode</div><div class="v" id="sactmode" style="font-size:12px">&hellip;</div></div>
<div class="ibox"><div class="l">DS3231 RTC</div><div class="v" id="srtc">&hellip;</div></div>
<div class="ibox"><div class="l">DS3231 Last Sync</div><div class="v" id="srtcsync">&hellip;</div></div>
<div class="ibox"><div class="l">DS3231 Temp</div><div class="v" id="srtctemp">&hellip;</div></div>
<div class="ibox"><div class="l">Sync Direction</div><div class="v" id="ssyncdir" style="font-size:12px;color:#7B1FA2;word-break:break-word">&hellip;</div></div>
<div class="ibox"><div class="l">WiFi Station</div><div class="v" id="stawifi">&hellip;</div></div>
</div>

<div class="card fcrd">
<p style="font-weight:700;margin-bottom:12px">Device Control</p>
<div style="display:flex;gap:8px;flex-wrap:wrap;justify-content:center">
<button class="btn bwarn" onclick="rst()" style="padding:9px 18px;border-radius:6px;font-size:13px;font-weight:600">&#x1F504; Verify Services</button>
<button class="btn bdanger" onclick="fct()" style="padding:9px 18px;border-radius:6px;font-size:13px;font-weight:600">&#x26A0; Factory Reset</button>
</div>
<p style="color:#90A4AE;font-size:12px;margin-top:10px">Factory reset clears all the settings.<br>Alternatively, hold the <strong>BOOT button</strong> for 5 seconds to trigger a hardware factory reset.</p>
</div>
</main>
<div id="toast"></div>
<script>
function toast(m,ok=true){const t=document.getElementById('toast');t.textContent=m;t.className='show '+(ok?'ok':'er');clearTimeout(t._t);t._t=setTimeout(()=>t.className='',3000);}

function tick(){
  if(tick._inflight) return;
  tick._inflight = true;
  fetch('/api/time').then(r=>{
    if(!r.ok) throw new Error('HTTP '+r.status);
    return r.json();
  }).then(d=>{
    tick._inflight = false;
    if(!d || typeof d !== 'object') return;
    const clkEl = document.getElementById('clk');
    if(clkEl) clkEl.textContent = (typeof d.time === 'string' && d.time) ? d.time : '--:--:--';
    const de = document.getElementById('date');
    if(de){
      const wk = (typeof d.weekday === 'string' && d.weekday) ? d.weekday + ' ' : '';
      const dt = (typeof d.date === 'string' && d.date) ? d.date : '--/--/----';
      de.textContent = wk + dt;
    }
    const w = document.querySelector('.wd');
    const t = document.querySelector('.td');
    if(w) w.className = 'dot ' + (d.wifi ? 'g' : 'r');
    if(t){
      let tc = 'y';
      const auth = (typeof d.authority === 'string' && d.authority) ? d.authority : d.timeSource;
      if(auth === 'ntp'){
        tc = (d.authorityStale === true) ? 'y' : 'g';
      }
      else if(auth === 'browser') tc = 'y';
      else if(auth === 'ds3231') tc = 'g';
      else if(auth === 'internal_rtc') tc = 'y';
      else if(auth === 'rtc') tc = (d.rtcPresent === true) ? 'g' : 'y';
      else tc = 'y';
      t.className = 'dot ' + tc;
    }
  }).catch(()=>{ tick._inflight = false; });
}
setInterval(tick,1000);tick();

function fmtUp(s){
  if(typeof s !== 'number' || !isFinite(s) || s < 0) s = 0;
  s = Math.floor(s);
  const d = Math.floor(s / 86400);
  const h = Math.floor((s % 86400) / 3600);
  const m = Math.floor((s % 3600) / 60);
  const ss = s % 60;
  if (d > 0) return d + 'd ' + h + 'h ' + m + 'm';
  if (h > 0) return h + 'h ' + m + 'm ' + ss + 's';
  if (m > 0) return m + 'm ' + ss + 's';
  return ss + 's';
}

function fmtAge(s){
  if(typeof s !== 'number' || !isFinite(s) || s < 0 || s === 4294967295) return 'Never';
  s = Math.floor(s);
  if(s < 60) return 'Just now';
  if(s < 3600) return Math.floor(s/60) + ' min ago';
  const h = Math.floor(s/3600);
  return h + 'h ' + Math.floor((s%3600)/60) + 'm ago';
}

function rssiDesc(r){
  if(typeof r !== 'number' || !isFinite(r) || r === 0) return '\u2014';
  return r + 'dBm (' + (r>=-50?'Excellent':r>=-60?'Good':r>=-70?'Fair':'Weak') + ')';
}

function formatGmtOffset(sec){
  if(typeof sec !== 'number' || !isFinite(sec)) sec = 0;
  const sign = sec >= 0 ? '+' : '-';
  const absG = Math.abs(sec);
  const gh = Math.floor(absG / 3600);
  const gmn = Math.floor((absG % 3600) / 60);
  const gss = absG % 60;
  let s = 'UTC' + sign + gh;
  if(gmn !== 0 || gss !== 0){
    s += ':' + String(gmn).padStart(2, '0');
    if(gss !== 0) s += ':' + String(gss).padStart(2, '0');
  }
  return s;
}

function safeAge(v){
  return (typeof v === 'number' && isFinite(v) && v >= 0 && v !== 4294967295) ? Math.floor(v) : -1;
}

function loadSys(){
  if(loadSys._inflight) return;
  loadSys._inflight = true;
  fetch('/api/system').then(r=>{
    if(!r.ok) throw new Error('HTTP '+r.status);
    return r.json();
  }).then(d=>{
    loadSys._inflight = false;
    if(!d || typeof d !== 'object' || Array.isArray(d) || Object.keys(d).length === 0) return;

    const sipEl = document.getElementById('sip');
    if(sipEl) sipEl.textContent = (d.wifiConnected && typeof d.ip === 'string' && d.ip && d.ip !== '0.0.0.0') ? d.ip : '(not connected)';

    const sapEl = document.getElementById('sap');
    if(sapEl) sapEl.textContent = (typeof d.ap_ip === 'string' && d.ap_ip && d.ap_ip !== '0.0.0.0') ? d.ap_ip : '\u2014';

    const shpEl = document.getElementById('shp');
    if(shpEl){
      const fh = (typeof d.freeHeap === 'number' && isFinite(d.freeHeap)) ? d.freeHeap : 0;
      shpEl.textContent = (fh/1024).toFixed(1) + ' KB';
    }

    const supEl = document.getElementById('sup');
    if(supEl) supEl.textContent = fmtUp(d.uptime);

    const srsEl = document.getElementById('srs');
    if(srsEl) srsEl.textContent = d.wifiConnected ? rssiDesc(d.rssi) : '\u2014';

    const tsrcEl = document.getElementById('stsrc');
    if(tsrcEl){
      const tsrc = (typeof d.timeSource === 'string' && d.timeSource) ? d.timeSource : 'None';
      let tsrcStyle = 'color:#C62828';
      if(tsrc === 'NTP') tsrcStyle = 'color:#2E7D32';
      else if(tsrc === 'Browser') tsrcStyle = 'color:#1565C0';
      else if(tsrc === 'DS3231') tsrcStyle = 'color:#7B1FA2';
      else if(tsrc === 'Internal RTC') tsrcStyle = 'color:#7B1FA2';
      tsrcEl.style.cssText = 'font-size:13px;font-weight:700;' + tsrcStyle;
      tsrcEl.textContent = tsrc;
    }

    const sauthEl = document.getElementById('sauth');
    if(sauthEl){
      const auth = (typeof d.authority === 'string' && d.authority) ? d.authority : '';
      let authLabel = '—';
      let authStyle = 'color:#C62828';
      if(auth === 'ntp'){
        authLabel = (d.authorityStale === true) ? 'NTP (stale)' : 'NTP';
        authStyle = (d.authorityStale === true) ? 'color:#F9A825' : 'color:#2E7D32';
      } else if(auth === 'browser'){
        authLabel = 'Browser';
        authStyle = 'color:#1565C0';
      } else if(auth === 'ds3231'){
        authLabel = 'DS3231';
        authStyle = 'color:#7B1FA2';
      } else if(auth === 'internal_rtc'){
        authLabel = 'Internal RTC';
        authStyle = 'color:#F9A825';
      } else if(auth === 'none'){
        authLabel = 'None';
        authStyle = 'color:#C62828';
      }
      sauthEl.style.cssText = 'font-size:13px;font-weight:700;' + authStyle;
      sauthEl.textContent = authLabel;
    }

    const stzEl = document.getElementById('stz');
    if(stzEl) stzEl.textContent = (typeof d.tzName === 'string' && d.tzName) ? d.tzName : '\u2014';

    const dstEl = document.getElementById('sdst');
    if(dstEl){
      const dstSec = (typeof d.dstOffset === 'number' && isFinite(d.dstOffset)) ? d.dstOffset : 0;
      if(d.dstActive){
        const dstH = Math.round(dstSec/3600*100)/100;
        dstEl.textContent = '\u{1F31E} ACTIVE (+' + dstH + 'h)';
        dstEl.style.color = '#E65100';
      } else {
        dstEl.textContent = '\u2744\uFE0F Standard';
        dstEl.style.color = '#1565C0';
      }
    }

    const sutcEl = document.getElementById('sutc');
    if(sutcEl){
      const utcStr = (typeof d.utcEpoch === 'string') ? d.utcEpoch : String(d.utcEpoch || '');
      sutcEl.textContent = (utcStr && utcStr !== '0') ? utcStr : '\u2014';
    }

    const sntEl = document.getElementById('snt');
    if(sntEl){
      const ntpAge = safeAge(d.ntpSyncAge);
      sntEl.textContent = ntpAge >= 0 ? fmtAge(ntpAge) : 'Never';
    }

    const sbsEl = document.getElementById('sbs');
    if(sbsEl){
      const bsAge = safeAge(d.browserSyncAge);
      sbsEl.textContent = bsAge >= 0 ? fmtAge(bsAge) : 'Never';
    }

    const snsEl = document.getElementById('sns');
    if(snsEl) snsEl.textContent = (typeof d.ntpServer === 'string' && d.ntpServer) ? d.ntpServer : '\u2014';

    const schEl = document.getElementById('sch');
    if(schEl) schEl.textContent = (typeof d.chipModel === 'string' && d.chipModel) ? d.chipModel : 'ESP32S3';

    const smdnsEl = document.getElementById('smdns');
    if(smdnsEl){
      if(d.mdnsStarted){
        smdnsEl.textContent = (typeof d.mdnsHostname === 'string' && d.mdnsHostname) ? d.mdnsHostname + '.local' : '\u2014';
      } else {
        smdnsEl.textContent = 'Not running';
      }
    }

    const srcEl = document.getElementById('src');
    if(srcEl){
      const rc = (typeof d.relayCount === 'number' && isFinite(d.relayCount)) ? d.relayCount : 0;
      const mr = (typeof d.maxRelays === 'number' && isFinite(d.maxRelays)) ? d.maxRelays : 16;
      srcEl.textContent = rc + ' / ' + mr;
    }

    const sgmtEl = document.getElementById('sgmt');
    if(sgmtEl) sgmtEl.textContent = formatGmtOffset(d.gmtOffset);

    const sdriftEl = document.getElementById('sdrift');
    if(sdriftEl){
      const dc = (typeof d.driftComp === 'number' && isFinite(d.driftComp) && d.driftComp > 0) ? d.driftComp : 1.0;
      sdriftEl.textContent = dc.toFixed(4);
    }

    const sactEl = document.getElementById('sactmode');
    if(sactEl){
      let modeText = '\u{1F527} Per-Relay';
      if(d.globalActiveMode === 1) modeText = '\u{1F30D} Global LOW';
      else if(d.globalActiveMode === 2) modeText = '\u{1F30D} Global HIGH';
      sactEl.textContent = modeText;
    }

    const srtcEl = document.getElementById('srtc');
    if(srtcEl){
      srtcEl.textContent = d.rtcPresent ? '\u2705 Present' : '\u274C Not detected';
      srtcEl.style.color = d.rtcPresent ? '#2E7D32' : '#C62828';
    }

    const srtcsyncEl = document.getElementById('srtcsync');
    if(srtcsyncEl){
      const rtcAge = safeAge(d.rtcSyncAge);
      srtcsyncEl.textContent = rtcAge >= 0 ? fmtAge(rtcAge) : 'Never';
    }

    const tEl = document.getElementById('srtctemp');
    if(tEl){
      if(d.rtcTemp === null || d.rtcTemp === undefined || typeof d.rtcTemp !== 'number' || !isFinite(d.rtcTemp)){
        tEl.textContent = '\u2014';
        tEl.style.color = '#90A4AE';
      } else {
        tEl.textContent = d.rtcTemp.toFixed(2) + ' \u00B0C';
        if(d.rtcTemp < 0)        tEl.style.color = '#0D47A1';
        else if(d.rtcTemp < 10)  tEl.style.color = '#1565C0';
        else if(d.rtcTemp < 20)  tEl.style.color = '#0288D1';
        else if(d.rtcTemp < 30)  tEl.style.color = '#2E7D32';
        else if(d.rtcTemp < 40)  tEl.style.color = '#F9A825';
        else if(d.rtcTemp < 50)  tEl.style.color = '#EF6C00';
        else                     tEl.style.color = '#C62828';
      }
    }

    const ssyncEl = document.getElementById('ssyncdir');
    if(ssyncEl){
      ssyncEl.textContent = (typeof d.syncDirection === 'string' && d.syncDirection) ? d.syncDirection : 'Not available';
    }

    const stawifiEl = document.getElementById('stawifi');
    if(stawifiEl){
      const staStatus = d.staEnabled ? (d.wifiConnected ? '\u2705 Connected' : '\u26A0\uFE0F Enabled but disconnected') : '\u274C Disabled';
      stawifiEl.textContent = staStatus;
      if(d.staEnabled && d.wifiConnected) stawifiEl.style.color = '#2E7D32';
      else if(d.staEnabled) stawifiEl.style.color = '#F9A825';
      else stawifiEl.style.color = '#C62828';
    }
  }).catch(()=>{
    loadSys._inflight = false;
    ['sip','sap','shp','sup','srs','stsrc','sauth','stz','sdst','sutc','snt','sbs','sns','sch','smdns','src','sgmt','sdrift','sactmode','srtc','srtcsync','srtctemp','ssyncdir','stawifi'].forEach(id=>{
      const el = document.getElementById(id);
      if(el && el.textContent === '\u2026') el.textContent = '\u2014';
    });
    const el = document.getElementById('stsrc');
    if(el && !el.textContent) el.textContent = '\u2014';
  });
}
loadSys();
setInterval(loadSys, 5000);

function rst(){
  if(!confirm('Verify all services without restarting? This will check WiFi, web server, DNS, and mDNS.'))return;
  fetch('/api/reset',{method:'POST'}).then(r=>r.json()).then(d=>{
    if(d && d.success)toast('Services verified!');
    else toast('Verification failed',false);
  }).catch(()=>toast('Error',false));
}

function fct(){
  if(!confirm('FACTORY RESET \u2014 ALL settings will be erased. Continue?'))return;
  fetch('/api/factory-reset',{method:'POST'}).then(r=>r.json()).then(d=>{
    if(d && d.success)toast('Factory reset complete');
    else toast('Reset failed',false);
  }).catch(()=>{});
  setTimeout(()=>window.location.reload(),15000);
}
</script></body></html>)raw";

// =============================================================================
//  BOOT BUTTON FACTORY RESET
// =============================================================================
void checkBootButton() {
    static bool lastStableState = false;
    static unsigned long lastDebounceTime = 0;
    bool rawState = !digitalRead(BOOT_BUTTON_PIN);
    unsigned long now = millis();
    if (rawState != lastStableState && timeHasElapsed(now, lastDebounceTime, 50)) {
        lastDebounceTime = now;
        lastStableState = rawState;
        if (rawState && !bootButtonPressed) {
            bootButtonPressed = true;
            bootButtonPressStart = now;
        } else if (!rawState && bootButtonPressed) {
            bootButtonPressed = false;
        }
    }
    if (bootButtonPressed && !factoryResetTriggered &&
        timeHasElapsed(now, bootButtonPressStart, FACTORY_RESET_HOLD)) {
        factoryResetTriggered = true;
        preferences.begin(NVS_NAMESPACE, false);
        preferences.clear();
        preferences.end();
        delay(100);
        ESP.restart();
    }
}

// =============================================================================
//  GPIO CONFIGURATION FUNCTIONS
// =============================================================================
void loadGPIOConfig() {
    preferences.begin(NVS_NAMESPACE, true);
    size_t len = preferences.getBytes("gpioConfig", &gpioConfig, sizeof(GPIOPinConfig));
    preferences.end();
    bool valid = (len == sizeof(GPIOPinConfig)) &&
                 (gpioConfig.magic == GPIO_CONFIG_MAGIC) &&
                 (gpioConfig.count > 0) &&
                 (gpioConfig.count <= MAX_RELAYS);
    if (valid) {
        // change gpio
        static const uint8_t SAFE_PINS[] = {4, 5, 6, 7, 11, 12, 13, 14, 1, 2, 42, 41, 47, 21, 20, 19};
        for (uint8_t i = 0; i < gpioConfig.count; i++) {
            bool allowed = false;
            for (uint8_t sp : SAFE_PINS) {
                if (sp == gpioConfig.pins[i]) { allowed = true; break; }
            }
            if (!allowed) { valid = false; break; }
            for (uint8_t j = i + 1; j < gpioConfig.count; j++) {
                if (gpioConfig.pins[i] == gpioConfig.pins[j]) { valid = false; break; }
            }
            if (!valid) break;
        }
    }
    if (!valid) {
        gpioConfig.count = 16;
        gpioConfig.magic = GPIO_CONFIG_MAGIC;
        memcpy(gpioConfig.pins, DEFAULT_RELAY_PINS, sizeof(uint8_t) * 16);
        for (int i = 0; i < MAX_RELAYS; i++) {
            gpioConfig.activeLow[i] = true;
        }
        saveGPIOConfig();
    }
}

void saveGPIOConfig() {
    preferences.begin(NVS_NAMESPACE, false);
    preferences.putBytes("gpioConfig", &gpioConfig, sizeof(GPIOPinConfig));
    preferences.end();
}

// =============================================================================
//  NTP FUNCTIONS
// =============================================================================
void syncInternalRTC(uint64_t rawUtcEpoch, uint8_t source) {
    if (!VALID_UNIX_TIME_64(rawUtcEpoch)) return;
    unsigned long nowMicros = micros();
    unsigned long nowMillis = millis();
    internalEpoch = rawUtcEpoch;
    rtcMicrosAtLastSync = nowMicros;
    lastRTCRebase = nowMillis;
    rtcInitialized = true;
    if (source == TIME_SOURCE_BROWSER) {
        lastBrowserSync = nowMillis;
        timeSource = TIME_SOURCE_BROWSER;
    } else {
        lastNTPSync = nowMillis;
        timeSource = TIME_SOURCE_NTP;
        lastBrowserSync = 0;
    }
    ntpFailCount = 0;
    if (rtcPresent) {
        immediateDS3231Sync();
    } else {
        saveRTCState();
    }
    updateScheduleCache();
}

// =============================================================================
//  ASYNC NTP CLIENT FUNCTIONS
// =============================================================================
void startNTPRequest(const char* server) {
    memset(ntpPacketBuffer, 0, NTP_PACKET_SIZE);
    ntpPacketBuffer[0] = 0b11100011;
    ntpPacketBuffer[1] = 0;
    ntpPacketBuffer[2] = 6;
    ntpPacketBuffer[3] = 0xEC;
    uint64_t now = getCurrentEpoch();
    uint64_t ntpNow = now + NTP_EPOCH_OFFSET;
    uint32_t secs = (uint32_t)(ntpNow & 0xFFFFFFFF);
    ntpPacketBuffer[40] = (secs >> 24) & 0xFF;
    ntpPacketBuffer[41] = (secs >> 16) & 0xFF;
    ntpPacketBuffer[42] = (secs >> 8) & 0xFF;
    ntpPacketBuffer[43] = secs & 0xFF;
    ntpPacketBuffer[44] = 0;
    ntpPacketBuffer[45] = 0;
    ntpPacketBuffer[46] = 0;
    ntpPacketBuffer[47] = 0;
    if (!ntpUDP.begin(2390)) {
        ntpAsyncStage = 3;
        ntpAsyncPhaseStart = millis();
        return;
    }
    ntpUDP.beginPacket(server, NTP_PORT);
    ntpUDP.write(ntpPacketBuffer, NTP_PACKET_SIZE);
    ntpUDP.endPacket();
    ntpAsyncPhaseStart = millis();
    ntpReceived = false;
    ntpAsyncStage = 1;
}

void processNTPResponse() {
    if (ntpAsyncStage != 1) return;
    int packetSize = ntpUDP.parsePacket();
    if (packetSize >= NTP_PACKET_SIZE) {
        if (ntpUDP.remotePort() != NTP_PORT) {
            uint8_t discard[128];
            while (ntpUDP.available() > 0) {
                ntpUDP.read(discard, sizeof(discard));
            }
            return;
        }
        ntpUDP.read(ntpPacketBuffer, NTP_PACKET_SIZE);
        uint8_t mode = ntpPacketBuffer[0] & 0x07;
        uint8_t li = (ntpPacketBuffer[0] >> 6) & 0x03;
        if (mode == 4 && li != 3) {
            uint32_t secs = ((uint32_t)ntpPacketBuffer[40] << 24) |
                           ((uint32_t)ntpPacketBuffer[41] << 16) |
                           ((uint32_t)ntpPacketBuffer[42] << 8) |
                           (uint32_t)ntpPacketBuffer[43];
            uint64_t ntpSecs64 = (uint64_t)secs;
            uint64_t currentEpoch = getCurrentEpoch();
            if (currentEpoch >= 1577836800ULL) {
                uint64_t currentNtp = currentEpoch + NTP_EPOCH_OFFSET;
                uint64_t eraBase = currentNtp & ~0xFFFFFFFFULL;
                uint64_t candidate     = eraBase | (uint64_t)secs;
                uint64_t candidateNext = candidate + 0x100000000ULL;
                uint64_t candidatePrev = (eraBase >= 0x100000000ULL) ? (candidate - 0x100000000ULL) : candidate;
                uint64_t candidates[3] = { candidatePrev, candidate, candidateNext };
                uint64_t bestNtp = 0;
                uint64_t bestDiff = UINT64_MAX;
                for (int ci = 0; ci < 3; ci++) {
                    uint64_t candNtp = candidates[ci];
                    uint64_t candUnix = candNtp - NTP_EPOCH_OFFSET;
                    if (candNtp < NTP_EPOCH_OFFSET) continue;
                    if (!VALID_UNIX_TIME_64(candUnix)) continue;
                    uint64_t d = (candNtp > currentNtp) ? (candNtp - currentNtp) : (currentNtp - candNtp);
                    if (d < bestDiff) {
                        bestDiff = d;
                        bestNtp = candNtp;
                    }
                }
                if (bestDiff != UINT64_MAX) {
                    bool localTimeTrusted = (timeSource == TIME_SOURCE_NTP) ||
                                            (timeSource == TIME_SOURCE_BROWSER) ||
                                            (rtcPresent && rtcTimeValid);
                    if (localTimeTrusted && bestDiff > 31536000ULL) {
                        ntpUDP.stop();
                        ntpAsyncStage = 3;
                        return;
                    }
                    ntpSecs64 = bestNtp;
                } else {
                    uint64_t unixEra0 = ntpSecs64 - NTP_EPOCH_OFFSET;
                    uint64_t unixEra1 = unixEra0 + 0x100000000ULL;
                    if (!VALID_UNIX_TIME_64(unixEra0) &&
                        (VALID_UNIX_TIME_64(unixEra1) || secs < 0x70000000UL)) {
                        ntpSecs64 += 0x100000000ULL;
                    }
                }
            } else {
                uint64_t unixEra0 = ntpSecs64 - NTP_EPOCH_OFFSET;
                uint64_t unixEra1 = unixEra0 + 0x100000000ULL;
                if (!VALID_UNIX_TIME_64(unixEra0) &&
                    (VALID_UNIX_TIME_64(unixEra1) || secs < 0x70000000UL)) {
                    ntpSecs64 += 0x100000000ULL;
                }
            }
            uint64_t unixEpoch = ntpSecs64 - NTP_EPOCH_OFFSET;
            if (VALID_UNIX_TIME_64(unixEpoch)) {
                ntpResult = unixEpoch;
                ntpReceived = true;
                ntpAsyncStage = 2;
                return;
            }
        }
    }
    if (timeHasElapsed(millis(), ntpAsyncPhaseStart, NTP_TIMEOUT)) {
        ntpUDP.stop();
        ntpAsyncStage = 3;
    }
}

void updateNTPSync() {
    switch (ntpAsyncStage) {
        case 0:
            if (ntpRetryCount == 0 && strlen(sysConfig.ntp_server) > 0) {
                startNTPRequest(sysConfig.ntp_server);
            } else if (ntpRetryCount <= NUM_NTP_SERVERS) {
                uint8_t idx = (ntpRetryCount == 0) ? 0 : (ntpRetryCount - 1);
                if (idx < NUM_NTP_SERVERS) {
                    startNTPRequest(NTP_SERVERS[idx]);
                }
            } else {
                ntpFailCount++;
                ntpRetryCount = 0;
                ntpAsyncStage = 0;
                ntpAsyncState = NTP_STATE_IDLE;
            }
            break;
        case 1:
            processNTPResponse();
            break;
        case 2:
            ntpUDP.stop();
            if (ntpReceived) {
                syncInternalRTC(ntpResult, TIME_SOURCE_NTP);
                ntpFailCount = 0;
            }
            ntpRetryCount = 0;
            ntpAsyncStage = 0;
            ntpAsyncState = NTP_STATE_IDLE;
            break;
        case 3:
            ntpUDP.stop();
            ntpRetryCount++;
            ntpAsyncStage = 0;
            break;
    }
}

// =============================================================================
//  NTP SYNC INITIATION
// =============================================================================
void tryNTPSync() {
    if (!wifiConnected || !extConfig.sta_enabled) {
        ntpAsyncState = NTP_STATE_IDLE;
        ntpAsyncStage = 0;
        return;
    }
    if (ntpAsyncStage != 0 || ntpAsyncState == NTP_STATE_CONNECTING) {
        return;
    }
    unsigned long now = millis();
    if (lastNTPAttempt != 0 && !timeHasElapsed(now, lastNTPAttempt, NTP_RETRY_INTERVAL)) {
        return;
    }
    lastNTPAttempt = now;
    ntpRetryCount = 0;
    ntpAsyncStage = 0;
    ntpAsyncState = NTP_STATE_CONNECTING;
}

// =============================================================================
//  BROWSER TIME SYNC
// =============================================================================
void handleBrowserTimeSync() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json",
            "{\"success\":false,\"error\":\"No data\"}");
        return;
    }
    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, server.arg("plain"));
    if (err) {
        server.send(400, "application/json",
            "{\"success\":false,\"error\":\"Bad JSON\"}");
        return;
    }
    uint64_t browserUtcEpoch = doc["utc_epoch"].as<uint64_t>();
    if (!VALID_UNIX_TIME_64(browserUtcEpoch)) {
        server.send(400, "application/json",
            "{\"success\":false,\"error\":\"Invalid epoch time. Expected between 2020-01-01 and 2100-01-01.\"}");
        return;
    }
    uint64_t currentEpoch = getCurrentEpoch();
    bool trustCurrentTime = false;
    unsigned long nowMs = millis();
    if (timeSource == TIME_SOURCE_NTP && lastNTPSync > 0 &&
        !timeHasElapsed(nowMs, lastNTPSync, 86400000UL)) {
        trustCurrentTime = true;
    }
    if (timeSource == TIME_SOURCE_RTC && rtcPresent && rtcTimeValid &&
        lastRTCDSync > 0 &&
        !timeHasElapsed(nowMs, lastRTCDSync, 86400000UL)) {
        trustCurrentTime = true;
    }
    if (timeSource == TIME_SOURCE_BROWSER && lastBrowserSync > 0 &&
        !timeHasElapsed(nowMs, lastBrowserSync, 86400000UL)) {
        trustCurrentTime = true;
    }
    bool havePriorAuthority = (rtcPresent && rtcTimeValid) ||
                              (rtcInitialized && internalEpoch > 0);
    if ((trustCurrentTime || havePriorAuthority) &&
        currentEpoch >= MIN_UNIX_TIME_64) {
        uint64_t diff = (browserUtcEpoch > currentEpoch)
            ? (browserUtcEpoch - currentEpoch)
            : (currentEpoch - browserUtcEpoch);
        if (diff > 31536000ULL) {
            server.send(400, "application/json",
                "{\"success\":false,\"error\":\"Epoch differs too much from current time\"}");
            return;
        }
    }
    syncInternalRTC(browserUtcEpoch, TIME_SOURCE_BROWSER);
    if (ntpAsyncStage == 0 && ntpAsyncState != NTP_STATE_CONNECTING) {
        lastNTPSync = 0;
    }
    if (rtcPresent && rtcInitialized && internalEpoch > 0) {
        if (rtc.begin()) {
            DateTime dt = uint64ToRtcDateTime(internalEpoch);
            rtc.adjust(dt);
            rtcTimeValid = true;
            lastRTCDSync = millis();
        } else {
            rtcPresent = false;
            rtcTimeValid = false;
            saveRTCState();
        }
    } else {
        saveRTCState();
    }
    updateScheduleCache();
    uint64_t localEpoch = getLocalEpoch(browserUtcEpoch);
    struct tm* ti = gmtime64(&localEpoch);
    char timeStr[12];
    char dateStr[12];
    char weekdayStr[4] = "";
    static const char* WD[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    if (ti) {
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d",
                 ti->tm_hour, ti->tm_min, ti->tm_sec);
        snprintf(dateStr, sizeof(dateStr), "%02d/%02d/%04d",
                 ti->tm_mon + 1, ti->tm_mday, ti->tm_year + 1900);
        if (ti->tm_wday >= 0 && ti->tm_wday <= 6) {
            strncpy(weekdayStr, WD[ti->tm_wday], 3);
            weekdayStr[3] = '\0';
        }
    } else {
        strcpy(timeStr, "--:--:--");
        strcpy(dateStr, "--/--/----");
    }
    String resp;
    DynamicJsonDocument respDoc(512);
    respDoc["success"] = true;
    {
        char ebuf[24];
        snprintf(ebuf, sizeof(ebuf), "%llu", (unsigned long long)browserUtcEpoch);
        respDoc["utc_epoch"] = ebuf;
    }
    respDoc["local_time"] = timeStr;
    respDoc["local_date"] = dateStr;
    respDoc["weekday"] = weekdayStr;
    respDoc["gmt_offset"] = sysConfig.gmt_offset;
    respDoc["time_source"] = "browser";
    respDoc["rtc_present"] = rtcPresent;
    respDoc["rtc_synced"] = rtcPresent && rtcTimeValid;
    respDoc["drift"] = 1.0f;
    serializeJson(respDoc, resp);
    server.send(200, "application/json", resp);
}

// =============================================================================
//  WIFI FUNCTIONS
// =============================================================================
void beginWiFiConnect() {
    if (!extConfig.sta_enabled) return;
    if (strlen(sysConfig.sta_ssid) == 0) return;
    if (wifiGiveUpUntil != 0 && !isTimeReached(millis(), wifiGiveUpUntil)) return;
    if (wifiConnecting) return;
    if (wifiPausedForScan) return;
    WiFi.disconnect(false, false);
    delay(100);
    wifiReconnectAttempts++;
    if (WiFi.getMode() != WIFI_AP_STA) {
        WiFi.mode(WIFI_AP_STA);
    }
    WiFi.begin(sysConfig.sta_ssid, sysConfig.sta_password);
    wifiConnecting = true;
    wifiConnectStart = millis();
}

// =============================================================================
//  SINGLE SCHEDULE ACTIVE CHECK
// =============================================================================
inline bool isSingleScheduleSlotActive(uint64_t epoch,
                                       int currentWeekday, int currentMonthDay, int currentMonth, int currentSeconds,
                                       const TimerSchedule& sched, int slot) {
    if (!sched.enabled[slot]) return false;
    uint16_t monthMask = sched.monthMask[slot];
    uint32_t monthDayMask = sched.monthDays[slot];
    int start = sched.startHour[slot] * 3600 + sched.startMinute[slot] * 60 + sched.startSecond[slot];
    int stop  = sched.stopHour[slot]  * 3600 + sched.stopMinute[slot]  * 60 + sched.stopSecond[slot];
    int cur = currentSeconds;
    if (start == stop) {
        if (!(monthMask & (1 << currentMonth))) return false;
        if (!(monthDayMask & (1 << (currentMonthDay - 1)))) return false;
        if (sched.days[slot] & (1 << currentWeekday)) return true;
        return false;
    } else if (start < stop) {
        if (!(monthMask & (1 << currentMonth))) return false;
        if (!(monthDayMask & (1 << (currentMonthDay - 1)))) return false;
        if ((sched.days[slot] & (1 << currentWeekday)) && cur >= start && cur < stop) return true;
        return false;
    } else {
        if (cur >= start) {
            if (!(monthMask & (1 << currentMonth))) return false;
            if (!(monthDayMask & (1 << (currentMonthDay - 1)))) return false;
            if (sched.days[slot] & (1 << currentWeekday)) return true;
            return false;
        } else if (cur < stop) {
            uint64_t prevUtcEpoch = epoch - 86400ULL;
            uint64_t checkLocalEpoch = getLocalEpoch(prevUtcEpoch);
            struct tm* checkDate = gmtime64(&checkLocalEpoch);
            if (!checkDate) return false;
            if (!(monthMask & (1 << checkDate->tm_mon))) return false;
            if (!(monthDayMask & (1 << (checkDate->tm_mday - 1)))) return false;
            if (sched.days[slot] & (1 << checkDate->tm_wday)) return true;
            return false;
        } else {
            return false;
        }
    }
}

// =============================================================================
//  SCHEDULE CACHE
// =============================================================================
void updateScheduleCache() {
    uint64_t epoch = getCurrentEpoch();
    if (epoch < MIN_UNIX_TIME_64) return;
    uint64_t localEpoch = getLocalEpoch(epoch);
    struct tm* ti = gmtime64(&localEpoch);
    if (!ti) return;
    int currentWeekday = ti->tm_wday;
    int currentMonthDay = ti->tm_mday;
    int currentMonth = ti->tm_mon;
    int currentSeconds = ti->tm_hour * 3600 + ti->tm_min * 60 + ti->tm_sec;
    int cur = currentSeconds;
    for (int i = 0; i < gpioConfig.count; i++) {
        if (relayConfigs[i].manualOverride) {
            scheduleActiveCache[i] = false;
            continue;
        }
        bool hasActive = false;
        for (int s = 0; s < 8; s++) {
            if (isSingleScheduleSlotActive(epoch,
                                           currentWeekday, currentMonthDay, currentMonth, cur,
                                           relayConfigs[i].schedule, s)) {
                hasActive = true;
                break;
            }
        }
        scheduleActiveCache[i] = hasActive;
    }
    lastScheduleCacheUpdate = millis();
}

// =============================================================================
//  SCHEDULE ENGINE
// =============================================================================
void processRelaySchedules() {
    unsigned long now = millis();
    if (timeHasElapsed(now, lastScheduleCacheUpdate, SCHEDULE_CACHE_INTERVAL)) {
        updateScheduleCache();
    }
    uint64_t epoch = getCurrentEpoch();
    if (epoch < MIN_UNIX_TIME_64) return;
    uint64_t localEpoch = getLocalEpoch(epoch);
    struct tm* ti = gmtime64(&localEpoch);
    if (!ti) return;
    int currentWeekday = ti->tm_wday;
    int currentMonthDay = ti->tm_mday;
    int currentMonth = ti->tm_mon;
    int currentSeconds = ti->tm_hour * 3600 + ti->tm_min * 60 + ti->tm_sec;
    int cur = currentSeconds;
    for (int i = 0; i < gpioConfig.count; i++) {
        if (relayConfigs[i].manualOverride) {
            bool targetState = relayConfigs[i].manualState;
            if (lastRelayOutputs[i] != targetState) {
                setRelayOutput(i, targetState);
                lastRelayOutputs[i] = targetState;
            }
            lastDebouncedStateGlobal[i] = targetState;
            continue;
        }
        bool shouldBeOn = false;
        for (int s = 0; s < 8; s++) {
            if (isSingleScheduleSlotActive(epoch,
                                           currentWeekday, currentMonthDay, currentMonth, cur,
                                           relayConfigs[i].schedule, s)) {
                shouldBeOn = true;
                break;
            }
        }
        if (lastDebouncedStateGlobal[i] != shouldBeOn) {
            if (timeHasElapsed(now, lastStateChangeGlobal[i], 500)) {
                setRelayOutput(i, shouldBeOn);
                lastRelayOutputs[i] = shouldBeOn;
                lastDebouncedStateGlobal[i] = shouldBeOn;
                lastStateChangeGlobal[i] = now;
            }
        } else if (lastRelayOutputs[i] != shouldBeOn) {
            setRelayOutput(i, shouldBeOn);
            lastRelayOutputs[i] = shouldBeOn;
        }
    }
}

// =============================================================================
//  mDNS FUNCTIONS
// =============================================================================
void startMDNS() {
    if (mdnsStarted) return;
    preferences.begin(NVS_NAMESPACE, true);
    String persisted = preferences.getString("mdns_host", "");
    preferences.end();
    if (persisted.length() > 0 && persisted.length() < 32) {
        strncpy(mdnsHostname, persisted.c_str(), 31);
        mdnsHostname[31] = '\0';
    }
    String hostname = String(mdnsHostname);
    if (hostname.length() == 0 || hostname == MDNS_HOSTNAME_DEFAULT) {
        hostname = String(sysConfig.ap_ssid);
        hostname.toLowerCase();
        hostname.replace(" ", "-");
        hostname.replace("_", "-");
        String clean;
        for (char c : hostname) {
            unsigned char uc = (unsigned char)c;
            if (isalnum(uc) || uc == '-') clean += c;
        }
        if (clean.length() > 0) {
            hostname = clean;
        } else {
            hostname = MDNS_HOSTNAME_DEFAULT;
        }
        if (hostname.length() > 31) hostname = hostname.substring(0, 31);
        strcpy(mdnsHostname, hostname.c_str());
    }
    if (MDNS.begin(mdnsHostname)) {
        MDNS.addService("http", "tcp", 80);
        MDNS.addServiceTxt("http", "tcp", "model", "ESP32S3");
        MDNS.addServiceTxt("http", "tcp", "version", "v12");
        MDNS.addServiceTxt("http", "tcp", "channels", String(gpioConfig.count).c_str());
        MDNS.addServiceTxt("http", "tcp", "features", "monthmask,64bit,tz");
        mdnsStarted = true;
    } else {
        mdnsStarted = false;
    }
}
String getMDNSHostname() {
    return String(mdnsHostname);
}

// =============================================================================
//  SETUP
// =============================================================================
void setup() {
    initRTC();
    loadGPIOConfig();
    for (int i = 0; i < MAX_RELAYS; i++) {
        initScheduleDefaults(i);
    }
    loadConfiguration();
    loadExtConfig();
    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
    for (int i = 0; i < gpioConfig.count; i++) {
        int pin = getRelayPin(i);
        if (pin < 0) continue;
        bool initial = relayConfigs[i].manualOverride ? relayConfigs[i].manualState : false;
        digitalWrite(pin, isActiveLow(i) ? !initial : initial);
        pinMode(pin, OUTPUT);
        setRelayOutput(i, initial);
        lastRelayOutputs[i] = initial;
        lastDebouncedStateGlobal[i] = initial;
        lastStateChangeGlobal[i] = 0;
    }
    bool timeInitialized = false;
    if (loadRTCFromDS3231()) {
        timeInitialized = true;
    }
    if (!timeInitialized) {
        loadRTCState();
        if (rtcInitialized) {
            timeInitialized = true;
        }
    }
    if (!timeInitialized) {
        driftCompensation = 1.0f;
        rtcInitialized = false;
        timeSource = TIME_SOURCE_NONE;
    } else {
        if (rtcPresent && !rtcTimeValid && internalEpoch > 0) {
            healer.recoverRTC();
        }
    }
    WiFi.mode(WIFI_AP_STA);
    if (extConfig.sta_enabled && strlen(sysConfig.sta_ssid) > 0) {
        WiFi.disconnect(false, false);
        delay(100);
        WiFi.begin(sysConfig.sta_ssid, sysConfig.sta_password);
        wifiConnecting = true;
        wifiConnectStart = millis();
    } else if (!extConfig.sta_enabled) {
        WiFi.mode(WIFI_AP);
    }
    uint8_t ch = extConfig.ap_channel;
    if (ch < 1 || ch > 13) {
        ch = 6;
        extConfig.ap_channel = 6;
        saveExtConfig();
    }
    uint8_t hidden = extConfig.ap_hidden ? 1 : 0;
    WiFi.softAPdisconnect(true);
    delay(100);
    bool apOk = false;
    if (strlen(sysConfig.ap_password) > 0) {
        apOk = WiFi.softAP(sysConfig.ap_ssid, sysConfig.ap_password, ch, hidden);
    } else {
        apOk = WiFi.softAP(sysConfig.ap_ssid, NULL, ch, hidden);
    }
    if (!apOk) {
        WiFi.softAP(sysConfig.ap_ssid,
                    strlen(sysConfig.ap_password) > 0 ? sysConfig.ap_password : NULL,
                    6, hidden);
    }
    startMDNS();
    if (WiFi.softAPIP() != IPAddress(0, 0, 0, 0)) {
        dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    }
    setupWebServer();
    updateScheduleCache();
    lastMemoryCleanup = millis();
    lastHeapCheck = millis();
    lastInternalRTCSave = millis();
}

// =============================================================================
//  LOOP
// =============================================================================
void loop() {
    checkBootButton();
    server.handleClient();
    dnsServer.processNextRequest();
    static unsigned long lastConnectionCleanup = 0;
    static unsigned long lastHealingCheck = 0;
    static unsigned long lastDstInvalidate = 0;
    unsigned long now = millis();
    if (timeHasElapsed(now, lastDstInvalidate, DST_CACHE_INVALIDATE_INTERVAL)) {
        lastDstInvalidate = now;
        invalidateDstCache();
    }
    if (timeHasElapsed(now, lastConnectionCleanup, 60000)) {
        lastConnectionCleanup = now;
        if (!scanInProgress) {
            WiFi.scanDelete();
        }
    }
    static unsigned long lastMemClean = 0;
    if (timeHasElapsed(now, lastMemClean, MEMORY_CLEANUP_INTERVAL)) {
        lastMemClean = now;
        cleanupStaleResources();
    }
    if (timeHasElapsed(now, lastHealingCheck, 10000)) {
        lastHealingCheck = now;
        healer.smartRecovery();
    }
    checkAndCleanMemory();
    bool ntpIsFresh = (timeSource == TIME_SOURCE_NTP) &&
                      (lastNTPSync > 0) &&
                      !timeHasElapsed(now, lastNTPSync, getNTPInterval() * 2UL);
    if (rtcPresent && rtcTimeValid && rtcInitialized && !ntpIsFresh) {
        if (timeHasElapsed(now, lastRTCDSync, DS3231_SYNC_INTERVAL)) {
            if (rtc.begin()) {
                DateTime nowDS = rtc.now();
                if (nowDS.year() >= 2020 && nowDS.year() <= 2100) {
                    uint64_t dsEpoch = rtcDateTimeToUint64(nowDS);
                    if (VALID_UNIX_TIME_64(dsEpoch)) {
                        lastRTCDSync = now;
                        internalEpoch = dsEpoch;
                        driftCompensation = 1.0f;
                        rtcMicrosAtLastSync = micros();
                        lastRTCRebase = millis();
                        timeSource = TIME_SOURCE_RTC;
                    }
                }
            } else {
                rtcPresent = false;
                rtcTimeValid = false;
            }
        }
    }
    autoSaveInternalRTC();
    if (scanInProgress) {
        if (timeHasElapsed(now, scanStartTime, 10000UL)) {
            WiFi.scanDelete();
            scanInProgress = false;
            scanResultCount = -1;
        }
    }
    if (wifiConnecting) {
        wl_status_t status = WiFi.status();
        yield();
        server.handleClient();
        if (status == WL_CONNECTED) {
            wifiConnecting = false;
            wifiConnected = true;
            wifiReconnectAttempts = 0;
            wifiGiveUpUntil = 0;
            wifiPausedForScan = false;
            if (ntpAsyncStage == 0 && ntpAsyncState != NTP_STATE_CONNECTING) {
                lastNTPSync = 0;
            }
            tryNTPSync();
        } else if (timeHasElapsed(now, wifiConnectStart, WIFI_CONNECT_TIMEOUT)) {
            wifiConnecting = false;
            wifiConnected = false;
            if (wifiReconnectAttempts >= MAX_RECONNECT) {
                wifiGiveUpUntil = now + 300000UL;
                wifiReconnectAttempts = 0;
            }
        }
    }
    if (wifiPausedForScan && isTimeReached(now, wifiPauseUntil)) {
        wifiPausedForScan = false;
        wifiReconnectAttempts = 0;
        wifiGiveUpUntil = 0;
    }
    if (wifiPausedForScan && timeHasElapsed(now, lastScanAttempt, 30000)) {
        wifiPausedForScan = false;
    }
    if (timeHasElapsed(now, lastWiFiCheck, WIFI_CHECK_INTERVAL)) {
        lastWiFiCheck = now;
        bool shouldReconnect = false;
        if (!wifiPausedForScan && extConfig.sta_enabled) {
            if (!wifiConnecting && !wifiConnected && strlen(sysConfig.sta_ssid) > 0 &&
                (wifiGiveUpUntil == 0 || isTimeReached(now, wifiGiveUpUntil))) {
                shouldReconnect = true;
            }
            if (!wifiConnecting && WiFi.status() != WL_CONNECTED && wifiConnected) {
                wifiConnected = false;
                shouldReconnect = true;
            }
            if (shouldReconnect) {
                beginWiFiConnect();
            }
        }
        if (!wifiConnecting && WiFi.status() == WL_CONNECTED && !wifiConnected && extConfig.sta_enabled) {
            wifiConnected = true;
            wifiReconnectAttempts = 0;
            wifiPausedForScan = false;
            if (ntpAsyncStage == 0 && ntpAsyncState != NTP_STATE_CONNECTING) {
                lastNTPSync = 0;
            }
            tryNTPSync();
        }
    }
    if (wifiConnected && extConfig.sta_enabled) {
        bool doSync = false;
        if (lastNTPSync == 0) {
            doSync = true;
        } else if (ntpFailCount > 0 && timeHasElapsed(now, lastNTPAttempt, NTP_RETRY_INTERVAL)) {
            doSync = true;
        } else if (timeHasElapsed(now, lastNTPSync, getNTPInterval())) {
            doSync = true;
        }
        if (doSync) {
            tryNTPSync();
        }
    } else if (ntpAsyncStage != 0 || ntpAsyncState == NTP_STATE_CONNECTING) {
        if (ntpAsyncStage == 0 || ntpAsyncStage == 1) {
            ntpUDP.stop();
        }
        ntpAsyncStage = 0;
        ntpAsyncState = NTP_STATE_IDLE;
        ntpRetryCount = 0;
    }
    if (ntpAsyncStage != 0 || ntpAsyncState == NTP_STATE_CONNECTING) {
        updateNTPSync();
    }
    if (timeHasElapsed(now, lastScheduleProcess, SCHEDULE_PROCESS_INTERVAL)) {
        lastScheduleProcess = now;
        processRelaySchedules();
    }
    yield();
}

// =============================================================================
//  CONFIGURATION
// =============================================================================
void initDefaults() {
    memset(&sysConfig, 0, sizeof(SystemConfig));
    sysConfig.magic           = EEPROM_MAGIC;
    sysConfig.version         = EEPROM_VERSION;
    strcpy(sysConfig.ap_ssid,     "ESP32S3_16CH_Timer_Switch");
    strcpy(sysConfig.ap_password, "12345678");
    strcpy(sysConfig.ntp_server,  "time.google.com");
    sysConfig.gmt_offset      = 28800;
    sysConfig.daylight_offset = 0;
    sysConfig.last_rtc_epoch  = 0;
    strcpy(sysConfig.tz_name, "Asia/Manila");
    sysConfig.tz_rule_id      = findTzByName("Asia/Manila");
    if (sysConfig.tz_rule_id >= TZ_TABLE_SIZE) sysConfig.tz_rule_id = 0;
    sysConfig.tz_auto_dst     = 1;
    sysConfig.tz_reserved[0]  = 0;
    sysConfig.tz_reserved[1]  = 0;
    for (int i = 0; i < MAX_RELAYS; i++) {
        initScheduleDefaults(i);
    }
    gpioConfig.count = 16;
    gpioConfig.magic = GPIO_CONFIG_MAGIC;
    memcpy(gpioConfig.pins, DEFAULT_RELAY_PINS, sizeof(uint8_t) * 16);
    for (int i = 0; i < MAX_RELAYS; i++) {
        gpioConfig.activeLow[i] = true;
    }
    timeSource = TIME_SOURCE_NONE;
    lastBrowserSync = 0;
    driftCompensation = 1.0f;
    saveConfiguration();
    saveGPIOConfig();
}

void loadConfiguration() {
    preferences.begin(NVS_NAMESPACE, true);
    size_t sysLen   = preferences.getBytes("sysConfig",    &sysConfig,    sizeof(SystemConfig));
    size_t relayLen = preferences.getBytes("relayConfigs", relayConfigs, sizeof(RelayConfig) * MAX_RELAYS);
    preferences.end();
    bool valid = true;
    if (sysLen != sizeof(SystemConfig))                valid = false;
    if (sysConfig.magic != EEPROM_MAGIC)               valid = false;
    if (sysConfig.version != EEPROM_VERSION)           valid = false;
    if (relayLen != sizeof(RelayConfig) * MAX_RELAYS)  valid = false;
    if (valid) {
        for (int i = 0; i < MAX_RELAYS; i++) {
            if (relayConfigs[i].magic != RELAY_CONFIG_MAGIC) {
                valid = false;
                break;
            }
        }
    }
    if (!valid) {
        initDefaults();
    }
}

void saveConfiguration() {
    preferences.begin(NVS_NAMESPACE, false);
    preferences.putBytes("sysConfig", &sysConfig, sizeof(SystemConfig));
    preferences.putBytes("relayConfigs", relayConfigs, sizeof(RelayConfig) * MAX_RELAYS);
    preferences.end();
}

void loadExtConfig() {
    preferences.begin(NVS_NAMESPACE, true);
    size_t len = preferences.getBytes("extConfig", &extConfig, sizeof(ExtConfig));
    preferences.end();
    bool valid = (len == sizeof(ExtConfig)) && (extConfig.magic == EXT_CFG_MAGIC);
    if (!valid) {
        memset(&extConfig, 0, sizeof(ExtConfig));
        extConfig.magic              = EXT_CFG_MAGIC;
        extConfig.ap_channel         = 6;
        extConfig.ntp_sync_hours     = 1;
        extConfig.ap_hidden          = 0;
        extConfig.global_active_mode = 0;
        extConfig.sta_enabled        = 1;
        saveExtConfig();
        return;
    }
    if (extConfig.ap_channel < 1 || extConfig.ap_channel > 13) {
        extConfig.ap_channel = 6;
    }
    if (extConfig.ntp_sync_hours < 1 || extConfig.ntp_sync_hours > 24) {
        extConfig.ntp_sync_hours = 1;
    }
    if (extConfig.global_active_mode > 2) {
        extConfig.global_active_mode = 0;
    }
    if (extConfig.sta_enabled > 1) {
        extConfig.sta_enabled = 1;
    }
}

void saveExtConfig() {
    preferences.begin(NVS_NAMESPACE, false);
    preferences.putBytes("extConfig", &extConfig, sizeof(ExtConfig));
    preferences.end();
}

// =============================================================================
//  WEB SERVER SETUP
// =============================================================================
void setupWebServer() {
    server.on("/",       HTTP_GET, []() { server.send_P(200, "text/html", index_html);  });
    server.on("/wifi",   HTTP_GET, []() { server.send_P(200, "text/html", wifi_html);   });
    server.on("/ntp",    HTTP_GET, []() { server.send_P(200, "text/html", ntp_html);    });
    server.on("/ap",     HTTP_GET, []() { server.send_P(200, "text/html", ap_html);     });
    server.on("/gpio",   HTTP_GET, []() { server.send_P(200, "text/html", gpio_html);   });
    server.on("/system", HTTP_GET, []() { server.send_P(200, "text/html", system_html); });
    server.on("/style.css", HTTP_GET, []() { server.send_P(200, "text/css", style_css); });
    server.on("/api/relays",       HTTP_GET,  handleGetRelays);
    server.on("/api/relay/manual", HTTP_POST, handleManualControl);
    server.on("/api/relay/reset",  HTTP_POST, handleResetManual);
    server.on("/api/relay/save",   HTTP_POST, handleSaveRelay);
    server.on("/api/relay/name",   HTTP_POST, handleRelayName);
    server.on("/api/time", HTTP_GET, handleGetTime);
    server.on("/api/time/browser-sync", HTTP_POST, handleBrowserTimeSync);
    server.on("/api/wifi",        HTTP_GET,  handleGetWiFi);
    server.on("/api/wifi",        HTTP_POST, handleSaveWiFi);
    server.on("/api/wifi/scan",   HTTP_POST, handleWiFiScanStart);
    server.on("/api/wifi/scan",   HTTP_GET,  handleWiFiScanResults);
    server.on("/api/ntp",      HTTP_GET,  handleGetNTP);
    server.on("/api/ntp",      HTTP_POST, handleSaveNTP);
    server.on("/api/ntp/sync", HTTP_POST, handleSyncNTP);
    server.on("/api/timezones", HTTP_GET, []() {
        DynamicJsonDocument doc(4096);
        JsonArray zones = doc.createNestedArray("timezones");
        for (uint8_t i = 0; i < TZ_TABLE_SIZE; i++) {
            JsonObject z = zones.createNestedObject();
            z["id"]     = i;
            z["name"]   = TZ_TABLE[i].name;
            z["offset"] = TZ_TABLE[i].std_offset;
            z["dst"]    = (TZ_TABLE[i].dst_offset != 0);
        }
        String resp;
        serializeJson(doc, resp);
        server.send(200, "application/json", resp);
    });
    server.on("/api/ap", HTTP_GET,  handleGetAP);
    server.on("/api/ap", HTTP_POST, handleSaveAP);
    server.on("/api/gpio",                  HTTP_GET,  handleGetGPIOConfig);
    server.on("/api/gpio/save",             HTTP_POST, handleSaveGPIOConfig);
    server.on("/api/gpio/add",              HTTP_POST, handleAddGPIO);
    server.on("/api/gpio/delete",           HTTP_POST, handleDeleteGPIO);
    server.on("/api/gpio/toggle-active-low", HTTP_POST, handleToggleActiveLow);
    server.on("/api/gpio/global-mode", HTTP_GET, []() {
    server.send(200, "application/json",
        "{\"mode\":" + String(extConfig.global_active_mode) + "}");
    });
    server.on("/api/gpio/global-mode",      HTTP_POST, handleGlobalActiveMode);
    server.on("/api/system",        HTTP_GET,  handleGetSystem);
    server.on("/api/reset",         HTTP_POST, handleReset);
    server.on("/api/factory-reset", HTTP_POST, handleFactoryReset);
    server.on("/hotspot-detect.html", HTTP_GET, []() {
        server.send(200, "text/html",
            "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
    });
    server.on("/library/test/success.html", HTTP_GET, []() {
        server.send(200, "text/html",
            "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
    });
    server.on("/generate_204", HTTP_GET, []() {
        server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
        server.send(302, "text/plain", "");
    });
    server.on("/success.txt",   HTTP_GET, []() { server.send(200, "text/plain", "success\n"); });
    server.on("/canonical.html", HTTP_GET, []() {
        server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
        server.send(302, "text/plain", "");
    });
    server.on("/connecttest.txt", HTTP_GET, []() {
        server.send(200, "text/plain", "Microsoft Connect Test");
    });
    server.on("/ncsi.txt", HTTP_GET, []() {
        server.send(200, "text/plain", "Microsoft NCSI");
    });
    server.on("/redirect", HTTP_GET, []() {
        server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
        server.send(302, "text/plain", "");
    });
    server.onNotFound([]() {
        server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
        server.send(302, "text/plain", "");
    });
    server.begin();
}

// =============================================================================
//  API HANDLERS
// =============================================================================
void handleGetRelays() {
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "application/json", "");
    server.sendContent("[");
    if (gpioConfig.count > 0) {
        DynamicJsonDocument relayDoc(4096);
        for (int i = 0; i < gpioConfig.count; i++) {
            relayDoc.clear();
            if (i > 0) server.sendContent(",");
            relayDoc["state"] = lastRelayOutputs[i];
            relayDoc["manual"] = relayConfigs[i].manualOverride;
            relayDoc["name"] = String(relayConfigs[i].name);
            relayDoc["pin"] = getRelayPin(i);
            JsonArray schedules = relayDoc.createNestedArray("schedules");
            for (int s = 0; s < 8; s++) {
                JsonObject sch = schedules.createNestedObject();
                sch["startHour"] = relayConfigs[i].schedule.startHour[s];
                sch["startMinute"] = relayConfigs[i].schedule.startMinute[s];
                sch["startSecond"] = relayConfigs[i].schedule.startSecond[s];
                sch["stopHour"] = relayConfigs[i].schedule.stopHour[s];
                sch["stopMinute"] = relayConfigs[i].schedule.stopMinute[s];
                sch["stopSecond"] = relayConfigs[i].schedule.stopSecond[s];
                sch["enabled"] = relayConfigs[i].schedule.enabled[s];
                sch["days"] = relayConfigs[i].schedule.days[s];
                sch["monthDays"] = relayConfigs[i].schedule.monthDays[s];
                sch["monthMask"] = relayConfigs[i].schedule.monthMask[s];
            }
            String chunk;
            serializeJson(relayDoc, chunk);
            server.sendContent(chunk);
            yield();
        }
    }
    server.sendContent("]");
}

void handleManualControl() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"No data\"}");
        return;
    }
    StaticJsonDocument<128> doc;
    DeserializationError err = deserializeJson(doc, server.arg("plain"));
    if (err) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Bad JSON\"}");
        return;
    }
    if (!doc.containsKey("relay") || !doc["relay"].is<int>() ||
        !doc.containsKey("state") || !doc["state"].is<bool>()) {
        server.send(400, "application/json",
            "{\"success\":false,\"error\":\"Missing or invalid relay/state\"}");
        return;
    }
    int relay = doc["relay"];
    bool state = doc["state"];
    if (relay >= 0 && relay < gpioConfig.count) {
        bool needsSave = (!relayConfigs[relay].manualOverride) ||
                         (relayConfigs[relay].manualState != state);
        relayConfigs[relay].manualOverride = true;
        relayConfigs[relay].manualState    = state;
        scheduleActiveCache[relay] = false;
        setRelayOutput(relay, state);
        lastRelayOutputs[relay] = state;
        lastDebouncedStateGlobal[relay] = state;
        lastStateChangeGlobal[relay] = millis();
        if (needsSave) {
            saveConfiguration();
        }
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid relay\"}");
    }
}

void handleResetManual() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"No data\"}");
        return;
    }
    StaticJsonDocument<64> doc;
    DeserializationError err = deserializeJson(doc, server.arg("plain"));
    if (err) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Bad JSON\"}");
        return;
    }
    if (!doc.containsKey("relay") || !doc["relay"].is<int>()) {
        server.send(400, "application/json",
            "{\"success\":false,\"error\":\"Missing or invalid relay\"}");
        return;
    }
    int relay = doc["relay"];
    if (relay >= 0 && relay < gpioConfig.count) {
        relayConfigs[relay].manualOverride = false;
        updateScheduleCache();
        bool targetState = scheduleActiveCache[relay];
        setRelayOutput(relay, targetState);
        lastRelayOutputs[relay] = targetState;
        lastDebouncedStateGlobal[relay] = targetState;
        lastStateChangeGlobal[relay] = millis();
        saveConfiguration();
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid relay\"}");
    }
}

void handleSaveRelay() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"No data\"}");
        return;
    }
    DynamicJsonDocument doc(4096);
    DeserializationError err = deserializeJson(doc, server.arg("plain"));
    if (err) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Bad JSON\"}");
        return;
    }
    if (!doc.containsKey("relay") || !doc["relay"].is<int>()) {
        server.send(400, "application/json",
            "{\"success\":false,\"error\":\"Missing or invalid relay\"}");
        return;
    }
    int relay = doc["relay"];
    if (relay < 0 || relay >= gpioConfig.count) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid relay\"}");
        return;
    }
    if (!doc.containsKey("schedules") || !doc["schedules"].is<JsonArray>()) {
        server.send(400, "application/json",
            "{\"success\":false,\"error\":\"Missing or invalid schedules\"}");
        return;
    }
    JsonArray schedules = doc["schedules"].as<JsonArray>();
    int s = 0;
    for (JsonObject sch : schedules) {
        if (s >= 8) break;
        int32_t shRaw = sch["startHour"]   | 0;
        int32_t smRaw = sch["startMinute"] | 0;
        int32_t ssRaw = sch["startSecond"] | 0;
        int32_t ehRaw = sch["stopHour"]    | 0;
        int32_t emRaw = sch["stopMinute"]  | 0;
        int32_t esRaw = sch["stopSecond"]  | 0;
        uint8_t sh = (uint8_t)((shRaw < 0) ? 0 : (shRaw > 23) ? 23 : shRaw);
        uint8_t sm = (uint8_t)((smRaw < 0) ? 0 : (smRaw > 59) ? 59 : smRaw);
        uint8_t ss = (uint8_t)((ssRaw < 0) ? 0 : (ssRaw > 59) ? 59 : ssRaw);
        uint8_t eh = (uint8_t)((ehRaw < 0) ? 0 : (ehRaw > 23) ? 23 : ehRaw);
        uint8_t em = (uint8_t)((emRaw < 0) ? 0 : (emRaw > 59) ? 59 : emRaw);
        uint8_t es = (uint8_t)((esRaw < 0) ? 0 : (esRaw > 59) ? 59 : esRaw);
        relayConfigs[relay].schedule.startHour[s]   = sh;
        relayConfigs[relay].schedule.startMinute[s] = sm;
        relayConfigs[relay].schedule.startSecond[s] = ss;
        relayConfigs[relay].schedule.stopHour[s]    = eh;
        relayConfigs[relay].schedule.stopMinute[s]  = em;
        relayConfigs[relay].schedule.stopSecond[s]  = es;
        if (sch.containsKey("enabled")) {
            relayConfigs[relay].schedule.enabled[s] = sch["enabled"].as<bool>();
        } else {
            relayConfigs[relay].schedule.enabled[s] = false;
        }
        if (sch.containsKey("days")) {
            relayConfigs[relay].schedule.days[s] = sch["days"].as<uint8_t>() & 0x7F;
        } else {
            relayConfigs[relay].schedule.days[s] = DAY_ALL;
        }
        uint32_t rawMonthDays = 0x7FFFFFFFUL;
        if (sch.containsKey("monthDays") && sch["monthDays"].is<uint32_t>()) {
            rawMonthDays = sch["monthDays"].as<uint32_t>();
        }
        relayConfigs[relay].schedule.monthDays[s] = rawMonthDays & 0x7FFFFFFFUL;
        if (sch.containsKey("monthMask")) {
            relayConfigs[relay].schedule.monthMask[s] = sch["monthMask"].as<uint16_t>() & 0x0FFF;
        } else {
            relayConfigs[relay].schedule.monthMask[s] = MONTH_ALL;
        }
        s++;
    }
    for (; s < 8; s++) {
        relayConfigs[relay].schedule.startHour[s]   = 0;
        relayConfigs[relay].schedule.startMinute[s] = 0;
        relayConfigs[relay].schedule.startSecond[s] = 0;
        relayConfigs[relay].schedule.stopHour[s]    = 0;
        relayConfigs[relay].schedule.stopMinute[s]  = 0;
        relayConfigs[relay].schedule.stopSecond[s]  = 0;
        relayConfigs[relay].schedule.enabled[s]     = false;
        relayConfigs[relay].schedule.days[s]        = DAY_ALL;
        relayConfigs[relay].schedule.monthDays[s]   = 0x7FFFFFFFUL;
        relayConfigs[relay].schedule.monthMask[s]   = MONTH_ALL;
    }
    saveConfiguration();
    updateScheduleCache();
    server.send(200, "application/json", "{\"success\":true}");
}

void handleRelayName() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"No data\"}");
        return;
    }
    StaticJsonDocument<128> doc;
    DeserializationError err = deserializeJson(doc, server.arg("plain"));
    if (err) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Bad JSON\"}");
        return;
    }
    if (!doc.containsKey("relay") || !doc["relay"].is<int>()) {
        server.send(400, "application/json",
            "{\"success\":false,\"error\":\"Missing or invalid relay\"}");
        return;
    }
    int relay = doc["relay"];
    const char* name = doc["name"];
    if (relay >= 0 && relay < gpioConfig.count && name) {
        size_t nlen = strlen(name);
        if (nlen == 0) {
            server.send(400, "application/json",
                "{\"success\":false,\"error\":\"Name cannot be empty\"}");
            return;
        }
        if (nlen > 15) {
            nlen = 15;
            while (nlen > 0 && (((uint8_t)name[nlen]) & 0xC0) == 0x80) {
                nlen--;
            }
            if (nlen == 0) {
                const char* def = "Relay";
                memcpy(relayConfigs[relay].name, def, strlen(def));
                relayConfigs[relay].name[strlen(def)] = '\0';
                saveConfiguration();
                server.send(200, "application/json", "{\"success\":true}");
                return;
            }
        }
        memcpy(relayConfigs[relay].name, name, nlen);
        relayConfigs[relay].name[nlen] = '\0';
        saveConfiguration();
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid data\"}");
    }
}

void handleGetTime() {
    String ts = "--:--:--";
    String ds = "--/--/----";
    String ws = "";
    uint64_t utcEp = getCurrentEpoch();
    if (utcEp >= MIN_UNIX_TIME_64) {
        uint64_t localEp = getLocalEpoch(utcEp);
        struct tm* t = gmtime64(&localEp);
        if (t) {
            char buf[12];
            sprintf(buf, "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
            ts = buf;
            char dbuf[13];
            sprintf(dbuf, "%02d/%02d/%04d", t->tm_mon + 1, t->tm_mday,
                    t->tm_year + 1900);
            ds = dbuf;
            static const char* WD[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
            if (t->tm_wday >= 0 && t->tm_wday <= 6) ws = WD[t->tm_wday];
        }
    }
    String timeSourceStr = "none";
    if (timeSource == TIME_SOURCE_NTP) timeSourceStr = "ntp";
    else if (timeSource == TIME_SOURCE_BROWSER) timeSourceStr = "browser";
    else if (timeSource == TIME_SOURCE_RTC) timeSourceStr = "rtc";
    unsigned long rtcSyncAge = (lastRTCDSync > 0)
        ? (millis() - lastRTCDSync) / 1000UL
        : 0xFFFFFFFFUL;
    unsigned long ntpSyncAge = (lastNTPSync > 0)
        ? (millis() - lastNTPSync) / 1000UL
        : 0xFFFFFFFFUL;
    String authorityStr = "none";
    if (utcEp < MIN_UNIX_TIME_64) {
        authorityStr = "none";
    } else if (timeSource == TIME_SOURCE_NTP && lastNTPSync > 0 &&
               !timeHasElapsed(millis(), lastNTPSync, getNTPInterval() * 2UL)) {
        authorityStr = "ntp";
    } else if (timeSource == TIME_SOURCE_BROWSER && lastBrowserSync > 0 &&
               !timeHasElapsed(millis(), lastBrowserSync, 86400000UL)) {
        authorityStr = "browser";
    } else if (rtcPresent && rtcTimeValid) {
        authorityStr = "ds3231";
    } else if (rtcInitialized && internalEpoch > 0) {
        authorityStr = "internal_rtc";
    } else {
        authorityStr = "none";
    }
    bool authorityStale = false;
    if (authorityStr == "ntp" && ntpSyncAge != 0xFFFFFFFFUL &&
        timeHasElapsed(millis(), lastNTPSync, getNTPInterval())) {
        authorityStale = true;
    }
    int32_t dstOff = (utcEp >= MIN_UNIX_TIME_64) ? currentDstOffset(utcEp) : 0;
    bool dstOn = (dstOff != 0);
    String tzName = String(sysConfig.tz_name);
    String resp = "{\"time\":\"" + ts + "\",\"date\":\"" + ds + "\",\"weekday\":\"" + ws + "\",\"wifi\":" +
                  String(wifiConnected ? "true" : "false") + ",\"ntp\":" +
                  String((timeSource == TIME_SOURCE_NTP) ? "true" : "false") +
                  ",\"timeSource\":\"" + timeSourceStr +
                  "\",\"authority\":\"" + authorityStr +
                  "\",\"authorityStale\":" + String(authorityStale ? "true" : "false") +
                  ",\"rtcPresent\":" + String(rtcPresent ? "true" : "false") +
                  ",\"rtcSynced\":" + String(rtcTimeValid ? "true" : "false") +
                  ",\"rtcSyncAge\":" + String(rtcSyncAge) +
                  ",\"ntpSyncAge\":" + String(ntpSyncAge) +
                  ",\"dstActive\":" + String(dstOn ? "true" : "false") +
                  ",\"dstOffset\":" + String(dstOff) +
                  ",\"tzName\":\"" + tzName + "\"}";
    server.send(200, "application/json", resp);
}

void handleGetWiFi() {
    DynamicJsonDocument doc(384);
    doc["ssid"] = sysConfig.sta_ssid;
    doc["connected"] = wifiConnected;
    doc["ip"] = WiFi.localIP().toString();
    doc["rssi"] = wifiConnected ? (int)WiFi.RSSI() : 0;
    doc["sta_enabled"] = extConfig.sta_enabled ? true : false;
    String resp;
    serializeJson(doc, resp);
    server.send(200, "application/json", resp);
}

void handleSaveWiFi() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"No data\"}");
        return;
    }
    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, server.arg("plain"));
    if (err) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Bad JSON\"}");
        return;
    }
    if (doc.containsKey("sta_enabled")) {
        bool enable = doc["sta_enabled"];
        setWiFiStationEnabled(enable);
        server.send(200, "application/json", "{\"success\":true,\"sta_enabled\":" + String(enable ? "true" : "false") + "}");
        return;
    }
    const char* ssidRaw = doc["ssid"];
    if (!ssidRaw) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid SSID\"}");
        return;
    }
    char ssidTrim[64];
    size_t rawLen = strlen(ssidRaw);
    if (rawLen >= sizeof(ssidTrim)) rawLen = sizeof(ssidTrim) - 1;
    size_t s0 = 0;
    while (s0 < rawLen && isspace((unsigned char)ssidRaw[s0])) s0++;
    size_t s1 = rawLen;
    while (s1 > s0 && isspace((unsigned char)ssidRaw[s1 - 1])) s1--;
    size_t outLen = s1 - s0;
    memcpy(ssidTrim, ssidRaw + s0, outLen);
    ssidTrim[outLen] = '\0';
    const char* ssid = ssidTrim;
    if (strlen(ssid) > 0 && strlen(ssid) < 32) {
        bool ssidChanged = (strcmp(sysConfig.sta_ssid, ssid) != 0);
        bool passChanged = false;
        strncpy(sysConfig.sta_ssid, ssid, 31);
        sysConfig.sta_ssid[31] = '\0';
        if (doc.containsKey("password")) {
            if (doc["password"].is<const char*>()) {
                const char* pw = doc["password"];
                if (pw && strlen(pw) > 0) {
                    passChanged = (strcmp(sysConfig.sta_password, pw) != 0);
                    strncpy(sysConfig.sta_password, pw, 63);
                    sysConfig.sta_password[63] = '\0';
                } else {
                    passChanged = (sysConfig.sta_password[0] != '\0');
                    sysConfig.sta_password[0] = '\0';
                }
            } else {
                passChanged = (sysConfig.sta_password[0] != '\0');
                sysConfig.sta_password[0] = '\0';
            }
        } else if (ssidChanged) {
            passChanged = (sysConfig.sta_password[0] != '\0');
            sysConfig.sta_password[0] = '\0';
        }
        saveConfiguration();
        if (extConfig.sta_enabled && (ssidChanged || passChanged)) {
            wifiPausedForScan = false;
            WiFi.disconnect(false, false);
            delay(500);
            wifiConnected = false;
            wifiConnecting = true;
            wifiConnectStart = millis();
            wifiReconnectAttempts = 0;
            wifiGiveUpUntil = 0;
            if (WiFi.getMode() != WIFI_AP_STA) {
                WiFi.mode(WIFI_AP_STA);
            }
            WiFi.begin(sysConfig.sta_ssid, sysConfig.sta_password);
        }
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid SSID\"}");
    }
}

void handleWiFiScanStart() {
    if (!extConfig.sta_enabled) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"WiFi station is disabled\"}");
        return;
    }
    if (wifiConnecting && !wifiConnected && !wifiPausedForScan) {
        pauseWiFiForScan();
    }
    if (!scanInProgress) {
        scanInProgress  = true;
        scanResultCount = -1;
        scanStartTime   = millis();
        if (WiFi.getMode() == WIFI_AP) {
            WiFi.mode(WIFI_AP_STA);
        }
        WiFi.scanNetworks(true, true);
    }
    server.send(202, "application/json", "{\"scanning\":true}");
}

void handleWiFiScanResults() {
    if (scanInProgress) {
        int n = WiFi.scanComplete();
        if (n == WIFI_SCAN_RUNNING) {
            server.send(200, "application/json", "{\"scanning\":true}");
            return;
        }
        scanResultCount = (n >= 0) ? n : -1;
        scanInProgress  = false;
        if (wifiPausedForScan) {
            wifiPauseUntil = millis() + 5000;
        }
    }
    if (scanResultCount < 0) {
        server.send(200, "application/json", "{\"scanning\":false,\"networks\":[]}");
        return;
    }
    DynamicJsonDocument doc(8192);
    doc["scanning"] = false;
    JsonArray nets = doc.createNestedArray("networks");
    for (int i = 0; i < scanResultCount && i < 30; i++) {
        String ssid = WiFi.SSID(i);
        if (ssid.length() == 0) continue;
        JsonObject n = nets.createNestedObject();
        n["ssid"] = ssid;
        n["rssi"] = WiFi.RSSI(i);
        n["enc"]  = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }
    WiFi.scanDelete();
    scanResultCount = -1;
    String resp;
    serializeJson(doc, resp);
    server.send(200, "application/json", resp);
}

void handleGetNTP() {
    DynamicJsonDocument doc(512);
    doc["ntpServer"] = sysConfig.ntp_server;
    doc["gmtOffset"] = sysConfig.gmt_offset;
    doc["daylightOffset"] = sysConfig.daylight_offset;
    doc["syncHours"] = extConfig.ntp_sync_hours;
    doc["globalActiveMode"] = extConfig.global_active_mode;
    doc["tzName"] = sysConfig.tz_name;
    doc["tzRuleId"] = sysConfig.tz_rule_id;
    doc["tzAutoDst"] = sysConfig.tz_auto_dst ? 1 : 0;
    uint64_t now = getCurrentEpoch();
    int32_t dstOff = (now >= MIN_UNIX_TIME_64) ? currentDstOffset(now) : 0;
    doc["dstOffset"] = dstOff;
    doc["dstActive"] = (dstOff != 0);
    String resp;
    serializeJson(doc, resp);
    server.send(200, "application/json", resp);
}

void handleSaveNTP() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"No data\"}");
        return;
    }
    StaticJsonDocument<384> doc;
    DeserializationError err = deserializeJson(doc, server.arg("plain"));
    if (err) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Bad JSON\"}");
        return;
    }
    bool hasNtpServer = doc.containsKey("ntpServer");
    bool hasTzName    = doc.containsKey("tzName");
    bool hasGmtOffset = doc.containsKey("gmtOffset") && !hasTzName;
    bool hasDstOffset = doc.containsKey("daylightOffset") && !hasTzName;
    bool hasSyncHours = doc.containsKey("syncHours");
    if (hasNtpServer) {
        const char* srv = doc["ntpServer"];
        if (!srv || strlen(srv) == 0 || strlen(srv) >= 48) {
            server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid NTP server\"}");
            return;
        }
    }
    uint8_t newTzId = 0;
    const char* newTzName = nullptr;
    if (hasTzName) {
        newTzName = doc["tzName"];
        if (!newTzName || strlen(newTzName) == 0 || strlen(newTzName) >= 32) {
            server.send(400, "application/json",
                "{\"success\":false,\"error\":\"Invalid timezone name\"}");
            return;
        }
        if (strcmp(newTzName, "Manual") != 0) {
            newTzId = findTzByName(newTzName);
            if (newTzId >= TZ_TABLE_SIZE) {
                server.send(400, "application/json",
                    "{\"success\":false,\"error\":\"Unknown timezone\"}");
                return;
            }
        } else {
            newTzId = TZ_TABLE_SIZE;
        }
    }
    int32_t newGmtOffset = 0;
    if (hasGmtOffset) {
        newGmtOffset = doc["gmtOffset"] | 0;
        if (newGmtOffset > 50400) newGmtOffset = 50400;
        if (newGmtOffset < -50400) newGmtOffset = -50400;
    }
    int32_t newDstOffset = 0;
    if (hasDstOffset) {
        newDstOffset = doc["daylightOffset"] | 0;
        if (newDstOffset > 7200) newDstOffset = 7200;
        if (newDstOffset < -7200) newDstOffset = -7200;
    }
    uint8_t newSyncHours = 0;
    if (hasSyncHours) {
        int32_t syncHoursRaw = doc["syncHours"] | 0;
        if (syncHoursRaw < 1 || syncHoursRaw > 24) {
            server.send(400, "application/json",
                "{\"success\":false,\"error\":\"syncHours must be 1-24\"}");
            return;
        }
        newSyncHours = (uint8_t)syncHoursRaw;
    }
    bool changed = false;
    bool tzChanged = false;
    if (hasNtpServer) {
        strncpy(sysConfig.ntp_server, doc["ntpServer"], 47);
        sysConfig.ntp_server[47] = '\0';
        changed = true;
    }
    if (newTzName) {
        strncpy(sysConfig.tz_name, newTzName, 31);
        sysConfig.tz_name[31] = '\0';
        if (newTzId < TZ_TABLE_SIZE) {
            sysConfig.tz_rule_id  = newTzId;
            sysConfig.gmt_offset  = TZ_TABLE[newTzId].std_offset;
            sysConfig.daylight_offset = 0;
            sysConfig.tz_auto_dst = 1;
        } else {
            sysConfig.tz_rule_id  = TZ_TABLE_SIZE;
            sysConfig.tz_auto_dst = 0;
            sysConfig.daylight_offset = 0;
        }
        changed = true;
        tzChanged = true;
    }
    if (hasGmtOffset) {
        sysConfig.gmt_offset = newGmtOffset;
        strcpy(sysConfig.tz_name, "Manual");
        sysConfig.tz_auto_dst = 0;
        changed = true;
        tzChanged = true;
    }
    if (hasDstOffset) {
        sysConfig.daylight_offset = newDstOffset;
        changed = true;
        tzChanged = true;
    }
    if (hasSyncHours) {
        extConfig.ntp_sync_hours = newSyncHours;
        saveExtConfig();
        changed = true;
    }
    if (!changed) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"No fields to update\"}");
        return;
    }
    saveConfiguration();
    if (tzChanged) {
        invalidateDstCache();
        updateScheduleCache();
    }
    if (hasNtpServer) {
        ntpAsyncStage = 0;
        ntpAsyncState = NTP_STATE_IDLE;
        ntpRetryCount = 0;
        lastNTPSync = 0;
        lastNTPAttempt = 0;
    }
    server.send(200, "application/json", "{\"success\":true}");
}

void handleSyncNTP() {
    if (!wifiConnected || !extConfig.sta_enabled) {
        server.send(400, "application/json",
            "{\"success\":false,\"error\":\"WiFi not connected or station disabled\"}");
        return;
    }
    if (ntpAsyncStage != 0 || ntpAsyncState == NTP_STATE_CONNECTING) {
        server.send(200, "application/json",
            "{\"success\":true,\"message\":\"NTP sync already in progress\"}");
        return;
    }
    lastNTPSync = 0;
    lastNTPAttempt = 0;
    server.send(200, "application/json", "{\"success\":true,\"message\":\"NTP sync initiated\"}");
}

void handleGetAP() {
    DynamicJsonDocument doc(256);
    doc["ap_ssid"] = sysConfig.ap_ssid;
    doc["ap_password"] = "";
    doc["ap_channel"] = extConfig.ap_channel;
    doc["ap_hidden"] = extConfig.ap_hidden ? true : false;
    String resp;
    serializeJson(doc, resp);
    server.send(200, "application/json", resp);
}

void handleSaveAP() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"No data\"}");
        return;
    }
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, server.arg("plain"));
    if (err) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Bad JSON\"}");
        return;
    }
    const char* ssid = doc["ap_ssid"];
    const char* pw   = doc["ap_password"];
    bool ssidChanged = false;
    bool passChanged = false;
    bool channelChanged = false;
    bool hiddenChanged = false;
    if (ssid && strlen(ssid) > 0 && strlen(ssid) < 32) {
        ssidChanged = (strcmp(sysConfig.ap_ssid, ssid) != 0);
        strncpy(sysConfig.ap_ssid, ssid, 31);
        sysConfig.ap_ssid[31] = '\0';
    } else if (ssid && strlen(ssid) == 0) {
        server.send(400, "application/json",
            "{\"success\":false,\"error\":\"SSID cannot be empty\"}");
        return;
    } else if (ssid && strlen(ssid) >= 32) {
        server.send(400, "application/json",
            "{\"success\":false,\"error\":\"SSID must be 31 characters or less\"}");
        return;
    }
    if (pw) {
        size_t pwLen = strlen(pw);
        if (pwLen == 0) {
            if (doc.containsKey("ap_password")) {
                passChanged = (sysConfig.ap_password[0] != '\0');
                sysConfig.ap_password[0] = '\0';
            }
        } else if (pwLen >= 8 && pwLen <= 31) {
            passChanged = (strcmp(sysConfig.ap_password, pw) != 0);
            strncpy(sysConfig.ap_password, pw, 31);
            sysConfig.ap_password[31] = '\0';
        } else if (pwLen > 31) {
            server.send(400, "application/json",
                "{\"success\":false,\"error\":\"Password must be 31 characters or less\"}");
            return;
        } else {
            server.send(400, "application/json",
                "{\"success\":false,\"error\":\"Password must be 8-31 characters or empty\"}");
            return;
        }
    }
    if (doc.containsKey("ap_channel")) {
        int chRaw = doc["ap_channel"] | 0;
        if (chRaw < 1 || chRaw > 13) {
            server.send(400, "application/json",
                "{\"success\":false,\"error\":\"Channel must be 1-13\"}");
            return;
        }
        uint8_t ch = (uint8_t)chRaw;
        if (ch != extConfig.ap_channel) {
            extConfig.ap_channel = ch;
            channelChanged = true;
        }
    }
    if (doc.containsKey("ap_hidden")) {
        bool newHidden = doc["ap_hidden"] ? 1 : 0;
        if (newHidden != extConfig.ap_hidden) {
            extConfig.ap_hidden = newHidden;
            hiddenChanged = true;
        }
    }
    if (ssidChanged || passChanged) saveConfiguration();
    if (channelChanged || hiddenChanged) saveExtConfig();
    bool needsRestart = ssidChanged || passChanged || channelChanged || hiddenChanged;
    if (needsRestart) {
        server.send(200, "application/json", "{\"success\":true,\"restarted\":true}");
        server.client().flush();
        delay(200);
        yield();
        server.client().flush();
        delay(1200);
        yield();
        server.client().flush();
        healer.restartAPIfNeeded(true);
    } else {
        server.send(200, "application/json", "{\"success\":true,\"restarted\":false}");
    }
}

void handleGetSystem() {
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "0");
    DynamicJsonDocument doc(3072);
    doc["ip"] = WiFi.localIP().toString();
    doc["ap_ip"] = WiFi.softAPIP().toString();
    doc["uptime"] = (uint64_t)(esp_timer_get_time() / 1000000ULL);
    doc["freeHeap"] = ESP.getFreeHeap();
    char epochStr[32];
    snprintf(epochStr, sizeof(epochStr), "%llu", (unsigned long long)getCurrentEpoch());
    doc["utcEpoch"] = epochStr;
    String timeSourceStr = "None";
    if (timeSource == TIME_SOURCE_NTP) timeSourceStr = "NTP";
    else if (timeSource == TIME_SOURCE_BROWSER) timeSourceStr = "Browser";
    else if (timeSource == TIME_SOURCE_RTC) timeSourceStr = rtcPresent ? "DS3231" : "Internal RTC";
    doc["timeSource"] = timeSourceStr;
    uint64_t _authNowEp = getCurrentEpoch();
    String authorityStr = "none";
    bool authorityStale = false;
    if (_authNowEp < MIN_UNIX_TIME_64) {
        authorityStr = "none";
    } else if (timeSource == TIME_SOURCE_NTP && lastNTPSync > 0 &&
               !timeHasElapsed(millis(), lastNTPSync, getNTPInterval() * 2UL)) {
        authorityStr = "ntp";
        if (timeHasElapsed(millis(), lastNTPSync, getNTPInterval())) authorityStale = true;
    } else if (timeSource == TIME_SOURCE_BROWSER && lastBrowserSync > 0 &&
               !timeHasElapsed(millis(), lastBrowserSync, 86400000UL)) {
        authorityStr = "browser";
    } else if (rtcPresent && rtcTimeValid) {
        authorityStr = "ds3231";
    } else if (rtcInitialized && internalEpoch > 0) {
        authorityStr = "internal_rtc";
    } else {
        authorityStr = "none";
    }
    doc["authority"] = authorityStr;
    doc["authorityStale"] = authorityStale;
    if (timeSource == TIME_SOURCE_NTP) {
        doc["syncDirection"] = rtcPresent ? "NTP \u2192 RTC \u2192 DS3231" : "NTP \u2192 Internal RTC";
    } else if (timeSource == TIME_SOURCE_BROWSER) {
        doc["syncDirection"] = rtcPresent ? "Browser \u2192 RTC \u2192 DS3231" : "Browser \u2192 Internal RTC";
    } else if (timeSource == TIME_SOURCE_RTC && rtcPresent) {
        doc["syncDirection"] = "DS3231 \u2192 Internal RTC";
    } else if (timeSource == TIME_SOURCE_RTC && !rtcPresent) {
        doc["syncDirection"] = "Internal RTC (NVS)";
    } else {
        doc["syncDirection"] = "None";
    }
    doc["ntpServer"] = sysConfig.ntp_server;
    doc["ntpSyncAge"] = lastNTPSync > 0 ? (unsigned long)((millis() - lastNTPSync) / 1000UL) : (unsigned long)0xFFFFFFFF;
    doc["browserSyncAge"] = lastBrowserSync > 0 ? (unsigned long)((millis() - lastBrowserSync) / 1000UL) : (unsigned long)0xFFFFFFFF;
    doc["rtcSyncAge"] = lastRTCDSync > 0 ? (unsigned long)((millis() - lastRTCDSync) / 1000UL) : (unsigned long)0xFFFFFFFF;
    doc["wifiConnected"] = wifiConnected;
    doc["wifiSSID"] = sysConfig.sta_ssid;
    doc["rssi"] = wifiConnected ? (int)WiFi.RSSI() : 0;
    doc["version"] = EEPROM_VERSION;
#if defined(ESP_ARDUINO_VERSION_MAJOR) && defined(ESP_ARDUINO_VERSION_MINOR)
  #if (ESP_ARDUINO_VERSION_MAJOR > 2) || \
      (ESP_ARDUINO_VERSION_MAJOR == 2 && ESP_ARDUINO_VERSION_MINOR >= 1)
    {
        const char* cm = ESP.getChipModel();
        doc["chipModel"] = (cm && cm[0]) ? cm : "ESP32S3";
    }
  #else
    doc["chipModel"] = "ESP32S3";
  #endif
#else
    doc["chipModel"] = "ESP32S3";
#endif
    doc["mdnsHostname"] = getMDNSHostname();
    doc["mdnsStarted"] = mdnsStarted;
    doc["relayCount"] = gpioConfig.count;
    doc["maxRelays"] = MAX_RELAYS;
    doc["gmtOffset"] = sysConfig.gmt_offset;
    doc["driftComp"] = driftCompensation;
    doc["globalActiveMode"] = extConfig.global_active_mode;
    doc["rtcPresent"] = rtcPresent;
    doc["staEnabled"] = extConfig.sta_enabled ? true : false;
    doc["tzName"] = sysConfig.tz_name;
    doc["tzAutoDst"] = sysConfig.tz_auto_dst ? 1 : 0;
    uint64_t nowEp = getCurrentEpoch();
    int32_t dstOff = (nowEp >= MIN_UNIX_TIME_64) ? currentDstOffset(nowEp) : 0;
    doc["dstActive"] = (dstOff != 0);
    doc["dstOffset"] = dstOff;
    float rtcTempVal = getRTCTemperature();
    if (isnan(rtcTempVal)) {
        doc["rtcTemp"] = nullptr;
    } else {
        doc["rtcTemp"] = rtcTempVal;
    }
    String resp;
    serializeJson(doc, resp);
    server.send(200, "application/json", resp);
}

void handleReset() {
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Performing live service verification...\"}");
    healer.performTargetedRecovery();
}

void handleFactoryReset() {
    server.send(200, "application/json",
        "{\"success\":true,\"message\":\"Factory reset complete. Device will restart with default settings.\"}");
    server.client().flush();
    delay(500);
    preferences.begin(NVS_NAMESPACE, false);
    preferences.clear();
    preferences.end();
    delay(100);
    ESP.restart();
}

// =============================================================================
//  GPIO MANAGEMENT HANDLERS
// =============================================================================
void handleGetGPIOConfig() {
    DynamicJsonDocument doc(2048);
    doc["count"] = gpioConfig.count;
    doc["maxRelays"] = MAX_RELAYS;
    JsonArray pins = doc.createNestedArray("pins");
    JsonArray activeLow = doc.createNestedArray("activeLow");
    for (int i = 0; i < gpioConfig.count; i++) {
        pins.add(gpioConfig.pins[i]);
        activeLow.add(gpioConfig.activeLow[i]);
    }
    JsonArray available = doc.createNestedArray("availablePins");
    for (int p = 0; p < 16; p++) {
        available.add(DEFAULT_RELAY_PINS[p]);
    }
    String resp;
    serializeJson(doc, resp);
    server.send(200, "application/json", resp);
}

void handleSaveGPIOConfig() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"No data\"}");
        return;
    }
    StaticJsonDocument<1024> doc;
    DeserializationError err = deserializeJson(doc, server.arg("plain"));
    if (err) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Bad JSON\"}");
        return;
    }
    if (!doc.containsKey("pins") || !doc["pins"].is<JsonArray>()) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Missing or invalid pins array\"}");
        return;
    }
    JsonArray pins = doc["pins"].as<JsonArray>();
    uint8_t newCount = pins.size();
    if (newCount == 0) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Pins array cannot be empty\"}");
        return;
    }
    if (newCount > MAX_RELAYS) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Too many pins\"}");
        return;
    }
    // change gpio
    static const uint8_t SAFE_PINS[] = {4, 5, 6, 7, 11, 12, 13, 14, 1, 2, 42, 41, 47, 21, 20, 19};
    uint8_t parsed[MAX_RELAYS] = {0};
    uint8_t pi = 0;
    for (JsonVariant pv : pins) {
        if (pi >= MAX_RELAYS) break;
        parsed[pi++] = pv.as<uint8_t>();
    }
    for (uint8_t i = 0; i < newCount; i++) {
        bool allowed = false;
        for (uint8_t sp : SAFE_PINS) {
            if (sp == parsed[i]) { allowed = true; break; }
        }
        if (!allowed) {
            server.send(400, "application/json",
                "{\"success\":false,\"error\":\"GPIO pin not allowed\"}");
            return;
        }
        for (uint8_t j = i + 1; j < newCount; j++) {
            if (parsed[i] == parsed[j]) {
                server.send(400, "application/json",
                    "{\"success\":false,\"error\":\"Duplicate pin\"}");
                return;
            }
        }
    }
    bool oldManualOverride[MAX_RELAYS];
    bool oldManualState[MAX_RELAYS];
    char oldNames[MAX_RELAYS][16];
    TimerSchedule oldSchedules[MAX_RELAYS];
    bool oldActiveLow[MAX_RELAYS];
    uint8_t oldPins[MAX_RELAYS];
    bool oldRelayOutputs[MAX_RELAYS];
    uint8_t oldCount = gpioConfig.count;
    for (int i = 0; i < MAX_RELAYS; i++) {
        oldManualOverride[i] = relayConfigs[i].manualOverride;
        oldManualState[i] = relayConfigs[i].manualState;
        strcpy(oldNames[i], relayConfigs[i].name);
        oldSchedules[i] = relayConfigs[i].schedule;
        oldActiveLow[i] = gpioConfig.activeLow[i];
        oldPins[i] = gpioConfig.pins[i];
        oldRelayOutputs[i] = lastRelayOutputs[i];
    }
    gpioConfig.count = newCount;
    int idx = 0;
    for (JsonVariant pin : pins) {
        uint8_t newPinVal = pin.as<uint8_t>();
        gpioConfig.pins[idx] = newPinVal;
        if (idx < MAX_RELAYS) {
            int matchIdx = -1;
            for (uint8_t k = 0; k < oldCount; k++) {
                if (oldPins[k] == newPinVal) { matchIdx = k; break; }
            }
            if (matchIdx >= 0) {
                relayConfigs[idx].manualOverride = oldManualOverride[matchIdx];
                relayConfigs[idx].manualState    = oldManualState[matchIdx];
                strncpy(relayConfigs[idx].name, oldNames[matchIdx], 15);
                relayConfigs[idx].name[15] = '\0';
                relayConfigs[idx].schedule       = oldSchedules[matchIdx];
                gpioConfig.activeLow[idx]        = oldActiveLow[matchIdx];
            } else {
                relayConfigs[idx].manualOverride = false;
                relayConfigs[idx].manualState    = false;
                snprintf(relayConfigs[idx].name, 16, "Relay %d", idx + 1);
                memset(&relayConfigs[idx].schedule, 0, sizeof(TimerSchedule));
                for (int s = 0; s < 8; s++) {
                    relayConfigs[idx].schedule.days[s]      = DAY_ALL;
                    relayConfigs[idx].schedule.monthDays[s] = 0x7FFFFFFFUL;
                    relayConfigs[idx].schedule.monthMask[s] = MONTH_ALL;
                }
                gpioConfig.activeLow[idx] = true;
            }
            relayConfigs[idx].magic = RELAY_CONFIG_MAGIC;
        }
        idx++;
    }
    for (uint8_t i = newCount; i < MAX_RELAYS; i++) {
        initScheduleDefaults(i);
        gpioConfig.activeLow[i] = true;
    }
    for (uint8_t i = 0; i < oldCount; i++) {
        bool stillUsed = false;
        for (uint8_t j = 0; j < newCount; j++) {
            if (oldPins[i] == parsed[j]) { stillUsed = true; break; }
        }
        if (!stillUsed && oldPins[i] != 0xFF) {
            bool wasActiveLow = oldActiveLow[i];
            if (extConfig.global_active_mode == 1) wasActiveLow = true;
            else if (extConfig.global_active_mode == 2) wasActiveLow = false;
            digitalWrite(oldPins[i], wasActiveLow ? HIGH : LOW);
            pinMode(oldPins[i], INPUT);
        }
    }
    for (uint8_t i = 0; i < gpioConfig.count; i++) {
        pinMode(gpioConfig.pins[i], OUTPUT);
        int matchIdx = -1;
        for (uint8_t k = 0; k < oldCount; k++) {
            if (oldPins[k] == gpioConfig.pins[i]) { matchIdx = k; break; }
        }
        bool state = (matchIdx >= 0) ? oldRelayOutputs[matchIdx] : false;
        setRelayOutput(i, state);
        lastRelayOutputs[i] = state;
        lastDebouncedStateGlobal[i] = state;
        lastStateChangeGlobal[i] = 0;
    }
    saveGPIOConfig();
    saveConfiguration();
    updateScheduleCache();
    server.send(200, "application/json", "{\"success\":true,\"count\":" + String(newCount) + "}");
}

void handleAddGPIO() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"No data\"}");
        return;
    }
    StaticJsonDocument<128> doc;
    DeserializationError err = deserializeJson(doc, server.arg("plain"));
    if (err) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Bad JSON\"}");
        return;
    }
    if (!doc.containsKey("pin") || !doc["pin"].is<int>()) {
        server.send(400, "application/json",
            "{\"success\":false,\"error\":\"Missing or invalid pin field\"}");
        return;
    }
    int rawPin = doc["pin"] | -1;
    if (rawPin < 0 || rawPin > 48) {
        server.send(400, "application/json",
            "{\"success\":false,\"error\":\"Invalid pin (0-48)\"}");
        return;
    }
    uint8_t newPin = (uint8_t)rawPin;
    // change gpio
    static const uint8_t SAFE_PINS[] = {4, 5, 6, 7, 11, 12, 13, 14, 1, 2, 42, 41, 47, 21, 20, 19};
    bool pinAllowed = false;
    for (uint8_t sp : SAFE_PINS) {
        if (sp == newPin) { pinAllowed = true; break; }
    }
    if (!pinAllowed) {
        server.send(400, "application/json",
            "{\"success\":false,\"error\":\"GPIO pin not allowed (use a safe output pin)\"}");
        return;
    }
    if (gpioConfig.count >= MAX_RELAYS) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Maximum relays reached\"}");
        return;
    }
    for (uint8_t i = 0; i < gpioConfig.count; i++) {
        if (gpioConfig.pins[i] == newPin) {
            server.send(400, "application/json", "{\"success\":false,\"error\":\"Pin already in use\"}");
            return;
        }
    }
    gpioConfig.pins[gpioConfig.count] = newPin;
    gpioConfig.activeLow[gpioConfig.count] = true;
    initScheduleDefaults(gpioConfig.count);
    lastDebouncedStateGlobal[gpioConfig.count] = false;
    lastStateChangeGlobal[gpioConfig.count] = 0;
    gpioConfig.count++;
    pinMode(newPin, OUTPUT);
    setRelayOutput(gpioConfig.count - 1, false);
    lastRelayOutputs[gpioConfig.count - 1] = false;
    saveGPIOConfig();
    saveConfiguration();
    updateScheduleCache();
    server.send(200, "application/json", "{\"success\":true,\"count\":" + String(gpioConfig.count) + "}");
}

void handleDeleteGPIO() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"No data\"}");
        return;
    }
    StaticJsonDocument<128> doc;
    DeserializationError err = deserializeJson(doc, server.arg("plain"));
    if (err) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Bad JSON\"}");
        return;
    }
    if (!doc.containsKey("index") || !doc["index"].is<uint8_t>()) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Missing or invalid index field\"}");
        return;
    }
    uint8_t index = doc["index"];
    if (index >= gpioConfig.count) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid index\"}");
        return;
    }
    uint8_t removedPin = gpioConfig.pins[index];
    bool removedActiveLow = gpioConfig.activeLow[index];
    if (extConfig.global_active_mode == 1) removedActiveLow = true;
    else if (extConfig.global_active_mode == 2) removedActiveLow = false;
    setRelayOutput(index, false);
    delay(1);
    digitalWrite(removedPin, removedActiveLow ? HIGH : LOW);
    pinMode(removedPin, INPUT);
    for (uint8_t i = index; i < gpioConfig.count - 1; i++) {
        gpioConfig.pins[i] = gpioConfig.pins[i + 1];
        gpioConfig.activeLow[i] = gpioConfig.activeLow[i + 1];
        relayConfigs[i] = relayConfigs[i + 1];
        lastRelayOutputs[i] = lastRelayOutputs[i + 1];
        scheduleActiveCache[i] = scheduleActiveCache[i + 1];
        lastDebouncedStateGlobal[i] = lastRelayOutputs[i];
        lastStateChangeGlobal[i] = 0;
    }
    initScheduleDefaults(gpioConfig.count - 1);
    gpioConfig.activeLow[gpioConfig.count - 1] = true;
    lastRelayOutputs[gpioConfig.count - 1] = false;
    scheduleActiveCache[gpioConfig.count - 1] = false;
    lastDebouncedStateGlobal[gpioConfig.count - 1] = false;
    lastStateChangeGlobal[gpioConfig.count - 1] = 0;
    gpioConfig.count--;
    saveGPIOConfig();
    saveConfiguration();
    updateScheduleCache();
    server.send(200, "application/json", "{\"success\":true,\"count\":" + String(gpioConfig.count) + "}");
}

void handleToggleActiveLow() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"No data\"}");
        return;
    }
    StaticJsonDocument<128> doc;
    DeserializationError err = deserializeJson(doc, server.arg("plain"));
    if (err) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Bad JSON\"}");
        return;
    }
    if (!doc.containsKey("index") || !doc["index"].is<uint8_t>()) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Missing or invalid index field\"}");
        return;
    }
    uint8_t index = doc["index"];
    if (index >= gpioConfig.count) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid index\"}");
        return;
    }
    bool logicalState = lastRelayOutputs[index];
    gpioConfig.activeLow[index] = !gpioConfig.activeLow[index];
    saveGPIOConfig();
    pinMode(gpioConfig.pins[index], OUTPUT);
    setRelayOutput(index, logicalState);
    lastRelayOutputs[index] = logicalState;
    server.send(200, "application/json",
        "{\"success\":true,\"activeLow\":" + String(gpioConfig.activeLow[index] ? "true" : "false") + "}");
}

// =============================================================================
//  GLOBAL ACTIVE MODE HANDLER
// =============================================================================
void handleGlobalActiveMode() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"No data\"}");
        return;
    }
    StaticJsonDocument<64> doc;
    DeserializationError err = deserializeJson(doc, server.arg("plain"));
    if (err) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Bad JSON\"}");
        return;
    }
    if (!doc.containsKey("mode") || !doc["mode"].is<uint8_t>()) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Missing or invalid mode field\"}");
        return;
    }
    uint8_t mode = doc["mode"];
    if (mode > 2) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid mode (0-2)\"}");
        return;
    }
    extConfig.global_active_mode = mode;
    saveExtConfig();
    for (int i = 0; i < gpioConfig.count; i++) {
        bool currentState = lastRelayOutputs[i];
        setRelayOutput(i, currentState);
    }
    server.send(200, "application/json",
        "{\"success\":true,\"mode\":" + String(mode) + "}");
}
