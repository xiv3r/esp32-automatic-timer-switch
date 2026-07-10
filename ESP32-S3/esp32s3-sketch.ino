/*
===============================================================================
 *  ESP32-S3 16-Channel Automatic Relay Timer Switch
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
#define EEPROM_VERSION 11       
#define EXT_CFG_MAGIC  0xEC

// =============================================================================
//  Year 2106+ Support
// =============================================================================
#define MAX_UNIX_TIME_64 18446744073709551615ULL
#define MIN_UNIX_TIME_64 1000000000ULL
#define VALID_UNIX_TIME_64(epoch) ((epoch) > MIN_UNIX_TIME_64 && (epoch) < MAX_UNIX_TIME_64)

// =============================================================================
//  Day-of-Week Constants
// =============================================================================
#define DAY_SUNDAY    (1 << 0)
#define DAY_MONDAY    (1 << 1)
#define DAY_TUESDAY   (1 << 2)
#define DAY_WEDNESDAY (1 << 3)
#define DAY_THURSDAY  (1 << 4)
#define DAY_FRIDAY    (1 << 5)
#define DAY_SATURDAY  (1 << 6)
#define DAY_ALL       0x7F
#define DAY_WEEKDAYS  0x3E  
#define DAY_WEEKENDS  0x41  

// =============================================================================
//  Month-of-Year Constants
// =============================================================================
#define MONTH_JANUARY   (1 << 0)
#define MONTH_FEBRUARY  (1 << 1)
#define MONTH_MARCH     (1 << 2)
#define MONTH_APRIL     (1 << 3)
#define MONTH_MAY       (1 << 4)
#define MONTH_JUNE      (1 << 5)
#define MONTH_JULY      (1 << 6)
#define MONTH_AUGUST    (1 << 7)
#define MONTH_SEPTEMBER (1 << 8)
#define MONTH_OCTOBER   (1 << 9)
#define MONTH_NOVEMBER  (1 << 10)
#define MONTH_DECEMBER  (1 << 11)
#define MONTH_ALL       0x0FFF

// =============================================================================
//  Timing Constants
// =============================================================================
static const unsigned long NTP_RETRY_INTERVAL   =   30000UL;
static const unsigned long WIFI_CHECK_INTERVAL  =   10000UL;
static const unsigned long WIFI_CONNECT_TIMEOUT =   20000UL;
static const unsigned long RTC_UPDATE_INTERVAL  =     100UL;
static const unsigned long SCHEDULE_PROCESS_INTERVAL = 250UL;
static const unsigned long RELAY_UPDATE_INTERVAL =    500UL;
static const unsigned long RTC_REBASE_INTERVAL  =  300000UL;
static const unsigned long RTC_SYNC_INTERVAL    = 3600000UL;
static const unsigned long DS3231_SYNC_INTERVAL =  3600000UL; 

// =============================================================================
//  NTP Async State Machine Constants
// =============================================================================
static const unsigned long NTP_SERVER_TIMEOUT   =   5000UL;  
#define NTP_STATE_IDLE       0
#define NTP_STATE_CONNECTING 1
#define NTP_STATE_WAITING    2

// =============================================================================
//  64-bit NTP Constants
// =============================================================================
#define NTP_PACKET_SIZE 48
#define NTP_PORT 123
#define NTP_TIMEOUT 5000
#define NTP_RETRY_COUNT 3
#define NTP_EPOCH_OFFSET 2208988800ULL

// =============================================================================
//  Memory Management
// =============================================================================
static const unsigned long MEMORY_CLEANUP_INTERVAL = 30000UL;
static const unsigned long MEMORY_CHECK_INTERVAL = 60000UL;
static const unsigned long CONNECTION_TIMEOUT = 10000UL;

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
static const unsigned long MDNS_RESTART_DELAY = 2000UL;

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

// =============================================================================
//  WiFi Scan Pause System
// =============================================================================
static bool wifiPausedForScan = false;
static unsigned long wifiPauseUntil = 0;
static const unsigned long WIFI_PAUSE_DURATION = 15000UL;
static unsigned long lastScanAttempt = 0;

// =============================================================================
//  Millis-Safe Time Comparison Macro
// =============================================================================
inline bool timeHasElapsed(unsigned long current, unsigned long previous, unsigned long interval) {
    return (current - previous) >= interval;
}

// =============================================================================
//  Millis-Safe Future Time Check
// =============================================================================
inline bool isTimeReached(unsigned long current, unsigned long target) {
    return (long)(current - target) >= 0;
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
const uint8_t DEFAULT_RELAY_PINS[] = {
// change gpio pins
  1,  // IN1  - Relay 1
  2,  // IN2  - Relay 2
  3,  // IN3  - Relay 3
  4,  // IN4  - Relay 4
  5,  // IN5  - Relay 5
  6,  // IN6  - Relay 6
  7,  // IN7  - Relay 7
  10, // IN8  - Relay 8
  11, // IN9  - Relay 9
  12, // IN10 - Relay 10
  13, // IN11 - Relay 11
  14, // IN12 - Relay 12
  15, // IN13 - Relay 13
  16, // IN14 - Relay 14
  17, // IN15 - Relay 15
  18  // IN16 - Relay 16
};

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

// RelayConfig 
struct RelayConfig {
    TimerSchedule schedule;
    bool          manualOverride;
    bool          manualState;
    char          name[16];
};

// SystemConfig
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
    float    rtc_drift;
    char     hostname[32];
} __attribute__((packed));

// ExtConfig 
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
//  Self-Healing
// =============================================================================
struct HealthMetrics {
    uint32_t wifiFailures = 0;
    uint32_t ntpFailures = 0;
    uint32_t mdnsFailures = 0;
    uint32_t dnsFailures = 0;
    uint32_t webServerFailures = 0;
    unsigned long lastRecoveryAttempt = 0;  
    bool inRecoveryMode = false;
};
struct CriticalRelayState {
    uint32_t magic = 0xDEADBEEF;
    bool relayStates[MAX_RELAYS];
    bool manualOverrides[MAX_RELAYS];
    uint32_t timestamp;
    uint32_t checksum;
};

// =============================================================================
//  Globals
// =============================================================================
SystemConfig sysConfig;
ExtConfig    extConfig;
RelayConfig  relayConfigs[MAX_RELAYS];
HealthMetrics health;
CriticalRelayState criticalState;
static bool criticalStateInitialized = false;
static bool criticalStateDirty = false; 

// RTC
uint64_t      internalEpoch            = 0;
unsigned long internalMillisAtLastSync = 0;
float         driftCompensation        = 1.0f;
bool          rtcInitialized           = false;
unsigned long lastRTCUpdate            = 0;
unsigned long rtcMicrosAtLastSync      = 0;
unsigned long lastRTCRebase            = 0; 

// RTC Auto-Save
static unsigned long lastInternalRTCSave = 0;
static const unsigned long INTERNAL_RTC_SAVE_INTERVAL = 3600000UL; 

// NTP
uint8_t       ntpServerIndex  = 0;
uint8_t       ntpFailCount    = 0;
unsigned long lastNTPSync     = 0;
unsigned long lastNTPAttempt  = 0;

// NTP Async 
static uint8_t  ntpAsyncState = NTP_STATE_IDLE;
static uint8_t  ntpAsyncCurrentServer = 0;
static unsigned long ntpAsyncPhaseStart = 0;

// WiFi
bool          wifiConnected         = false;
unsigned long lastWiFiCheck         = 0;
uint8_t       wifiReconnectAttempts = 0;
unsigned long wifiGiveUpUntil       = 0;
static const uint8_t MAX_RECONNECT  = 10;
bool          wifiConnecting        = false;
unsigned long wifiConnectStart      = 0;
bool          wifiFirstAttempt      = true;

// WiFi Scan
volatile bool scanInProgress  = false;
volatile int  scanResultCount = -1;
unsigned long scanStartTime   = 0;

// AP
char ap_ssid[32]     = "ESP32S3_16CH_Timer_Switch";
char ap_password[32] = "ESP32-admin";

// mDNS
bool          mdnsStarted           = false;
char          mdnsHostname[32]      = MDNS_HOSTNAME_DEFAULT;
unsigned long mdnsRestartPending    = 0;
bool          mdnsRestartScheduled  = false;

// =============================================================================
//  Schedule Engine
// =============================================================================
static uint8_t  cachedTodayBit = 0;
static int      cachedMonthDay = 0;
static int      cachedMonth = 0;      
static uint64_t lastScheduleEpoch = 0;
static bool     scheduleActiveCache[MAX_RELAYS] = {false};
static unsigned long lastScheduleCacheUpdate = 0;
static const unsigned long SCHEDULE_CACHE_INTERVAL = 1000UL;
static unsigned long lastScheduleProcess = 0;
static unsigned long lastRelayUpdate = 0;
static bool lastRelayOutputs[MAX_RELAYS] = {false};
static bool relayOutputsInitialized = false;

// =============================================================================
//  Memory Management
// =============================================================================
static unsigned long lastMemoryCleanup = 0;
static unsigned long lastHeapCheck = 0;
static size_t minFreeHeap = 0;
static unsigned long lastConnectionActivity = 0;
static unsigned long lastServerRestart = 0;

// =============================================================================
//  Response Cache
// =============================================================================
struct ResponseCache {
    String relaysJson;
    String systemJson;
    String timeJson;
    unsigned long lastUpdate = 0;
    bool valid = false;
};
ResponseCache responseCache;

// =============================================================================
//  Self-Healing Class
// =============================================================================
class SelfHealingSystem {
public:
    bool liveReconfigureWiFi();
    bool liveReconfigureMDNS();
    bool liveReconfigureDNS();
    bool liveReconfigureWebServer();
    bool liveReconfigureAP();    
    bool recoverWiFi();
    bool recoverMDNS();
    bool recoverDNS();
    bool recoverWebServer();
    bool recoverNTP();
    bool recoverRTC();
    void smartRecovery();
    void verifyRelayStates();
    void saveCriticalState();
    bool restoreCriticalState();
    void performTargetedRecovery();    
    void restartAPIfNeeded(bool forceRestart = false);    
private:
    unsigned long lastMDNSAnnounce = 0;
    unsigned long lastWebServerCheck = 0;
    unsigned long lastFullHealthCheck = 0;
    unsigned long lastWiFiReconfigure = 0;
    unsigned long lastMDNSReconfigure = 0;
    unsigned long lastDNSReconfigure = 0;
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
void syncInternalRTC(uint64_t rawUtcEpoch);
void updateScheduleCache();
void autoSaveInternalRTC();
void setupWebServer();
void handleGetRelays();
void handleManualControl();
void handleResetManual();
void handleSaveRelay();
void handleRelayName();
void handleGetTime();
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
void checkWebServerHealth();
void immediateDS3231Sync();
void pauseWiFiForScan();
void setWiFiStationEnabled(bool enabled);
void initScheduleDefaults(int relayIndex);
uint64_t getNTP64Time(const char* server);

// =============================================================================
//  32-bit to 64-bit Conversion Helpers
// =============================================================================
inline uint64_t time32To64(time_t t32) {
    return (uint64_t)t32;
}
inline time_t time64To32(uint64_t t64) {
    if (t64 > 0xFFFFFFFFULL) t64 = 0xFFFFFFFFULL;
    return (time_t)t64;
}
inline uint64_t rtcDateTimeToUint64(DateTime dt) {
    return (uint64_t)dt.unixtime();
}
inline DateTime uint64ToRtcDateTime(uint64_t t64) {
    time_t t32 = time64To32(t64);
    return DateTime(t32);
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
    uint64_t era = (z >= 0 ? z : z - 146096ULL) / 146097ULL;
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
    tm.tm_yday = (int)doy;  
    tm.tm_isdst = 0;    
    return &tm;
}

// =============================================================================
//  64-bit Localtime Implementation
// =============================================================================
struct tm* localtime64(uint64_t* timep) {
    uint64_t localTime = *timep + (uint64_t)(sysConfig.gmt_offset + sysConfig.daylight_offset);
    return gmtime64(&localTime);
}

// =============================================================================
//  Get Local Epoch
// =============================================================================
inline uint64_t getLocalEpoch(uint64_t utcEpoch) {
    return utcEpoch + (uint64_t)(sysConfig.gmt_offset + sysConfig.daylight_offset);
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
//  Inline Helper
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
inline uint8_t getActiveRelayCount() {
    return gpioConfig.count;
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
        relayConfigs[relayIndex].schedule.monthDays[s] = 0;
        relayConfigs[relayIndex].schedule.monthMask[s] = MONTH_ALL;  
    }
    snprintf(relayConfigs[relayIndex].name, 16, "Relay %d", relayIndex + 1);
}

// =============================================================================
//  WiFi Station Control
// =============================================================================
void setWiFiStationEnabled(bool enabled) {
    extConfig.sta_enabled = enabled ? 1 : 0;
    saveExtConfig();    
    if (!enabled) {
        WiFi.disconnect(true);
        wifiConnected = false;
        wifiConnecting = false;
        wifiPausedForScan = false;
        wifiReconnectAttempts = 0;
        wifiGiveUpUntil = 0;
        if (WiFi.getMode() != WIFI_AP) {
            WiFi.mode(WIFI_AP);
        }
        ntpAsyncState = NTP_STATE_IDLE;
        lastNTPSync = 0;
    } else {
        if (WiFi.getMode() != WIFI_AP_STA) {
            WiFi.mode(WIFI_AP_STA);
        }
        if (strlen(sysConfig.sta_ssid) > 0) {
            WiFi.begin(sysConfig.sta_ssid, sysConfig.sta_password);
            wifiConnecting = true;
            wifiConnectStart = millis();
            wifiFirstAttempt = true;
            wifiReconnectAttempts = 0;
            wifiGiveUpUntil = 0;
        }
    }
    responseCache.valid = false;
}

// =============================================================================
//  DS3231 RTC Functions
// =============================================================================
void initRTC() {
    Wire.begin(8, 9); 
    if (!rtc.begin()) {
        rtcPresent = false;
        rtcTimeValid = false;
        return;
    }    
    rtcPresent = true;
    rtcTimeValid = false;    
    if (rtc.lostPower()) {
        rtcTimeValid = false;
        return;
    }    
    DateTime now = rtc.now();
    if (now.year() >= 2020 && now.year() <= 2100) {
        uint64_t rtcEpoch = rtcDateTimeToUint64(now);
        if (VALID_UNIX_TIME_64(rtcEpoch)) {
            rtcTimeValid = true;
            internalEpoch = rtcEpoch;
            driftCompensation = 1.0f;
            internalMillisAtLastSync = millis();
            rtcMicrosAtLastSync = micros();
            lastRTCRebase = millis();
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
        }
    }
}

void syncDS3231FromInternalRTC() {
    if (!rtcPresent) return;
    if (!rtcInitialized || internalEpoch == 0) return;    
    DateTime dt = uint64ToRtcDateTime(internalEpoch);
    rtc.adjust(dt);
    rtcTimeValid = true;
    lastRTCDSync = millis();
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
        float elapsedSeconds = (float)elapsedMicros / 1000000.0f;
        float adjustedSeconds = elapsedSeconds * driftCompensation;
        uint64_t secondsToAdd = (uint64_t)adjustedSeconds;
        internalEpoch += secondsToAdd;
        float fractionalSeconds = adjustedSeconds - (float)secondsToAdd;
        if (fractionalSeconds >= 0.5f) {
            internalEpoch++; 
        }
    }    
    rtcMicrosAtLastSync = currentMicros;
    lastRTCRebase = currentMillis;
    lastRTCUpdate = currentMillis;
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
        float elapsedSeconds = (float)elapsedMicros / 1000000.0f;
        float adjustedSeconds = elapsedSeconds * driftCompensation;
        uint64_t secondsToAdd = (uint64_t)adjustedSeconds;
        uint64_t result = internalEpoch + secondsToAdd;
        float fractional = adjustedSeconds - (float)secondsToAdd;
        if (fractional >= 0.5f) {
            result++;
        }        
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
    sysConfig.rtc_drift      = 1.0f;  
    saveConfiguration();
}

void loadRTCState() {
    if (VALID_UNIX_TIME_64(sysConfig.last_rtc_epoch)) {
        internalEpoch            = sysConfig.last_rtc_epoch;
        driftCompensation        = 1.0f;  
        internalMillisAtLastSync = millis();
        rtcMicrosAtLastSync      = micros();
        lastRTCRebase            = millis();
        rtcInitialized           = true;
    }
}

// =============================================================================
//  Auto-Save Internal RTC
// =============================================================================
void autoSaveInternalRTC() {
    unsigned long now = millis();
    if (!rtcInitialized || internalEpoch == 0) return;
    bool ntpAvailable = wifiConnected && extConfig.sta_enabled && (timeSource == TIME_SOURCE_NTP);
    bool rtcAvailable = rtcPresent && rtcTimeValid;
    if (ntpAvailable || rtcAvailable) {
        lastInternalRTCSave = now; 
        return;
    }
    if (timeHasElapsed(now, lastInternalRTCSave, INTERNAL_RTC_SAVE_INTERVAL)) {
        lastInternalRTCSave = now;
        performRTCReabase();
        sysConfig.last_rtc_epoch = internalEpoch;
        sysConfig.rtc_drift = driftCompensation;
        saveConfiguration();
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
                internalMillisAtLastSync = millis();
                rtcMicrosAtLastSync = micros();
                lastRTCRebase = millis();
                rtcInitialized = true;
                timeSource = TIME_SOURCE_RTC;
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
        WiFi.disconnect(true);
        delay(100);
        wifiConnecting = false;
        lastScanAttempt = millis();
    }
}

// =============================================================================
//  SELF-HEALING SYSTEM
// =============================================================================
uint32_t calculateCriticalChecksum() {
    uint32_t sum = 0;
    for (int i = 0; i < MAX_RELAYS; i++) {
        sum += criticalState.relayStates[i] ? (1 << (i % 32)) : 0;
        sum += criticalState.manualOverrides[i] ? (1 << ((i+16) % 32)) : 0;
    }
    return sum ^ criticalState.timestamp;
}

void SelfHealingSystem::saveCriticalState() {
    for (int i = 0; i < gpioConfig.count; i++) {
        criticalState.relayStates[i] = lastRelayOutputs[i];
        criticalState.manualOverrides[i] = relayConfigs[i].manualOverride;
    }
    for (int i = gpioConfig.count; i < MAX_RELAYS; i++) {
        criticalState.relayStates[i] = false;
        criticalState.manualOverrides[i] = false;
    }
    criticalState.timestamp = millis();
    criticalState.checksum = calculateCriticalChecksum();
    criticalState.magic = 0xDEADBEEF;    
    preferences.begin(NVS_NAMESPACE, false);
    preferences.putBytes("criticalState", &criticalState, sizeof(CriticalRelayState));
    preferences.end();
    criticalStateInitialized = true;
}

bool SelfHealingSystem::restoreCriticalState() {
    preferences.begin(NVS_NAMESPACE, true);
    size_t len = preferences.getBytes("criticalState", &criticalState, sizeof(CriticalRelayState));
    preferences.end();    
    if (len == sizeof(CriticalRelayState) && criticalState.magic == 0xDEADBEEF) {
        if (criticalState.checksum == calculateCriticalChecksum()) {
            for (int i = 0; i < gpioConfig.count; i++) {
                if (criticalState.manualOverrides[i]) {
                    setRelayOutput(i, criticalState.relayStates[i]);
                    lastRelayOutputs[i] = criticalState.relayStates[i];
                    relayConfigs[i].manualOverride = true;
                    relayConfigs[i].manualState = criticalState.relayStates[i];
                }
            }
            return true;
        }
    }
    return false;
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
            MDNS.addServiceTxt("http", "tcp", "version", "v11");  
            MDNS.addServiceTxt("http", "tcp", "channels", String(gpioConfig.count).c_str());
            MDNS.addServiceTxt("http", "tcp", "features", "monthmask,64bit");  
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

bool SelfHealingSystem::liveReconfigureDNS() {
    unsigned long now = millis();    
    if (!timeHasElapsed(now, lastDNSReconfigure, 60000)) return true;    
    lastDNSReconfigure = now;
    return true;
}

bool SelfHealingSystem::liveReconfigureWebServer() {
    unsigned long now = millis();    
    if (!timeHasElapsed(now, lastWebServerCheck, 30000)) return true;    
    lastWebServerCheck = now;
    return true;
}

bool SelfHealingSystem::liveReconfigureAP() {
    if (WiFi.softAPIP().toString() == "0.0.0.0") {
        uint8_t ch = extConfig.ap_channel;
        if (ch < 1 || ch > 13) ch = 6;
        uint8_t hidden = extConfig.ap_hidden ? 1 : 0;        
        if (strlen(sysConfig.ap_password) > 0) {
            WiFi.softAP(sysConfig.ap_ssid, sysConfig.ap_password, ch, hidden);
        } else {
            WiFi.softAP(sysConfig.ap_ssid, NULL, ch, hidden);
        }        
        liveReconfigureDNS();
        liveReconfigureMDNS();
        return true;
    }    
    return true;
}

void SelfHealingSystem::restartAPIfNeeded(bool forceRestart) {
    if (forceRestart) {
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
        if (apStarted) {
            delay(100);
            liveReconfigureDNS();
            liveReconfigureMDNS();
        }
    }
}

// =============================================================================
//  Recovery Functions
// =============================================================================
bool SelfHealingSystem::recoverWiFi() {
    unsigned long now = millis();    
    if (!timeHasElapsed(now, lastWiFiReconfigure, 30000)) return false;
    if (!extConfig.sta_enabled) return true;  
    if (strlen(sysConfig.sta_ssid) > 0 && WiFi.status() != WL_CONNECTED) {
        WiFi.begin(sysConfig.sta_ssid, sysConfig.sta_password);
        wifiConnecting = true;
        wifiConnectStart = now;
        lastWiFiReconfigure = now;
        return true;
    }    
    return (WiFi.status() == WL_CONNECTED);
}

bool SelfHealingSystem::recoverMDNS() {
    return liveReconfigureMDNS();
}

bool SelfHealingSystem::recoverDNS() {
    return liveReconfigureDNS();
}

bool SelfHealingSystem::recoverWebServer() {
    return liveReconfigureWebServer();
}

// =============================================================================
//  recoverNTP 64-bit
// =============================================================================
bool SelfHealingSystem::recoverNTP() {
    if (!wifiConnected || !extConfig.sta_enabled) return false; 
    uint8_t startIndex = ntpServerIndex;
    for (uint8_t attempt = 0; attempt < NUM_NTP_SERVERS; attempt++) {
        uint8_t idx = (startIndex + attempt) % NUM_NTP_SERVERS;
        uint64_t ntpTime = getNTP64Time(NTP_SERVERS[idx]);
        if (ntpTime > 1577836800ULL) {  
            timeSource = TIME_SOURCE_NTP;
            syncInternalRTC(ntpTime);
            ntpServerIndex = idx;
            return true;
        }
        delay(100);
        yield();
    }
    return false;
}

bool SelfHealingSystem::recoverRTC() {
    if (!rtcPresent) return false;    
    Wire.begin(8, 9);
    if (rtc.begin()) {
        if (internalEpoch > 0) {
            DateTime dt = uint64ToRtcDateTime(internalEpoch);
            rtc.adjust(dt);
            rtcTimeValid = true;
            return true;
        } else if (rtcTimeValid) {
            DateTime now = rtc.now();
            if (now.year() >= 2020 && now.year() <= 2100) {
                internalEpoch = rtcDateTimeToUint64(now);
                rtcInitialized = true;
                return true;
            }
        }
    }
    return false;
}

void SelfHealingSystem::performTargetedRecovery() {
    liveReconfigureWebServer();
    delay(50);    
    liveReconfigureDNS();
    delay(50);    
    liveReconfigureMDNS();
    delay(50);    
    liveReconfigureWiFi();
    delay(50);    
    liveReconfigureAP();
    delay(50);
    recoverRTC();  
    verifyRelayStates();    
    health.webServerFailures = 0;
    health.mdnsFailures = 0;
    health.dnsFailures = 0;
    health.wifiFailures = 0;
}

void SelfHealingSystem::verifyRelayStates() {
    static unsigned long lastVerification = 0;
    unsigned long now = millis();
    if (!timeHasElapsed(now, lastVerification, 30000)) return;
    lastVerification = now;    
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
        if (criticalStateDirty) {
            saveCriticalState();
            criticalStateDirty = false;
        }
    }    
    verifyRelayStates();
    liveReconfigureAP();
}

void performMemoryCleanup() {
    if (!scanInProgress) {
        WiFi.scanDelete();
    }    
    for (int i = 0; i < 10; i++) {
        yield();
        delay(1);
    }    
    preferences.end();  
    delay(50);
    for (int i = 0; i < 5; i++) {
        void* ptr = malloc(512);
        if (ptr) free(ptr);
        yield();
    }
}

void cleanupStaleResources() {
    unsigned long now = millis();    
    if (!scanInProgress && WiFi.scanComplete() == WIFI_SCAN_RUNNING) {
        WiFi.scanDelete();
    }    
    if (timeHasElapsed(now, responseCache.lastUpdate, 5000)) {
        responseCache.valid = false;
        responseCache.relaysJson = "";
        responseCache.systemJson = "";
        responseCache.timeJson = "";
    }    
    performMemoryCleanup();
}

void checkWebServerHealth() {
    unsigned long now = millis();    
    if (timeHasElapsed(now, lastConnectionActivity, 300000) && lastConnectionActivity > 0) {
        healer.liveReconfigureWebServer();        
        lastConnectionActivity = now;
    }
}

void checkAndCleanMemory() {
    unsigned long now = millis();
    if (timeHasElapsed(now, lastHeapCheck, 300000)) {
        lastHeapCheck = now;
        size_t freeHeap = ESP.getFreeHeap();        
        if (freeHeap < minFreeHeap || minFreeHeap == 0) {
            minFreeHeap = freeHeap;
        }        
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
.logo{font-size:13px;font-weight:700;white-space:nowrap}
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
.bon{background:#E8F5E9;color:#2E7D32}.boff{background:#FFEBEE;color:#C62828}.bman{background:#FFF3E0;color:#E65100}
.brow{display:flex;gap:6px;flex-wrap:wrap;margin-bottom:10px}
.btn{border:none;padding:7px 12px;border-radius:6px;cursor:pointer;font-size:12px;font-weight:600;transition:.15s}
.btn:hover{filter:brightness(1.1)}.btn:disabled{opacity:.5;cursor:default}
.bon-b{background:#43A047;color:#fff}.boff-b{background:#E53935;color:#fff}.baut{background:#546E7A;color:#fff}
.bsave{background:#1565C0;color:#fff;width:100%;padding:9px;font-size:13px;border-radius:6px;margin-top:8px}
.bsync{background:#FB8C00;color:#fff}.bdanger{background:#B71C1C;color:#fff}.bwarn{background:#F9A825;color:#212121}
.bscan{background:#0288D1;color:#fff}
.slist{display:flex;flex-direction:column;gap:6px;margin-bottom:8px;max-height:500px;overflow-y:auto;padding-right:2px}
.si{border:1px solid #E3E8EF;border-radius:7px;padding:9px}
.si.act{border-color:#90CAF9;background:#F0F7FF}
.shdr{display:flex;align-items:center;gap:7px;margin-bottom:7px;font-size:11px;font-weight:700;color:#607D8B;text-transform:uppercase}
.shdr label{display:flex;align-items:center;gap:4px;cursor:pointer;font-size:12px;font-weight:700;color:#1A1A2E;text-transform:none}
.trow{display:flex;align-items:center;gap:8px;font-size:12px;margin-top:5px}
.trow .l{color:#90A4AE;font-weight:600;width:32px;flex-shrink:0}
.days{display:flex;gap:3px;margin-top:5px;flex-wrap:wrap}
.day{width:28px;height:24px;border-radius:4px;border:1px solid #CFD8DC;display:flex;align-items:center;justify-content:center;font-size:11px;font-weight:600;cursor:pointer;background:#FAFAFA;transition:.15s;user-select:none}
.day:hover{border-color:#90CAF9;background:#E3F2FD}
.day.on{background:#1565C0;color:#fff;border-color:#1565C0}
.mdays{display:flex;gap:2px;margin-top:5px;flex-wrap:wrap}
.mday{width:26px;height:22px;border-radius:3px;border:1px solid #CFD8DC;display:flex;align-items:center;justify-content:center;font-size:10px;font-weight:600;cursor:pointer;background:#FAFAFA;transition:.15s;user-select:none}
.mday:hover{border-color:#CE93D8;background:#F3E5F5}
.mday.on{background:#7B1FA2;color:#fff;border-color:#7B1FA2}
.months{display:flex;gap:3px;margin-top:5px;flex-wrap:wrap}  /* NEW: month styles */
.month{width:36px;height:24px;border-radius:4px;border:1px solid #CFD8DC;display:flex;align-items:center;justify-content:center;font-size:10px;font-weight:600;cursor:pointer;background:#FAFAFA;transition:.15s;user-select:none}
.month:hover{border-color:#81D4FA;background:#E1F5FE}
.month.on{background:#0277BD;color:#fff;border-color:#0277BD}
.sched-section{margin-top:4px;font-size:10px;font-weight:600;color:#90A4AE;text-transform:uppercase;margin-bottom:2px}
.night{font-size:10px;color:#7B1FA2;background:#F3E5F5;padding:2px 6px;border-radius:4px;margin-left:auto}
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
@media(max-width:500px){.grid{grid-template-columns:1fr}.ibar{grid-template-columns:1fr}.input-row{flex-direction:column}.day{width:24px;height:22px;font-size:10px}.mday{width:22px;height:20px;font-size:9px}.month{width:30px;height:22px;font-size:9px}}
)css";

// =============================================================================
// INDEX PAGES
// =============================================================================
const char index_html[] PROGMEM = R"raw(<!DOCTYPE html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
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
<div class="hdr-r"><span class="dot wd"></span><span class="dot td"></span>&nbsp;<span id="clk">--:--:--</span></div>
</header>
<main>
<p class="ptitle">Relay Controls &amp; Schedules</p>
<div class="grid" id="grid"></div>
</main>
<div id="toast"></div>
<script>
const D=['Sun','Mon','Tue','Wed','Thu','Fri','Sat'];
const M=['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'];
function toast(m,ok=true){const t=document.getElementById('toast');t.textContent=m;t.className='show '+(ok?'ok':'er');clearTimeout(t._t);t._t=setTimeout(()=>t.className='',3000);}
function tick(){fetch('/api/time').then(r=>r.json()).then(d=>{document.getElementById('clk').textContent=d.time||'--:--:--';const w=document.querySelector('.wd'),t=document.querySelector('.td');if(w)w.className='dot '+(d.wifi?'g':'r');if(t){let tc='y';if(d.timeSource==='ntp')tc='g';else if(d.timeSource==='browser')tc='b';else if(d.timeSource==='rtc')tc='b';else tc='y';t.className='dot '+tc;}}).catch(()=>{});}
setInterval(tick,1000);tick();

const NS=8;
let relays=[],busy=false;
let editingRelay = -1;
let editingInput = null;

function escapeHtml(text) {
  const div = document.createElement('div');
  div.textContent = text;
  return div.innerHTML;
}

function load(){
  if(busy)return;
  fetch('/api/relays').then(r=>r.json()).then(d=>{relays=d;render();}).catch(()=>toast('Load error',false));
}

function toTS(h,m,s){return String(h).padStart(2,'0')+':'+String(m).padStart(2,'0')+':'+String(s).padStart(2,'0');}
function fromTS(v){const p=(v||'00:00:00').split(':');return{h:parseInt(p[0])||0,m:parseInt(p[1])||0,s:parseInt(p[2])||0};}

function dayMaskToStr(d){
  if(d===0x7F) return 'Everyday';
  let s='';
  for(let i=0;i<7;i++) if(d&(1<<i)) s+=D[i]+' ';
  return s.trim()||'None';
}

function monthDayMaskToStr(md){
  if(md===0) return '';
  if(md===0xFFFFFFFF) return 'All month days';
  let s='';
  for(let i=0;i<31;i++) if(md&(1<<i)) s+=(i+1)+',';
  return s.replace(/,$/,'')||'None';
}

function monthMaskToStr(mm) {  
  if (mm === 0x0FFF || mm === 0) return 'All months';
  let s='';
  for(let i=0;i<12;i++) if(mm&(1<<i)) s+=M[i]+' ';
  return s.trim()||'None';
}

function nightBadge(sc){
  if(!sc.enabled)return'';
  const a=sc.startHour*3600+sc.startMinute*60+sc.startSecond;
  const b=sc.stopHour*3600+sc.stopMinute*60+sc.stopSecond;
  const ds=dayMaskToStr(sc.days);
  const ms=monthDayMaskToStr(sc.monthDays||0);
  const mm=monthMaskToStr(sc.monthMask||0x0FFF);  
  let info=ds;
  if(ms) info+=' | Days:'+ms;
  if(mm!=='All months') info+=' | Months:'+mm;  
  if(a===b)return'<span class="night always">&#x25CF; Always ON ('+info+')</span>';
  if(a>b) return'<span class="night">&#x1F319; Overnight ('+info+')</span>';
  return'<span class="night">&#x1F319; '+info+'</span>';
}

function startEditName(relayIdx) {
  if (editingRelay !== -1) cancelEdit();
  
  const nameSpan = document.getElementById('name_' + relayIdx);
  if (!nameSpan) return;
  
  const currentName = relays[relayIdx].name || ('Relay ' + (relayIdx + 1));
  
  const input = document.createElement('input');
  input.type = 'text';
  input.value = currentName;
  input.maxLength = 15;
  input.style.cssText = 'font-size:15px;font-weight:700;padding:2px 6px;border:1px solid #1565C0;border-radius:5px;width:120px;background:#fff;color:#1A1A2E;';
  input.id = 'edit_' + relayIdx;
  
  input.onblur = () => saveNameEdit(relayIdx, input.value);
  input.onkeydown = (e) => {
    if (e.key === 'Enter') {
      e.preventDefault();
      saveNameEdit(relayIdx, input.value);
    } else if (e.key === 'Escape') {
      cancelEdit();
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
    if (editingInput) editingInput.remove();
    editingRelay = -1;
    editingInput = null;
  }
}

function saveNameEdit(relayIdx, newName) {
  newName = newName.trim();
  if (newName.length === 0) {
    newName = 'Relay ' + (relayIdx + 1);
  }
  
  const nameSpan = document.getElementById('name_' + relayIdx);
  
  relays[relayIdx].name = newName;
  if (nameSpan) {
    nameSpan.textContent = newName;
    nameSpan.style.display = '';
  }
  if (editingInput) editingInput.remove();
  editingRelay = -1;
  editingInput = null;
  
  fetch('/api/relay/name', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({relay: relayIdx, name: newName})
  })
  .then(r => r.json())
  .then(d => {
    if (d.success) {
      toast('Name saved!');
    } else {
      toast('Failed to save name', false);
    }
  })
  .catch(() => toast('Error saving name', false));
}

function render(){
  const g=document.getElementById('grid');
  g.innerHTML='';
  if (!relays || relays.length === 0) {
    g.innerHTML = '<div class="card" style="text-align:center;padding:40px"><p style="color:#90A4AE">No relays configured. <a href="/gpio" style="color:#1565C0">Configure GPIO pins</a></p></div>';
    return;
  }
  relays.forEach((r,i)=>{
    const sl=r.manual?'MANUAL':r.state?'ON':'OFF';
    const sc=r.manual?'bman':r.state?'bon':'boff';
    const displayName = r.name || ('Relay '+(i+1));
    let html=`<div class="card">
<div class="card-hdr">
<span class="ctitle" id="name_${i}" ondblclick="startEditName(${i})" title="Double-click to rename">${escapeHtml(displayName)}</span>
<span class="badge ${sc}">${sl}</span>
</div>
<div class="brow">
<button class="btn bon-b" onclick="mc(${i},true)">ON</button>
<button class="btn boff-b" onclick="mc(${i},false)">OFF</button>
<button class="btn baut" onclick="ra(${i})">Auto</button>
</div>
<div class="slist">`;
    for(let s=0;s<NS;s++){
      const sc2=r.schedules[s];
      const dayBits = sc2.days || 0x7F;
      const monthDayBits = sc2.monthDays || 0;
      const monthMask = sc2.monthMask || 0x0FFF;  
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
        html+=`<div class="day${(dayBits&mask)?' on':''}" onclick="toggleDay(${i},${s},${d})">${D[d]}</div>`;
      }
      html+=`</div>
<div class="sched-section">Days of Month</div>
<div class="mdays" id="mday_${i}_${s}">`;
      for(let d=0;d<31;d++){
        const mask = 1<<d;
        html+=`<div class="mday${(monthDayBits&mask)?' on':''}" onclick="toggleMonthDay(${i},${s},${d})" title="Day ${d+1}">${d+1}</div>`;
      }
      html+=`</div>
<div class="sched-section">Months of Year</div>  <!-- NEW: month selector -->
<div class="months" id="mon_${i}_${s}">`;
      for(let m=0;m<12;m++){
        const mask = 1<<m;
        html+=`<div class="month${(monthMask&mask)?' on':''}" onclick="toggleMonth(${i},${s},${m})">${M[m]}</div>`;
      }
      html+=`</div>
</div>`;
    }
    html+=`</div><button class="btn bsave" onclick="save(${i})">&#x1F4BE; Save ${escapeHtml(displayName)}</button></div>`;
    const el=document.createElement('div');
    el.innerHTML=html;
    g.appendChild(el.firstChild);
  });
}

function toggleDay(ri,si,dayIdx){
  const mask = 1<<dayIdx;
  relays[ri].schedules[si].days ^= mask;
  const dayEl = document.getElementById('day_'+ri+'_'+si).children[dayIdx];
  if(dayEl) dayEl.className = 'day' + ((relays[ri].schedules[si].days & mask)?' on':'');
  const nb=document.getElementById('nb_'+ri+'_'+si);
  if(nb)nb.innerHTML=nightBadge(relays[ri].schedules[si]);
}

function toggleMonthDay(ri,si,dayIdx){
  const mask = 1<<dayIdx;
  if(!relays[ri].schedules[si].monthDays) relays[ri].schedules[si].monthDays = 0;
  relays[ri].schedules[si].monthDays ^= mask;
  const mdayEl = document.getElementById('mday_'+ri+'_'+si).children[dayIdx];
  if(mdayEl) mdayEl.className = 'mday' + ((relays[ri].schedules[si].monthDays & mask)?' on':'');
  const nb=document.getElementById('nb_'+ri+'_'+si);
  if(nb)nb.innerHTML=nightBadge(relays[ri].schedules[si]);
}

function toggleMonth(ri,si,mIdx){  
  const mask = 1<<mIdx;
  if(!relays[ri].schedules[si].monthMask) relays[ri].schedules[si].monthMask = 0x0FFF;
  relays[ri].schedules[si].monthMask ^= mask;
  const monEl = document.getElementById('mon_'+ri+'_'+si).children[mIdx];
  if(monEl) monEl.className = 'month' + ((relays[ri].schedules[si].monthMask & mask)?' on':'');
  const nb=document.getElementById('nb_'+ri+'_'+si);
  if(nb)nb.innerHTML=nightBadge(relays[ri].schedules[si]);
}

function uf(ri,si,field,val){
  const sc=relays[ri].schedules[si];
  if(field==='en'){
    sc.enabled=val;
    const el=document.getElementById('si_'+ri+'_'+si);
    if(el)el.className='si'+(val?' act':'');
  }else if(field==='start'){
    const t=fromTS(val);sc.startHour=t.h;sc.startMinute=t.m;sc.startSecond=t.s;
  }else if(field==='stop'){
    const t=fromTS(val);sc.stopHour=t.h;sc.stopMinute=t.m;sc.stopSecond=t.s;
  }
  const nb=document.getElementById('nb_'+ri+'_'+si);
  if(nb)nb.innerHTML=nightBadge(sc);
}

function mc(ri,state){
  fetch('/api/relay/manual',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({relay:ri,state})})
  .then(r=>r.json()).then(d=>{if(d.success){toast((relays[ri].name||('Relay '+(ri+1)))+' '+(state?'ON':'OFF'));load();}else toast('Failed',false);})
  .catch(()=>toast('Error',false));
}
function ra(ri){
  fetch('/api/relay/reset',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({relay:ri})})
  .then(r=>r.json()).then(d=>{if(d.success){toast((relays[ri].name||('Relay '+(ri+1)))+' \u2192 Auto');load();}else toast('Failed',false);})
  .catch(()=>toast('Error',false));
}
function save(ri){
  busy=true;
  fetch('/api/relay/save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({relay:ri,schedules:relays[ri].schedules})})
  .then(r=>r.json()).then(d=>{busy=false;if(d.success)toast((relays[ri].name||('Relay '+(ri+1)))+' saved!');else toast('Save failed',false);})
  .catch(()=>{busy=false;toast('Error',false);});
}
load();
</script></body></html>)raw";

// =============================================================================
//  WiFi PAGES
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
<div class="hdr-r"><span class="dot wd"></span><span class="dot td"></span>&nbsp;<span id="clk">--:--:--</span></div>
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
<small>When disabled, ESP32-S3 runs in AP-only mode (no internet/NTP). Relay schedules using internal RTC/DS3231 will still work. You can still access this UI via the AP.</small>
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
function toast(m,ok=true){const t=document.getElementById('toast');t.textContent=m;t.className='show '+(ok?'ok':'er');clearTimeout(t._t);t._t=setTimeout(()=>t.className='',3000);}
function tick(){fetch('/api/time').then(r=>r.json()).then(d=>{document.getElementById('clk').textContent=d.time||'--:--:--';const w=document.querySelector('.wd'),t=document.querySelector('.td');if(w)w.className='dot '+(d.wifi?'g':'r');if(t){let tc='y';if(d.timeSource==='ntp')tc='g';else if(d.timeSource==='browser')tc='b';else if(d.timeSource==='rtc')tc='b';else tc='y';t.className='dot '+tc;}}).catch(()=>{});}
setInterval(tick,1000);tick();

function updateStaButton(enabled) {
    const btn = document.getElementById('staToggleBtn');
    if(enabled) {
        btn.textContent = '✓ ON';
        btn.className = 'btn btn-sta-on';
        btn.style.minWidth = '90px';
    } else {
        btn.textContent = '✗ OFF';
        btn.className = 'btn btn-sta-off';
        btn.style.minWidth = '90px';
    }
}

function loadWiFiStatus() {
    fetch('/api/wifi').then(r=>r.json()).then(d=>{
        document.getElementById('ssid').value=d.ssid||'';
        const staEnabled = d.sta_enabled !== undefined ? d.sta_enabled : true;
        updateStaButton(staEnabled);
        const stationDiv = document.getElementById('stationConfig');
        if(stationDiv) {
            stationDiv.style.opacity = staEnabled ? '1' : '0.5';
            const inputs = stationDiv.querySelectorAll('input, button');
            inputs.forEach(inp => inp.disabled = !staEnabled);
        }
        const s=document.getElementById('status');
        s.style.display='';
        if(staEnabled && d.connected){
            const bars=rssiBar(d.rssi||0);
            s.innerHTML='<span style="font-weight:700">✓ Connected</span> to: <strong>'+d.ssid+'</strong> ('+d.ip+') &nbsp;'+bars+' '+d.rssi+'dBm';
            s.className='alert ai';
        } else if(staEnabled && !d.connected) {
            s.innerHTML='<span style="font-weight:700">⚠ Not Connected</span><br>Configure SSID/password above to connect to a WiFi network.';
            s.className='alert aw';
        } else {
            s.innerHTML='<span style="font-weight:700">⛔ WiFi Station Disabled</span><br>ESP32-S3 running in AP-only mode. Enable above to connect to WiFi networks for NTP time sync.';
            s.className='alert aw';
        }
    }).catch(()=>{});
}

function rssiBar(rssi){
    const b=rssi>=-50?4:rssi>=-60?3:rssi>=-70?2:1;
    let s='<span class="bars">';
    for(let i=1;i<=4;i++)s+='<span class="bar'+(i<=b?' on':'')+('" style="height:'+(i*3+2)+'px"></span>');
    return s+'</span>';
}

function toggleStation() {
    const btn = document.getElementById('staToggleBtn');
    const wasEnabled = btn.textContent === '✓ ON';
    const willEnable = !wasEnabled;
    
    btn.disabled = true;
    btn.textContent = '...';
    
    fetch('/api/wifi', {
        method:'POST',
        headers:{'Content-Type':'application/json'},
        body:JSON.stringify({sta_enabled: willEnable})
    }).then(r=>r.json()).then(d=>{
        btn.disabled = false;
        if(d.success) {
            updateStaButton(willEnable);
            toast('WiFi Station ' + (willEnable ? 'ON' : 'OFF'), true);
            if(!willEnable) {
                toast('NTP time sync unavailable until re-enabled. Use DS3231 RTC or browser sync.', false);
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
    });
}

let scanTimer=null,scanning=false;
function startScan(){
    const staEnabled = document.getElementById('staToggleBtn').textContent === '✓ ON';
    if(!staEnabled) {
        toast('Please enable WiFi Station Mode first', false);
        return;
    }
    if(scanning)return;
    scanning=true;
    document.getElementById('scanBtn').textContent='\u23F3 Scanning\u2026';
    document.getElementById('scanBtn').disabled=true;
    const nl=document.getElementById('netlist');
    nl.style.display='block';
    nl.innerHTML='<div style="text-align:center;color:#90A4AE;padding:12px;font-size:13px">📡 Scanning for networks...</div>';
    fetch('/api/wifi/scan',{method:'POST'})
    .then(()=>{ 
        scanTimer=setInterval(pollScan,2500); 
    })
    .catch(()=>endScan());
}
function pollScan(){
    fetch('/api/wifi/scan').then(r=>r.json()).then(d=>{
        if(!d.scanning){clearInterval(scanTimer);endScan();renderNets(d.networks||[]);}
    }).catch(()=>{clearInterval(scanTimer);endScan();});
}
function endScan(){
    scanning=false;
    document.getElementById('scanBtn').textContent='\uD83D\uDCF6 Scan';
    document.getElementById('scanBtn').disabled=false;
}
function renderNets(nets){
    const nl=document.getElementById('netlist');
    if(!nets.length){nl.innerHTML='<div style="color:#90A4AE;text-align:center;padding:8px;font-size:13px">No networks found. Make sure WiFi is enabled.</div>';return;}
    const frag=document.createDocumentFragment();
    const hdr=document.createElement('div');hdr.className='net-hdr';hdr.textContent='Available Networks';frag.appendChild(hdr);
    nets.sort((a,b)=>b.rssi-a.rssi).forEach(n=>{
        const d=document.createElement('div');d.className='netitem';
        const ns=document.createElement('span');ns.className='ns';ns.textContent=n.ssid;
        const nr=document.createElement('span');nr.className='nr';nr.textContent=n.rssi+'dBm';
        const bar=document.createElement('span');bar.innerHTML=rssiBar(n.rssi);
        const lock=document.createElement('span');lock.style.fontSize='13px';lock.textContent=n.enc?'\uD83D\uDD12':'';
        d.appendChild(ns);d.appendChild(nr);d.appendChild(bar);d.appendChild(lock);
        d.addEventListener('click',()=>{document.getElementById('ssid').value=n.ssid;document.getElementById('pw').focus();});
        frag.appendChild(d);
    });
    nl.innerHTML='';nl.appendChild(frag);
}
function saveWiFi(){
    const staEnabled = document.getElementById('staToggleBtn').textContent === '✓ ON';
    if(!staEnabled) {
        toast('Enable WiFi Station Mode first to save credentials', false);
        return;
    }
    const ssid=document.getElementById('ssid').value.trim();
    if(!ssid){toast('SSID required',false);return;}
    const btn = document.querySelector('.bsave');
    btn.disabled = true;
    btn.textContent = 'Saving...';
    fetch('/api/wifi',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid,password:document.getElementById('pw').value})})
    .then(r=>r.json()).then(d=>{
        btn.disabled = false;
        btn.innerHTML = '&#x1F4BE; Save &amp; Connect';
        if(d.success){
            toast('Credentials saved! Reconnecting...');
            setTimeout(()=>window.location.href='/',3000);
        } else {
            toast('Failed: '+d.error, false);
        }
    }).catch(()=>{
        btn.disabled = false;
        btn.innerHTML = '&#x1F4BE; Save &amp; Connect';
        toast('Error',false);
    });
}

loadWiFiStatus();
</script></body></html>)raw";

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
<div class="hdr-r"><span class="dot wd"></span><span class="dot td"></span>&nbsp;<span id="clk">--:--:--</span></div>
</header>
<main>
<p class="ptitle">Time &amp; NTP Settings</p>
<div class="card fcrd">
<div id="timeStatus" class="alert ai" style="display:none;margin-bottom:12px"></div>
<div class="fg">
<label>NTP Server</label>
<input type="text" id="srv" placeholder="time.google.com" required>
<small>Fallbacks: time.google.com &rarr; time.windows.com &rarr; time.cloudflare.com &rarr; time.facebook.com</small>
</div>
<div class="fg"><label>GMT Offset (seconds)</label><input type="number" id="gmt" required></div>
<div class="fg"><label>Daylight Saving (seconds)</label><input type="number" id="dst" value="0"></div>
<div class="fg"><label>Auto Sync (hours, 1&ndash;24)</label><input type="number" id="shi" min="1" max="24" value="1"></div>
<div style="display:flex;gap:8px;flex-wrap:wrap">
<button class="btn bsave" style="flex:1;margin-top:0" onclick="save()">&#x1F4BE; Save Settings</button>
<button class="btn bsync" id="sbtn" onclick="sync()" style="padding:9px 16px;border-radius:6px;font-size:13px;font-weight:600;margin-top:8px;white-space:nowrap">&#x1F504; Sync NTP</button>
<button class="btn bwarn" id="bsbtn" onclick="syncFromBrowser()" style="padding:9px 16px;border-radius:6px;font-size:13px;font-weight:600;margin-top:8px;white-space:nowrap">&#x1F310; Sync Browser</button>
</div>
<small style="display:block;margin-top:12px;color:#90A4AE">Use <strong>Sync Browser</strong> when NTP servers are unreachable. <strong>NTP</strong> will automatically override browser time when available. <strong>DS3231 RTC Module</strong> is the primary time authority and maintains time across power cycles.</small>
</div>
</main>
<div id="toast"></div>
<script>
function toast(m,ok=true){const t=document.getElementById('toast');t.textContent=m;t.className='show '+(ok?'ok':'er');clearTimeout(t._t);t._t=setTimeout(()=>t.className='',3000);}
function tick(){fetch('/api/time').then(r=>r.json()).then(d=>{document.getElementById('clk').textContent=d.time||'--:--:--';const w=document.querySelector('.wd'),t=document.querySelector('.td');if(w)w.className='dot '+(d.wifi?'g':'r');if(t){let tc='y';if(d.timeSource==='ntp')tc='g';else if(d.timeSource==='browser')tc='b';else if(d.timeSource==='rtc')tc='b';else tc='y';t.className='dot '+tc;}updateTimeStatus(d);}).catch(()=>{});}
function updateTimeStatus(d){
  const s=document.getElementById('timeStatus');
  s.style.display='block';
  let icon='',msg='',cls='ai';
  if(d.timeSource==='ntp'){
    icon='&#x2705;';msg='Time source: <strong>NTP Server</strong> — High Accuracy';
    cls='ai';
  }else if(d.timeSource==='browser'){
    icon='&#x1F310;';msg='Time source: <strong>Browser Sync</strong> — Moderate Accuracy';
    cls='aw';
  }else if(d.timeSource==='rtc'){
    icon='&#x1F4BF;';msg='Time source: <strong>DS3231 Real Time Clock</strong> — High Accuracy';
    cls='ai';
  }else{
    icon='&#x26A0;&#xFE0F;';msg='Time source: <strong>None</strong> — Please sync time';
    cls='aw';
  }
  let rtcInfo = '';
  if(d.rtcPresent){
    rtcInfo = '<br><small>✅ DS3231 RTC detected on GPIO8/9';
    if(d.rtcSynced) rtcInfo += ' | Last sync: ' + d.rtcSyncAge + 's ago';
    rtcInfo += '</small>';
  } else {
    rtcInfo = '<br><small>⚠️ DS3231 RTC not detected</small>';
  }
  s.innerHTML=icon+' '+msg+rtcInfo;
  s.className='alert '+cls;
}
setInterval(tick,1000);tick();
fetch('/api/ntp').then(r=>r.json()).then(d=>{
  document.getElementById('srv').value=d.ntpServer||'time.google.com';
  document.getElementById('gmt').value=d.gmtOffset||28800;
  document.getElementById('dst').value=d.daylightOffset||0;
  document.getElementById('shi').value=d.syncHours||1;
}).catch(()=>{});
function save(){
  const h=parseInt(document.getElementById('shi').value);
  if(h<1||h>24){toast('Sync interval must be 1\u201324 h',false);return;}
  fetch('/api/ntp',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({
    ntpServer:document.getElementById('srv').value,
    gmtOffset:parseInt(document.getElementById('gmt').value),
    daylightOffset:parseInt(document.getElementById('dst').value),
    syncHours:h
  })}).then(r=>r.json()).then(d=>{if(d.success)toast('NTP settings saved!');else toast('Failed: '+d.error,false);})
  .catch(()=>toast('Error',false));
}
function sync(){
  const b=document.getElementById('sbtn');b.disabled=true;b.textContent='Syncing\u2026';
  fetch('/api/ntp/sync',{method:'POST'}).then(r=>r.json()).then(d=>{
    b.disabled=false;b.innerHTML='&#x1F504; Sync Now';
    if(d.success)toast('Time synced via NTP!');else toast('Sync failed \u2014 check WiFi/NTP',false);
  }).catch(()=>{b.disabled=false;b.innerHTML='&#x1F504; Sync Now';toast('Error',false);});
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
    b.innerHTML = '&#x1F310; Sync from Browser';
    if (d.success) {
      document.getElementById('clk').textContent = d.local_time;
      let msg = 'Time synced from browser! Local time: ' + d.local_time;
      if(d.rtc_synced) msg += ' | DS3231 updated';
      toast(msg);
      tick();
    } else {
      toast('Sync failed: ' + (d.error || 'Unknown error'), false);
    }
  })
  .catch(() => {
    b.disabled = false;
    b.innerHTML = '&#x1F310; Sync from Browser';
    toast('Error syncing time from browser', false);
  });
}
</script></body></html>)raw";

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
<div class="hdr-r"><span class="dot wd"></span><span class="dot td"></span>&nbsp;<span id="clk">--:--:--</span></div>
</header>
<main>
<p class="ptitle">Access Point Settings</p>
<div class="card fcrd">
<div class="alert aw">&#x26A0;&#xFE0F; Changing SSID, Password, Channel or Visibility will restart the AP and disconnect all clients. Reconnect to the new SSID afterward.</div>
<div class="fg"><label>AP SSID</label><input type="text" id="ssid" maxlength="31" required></div>
<div class="fg"><label>AP Password</label><input type="password" id="pw" minlength="8" placeholder="Enter wifi password"></div>
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
<button class="btn bsave" onclick="save()">&#x1F4BE; Save Settings</button>
</div>
</main>
<div id="toast"></div>
<script>
function toast(m,ok=true){const t=document.getElementById('toast');t.textContent=m;t.className='show '+(ok?'ok':'er');clearTimeout(t._t);t._t=setTimeout(()=>t.className='',3000);}
function tick(){fetch('/api/time').then(r=>r.json()).then(d=>{document.getElementById('clk').textContent=d.time||'--:--:--';const w=document.querySelector('.wd'),t=document.querySelector('.td');if(w)w.className='dot '+(d.wifi?'g':'r');if(t){let tc='y';if(d.timeSource==='ntp')tc='g';else if(d.timeSource==='browser')tc='b';else if(d.timeSource==='rtc')tc='b';else tc='y';t.className='dot '+tc;}}).catch(()=>{});}
setInterval(tick,1000);tick();
fetch('/api/ap').then(r=>r.json()).then(d=>{
  document.getElementById('ssid').value=d.ap_ssid||'';
  document.getElementById('ch').value=d.ap_channel||6;
  document.getElementById('hidden').value=d.ap_hidden?'1':'0';
}).catch(()=>{});
function save(){
  const pw=document.getElementById('pw').value;
  if(pw.length>0&&pw.length<8){toast('Password must be 8+ chars or blank',false);return;}
  fetch('/api/ap',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({
    ap_ssid:document.getElementById('ssid').value,
    ap_password:pw,
    ap_channel:parseInt(document.getElementById('ch').value),
    ap_hidden:document.getElementById('hidden').value==='1'
  })}).then(r=>r.json()).then(d=>{
    if(d.success){toast('Settings saved!');if(d.restarted)toast('AP restarted - reconnect to new network');setTimeout(()=>location.reload(),4000);}
    else toast('Failed: '+d.error,false);
  }).catch(()=>toast('Error',false));
}
</script></body></html>)raw";

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
<div class="hdr-r"><span class="dot wd"></span><span class="dot td"></span>&nbsp;<span id="clk">--:--:--</span></div>
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

<div style="margin-top:16px;display:flex;gap:8px">
<button class="btn bwarn" onclick="resetDefaults()" style="padding:9px 18px">&#x1F504; Reset to Default</button>
</div>
</div>
</main>
<div id="toast"></div>
<script>
function toast(m,ok=true){const t=document.getElementById('toast');t.textContent=m;t.className='show '+(ok?'ok':'er');clearTimeout(t._t);t._t=setTimeout(()=>t.className='',3000);}
function tick(){fetch('/api/time').then(r=>r.json()).then(d=>{document.getElementById('clk').textContent=d.time||'--:--:--';const w=document.querySelector('.wd'),t=document.querySelector('.td');if(w)w.className='dot '+(d.wifi?'g':'r');if(t){let tc='y';if(d.timeSource==='ntp')tc='g';else if(d.timeSource==='browser')tc='b';else if(d.timeSource==='rtc')tc='b';else tc='y';t.className='dot '+tc;}}).catch(()=>{});}
setInterval(tick,1000);tick();
// change gpio pins
const DEFAULT_PINS = [1,2,3,4,5,6,7,10,11,12,13,14,15,16,17,18];
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

function loadGPIO() {
    fetch('/api/gpio').then(r=>r.json()).then(d=>{
        gpioData = d;
        document.getElementById('relayCount').textContent = d.count;
        
        fetch('/api/gpio/global-mode').then(r=>r.json()).then(data=>{
    if(data.mode !== undefined) {
        document.getElementById('globalMode').value = data.mode;
    }
       }).catch(()=>{});
        
        let pinsHtml = '';
        const globalMode = parseInt(document.getElementById('globalMode').value);
        const isGlobalMode = (globalMode === 1 || globalMode === 2);
        const globalModeText = (globalMode === 1) ? 'GLOBAL ACTIVE LOW' : (globalMode === 2) ? 'GLOBAL ACTIVE HIGH' : null;
        
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
    const pin = parseInt(document.getElementById('newPin').value);
    if(!pin) return;
    
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
<div class="hdr-r"><span class="dot wd"></span><span class="dot td"></span>&nbsp;<span id="clk">--:--:--</span></div>
</header>
<main>
<p class="ptitle">System Information &amp; Settings</p>
<div class="ibar" id="ibar">
<div class="ibox"><div class="l">STA IP</div><div class="v" id="sip">&hellip;</div></div>
<div class="ibox"><div class="l">AP IP</div><div class="v" id="sap">&hellip;</div></div>
<div class="ibox"><div class="l">Free Heap</div><div class="v" id="shp">&hellip;</div></div>
<div class="ibox"><div class="l">Uptime</div><div class="v" id="sup">&hellip;</div></div>
<div class="ibox"><div class="l">WiFi RSSI</div><div class="v" id="srs">&hellip;</div></div>
<div class="ibox"><div class="l">Time Source</div><div class="v" id="stsrc" style="font-size:13px">&hellip;</div></div>
<div class="ibox"><div class="l">UTC Epoch</div><div class="v" id="sutc" style="font-size:11px">&hellip;</div></div>
<div class="ibox"><div class="l">NTP Last Sync</div><div class="v" id="snt">&hellip;</div></div>
<div class="ibox"><div class="l">Last Browser Sync</div><div class="v" id="sbs">&hellip;</div></div>
<div class="ibox"><div class="l">NTP Server</div><div class="v" id="sns" style="font-size:12px">&hellip;</div></div>
<div class="ibox"><div class="l">Chip Model</div><div class="v" id="sch">&hellip;</div></div>
<div class="ibox"><div class="l">mDNS Hostname</div><div class="v" id="smdns">&hellip;</div></div>
<div class="ibox"><div class="l">Relay Count</div><div class="v" id="src">&hellip;</div></div>
<div class="ibox"><div class="l">GMT Offset</div><div class="v" id="sgmt" style="font-size:13px">&hellip;</div></div>
<div class="ibox"><div class="l">Drift Comp</div><div class="v" id="sdrift" style="font-size:13px">&hellip;</div></div>
<div class="ibox"><div class="l">GPIO Mode</div><div class="v" id="sactmode" style="font-size:12px">&hellip;</div></div>
<div class="ibox"><div class="l">DS3231 RTC</div><div class="v" id="srtc">&hellip;</div></div>
<div class="ibox"><div class="l">DS3231 Last Sync</div><div class="v" id="srtcsync">&hellip;</div></div>
<div class="ibox"><div class="l">Sync Direction</div><div class="v" id="ssyncdir" style="font-size:12px;color:#7B1FA2">&hellip;</div></div>
<div class="ibox"><div class="l">WiFi Station</div><div class="v" id="stawifi">&hellip;</div></div>
</div>

<div class="card fcrd">
<p style="font-weight:700;margin-bottom:12px">Device Control</p>
<div style="display:flex;gap:8px;flex-wrap:wrap">
<button class="btn bwarn" onclick="rst()" style="padding:9px 18px;border-radius:6px;font-size:13px;font-weight:600">&#x1F504; Verify Services</button>
<button class="btn bdanger" onclick="fct()" style="padding:9px 18px;border-radius:6px;font-size:13px;font-weight:600">&#x26A0; Factory Reset</button>
</div>
<p style="color:#90A4AE;font-size:12px;margin-top:10px">Factory reset clears all the settings without restarting the device.<br>Alternatively, hold the <strong>BOOT button</strong> for 5 seconds to trigger a hardware factory reset.</p>
</div>
</main>
<div id="toast"></div>
<script>
function toast(m,ok=true){const t=document.getElementById('toast');t.textContent=m;t.className='show '+(ok?'ok':'er');clearTimeout(t._t);t._t=setTimeout(()=>t.className='',3000);}
function tick(){fetch('/api/time').then(r=>r.json()).then(d=>{document.getElementById('clk').textContent=d.time||'--:--:--';const w=document.querySelector('.wd'),t=document.querySelector('.td');if(w)w.className='dot '+(d.wifi?'g':'r');if(t){let tc='y';if(d.timeSource==='ntp')tc='g';else if(d.timeSource==='browser')tc='b';else if(d.timeSource==='rtc')tc='b';else tc='y';t.className='dot '+tc;}}).catch(()=>{});}
setInterval(tick,1000);tick();
function fmtUp(s){const h=Math.floor(s/3600),m=Math.floor((s%3600)/60),ss=s%60;return h+'h '+m+'m '+ss+'s';}
function fmtAge(s){if(s===4294967295||s<0)return'Never';if(s<60)return'Just now';if(s<3600)return Math.floor(s/60)+' min ago';const h=Math.floor(s/3600);return h+'h '+Math.floor((s%3600)/60)+'m ago';}
function rssiDesc(r){if(!r)return'\u2014';return r+'dBm ('+( r>=-50?'Excellent':r>=-60?'Good':r>=-70?'Fair':'Weak')+')';}
function loadSys(){
  fetch('/api/system').then(r=>r.json()).then(d=>{
    document.getElementById('sip').textContent=d.wifiConnected?d.ip:'(not connected)';
    document.getElementById('sap').textContent=d.ap_ip;
    document.getElementById('shp').textContent=(d.freeHeap/1024).toFixed(1)+' KB';
    document.getElementById('sup').textContent=fmtUp(d.uptime);
    document.getElementById('srs').textContent=d.wifiConnected?rssiDesc(d.rssi):'\u2014';
    const tsrc=d.timeSource||'None';
    let tsrcStyle='';
    if(tsrc==='NTP')tsrcStyle='color:#2E7D32';
    else if(tsrc==='Browser')tsrcStyle='color:#1565C0';
    else if(tsrc==='RTC')tsrcStyle='color:#7B1FA2';
    else tsrcStyle='color:#C62828';
    document.getElementById('stsrc').style.cssText='font-size:13px;font-weight:700;'+tsrcStyle;
    document.getElementById('stsrc').textContent=tsrc;
    document.getElementById('sutc').textContent=d.utcEpoch||'\u2014';
    document.getElementById('snt').textContent=d.ntpSyncAge>=0?fmtAge(d.ntpSyncAge):'Never';
    document.getElementById('sbs').textContent=d.browserSyncAge>=0?fmtAge(d.browserSyncAge):'Never';
    document.getElementById('sns').textContent=d.ntpServer||'\u2014';
    document.getElementById('sch').textContent=d.chipModel||'ESP32S3';
    document.getElementById('smdns').textContent=d.mdnsStarted ? d.mdnsHostname+'.local' : 'Not running';
    document.getElementById('src').textContent=d.relayCount+' / '+d.maxRelays;
    document.getElementById('sgmt').textContent=(d.gmtOffset/3600).toFixed(1)+'h (UTC'+(d.gmtOffset>=0?'+':'')+(d.gmtOffset/3600).toFixed(1)+')';
    document.getElementById('sdrift').textContent=d.driftComp ? d.driftComp.toFixed(4) : '1.0000';
    let modeText = '';
    if(d.globalActiveMode === 1) modeText = '🌍 Global LOW';
    else if(d.globalActiveMode === 2) modeText = '🌍 Global HIGH';
    else modeText = '🔧 Per-Relay';
    document.getElementById('sactmode').textContent = modeText;
    document.getElementById('srtc').textContent = d.rtcPresent ? '✅ Present' : '❌ Not detected';
    if(d.rtcPresent) document.getElementById('srtc').style.color='#2E7D32';
    else document.getElementById('srtc').style.color='#C62828';
    document.getElementById('srtcsync').textContent = d.rtcSyncAge >= 0 ? fmtAge(d.rtcSyncAge) : 'Never';
    document.getElementById('ssyncdir').textContent = d.rtcPresent ? 'DS3231 → Internal RTC' : 'Not available';
    const staStatus = d.staEnabled ? (d.wifiConnected ? '✅ Connected' : '⚠️ Enabled but disconnected') : '❌ Disabled';
    document.getElementById('stawifi').textContent = staStatus;
    if(d.staEnabled && d.wifiConnected) document.getElementById('stawifi').style.color='#2E7D32';
    else if(d.staEnabled) document.getElementById('stawifi').style.color='#F9A825';
    else document.getElementById('stawifi').style.color='#C62828';
  }).catch(()=>{});
}
loadSys();setInterval(loadSys,5000);
function rst(){
  if(!confirm('Verify all services without restarting? This will check WiFi, web server, DNS, and mDNS.'))return;
  fetch('/api/reset',{method:'POST'}).then(r=>r.json()).then(d=>{
    if(d.success)toast('Services verified!');
    else toast('Verification failed',false);
  }).catch(()=>toast('Error',false));
}
function fct(){
  if(!confirm('FACTORY RESET \u2014 ALL settings will be erased. Continue?'))return;
  fetch('/api/factory-reset',{method:'POST'}).then(r=>r.json()).then(d=>{
    if(d.success)toast('Factory reset complete');
    else toast('Reset failed',false);
  }).catch(()=>{});
  setTimeout(()=>window.location.href='/',7000);
}
</script></body></html>)raw";

// =============================================================================
//  BOOT BUTTON FACTORY RESET
// =============================================================================
void checkBootButton() {
    bool buttonState = !digitalRead(BOOT_BUTTON_PIN);     
    if (buttonState && !bootButtonPressed) {
        bootButtonPressed = true;
        bootButtonPressStart = millis();
    }
    else if (!buttonState && bootButtonPressed) {
        bootButtonPressed = false;
    }    
    if (bootButtonPressed && !factoryResetTriggered && 
        (millis() - bootButtonPressStart >= FACTORY_RESET_HOLD)) {        
        factoryResetTriggered = true;        
        preferences.begin(NVS_NAMESPACE, false);
        preferences.clear();
        preferences.end();        
        initDefaults();
        loadGPIOConfig();
        loadConfiguration();
        loadExtConfig();        
        for (int i = 0; i < gpioConfig.count; i++) {
            setRelayOutput(i, false);
            lastRelayOutputs[i] = false;
            relayConfigs[i].manualOverride = false;
            relayConfigs[i].manualState = false;
        }        
        relayOutputsInitialized = true;        
        if (extConfig.sta_enabled && strlen(sysConfig.sta_ssid) > 0) {
            WiFi.disconnect(true);
            delay(100);
            WiFi.begin(sysConfig.sta_ssid, sysConfig.sta_password);
            wifiConnecting = true;
            wifiConnectStart = millis();
            wifiConnected = false;
            wifiReconnectAttempts = 0;
            wifiGiveUpUntil = 0;
            wifiFirstAttempt = true;
        }        
        healer.restartAPIfNeeded(true);        
        healer.performTargetedRecovery();        
        updateScheduleCache();        
        factoryResetTriggered = false;
    }
}

// =============================================================================
//  GPIO CONFIGURATION FUNCTIONS
// =============================================================================
void loadGPIOConfig() {
    preferences.begin(NVS_NAMESPACE, true);
    size_t len = preferences.getBytes("gpioConfig", &gpioConfig, sizeof(GPIOPinConfig));
    preferences.end();    
    if (len != sizeof(GPIOPinConfig) || gpioConfig.magic != GPIO_CONFIG_MAGIC || 
        gpioConfig.count == 0 || gpioConfig.count > MAX_RELAYS) {
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
void syncInternalRTC(uint64_t rawUtcEpoch) {
    if (!VALID_UNIX_TIME_64(rawUtcEpoch)) return;    
    unsigned long nowMicros = micros();
    unsigned long nowMillis = millis();    
    internalEpoch = rawUtcEpoch;
    internalMillisAtLastSync = nowMillis;
    rtcMicrosAtLastSync = nowMicros;
    lastRTCRebase = nowMillis;
    rtcInitialized = true;
    if (timeSource == TIME_SOURCE_BROWSER) {
        lastBrowserSync = nowMillis;
    } else {
        lastNTPSync = nowMillis;
        timeSource = TIME_SOURCE_NTP;
        lastBrowserSync = 0;
    }    
    ntpFailCount = 0;    
    if (rtcPresent) {
        immediateDS3231Sync();
    } 
    saveRTCState();
    criticalStateDirty = true;    
    updateScheduleCache();
}

// =============================================================================
//  64-bit NTP Client Function
// =============================================================================
uint64_t getNTP64Time(const char* server) {
    WiFiUDP ntpUDP;
    byte packetBuffer[NTP_PACKET_SIZE];    
    for (int retry = 0; retry < NTP_RETRY_COUNT; retry++) {
        if (!ntpUDP.begin(2390)) {
            delay(100);
            continue;
        }        
        memset(packetBuffer, 0, NTP_PACKET_SIZE);
        packetBuffer[0] = 0b11100011;  
        packetBuffer[1] = 0;           
        packetBuffer[2] = 6;           
        packetBuffer[3] = 0xEC;        
        uint64_t now = getCurrentEpoch();  
        uint64_t ntpNow = now + NTP_EPOCH_OFFSET;  
        uint32_t seconds = (uint32_t)(ntpNow & 0xFFFFFFFF);
        packetBuffer[40] = (seconds >> 24) & 0xFF;
        packetBuffer[41] = (seconds >> 16) & 0xFF;
        packetBuffer[42] = (seconds >> 8) & 0xFF;
        packetBuffer[43] = seconds & 0xFF;
        packetBuffer[44] = 0;
        packetBuffer[45] = 0;
        packetBuffer[46] = 0;
        packetBuffer[47] = 0;        
        ntpUDP.beginPacket(server, NTP_PORT);
        ntpUDP.write(packetBuffer, NTP_PACKET_SIZE);
        if (!ntpUDP.endPacket()) {
            ntpUDP.stop();
            delay(500);
            continue;
        }        
        unsigned long startTime = millis();
        while (millis() - startTime < NTP_TIMEOUT) {
            int packetSize = ntpUDP.parsePacket();
            if (packetSize >= NTP_PACKET_SIZE) {
                ntpUDP.read(packetBuffer, NTP_PACKET_SIZE);
                ntpUDP.stop();
                uint8_t mode = packetBuffer[0] & 0x07;
                if (mode != 4) continue;                  
                uint8_t li = (packetBuffer[0] >> 6) & 0x03;
                if (li == 3) return 0;  
                uint32_t secs = (packetBuffer[40] << 24) |
                               (packetBuffer[41] << 16) |
                               (packetBuffer[42] << 8) |
                               packetBuffer[43];                
                uint64_t unixEpoch = (uint64_t)secs - NTP_EPOCH_OFFSET;
                if (unixEpoch < 1577836800ULL) return 0;                
                return unixEpoch;
            }
            delay(10);
            yield();
        }        
        ntpUDP.stop();
        delay(500);
    }    
    ntpUDP.stop();
    return 0;
}

// =============================================================================
//  64-bit NTP SYNC
// =============================================================================
void tryNTPSync() {
    if (!wifiConnected || !extConfig.sta_enabled) {
        ntpAsyncState = NTP_STATE_IDLE;
        return;
    }    
    unsigned long now = millis();
    if (!timeHasElapsed(now, lastNTPAttempt, NTP_RETRY_INTERVAL)) {
        return;
    }
    lastNTPAttempt = now;
    uint64_t ntpTime = getNTP64Time(NTP_SERVERS[ntpServerIndex]);    
    if (ntpTime > 1577836800ULL) {  
        syncInternalRTC(ntpTime);
        ntpFailCount = 0;
        health.ntpFailures = 0;
        ntpServerIndex = (ntpServerIndex + 1) % NUM_NTP_SERVERS;  
        return;
    }
    for (uint8_t i = 0; i < NUM_NTP_SERVERS - 1; i++) {
        uint8_t idx = (ntpServerIndex + 1 + i) % NUM_NTP_SERVERS;
        ntpTime = getNTP64Time(NTP_SERVERS[idx]);        
        if (ntpTime > 1577836800ULL) {
            syncInternalRTC(ntpTime);
            ntpFailCount = 0;
            health.ntpFailures = 0;
            ntpServerIndex = (idx + 1) % NUM_NTP_SERVERS;
            return;
        }
    }
    ntpFailCount++;
    health.ntpFailures++;
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
    uint64_t browserUtcEpoch = doc["utc_epoch"];    
    if (browserUtcEpoch < 1577836800ULL || browserUtcEpoch > MAX_UNIX_TIME_64) {
        server.send(400, "application/json", 
            "{\"success\":false,\"error\":\"Invalid epoch time. Expected between 2020-2106+.\"}");
        return;
    }    
    timeSource = TIME_SOURCE_BROWSER;  
    syncInternalRTC(browserUtcEpoch);    
    timeSource = TIME_SOURCE_BROWSER;
    lastBrowserSync = millis();
    lastNTPSync = 0;
    if (rtcPresent && rtcInitialized && internalEpoch > 0) {
        if (rtc.begin()) {
            DateTime dt = uint64ToRtcDateTime(internalEpoch);
            rtc.adjust(dt);
            rtcTimeValid = true;
            lastRTCDSync = millis();
        }
    }
    saveRTCState();
    updateScheduleCache();
    criticalStateDirty = true;    
    uint64_t localEpoch = getLocalEpoch(browserUtcEpoch);
    struct tm* ti = gmtime64(&localEpoch);
    char timeStr[20];
    if (ti) {
        snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d",
                 ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday,
                 ti->tm_hour, ti->tm_min, ti->tm_sec);
    } else {
        strcpy(timeStr, "Error");
    }    
    String resp;
    DynamicJsonDocument respDoc(256);
    respDoc["success"] = true;
    respDoc["utc_epoch"] = (uint32_t)browserUtcEpoch;  
    respDoc["local_time"] = timeStr;
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
    if (!isTimeReached(millis(), wifiGiveUpUntil)) return;  
    if (wifiConnecting) return;
    if (wifiPausedForScan) return; 
    WiFi.disconnect(true);
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
//  RELAY FUNCTIONS
// =============================================================================
void updateRelayOutputs() {
    for (int i = 0; i < gpioConfig.count; i++) {
        bool state = false;        
        if (relayConfigs[i].manualOverride) {
            state = relayConfigs[i].manualState;
        } else {
            state = scheduleActiveCache[i];
        }     
        if (!relayOutputsInitialized || state != lastRelayOutputs[i]) {
            setRelayOutput(i, state);
            lastRelayOutputs[i] = state;
        }
    }
    relayOutputsInitialized = true;
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
    cachedTodayBit = (1 << ti->tm_wday);
    cachedMonthDay = ti->tm_mday;
    cachedMonth = ti->tm_mon;        
    lastScheduleEpoch = epoch;    
    int cur = ti->tm_hour * 3600 + ti->tm_min * 60 + ti->tm_sec;    
    for (int i = 0; i < gpioConfig.count; i++) {
        if (relayConfigs[i].manualOverride) {
            scheduleActiveCache[i] = false;
            continue;
        }     
        bool hasActive = false;        
        for (int s = 0; s < 8; s++) {
            if (!relayConfigs[i].schedule.enabled[s]) continue;
            uint16_t monthMask = relayConfigs[i].schedule.monthMask[s];
            if (monthMask != 0 && !(monthMask & (1 << cachedMonth))) continue;            
            if (!(relayConfigs[i].schedule.days[s] & cachedTodayBit)) continue;
            uint32_t monthDayMask = relayConfigs[i].schedule.monthDays[s];
            if (monthDayMask != 0 && !(monthDayMask & (1 << (cachedMonthDay - 1)))) continue;            
            int start = relayConfigs[i].schedule.startHour[s] * 3600
                      + relayConfigs[i].schedule.startMinute[s] * 60
                      + relayConfigs[i].schedule.startSecond[s];
            int stop = relayConfigs[i].schedule.stopHour[s] * 3600
                     + relayConfigs[i].schedule.stopMinute[s] * 60
                     + relayConfigs[i].schedule.stopSecond[s];
            if (start == stop) {
                hasActive = true;
                break;
            }
            if (start < stop) {
                if (cur >= start && cur < stop) {
                    hasActive = true;
                    break;
                }
            }
            else {
                if (cur >= start || cur < stop) {
                    hasActive = true;
                    break;
                }
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
    int cur = ti->tm_hour * 3600 + ti->tm_min * 60 + ti->tm_sec;
    uint8_t todayBit = cachedTodayBit;
    int monthDay = cachedMonthDay;
    int currentMonth = cachedMonth;      
    static unsigned long lastStateChange[MAX_RELAYS] = {0};
    static bool lastDebouncedState[MAX_RELAYS] = {false};        
    for (int i = 0; i < gpioConfig.count; i++) {
        if (relayConfigs[i].manualOverride) {
            bool targetState = relayConfigs[i].manualState;
            if (lastRelayOutputs[i] != targetState) {
                setRelayOutput(i, targetState);
                lastRelayOutputs[i] = targetState;
                lastDebouncedState[i] = targetState;
                criticalStateDirty = true;
            }
            continue;
        }
        bool shouldBeOn = false;        
        for (int s = 0; s < 8; s++) {
            if (!relayConfigs[i].schedule.enabled[s]) continue;            
            uint16_t monthMask = relayConfigs[i].schedule.monthMask[s];
            if (monthMask != 0 && !(monthMask & (1 << currentMonth))) continue;            
            if (!(relayConfigs[i].schedule.days[s] & todayBit)) continue;           
            uint32_t monthDayMask = relayConfigs[i].schedule.monthDays[s];
            if (monthDayMask != 0 && !(monthDayMask & (1 << (monthDay - 1)))) continue;            
            int start = relayConfigs[i].schedule.startHour[s]   * 3600
                      + relayConfigs[i].schedule.startMinute[s]  * 60
                      + relayConfigs[i].schedule.startSecond[s];
            int stop  = relayConfigs[i].schedule.stopHour[s]    * 3600
                      + relayConfigs[i].schedule.stopMinute[s]   * 60
                      + relayConfigs[i].schedule.stopSecond[s];
            if (start == stop) {
                shouldBeOn = true;
                break;  
            }            
            if (start < stop) {
                if (cur >= start && cur < stop) {
                    shouldBeOn = true;
                    break;  
                }
            }
            else {
                if (cur >= start || cur < stop) {
                    shouldBeOn = true;
                    break;  
                }
            }
        }        
        if (lastDebouncedState[i] != shouldBeOn) {
            if (timeHasElapsed(now, lastStateChange[i], 500)) {
                setRelayOutput(i, shouldBeOn);
                lastRelayOutputs[i] = shouldBeOn;
                lastDebouncedState[i] = shouldBeOn;
                lastStateChange[i] = now;
                criticalStateDirty = true;
            }
        }
    }
    relayOutputsInitialized = true;
}
void restartAP() {
    healer.restartAPIfNeeded(false);
}

// =============================================================================
//  mDNS FUNCTIONS
// =============================================================================
void startMDNS() {
    if (mdnsStarted) return;    
    String hostname = String(mdnsHostname);
    if (hostname.length() == 0 || hostname == MDNS_HOSTNAME_DEFAULT) {
        hostname = String(sysConfig.ap_ssid);
        hostname.toLowerCase();
        hostname.replace(" ", "-");
        hostname.replace("_", "-");        
        String clean;
        for (char c : hostname) {
            if (isalnum(c) || c == '-') clean += c;
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
        MDNS.addServiceTxt("http", "tcp", "version", "v11"); 
        MDNS.addServiceTxt("http", "tcp", "channels", String(gpioConfig.count).c_str());
        MDNS.addServiceTxt("http", "tcp", "features", "monthmask,64bit");  
        mdnsStarted = true;
    } else {
        mdnsStarted = false;
    }
}

void stopMDNS() {
    if (mdnsStarted) {
        MDNS.end();
        mdnsStarted = false;
    }
}

void restartMDNS() {
    healer.liveReconfigureMDNS();
}

void scheduleMDNSRestart() {
    mdnsRestartScheduled = true;
    mdnsRestartPending = millis() + MDNS_RESTART_DELAY;
}

String getMDNSHostname() {
    return String(mdnsHostname);
}

void setMDNSHostname(const char* hostname) {
    if (hostname && strlen(hostname) > 0 && strlen(hostname) < 32) {
        String sanitized;
        for (size_t i = 0; i < strlen(hostname); i++) {
            char c = tolower(hostname[i]);
            if (isalnum(c) || c == '-') {
                sanitized += c;
            } else if (c == ' ' || c == '_') {
                sanitized += '-';
            }
        }        
        if (sanitized.length() > 0) {
            strncpy(mdnsHostname, sanitized.c_str(), 31);
            mdnsHostname[31] = '\0';
            if (mdnsStarted) {
                scheduleMDNSRestart();
            }
        }
    }
}

// =============================================================================
//  SETUP
// =============================================================================
void setup() {
    initRTC();    
    loadGPIOConfig();
    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
    for (int i = 0; i < gpioConfig.count; i++) {
        pinMode(getRelayPin(i), OUTPUT);
        setRelayOutput(i, false); 
        lastRelayOutputs[i] = false;
    }
    relayOutputsInitialized = true;    
    for (int i = 0; i < MAX_RELAYS; i++) {
        initScheduleDefaults(i);  
    }
    loadConfiguration();
    loadExtConfig();    
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
    }    
    WiFi.mode(WIFI_AP_STA);    
    if (extConfig.sta_enabled && strlen(sysConfig.sta_ssid) > 0) {
        WiFi.disconnect(true);
        delay(100);
        WiFi.begin(sysConfig.sta_ssid, sysConfig.sta_password);
        wifiConnecting = true;
        wifiConnectStart = millis();
        wifiFirstAttempt = true;
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
    if (strlen(sysConfig.ap_password) > 0) {
        WiFi.softAP(sysConfig.ap_ssid, sysConfig.ap_password, ch, hidden);
    } else {
        WiFi.softAP(sysConfig.ap_ssid, NULL, ch, hidden);
    }
    startMDNS();
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    setupWebServer();
    updateScheduleCache();
    if (!healer.restoreCriticalState()) {
    }    
    lastMemoryCleanup = millis();
    lastHeapCheck = millis();
    lastConnectionActivity = millis();
    lastServerRestart = millis();
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
    unsigned long now = millis();    
    if (server.client()) {
        lastConnectionActivity = now;
    }    
    if (timeHasElapsed(now, lastConnectionCleanup, 60000)) {
        lastConnectionCleanup = now;        
        WiFiClient client = server.client();
        int closedCount = 0;
        while (client && closedCount < 5) {
            if (client.connected()) {
                client.stop();
                closedCount++;
            }
            client = server.client();
        }        
        if (!scanInProgress) {
            WiFi.scanDelete();
        }
    }
    static unsigned long lastMemClean = 0;
    if (timeHasElapsed(now, lastMemClean, MEMORY_CLEANUP_INTERVAL)) {
        lastMemClean = now;
        cleanupStaleResources();
    }    
    static unsigned long lastHealthCheck = 0;
    if (timeHasElapsed(now, lastHealthCheck, 60000)) {
        lastHealthCheck = now;
        checkWebServerHealth();
    }    
    if (timeHasElapsed(now, lastHealingCheck, 10000)) {
        lastHealingCheck = now;
        healer.smartRecovery();
    }
    checkAndCleanMemory();    
    if (rtcPresent && rtcTimeValid && rtcInitialized) {
        if (timeHasElapsed(now, lastRTCDSync, DS3231_SYNC_INTERVAL)) {
            lastRTCDSync = now;
            if (rtc.begin()) {
                DateTime nowDS = rtc.now();
                if (nowDS.year() >= 2020 && nowDS.year() <= 2100) {
                    uint64_t dsEpoch = rtcDateTimeToUint64(nowDS);
                    if (VALID_UNIX_TIME_64(dsEpoch)) {
                        internalEpoch = dsEpoch;
                        driftCompensation = 1.0f;
                        rtcMicrosAtLastSync = micros();
                        lastRTCRebase = millis();
                        if (timeSource != TIME_SOURCE_NTP) {
                            timeSource = TIME_SOURCE_RTC;
                        }
                    }
                }
            }
        }
    }    
    autoSaveInternalRTC();    
    if (mdnsRestartScheduled && isTimeReached(now, mdnsRestartPending)) { 
        mdnsRestartScheduled = false;
        restartMDNS();
    }
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
            wifiFirstAttempt = false;
            wifiPausedForScan = false; 
            lastNTPSync = 0;
            tryNTPSync();
        } else if (timeHasElapsed(now, wifiConnectStart, WIFI_CONNECT_TIMEOUT)) {
            wifiConnecting = false;
            wifiConnected = false;            
            if (wifiReconnectAttempts >= MAX_RECONNECT && !wifiFirstAttempt) {
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
            if (!wifiConnecting && !wifiConnected && strlen(sysConfig.sta_ssid) > 0 && isTimeReached(now, wifiGiveUpUntil)) {  
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
            lastNTPSync = 0;
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
    }
    if (timeHasElapsed(now, lastScheduleProcess, SCHEDULE_PROCESS_INTERVAL)) {
        lastScheduleProcess = now;
        processRelaySchedules();
    }    
    static unsigned long lastCriticalSave = 0;
    if (criticalStateDirty && timeHasElapsed(now, lastCriticalSave, 300000)) {
        lastCriticalSave = now;
        healer.saveCriticalState();
        criticalStateDirty = false;
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
    strcpy(sysConfig.ap_password, "ESP32-admin");
    strcpy(sysConfig.ntp_server,  "time.google.com");
    sysConfig.gmt_offset      = 28800;
    sysConfig.daylight_offset = 0;
    sysConfig.last_rtc_epoch  = 0;
    sysConfig.rtc_drift       = 1.0f;  
    strcpy(sysConfig.hostname, "esp32s3");    
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
    size_t len = preferences.getBytes("sysConfig", &sysConfig, sizeof(SystemConfig));
    bool configValid = true;    
    if (len != sizeof(SystemConfig) || sysConfig.magic != EEPROM_MAGIC) {
        configValid = false;
    }    
    if (!configValid) {
        preferences.end();
        initDefaults();
        preferences.begin(NVS_NAMESPACE, true);
        len = preferences.getBytes("sysConfig", &sysConfig, sizeof(SystemConfig));
    }
    if (sysConfig.version < 11) {
        if (sysConfig.version < 10) {
            for (int i = 0; i < MAX_RELAYS; i++) {
                for (int s = 0; s < 8; s++) {
                    relayConfigs[i].schedule.monthMask[s] = MONTH_ALL;
                }
            }
        }
        sysConfig.version = 11;
        saveConfiguration();
    }    
    len = preferences.getBytes("relayConfigs", relayConfigs, sizeof(RelayConfig) * MAX_RELAYS);
    if (len != sizeof(RelayConfig) * MAX_RELAYS) {
        for (int i = 0; i < MAX_RELAYS; i++) {
            initScheduleDefaults(i);  
        }
    }    
    preferences.end();    
    strcpy(ap_ssid,     sysConfig.ap_ssid);
    strcpy(ap_password, sysConfig.ap_password);
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
    if (len != sizeof(ExtConfig) || extConfig.magic != EXT_CFG_MAGIC) {
        memset(&extConfig, 0, sizeof(ExtConfig));
        extConfig.magic          = EXT_CFG_MAGIC;
        extConfig.ap_channel     = 6;
        extConfig.ntp_sync_hours = 1;
        extConfig.ap_hidden      = 0;
        extConfig.global_active_mode = 0; 
        extConfig.sta_enabled    = 1;  
        preferences.end();
        saveExtConfig();
        preferences.begin(NVS_NAMESPACE, true);
    } else {
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
    preferences.end();
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
    server.on("/api/mdns", HTTP_GET, []() {
    String resp = "{\"hostname\":\"" + getMDNSHostname() + 
                      "\",\"started\":" + String(mdnsStarted ? "true" : "false") +
                      ",\"url\":\"http://" + getMDNSHostname() + ".local\"}";
    server.send(200, "application/json", resp);
    });  
    server.on("/api/mdns", HTTP_POST, []() {
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
        const char* hostname = doc["hostname"];
        if (hostname && strlen(hostname) > 0 && strlen(hostname) < 32) {
            setMDNSHostname(hostname);
            server.send(200, "application/json", "{\"success\":true,\"hostname\":\"" + getMDNSHostname() + "\"}");
        } else {
            server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid hostname\"}");
        }
    });    
    server.on("/api/mdns/restart", HTTP_POST, []() {
        restartMDNS();
        server.send(200, "application/json", "{\"success\":true}");
    });
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
    lastConnectionActivity = millis();    
    if (responseCache.valid && 
        !timeHasElapsed(millis(), responseCache.lastUpdate, 2000) && 
        responseCache.relaysJson.length() > 0) {
        server.send(200, "application/json", responseCache.relaysJson);
        return;
    }    
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "application/json", "");    
    server.sendContent("[");
    for (int i = 0; i < gpioConfig.count; i++) {
        if (i > 0) server.sendContent(",");        
        StaticJsonDocument<512> relayDoc;
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
    server.sendContent("]");    
    responseCache.valid = false;
}

void handleManualControl() {
    lastConnectionActivity = millis();    
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
    int relay = doc["relay"]; 
    bool state = doc["state"];
    if (relay >= 0 && relay < gpioConfig.count) {
        relayConfigs[relay].manualOverride = true;
        relayConfigs[relay].manualState    = state;
        scheduleActiveCache[relay] = false;
        setRelayOutput(relay, state);
        lastRelayOutputs[relay] = state;
        saveConfiguration();
        criticalStateDirty = true;
        responseCache.valid = false;
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid relay\"}");
    }
}

void handleResetManual() {
    lastConnectionActivity = millis();    
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
    int relay = doc["relay"];
    if (relay >= 0 && relay < gpioConfig.count) {
        relayConfigs[relay].manualOverride = false;
        updateScheduleCache();
        saveConfiguration();
        responseCache.valid = false;
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid relay\"}");
    }
}

void handleSaveRelay() {
    lastConnectionActivity = millis();    
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"No data\"}"); 
        return;
    }    
    StaticJsonDocument<5120> doc;  
    DeserializationError err = deserializeJson(doc, server.arg("plain"));
    if (err) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Bad JSON\"}"); 
        return;
    }    
    int relay = doc["relay"];
    if (relay < 0 || relay >= gpioConfig.count) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid relay\"}");
        return;
    }    
    JsonArray schedules = doc["schedules"].as<JsonArray>();
    int s = 0;
    for (JsonObject sch : schedules) {
        if (s >= 8) break;
        relayConfigs[relay].schedule.startHour[s]   = sch["startHour"];
        relayConfigs[relay].schedule.startMinute[s] = sch["startMinute"];
        relayConfigs[relay].schedule.startSecond[s] = sch["startSecond"];
        relayConfigs[relay].schedule.stopHour[s]    = sch["stopHour"];
        relayConfigs[relay].schedule.stopMinute[s]  = sch["stopMinute"];
        relayConfigs[relay].schedule.stopSecond[s]  = sch["stopSecond"];
        relayConfigs[relay].schedule.enabled[s]     = sch["enabled"];
        relayConfigs[relay].schedule.days[s]        = sch["days"] | 0;
        relayConfigs[relay].schedule.monthDays[s]   = sch["monthDays"] | 0;
        relayConfigs[relay].schedule.monthMask[s]   = sch["monthMask"] | MONTH_ALL;  
        s++;
    }    
    saveConfiguration();
    updateScheduleCache();
    responseCache.valid = false;
    server.send(200, "application/json", "{\"success\":true}");
}

void handleRelayName() {
    lastConnectionActivity = millis();    
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
    int relay = doc["relay"];
    const char* name = doc["name"];    
    if (relay >= 0 && relay < gpioConfig.count && name) {
        strncpy(relayConfigs[relay].name, name, 15);
        relayConfigs[relay].name[15] = '\0';
        saveConfiguration();
        responseCache.valid = false;
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid data\"}");
    }
}

void handleGetTime() {
    lastConnectionActivity = millis();    
    String ts = "--:--:--";
    uint64_t utcEp = getCurrentEpoch();
    if (utcEp > MIN_UNIX_TIME_64) {
        uint64_t localEp = getLocalEpoch(utcEp);
        struct tm* t = gmtime64(&localEp);
        if (t) {
            char buf[12];
            sprintf(buf, "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
            ts = buf;
        }
    }    
    String timeSourceStr = "none";
    if (timeSource == TIME_SOURCE_NTP) timeSourceStr = "ntp";
    else if (timeSource == TIME_SOURCE_BROWSER) timeSourceStr = "browser";
    else if (timeSource == TIME_SOURCE_RTC) timeSourceStr = "rtc";    
    unsigned long rtcSyncAge = (lastRTCDSync > 0) ? (millis() - lastRTCDSync) / 1000UL : 0;
    String resp = "{\"time\":\"" + ts + "\",\"wifi\":" + 
                  String(wifiConnected ? "true" : "false") + ",\"ntp\":" + 
                  String((timeSource == TIME_SOURCE_NTP) ? "true" : "false") + 
                  ",\"timeSource\":\"" + timeSourceStr + 
                  "\",\"rtcPresent\":" + String(rtcPresent ? "true" : "false") + 
                  ",\"rtcSynced\":" + String(rtcTimeValid ? "true" : "false") +
                  ",\"rtcSyncAge\":" + String(rtcSyncAge) + "}";
    server.send(200, "application/json", resp);
}

void handleGetWiFi() {
    lastConnectionActivity = millis();   
    char buffer[384];
    snprintf(buffer, sizeof(buffer),
        "{\"ssid\":\"%s\",\"connected\":%s,\"ip\":\"%s\",\"rssi\":%d,\"sta_enabled\":%s}",
        sysConfig.sta_ssid,
        wifiConnected ? "true" : "false",
        WiFi.localIP().toString().c_str(),
        wifiConnected ? (int)WiFi.RSSI() : 0,
        extConfig.sta_enabled ? "true" : "false"
    );
    server.send(200, "application/json", buffer);
}

void handleSaveWiFi() {
    lastConnectionActivity = millis();    
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
    const char* ssid = doc["ssid"];
    const char* pw   = doc["password"];
    if (ssid && strlen(ssid) > 0 && strlen(ssid) < 32) {
        bool ssidChanged = (strcmp(sysConfig.sta_ssid, ssid) != 0);
        bool passChanged = false;        
        strncpy(sysConfig.sta_ssid, ssid, 31); 
        sysConfig.sta_ssid[31] = '\0';
        if (pw && strlen(pw) > 0) { 
            passChanged = (strcmp(sysConfig.sta_password, pw) != 0);
            strncpy(sysConfig.sta_password, pw, 63); 
            sysConfig.sta_password[63] = '\0'; 
        } else { 
            passChanged = (sysConfig.sta_password[0] != '\0');
            sysConfig.sta_password[0] = '\0'; 
        }
        saveConfiguration();
        if (extConfig.sta_enabled && (ssidChanged || passChanged)) {
            wifiPausedForScan = false;            
            WiFi.disconnect(true);  
            delay(500);  
            wifiConnected = false;
            wifiConnecting = true;
            wifiConnectStart = millis();
            wifiReconnectAttempts = 0;
            wifiGiveUpUntil = 0;
            wifiFirstAttempt = true;
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
    lastConnectionActivity = millis();    
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
    lastConnectionActivity = millis();    
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
    lastConnectionActivity = millis();    
    char buffer[256];
    snprintf(buffer, sizeof(buffer),
        "{\"ntpServer\":\"%s\",\"gmtOffset\":%d,\"daylightOffset\":%d,\"syncHours\":%d,\"globalActiveMode\":%d}",
        sysConfig.ntp_server,
        sysConfig.gmt_offset,
        sysConfig.daylight_offset,
        extConfig.ntp_sync_hours,
        extConfig.global_active_mode
    );
    server.send(200, "application/json", buffer);
}

void handleSaveNTP() {
    lastConnectionActivity = millis();    
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
    const char* srv = doc["ntpServer"];
    if (srv && strlen(srv) > 0 && strlen(srv) < 48) {
        strncpy(sysConfig.ntp_server, srv, 47); 
        sysConfig.ntp_server[47] = '\0';        
        sysConfig.gmt_offset      = doc["gmtOffset"] | 0;
        sysConfig.daylight_offset = doc["daylightOffset"] | 0;        
        if (doc.containsKey("syncHours")) {
            uint8_t h = doc["syncHours"];
            if (h >= 1 && h <= 24) { 
                extConfig.ntp_sync_hours = h; 
                saveExtConfig(); 
            }
        }        
        saveConfiguration();        
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid NTP server\"}");
    }
}

void handleSyncNTP() {
    lastConnectionActivity = millis();    
    if (!wifiConnected || !extConfig.sta_enabled) {
        server.send(400, "application/json",
            "{\"success\":false,\"error\":\"WiFi not connected or station disabled\"}"); 
        return;
    }    
    ntpAsyncState = NTP_STATE_IDLE;
    lastNTPSync = 0;
    lastNTPAttempt = 0;
    server.send(200, "application/json", "{\"success\":true,\"message\":\"NTP sync initiated\"}");
}

void handleGetAP() {
    lastConnectionActivity = millis();    
    char buffer[256];
    snprintf(buffer, sizeof(buffer),
        "{\"ap_ssid\":\"%s\",\"ap_password\":\"%s\",\"ap_channel\":%d,\"ap_hidden\":%s}",
        sysConfig.ap_ssid,
        sysConfig.ap_password,
        extConfig.ap_channel,
        extConfig.ap_hidden ? "true" : "false"
    );
    server.send(200, "application/json", buffer);
}

void handleSaveAP() {
    lastConnectionActivity = millis();    
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
        strcpy(ap_ssid, sysConfig.ap_ssid);        
    }    
    if (pw && strlen(pw) > 0) {
        if (strlen(pw) >= 8) {
            passChanged = (strcmp(sysConfig.ap_password, pw) != 0);
            strncpy(sysConfig.ap_password, pw, 31); 
            sysConfig.ap_password[31] = '\0';
            strcpy(ap_password, sysConfig.ap_password);
        }
    } else if (!pw || strlen(pw) == 0) {
        passChanged = (sysConfig.ap_password[0] != '\0');
        sysConfig.ap_password[0] = '\0';
        ap_password[0] = '\0';
    }    
    if (doc.containsKey("ap_channel")) {
        uint8_t ch = doc["ap_channel"];
        if (ch >= 1 && ch <= 13 && ch != extConfig.ap_channel) {
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
    saveConfiguration();
    saveExtConfig();    
    bool needsRestart = ssidChanged || passChanged || channelChanged || hiddenChanged;    
    if (needsRestart) {
        healer.restartAPIfNeeded(true);
        server.send(200, "application/json", "{\"success\":true,\"restarted\":true}");
    } else {
        server.send(200, "application/json", "{\"success\":true,\"restarted\":false}");
    }
}

void handleGetSystem() {
    lastConnectionActivity = millis();    
    DynamicJsonDocument doc(2048);    
    doc["ip"] = WiFi.localIP().toString();
    doc["ap_ip"] = WiFi.softAPIP().toString();
    doc["uptime"] = millis() / 1000UL;
    doc["freeHeap"] = ESP.getFreeHeap();
    char epochStr[32];
snprintf(epochStr, sizeof(epochStr), "%llu", (unsigned long long)getCurrentEpoch());
doc["utcEpoch"] = epochStr;      
    String timeSourceStr = "None";
    if (timeSource == TIME_SOURCE_NTP) timeSourceStr = "NTP";
    else if (timeSource == TIME_SOURCE_BROWSER) timeSourceStr = "Browser";
    else if (timeSource == TIME_SOURCE_RTC) timeSourceStr = "RTC";
    doc["timeSource"] = timeSourceStr;    
    doc["ntpServer"] = sysConfig.ntp_server;
    doc["ntpSyncAge"] = lastNTPSync > 0 ? (unsigned long)((millis() - lastNTPSync) / 1000UL) : (unsigned long)0xFFFFFFFF;  
    doc["browserSyncAge"] = lastBrowserSync > 0 ? (unsigned long)((millis() - lastBrowserSync) / 1000UL) : (unsigned long)0xFFFFFFFF;  
    doc["rtcSyncAge"] = lastRTCDSync > 0 ? (unsigned long)((millis() - lastRTCDSync) / 1000UL) : (unsigned long)0xFFFFFFFF; 
    doc["wifiConnected"] = wifiConnected;
    doc["wifiSSID"] = sysConfig.sta_ssid;
    doc["rssi"] = wifiConnected ? (int)WiFi.RSSI() : 0;
    doc["version"] = EEPROM_VERSION;
    doc["chipModel"] = "ESP32S3";
    doc["mdnsHostname"] = getMDNSHostname();
    doc["mdnsStarted"] = mdnsStarted;
    doc["relayCount"] = gpioConfig.count;
    doc["maxRelays"] = MAX_RELAYS;
    doc["gmtOffset"] = sysConfig.gmt_offset;
    doc["driftComp"] = driftCompensation;
    doc["globalActiveMode"] = extConfig.global_active_mode;
    doc["rtcPresent"] = rtcPresent;
    doc["staEnabled"] = extConfig.sta_enabled ? true : false;  
    String resp;
    serializeJson(doc, resp);
    server.send(200, "application/json", resp);
}

void handleReset() {
    lastConnectionActivity = millis();    
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Performing live service verification...\"}");
    healer.performTargetedRecovery();  
}

void handleFactoryReset() {
    lastConnectionActivity = millis();    
    preferences.begin(NVS_NAMESPACE, false);
    preferences.clear();
    preferences.end();
    initDefaults();
    loadGPIOConfig();
    loadConfiguration();
    loadExtConfig();
    for (int i = 0; i < gpioConfig.count; i++) {
        setRelayOutput(i, false);
        lastRelayOutputs[i] = false;
        relayConfigs[i].manualOverride = false;
        relayConfigs[i].manualState = false;
    }
    relayOutputsInitialized = true;    
    if (extConfig.sta_enabled && strlen(sysConfig.sta_ssid) > 0) {
        WiFi.disconnect(true);
        delay(100);
        WiFi.begin(sysConfig.sta_ssid, sysConfig.sta_password);
        wifiConnecting = true;
        wifiConnectStart = millis();
        wifiConnected = false;
        wifiReconnectAttempts = 0;
        wifiGiveUpUntil = 0;
        wifiFirstAttempt = true;
    }    
    healer.restartAPIfNeeded(true);    
    healer.performTargetedRecovery();    
    updateScheduleCache();    
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Factory reset complete. Settings restored to defaults.\"}");
}

// =============================================================================
//  GPIO MANAGEMENT HANDLERS
// =============================================================================
void handleGetGPIOConfig() {
    lastConnectionActivity = millis();    
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
    // change gpio pins
    int validPins[] = {1, 2, 3, 4, 5, 6, 7, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};    
    for (int p : validPins) {
        available.add(p);
    }    
    String resp;
    serializeJson(doc, resp);
    server.send(200, "application/json", resp);
}

void handleSaveGPIOConfig() {
    lastConnectionActivity = millis();    
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
    JsonArray pins = doc["pins"].as<JsonArray>();
    uint8_t newCount = pins.size();    
    if (newCount > MAX_RELAYS) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Too many pins\"}");
        return;
    }    
    bool oldManualOverride[MAX_RELAYS];
    bool oldManualState[MAX_RELAYS];
    char oldNames[MAX_RELAYS][16];
    TimerSchedule oldSchedules[MAX_RELAYS];
    bool oldActiveLow[MAX_RELAYS];    
    for (int i = 0; i < MAX_RELAYS; i++) {
        oldManualOverride[i] = relayConfigs[i].manualOverride;
        oldManualState[i] = relayConfigs[i].manualState;
        strcpy(oldNames[i], relayConfigs[i].name);
        oldSchedules[i] = relayConfigs[i].schedule;
        oldActiveLow[i] = gpioConfig.activeLow[i];
    }    
    gpioConfig.count = newCount;
    int idx = 0;
    for (JsonVariant pin : pins) {
        gpioConfig.pins[idx] = pin.as<uint8_t>();        
        if (idx < MAX_RELAYS) {
            relayConfigs[idx].manualOverride = oldManualOverride[idx];
            relayConfigs[idx].manualState = oldManualState[idx];
            strcpy(relayConfigs[idx].name, oldNames[idx]);
            relayConfigs[idx].schedule = oldSchedules[idx];
            gpioConfig.activeLow[idx] = oldActiveLow[idx];
        }
        idx++;
    }    
    for (uint8_t i = newCount; i < MAX_RELAYS; i++) {
        initScheduleDefaults(i);  
        gpioConfig.activeLow[i] = true; 
    }    
    for (uint8_t i = 0; i < gpioConfig.count; i++) {
        pinMode(gpioConfig.pins[i], OUTPUT);
        setRelayOutput(i, false);
        lastRelayOutputs[i] = false;
    }
    relayOutputsInitialized = true;    
    saveGPIOConfig();
    saveConfiguration();
    updateScheduleCache();
    criticalStateDirty = true;
    responseCache.valid = false;    
    server.send(200, "application/json", "{\"success\":true,\"count\":" + String(newCount) + "}");
}

void handleAddGPIO() {
    lastConnectionActivity = millis();    
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
    uint8_t newPin = doc["pin"];    
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
    gpioConfig.count++;
    pinMode(newPin, OUTPUT);
    setRelayOutput(gpioConfig.count - 1, false);
    lastRelayOutputs[gpioConfig.count - 1] = false;    
    saveGPIOConfig();
    saveConfiguration();
    updateScheduleCache();
    criticalStateDirty = true;
    responseCache.valid = false;    
    server.send(200, "application/json", "{\"success\":true,\"count\":" + String(gpioConfig.count) + "}");
}

void handleDeleteGPIO() {
    lastConnectionActivity = millis();    
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
    uint8_t index = doc["index"];    
    if (index >= gpioConfig.count) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid index\"}");
        return;
    }    
    setRelayOutput(index, false);    
    for (uint8_t i = index; i < gpioConfig.count - 1; i++) {
        gpioConfig.pins[i] = gpioConfig.pins[i + 1];
        gpioConfig.activeLow[i] = gpioConfig.activeLow[i + 1];
        relayConfigs[i] = relayConfigs[i + 1];
        lastRelayOutputs[i] = lastRelayOutputs[i + 1];
    }    
    initScheduleDefaults(gpioConfig.count - 1);  
    gpioConfig.activeLow[gpioConfig.count - 1] = true;     
    gpioConfig.count--;
    saveGPIOConfig();
    saveConfiguration();
    updateScheduleCache();
    criticalStateDirty = true;
    responseCache.valid = false;    
    server.send(200, "application/json", "{\"success\":true,\"count\":" + String(gpioConfig.count) + "}");
}

void handleToggleActiveLow() {
    lastConnectionActivity = millis();    
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
    uint8_t index = doc["index"];    
    if (index >= gpioConfig.count) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid index\"}");
        return;
    }    
    gpioConfig.activeLow[index] = !gpioConfig.activeLow[index];
    saveGPIOConfig();    
    pinMode(gpioConfig.pins[index], OUTPUT);
    setRelayOutput(index, false);
    lastRelayOutputs[index] = false;
    relayOutputsInitialized = true;
    criticalStateDirty = true;
    responseCache.valid = false;    
    server.send(200, "application/json", 
        "{\"success\":true,\"activeLow\":" + String(gpioConfig.activeLow[index] ? "true" : "false") + "}");
}

// =============================================================================
//  GLOBAL ACTIVE MODE HANDLER
// =============================================================================
void handleGlobalActiveMode() {
    lastConnectionActivity = millis();    
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
    criticalStateDirty = true;
    responseCache.valid = false;    
    server.send(200, "application/json", 
        "{\"success\":true,\"mode\":" + String(mode) + "}");
}
