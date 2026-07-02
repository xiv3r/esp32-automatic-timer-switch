/*
===============================================================================
 *  ESP32-S3 16-Channel Automatic Relay Timer Switch with Full MQTT Support
 *  Author: Raff Alds
 *  Github: https://www.github.com/xiv3r
 *  License: GPLv3
===============================================================================
*/

#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include <RTClib.h>
#include <PubSubClient.h>

// =============================================================================
//  Preferences (NVS)
// =============================================================================
Preferences preferences;
#define NVS_NAMESPACE "relay16"
#define EEPROM_MAGIC   0x1234
#define EEPROM_VERSION 9       
#define EXT_CFG_MAGIC  0xEC

// =============================================================================
//  Year 2106 Support
// =============================================================================
#define MAX_UNIX_TIME 4294967295UL  
#define MIN_UNIX_TIME 1000000000UL  
#define VALID_UNIX_TIME(epoch) ((epoch) > MIN_UNIX_TIME && (epoch) < MAX_UNIX_TIME)

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
static const unsigned long DS3231_SYNC_INTERVAL_MASTER = 3600000UL; 

// =============================================================================
//  NTP Async State Machine Constants
// =============================================================================
static const unsigned long NTP_SERVER_TIMEOUT   =   5000UL;  
#define NTP_STATE_IDLE       0
#define NTP_STATE_CONNECTING 1
#define NTP_STATE_WAITING    2

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
//  NTP Client
// =============================================================================
WiFiUDP   ntpUDP;
NTPClient timeClient(ntpUDP, NTP_SERVERS[0], 0, 3600000UL);

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
    uint16_t months[8];     
};
struct RelayConfig {
    TimerSchedule schedule;
    bool          manualOverride;
    bool          manualState;
    char          name[16];
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
    time_t   last_rtc_epoch;
    float    rtc_drift;
    char     hostname[32];
} __attribute__((packed));
struct ExtConfig {
    uint8_t magic;
    uint8_t ap_channel;
    uint8_t ntp_sync_hours;
    uint8_t ap_hidden;
    uint8_t global_active_mode;  
    uint8_t sta_enabled;          
    uint8_t ds3231_master;        
    uint8_t reserved[25];
} __attribute__((packed));

// =============================================================================
//  MQTT Configuration Structure
// =============================================================================
#define MQTT_FULL_MAGIC 0x7F
struct MqttFullConfig {
    char broker_ip[64];
    uint16_t broker_port;
    char client_id[40];
    char username[32];
    char password[32];
    char base_topic[50];
    bool enabled;
    bool ha_discovery;
    bool tls_enabled;
    uint8_t qos;
    uint16_t keepalive;
    uint16_t status_interval;
    uint8_t magic;
};

// =============================================================================
//  MQTT Message Queue
// =============================================================================
#define MQTT_QUEUE_SIZE 20
struct MqttMessage {
    String topic;
    String payload;
    bool retained;
    uint8_t qos;
    unsigned long timestamp;
};

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
time_t        internalEpoch            = 0;
unsigned long internalMillisAtLastSync = 0;
float         driftCompensation        = 1.0f;
bool          rtcInitialized           = false;
unsigned long lastRTCUpdate            = 0;
unsigned long rtcMicrosAtLastSync      = 0;
unsigned long lastRTCRebase            = 0; 

// Internal RTC Auto-Save
static unsigned long lastInternalRTCSave = 0;
static const unsigned long INTERNAL_RTC_SAVE_INTERVAL = 3600000UL; 

// DS3231 sync timer
static unsigned long lastDS3231SyncInternal = 0;

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
char ap_ssid[32]     = "ESP32_S3_16CH_Timer_Switch";
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
static time_t   lastScheduleEpoch = 0;
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
//  MQTT Globals
// =============================================================================
WiFiClient mqttWiFiClient;
PubSubClient* mqttClient = nullptr;
MqttFullConfig mqttFullConfig;
MqttMessage mqttQueue[MQTT_QUEUE_SIZE];
uint8_t mqttQueueHead = 0;
uint8_t mqttQueueTail = 0;
bool mqttQueueFull = false;
unsigned long lastMqttReconnect = 0;
unsigned long lastMqttStatusPublish = 0;
unsigned long lastMqttStatsPublish = 0;
uint32_t mqttReconnectCount = 0;
uint32_t mqttPublishCount = 0;
uint32_t mqttReceiveCount = 0;
static const unsigned long MQTT_RECONNECT_INTERVAL = 10000UL;

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
void syncInternalRTC(time_t rawUtcEpoch);
void syncInternalRTCFromDS3231();
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
void handleBrowserTimeSync();
void handleGetMqttConfig();
void handleGetMqttStatus();
void handleSaveMqttConfig();
void handleDS3231MasterToggle();

// MQTT Forward Declarations
void loadMqttFullConfig();
void saveMqttFullConfig();
void handleMqtt();
bool connectMqtt();
void mqttFullCallback(char* topic, byte* payload, unsigned int length);
void publishRelayStates();
void publishSystemStats();
void publishSystemState(const char* state);
void publishHADiscovery();
void publishSchedule(uint8_t relayIndex);
void handleScheduleSet(uint8_t relayIndex, const char* jsonPayload);
bool mqttQueuePush(const char* topic, const char* payload, bool retained, uint8_t qos);
bool mqttQueuePop(MqttMessage& msg);
void processMqttQueue();
void mqttPublish(const char* topic, const char* payload, bool retained);

// =============================================================================
//  Get local epoch (UTC + GMT offset + DST)
// =============================================================================
inline time_t getLocalEpoch(time_t utcEpoch) {
    return utcEpoch + sysConfig.gmt_offset + sysConfig.daylight_offset;
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
        time_t rtcEpoch = now.unixtime();
        if (VALID_UNIX_TIME(rtcEpoch)) {
            rtcTimeValid = true;
        }
    }
}
void immediateDS3231Sync() {
    if (rtcPresent && rtcInitialized && internalEpoch > 0) {
        if (rtc.begin()) {
            DateTime dt(internalEpoch);
            rtc.adjust(dt);
            rtcTimeValid = true;
            lastRTCDSync = millis();
        }
    }
}
void syncDS3231FromInternalRTC() {
    if (!rtcPresent) return;
    if (!rtcInitialized || internalEpoch == 0) return;    
    DateTime dt(internalEpoch);
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
        time_t secondsToAdd = (time_t)adjustedSeconds;
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

// =============================================================================
//  Get Current Epoch
// =============================================================================
time_t getCurrentEpoch() {
    if (rtcPresent && rtcTimeValid && extConfig.ds3231_master) {
        DateTime now = rtc.now();
        if (now.year() >= 2020 && now.year() <= 2100) {
            time_t rtcEpoch = now.unixtime();
            if (VALID_UNIX_TIME(rtcEpoch)) {
                internalEpoch = rtcEpoch;
                driftCompensation = 1.0f;
                internalMillisAtLastSync = millis();
                rtcMicrosAtLastSync = micros();
                lastRTCRebase = millis();
                rtcInitialized = true;
                return rtcEpoch;
            }
        }
    }
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
        time_t secondsToAdd = (time_t)adjustedSeconds;
        time_t result = internalEpoch + secondsToAdd;
        float fractional = adjustedSeconds - (float)secondsToAdd;
        if (fractional >= 0.5f) {
            result++;
        }        
        return result;
    }    
    return 0;
}

// =============================================================================
//  Sync Internal RTC from DS3231
// =============================================================================
void syncInternalRTCFromDS3231() {
    if (!rtcPresent || !rtcTimeValid || !extConfig.ds3231_master) return;    
    DateTime now = rtc.now();
    if (now.year() >= 2020 && now.year() <= 2100) {
        time_t rtcEpoch = now.unixtime();
        if (VALID_UNIX_TIME(rtcEpoch)) {
            syncInternalRTC(rtcEpoch);
            timeSource = TIME_SOURCE_RTC;
            Serial.println("[TIME] Internal RTC synced from DS3231");
        }
    }
}

// =============================================================================
//  Save RTC State
// =============================================================================
void saveRTCState() {
    performRTCReabase();
    sysConfig.last_rtc_epoch = internalEpoch;
    sysConfig.rtc_drift      = 1.0f;  
    saveConfiguration();
}

// =============================================================================
//  Load RTC State
// =============================================================================
void loadRTCState() {
    if (VALID_UNIX_TIME(sysConfig.last_rtc_epoch)) {
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
    if (extConfig.ds3231_master && rtcPresent && rtcTimeValid) {
        return; 
    }    
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
            time_t rtcEpoch = now.unixtime();
            if (VALID_UNIX_TIME(rtcEpoch)) {
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
            MDNS.addServiceTxt("http", "tcp", "version", "v9");
            MDNS.addServiceTxt("http", "tcp", "channels", String(gpioConfig.count).c_str());
            mdnsStarted = true;
        }
    } else {
        MDNS.addService("http", "tcp", 80);
        MDNS.addServiceTxt("http", "tcp", "channels", String(gpioConfig.count).c_str());
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
bool SelfHealingSystem::recoverNTP() {
    if (!wifiConnected || !extConfig.sta_enabled) return false; 
    uint8_t startIndex = ntpServerIndex;
    for (uint8_t attempt = 0; attempt < NUM_NTP_SERVERS; attempt++) {
        uint8_t idx = (startIndex + attempt) % NUM_NTP_SERVERS;
        timeClient.setPoolServerName(NTP_SERVERS[idx]);
        timeClient.setTimeOffset(0);        
        if (timeClient.forceUpdate()) {
        time_t rawUtcEpoch = timeClient.getEpochTime();
        if (VALID_UNIX_TIME(rawUtcEpoch)) {
        timeSource = TIME_SOURCE_NTP; 
        syncInternalRTC(rawUtcEpoch);
        ntpServerIndex = idx;
        return true;
       }
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
            DateTime dt(internalEpoch);
            rtc.adjust(dt);
            rtcTimeValid = true;
            return true;
        } else if (rtcTimeValid) {
            DateTime now = rtc.now();
            if (now.year() >= 2020 && now.year() <= 2100) {
                internalEpoch = now.unixtime();
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
//  MQTT Utility Functions
// =============================================================================
bool mqttQueuePush(const char* topic, const char* payload, bool retained, uint8_t qos) {
    if (mqttQueueFull) return false;
    mqttQueue[mqttQueueHead].topic = String(topic);
    mqttQueue[mqttQueueHead].payload = String(payload);
    mqttQueue[mqttQueueHead].retained = retained;
    mqttQueue[mqttQueueHead].qos = qos;
    mqttQueue[mqttQueueHead].timestamp = millis();
    mqttQueueHead = (mqttQueueHead + 1) % MQTT_QUEUE_SIZE;
    if (mqttQueueHead == mqttQueueTail) mqttQueueFull = true;
    return true;
}
bool mqttQueuePop(MqttMessage& msg) {
    if (mqttQueueHead == mqttQueueTail && !mqttQueueFull) return false;
    msg = mqttQueue[mqttQueueTail];
    mqttQueueTail = (mqttQueueTail + 1) % MQTT_QUEUE_SIZE;
    mqttQueueFull = false;
    return true;
}
void mqttPublish(const char* topic, const char* payload, bool retained) {
    if (!mqttClient || !mqttClient->connected()) {
        mqttQueuePush(topic, payload, retained, mqttFullConfig.qos);
        return;
    }
    mqttClient->publish(topic, payload, retained);
    mqttPublishCount++;
}
void processMqttQueue() {
    if (!mqttClient || !mqttClient->connected()) return;
    MqttMessage msg;
    int processed = 0;
    while (mqttQueuePop(msg) && processed < 5) {
        mqttClient->publish(msg.topic.c_str(), msg.payload.c_str(), msg.retained);
        mqttPublishCount++;
        processed++;
        yield();
    }
}
String mqttGetRelayStateTopic(uint8_t index) {
    return String(mqttFullConfig.base_topic) + "/relay/" + String(index + 1) + "/state";
}
String mqttGetRelaySetTopic(uint8_t index) {
    return String(mqttFullConfig.base_topic) + "/relay/" + String(index + 1) + "/set";
}
String mqttGetRelayScheduleTopic(uint8_t index) {
    return String(mqttFullConfig.base_topic) + "/relay/" + String(index + 1) + "/schedule";
}
String mqttGetRelayAvailableTopic(uint8_t index) {
    return String(mqttFullConfig.base_topic) + "/relay/" + String(index + 1) + "/available";
}
String mqttGetSystemStatusTopic() {
    return String(mqttFullConfig.base_topic) + "/status";
}
String mqttGetSystemStatsTopic() {
    return String(mqttFullConfig.base_topic) + "/system/stats";
}
String mqttGetSystemStateTopic() {
    return String(mqttFullConfig.base_topic) + "/system/state";
}

// =============================================================================
//  MQTT Publish Functions
// =============================================================================
void publishRelayStates() {
    if (!mqttClient || !mqttClient->connected()) return;
    for (int i = 0; i < getActiveRelayCount(); i++) {
        mqttPublish(mqttGetRelayStateTopic(i).c_str(), lastRelayOutputs[i] ? "ON" : "OFF", true);
        mqttPublish(mqttGetRelayAvailableTopic(i).c_str(), "online", true);
    }
}
void publishSystemStats() {
    if (!mqttClient || !mqttClient->connected()) return;    
    DynamicJsonDocument doc(1024);
    doc["uptime"] = millis() / 1000UL;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["wifi_connected"] = wifiConnected;
    doc["wifi_ssid"] = String(sysConfig.sta_ssid);
    doc["ip"] = WiFi.localIP().toString();    
    time_t utcEp = getCurrentEpoch();
    time_t localEp = getLocalEpoch(utcEp);
    struct tm* t = gmtime(&localEp);
    char timeStr[20];
    if (t) sprintf(timeStr, "%04d-%02d-%02d %02d:%02d:%02d",
                   t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                   t->tm_hour, t->tm_min, t->tm_sec);
    else strcpy(timeStr, "--");
    doc["local_time"] = timeStr;
    doc["utc_epoch"] = (uint32_t)utcEp;    
    String timeSourceStr = "none";
    if (timeSource == TIME_SOURCE_NTP) timeSourceStr = "ntp";
    else if (timeSource == TIME_SOURCE_BROWSER) timeSourceStr = "browser";
    else if (timeSource == TIME_SOURCE_RTC) timeSourceStr = "rtc";
    doc["time_source"] = timeSourceStr;
    doc["rtc_present"] = rtcPresent;
    doc["relay_count"] = getActiveRelayCount();
    doc["mqtt_reconnects"] = mqttReconnectCount;
    doc["mqtt_published"] = mqttPublishCount;
    doc["mqtt_received"] = mqttReceiveCount;    
    String payload;
    serializeJson(doc, payload);
    mqttPublish(mqttGetSystemStatsTopic().c_str(), payload.c_str(), false);
}
void publishSystemState(const char* state) {
    if (!mqttClient || !mqttClient->connected()) return;
    mqttPublish(mqttGetSystemStatusTopic().c_str(), state, true);
}
void publishHADiscovery() {
    if (!mqttFullConfig.ha_discovery || !mqttClient || !mqttClient->connected()) return;    
    String base = mqttFullConfig.base_topic;
    String deviceName = String(sysConfig.hostname);
    if (deviceName.isEmpty()) deviceName = "ESP32_S3_Relay16";    
    for (int i = 0; i < getActiveRelayCount(); i++) {
        String relayName = String(relayConfigs[i].name);
        String uid = deviceName + "_relay_" + String(i + 1);        
        StaticJsonDocument<512> haDoc;
        haDoc["name"] = relayName;
        haDoc["unique_id"] = uid;
        haDoc["command_topic"] = base + "/relay/" + String(i + 1) + "/set";
        haDoc["state_topic"] = base + "/relay/" + String(i + 1) + "/state";
        haDoc["availability_topic"] = base + "/status";
        haDoc["payload_on"] = "ON";
        haDoc["payload_off"] = "OFF";
        haDoc["state_on"] = "ON";
        haDoc["state_off"] = "OFF";        
        JsonObject device = haDoc.createNestedObject("device");
        device["name"] = deviceName;
        device["model"] = "ESP32S3 16CH Relay";
        device["manufacturer"] = "Raff Alds";
        device["sw_version"] = "v9";
        JsonArray identifiers = device.createNestedArray("identifiers");
        identifiers.add(deviceName);        
        String topic = "homeassistant/switch/" + uid + "/config";
        String payload;
        serializeJson(haDoc, payload);
        mqttPublish(topic.c_str(), payload.c_str(), true);
    }    
    String uid = deviceName + "_uptime";
    StaticJsonDocument<384> haDoc;
    haDoc["name"] = deviceName + " Uptime";
    haDoc["unique_id"] = uid;
    haDoc["state_topic"] = base + "/system/stats";
    haDoc["value_template"] = "{{ value_json.uptime }}";
    haDoc["unit_of_measurement"] = "s";
    haDoc["device_class"] = "duration";
    haDoc["availability_topic"] = base + "/status";
    JsonObject device = haDoc.createNestedObject("device");
    device["name"] = deviceName;
    device["identifiers"].add(deviceName);    
    String topic = "homeassistant/sensor/" + uid + "/config";
    String payload;
    serializeJson(haDoc, payload);
    mqttPublish(topic.c_str(), payload.c_str(), true);
}
void publishSchedule(uint8_t relayIndex) {
    if (!mqttClient || !mqttClient->connected()) return;    
    DynamicJsonDocument doc(4096);
    doc["relay"] = relayIndex + 1;
    doc["name"] = String(relayConfigs[relayIndex].name);
    doc["manual_override"] = relayConfigs[relayIndex].manualOverride;
    doc["manual_state"] = relayConfigs[relayIndex].manualState;    
    JsonArray schedules = doc.createNestedArray("schedules");
    for (int s = 0; s < 8; s++) {
        JsonObject sch = schedules.createNestedObject();
        sch["index"] = s + 1;
        sch["enabled"] = relayConfigs[relayIndex].schedule.enabled[s];
        char startBuf[16], stopBuf[16];
        sprintf(startBuf, "%02d:%02d:%02d", 
                relayConfigs[relayIndex].schedule.startHour[s],
                relayConfigs[relayIndex].schedule.startMinute[s],
                relayConfigs[relayIndex].schedule.startSecond[s]);
        sprintf(stopBuf, "%02d:%02d:%02d",
                relayConfigs[relayIndex].schedule.stopHour[s],
                relayConfigs[relayIndex].schedule.stopMinute[s],
                relayConfigs[relayIndex].schedule.stopSecond[s]);
        sch["start"] = startBuf;
        sch["stop"] = stopBuf;
        sch["days"] = relayConfigs[relayIndex].schedule.days[s];
        sch["month_days"] = relayConfigs[relayIndex].schedule.monthDays[s];
        sch["months"] = relayConfigs[relayIndex].schedule.months[s];  
    }    
    String payload;
    serializeJson(doc, payload);
    mqttPublish(mqttGetRelayScheduleTopic(relayIndex).c_str(), payload.c_str(), true);
}
void handleScheduleSet(uint8_t relayIndex, const char* jsonPayload) {
    DynamicJsonDocument doc(4096);
    DeserializationError err = deserializeJson(doc, jsonPayload);
    if (err) return;    
    JsonArray schedules = doc["schedules"].as<JsonArray>();
    int s = 0;
    for (JsonObject sch : schedules) {
        if (s >= 8) break;        
        if (sch.containsKey("enabled"))
            relayConfigs[relayIndex].schedule.enabled[s] = sch["enabled"];        
        if (sch.containsKey("start")) {
            String start = sch["start"];
            int h, m, sec;
            sscanf(start.c_str(), "%d:%d:%d", &h, &m, &sec);
            relayConfigs[relayIndex].schedule.startHour[s] = h;
            relayConfigs[relayIndex].schedule.startMinute[s] = m;
            relayConfigs[relayIndex].schedule.startSecond[s] = sec;
        }        
        if (sch.containsKey("stop")) {
            String stop = sch["stop"];
            int h, m, sec;
            sscanf(stop.c_str(), "%d:%d:%d", &h, &m, &sec);
            relayConfigs[relayIndex].schedule.stopHour[s] = h;
            relayConfigs[relayIndex].schedule.stopMinute[s] = m;
            relayConfigs[relayIndex].schedule.stopSecond[s] = sec;
        }        
        if (sch.containsKey("days"))
            relayConfigs[relayIndex].schedule.days[s] = sch["days"];        
        if (sch.containsKey("month_days"))
            relayConfigs[relayIndex].schedule.monthDays[s] = sch["month_days"];
        if (sch.containsKey("months"))  
            relayConfigs[relayIndex].schedule.months[s] = sch["months"];        
        s++;
    }    
    saveConfiguration();
    updateScheduleCache();
    responseCache.valid = false;
    criticalStateDirty = true;
    publishSchedule(relayIndex);
}

// =============================================================================
//  MQTT Callback
// =============================================================================
void mqttFullCallback(char* topic, byte* payload, unsigned int length) {
    char* message = (char*)malloc(length + 1);
    if (!message) return;
    memcpy(message, payload, length);
    message[length] = '\0';    
    mqttReceiveCount++;
    String topicStr = String(topic);
    String base = String(mqttFullConfig.base_topic);    
    if (topicStr == "homeassistant/status") {
        if (strcmp(message, "online") == 0) {
            publishHADiscovery();
            publishRelayStates();
        }
        free(message);
        return;
    }    
    if (topicStr == base + "/relay/all/set") {
        bool state = (strcmp(message, "ON") == 0 || strcmp(message, "1") == 0);
        for (int i = 0; i < getActiveRelayCount(); i++) {
            relayConfigs[i].manualOverride = true;
            relayConfigs[i].manualState = state;
            scheduleActiveCache[i] = false;
            setRelayOutput(i, state);
            lastRelayOutputs[i] = state;
            mqttPublish(mqttGetRelayStateTopic(i).c_str(), state ? "ON" : "OFF", true);
        }
        criticalStateDirty = true;
        responseCache.valid = false;
        saveConfiguration();
        free(message);
        return;
    }    
    if (topicStr.startsWith(base + "/relay/")) {
        int relayStart = base.length() + 7;
        int slashPos = topicStr.indexOf('/', relayStart);
        if (slashPos == -1) { free(message); return; }        
        int relayIndex = topicStr.substring(relayStart, slashPos).toInt() - 1;
        if (relayIndex < 0 || relayIndex >= getActiveRelayCount()) {
            free(message);
            return;
        }        
        String command = topicStr.substring(slashPos + 1);        
        if (command == "set") {
            bool state = false;
            if (strcmp(message, "ON") == 0 || strcmp(message, "1") == 0 || strcmp(message, "true") == 0)
                state = true;
            else if (strcmp(message, "OFF") == 0 || strcmp(message, "0") == 0 || strcmp(message, "false") == 0)
                state = false;
            else if (strcmp(message, "TOGGLE") == 0)
                state = !lastRelayOutputs[relayIndex];
            else { free(message); return; }            
            relayConfigs[relayIndex].manualOverride = true;
            relayConfigs[relayIndex].manualState = state;
            scheduleActiveCache[relayIndex] = false;
            setRelayOutput(relayIndex, state);
            lastRelayOutputs[relayIndex] = state;
            criticalStateDirty = true;
            responseCache.valid = false;
            mqttPublish(mqttGetRelayStateTopic(relayIndex).c_str(), state ? "ON" : "OFF", true);
        }
        else if (command == "toggle") {
            bool newState = !lastRelayOutputs[relayIndex];
            relayConfigs[relayIndex].manualOverride = true;
            relayConfigs[relayIndex].manualState = newState;
            scheduleActiveCache[relayIndex] = false;
            setRelayOutput(relayIndex, newState);
            lastRelayOutputs[relayIndex] = newState;
            criticalStateDirty = true;
            responseCache.valid = false;
            mqttPublish(mqttGetRelayStateTopic(relayIndex).c_str(), newState ? "ON" : "OFF", true);
        }
        else if (command == "pulse") {
            int duration = atoi(message);
            if (duration > 0 && duration <= 30000) {
                relayConfigs[relayIndex].manualOverride = true;
                relayConfigs[relayIndex].manualState = true;
                scheduleActiveCache[relayIndex] = false;
                setRelayOutput(relayIndex, true);
                lastRelayOutputs[relayIndex] = true;
                mqttPublish(mqttGetRelayStateTopic(relayIndex).c_str(), "ON", true);
                delay(duration);
                relayConfigs[relayIndex].manualState = false;
                setRelayOutput(relayIndex, false);
                lastRelayOutputs[relayIndex] = false;
                mqttPublish(mqttGetRelayStateTopic(relayIndex).c_str(), "OFF", true);
                criticalStateDirty = true;
                responseCache.valid = false;
            }
        }
        else if (command == "schedule/get") {
            publishSchedule(relayIndex);
        }
        else if (command == "schedule/set") {
            handleScheduleSet(relayIndex, message);
        }        
        free(message);
        return;
    }    
    if (topicStr == base + "/system/command") {
        if (strcmp(message, "reboot") == 0) {
            mqttPublish(mqttGetSystemStatusTopic().c_str(), "offline", true);
            delay(500);
            ESP.restart();
        } else if (strcmp(message, "status") == 0) {
            publishSystemStats();
            publishRelayStates();
        } else if (strcmp(message, "save_config") == 0) {
            saveConfiguration();
            mqttPublish(mqttGetSystemStateTopic().c_str(), "Config saved", false);
        }
        free(message);
        return;
    }    
    free(message);
}

// =============================================================================
//  MQTT Connection Handler
// =============================================================================
bool connectMqtt() {
    if (!mqttClient) return false;    
    String willTopic = String(mqttFullConfig.base_topic) + "/status";    
    bool connected = false;
    if (strlen(mqttFullConfig.username) > 0) {
        connected = mqttClient->connect(
            mqttFullConfig.client_id,
            mqttFullConfig.username,
            mqttFullConfig.password,
            willTopic.c_str(),
            mqttFullConfig.qos,
            true,
            "offline"
        );
    } else {
        connected = mqttClient->connect(
            mqttFullConfig.client_id,
            willTopic.c_str(),
            mqttFullConfig.qos,
            true,
            "offline"
        );
    }    
    if (connected) {
        String base = mqttFullConfig.base_topic;        
        mqttClient->subscribe((base + "/relay/+/set").c_str(), mqttFullConfig.qos);
        mqttClient->subscribe((base + "/relay/+/toggle").c_str(), mqttFullConfig.qos);
        mqttClient->subscribe((base + "/relay/+/pulse").c_str(), mqttFullConfig.qos);
        mqttClient->subscribe((base + "/relay/+/schedule/get").c_str(), mqttFullConfig.qos);
        mqttClient->subscribe((base + "/relay/+/schedule/set").c_str(), mqttFullConfig.qos);
        mqttClient->subscribe((base + "/relay/all/set").c_str(), mqttFullConfig.qos);
        mqttClient->subscribe((base + "/system/command").c_str(), mqttFullConfig.qos);
            if (mqttFullConfig.ha_discovery) {
            mqttClient->subscribe("homeassistant/status", mqttFullConfig.qos);
        }        
        publishSystemState("online");
        publishRelayStates();
        publishSystemStats();        
        if (mqttFullConfig.ha_discovery) {
            publishHADiscovery();
        }        
        processMqttQueue();
        return true;
    }    
    return false;
}

// =============================================================================
//  Main MQTT Handler
// =============================================================================
void handleMqtt() {
    if (!mqttFullConfig.enabled) return;    
    static bool mqttInitialized = false;
    static PubSubClient mqttClientInstance(mqttWiFiClient);    
    if (!mqttInitialized) {
        mqttClient = &mqttClientInstance;
        mqttClient->setServer(mqttFullConfig.broker_ip, mqttFullConfig.broker_port);
        mqttClient->setCallback(mqttFullCallback);
        mqttClient->setKeepAlive(mqttFullConfig.keepalive);
        mqttInitialized = true;
    }    
    unsigned long now = millis();    
    if (!mqttClient->connected()) {
        if (timeHasElapsed(now, lastMqttReconnect, MQTT_RECONNECT_INTERVAL)) {
            lastMqttReconnect = now;
            if (wifiConnected) {
                mqttReconnectCount++;
                connectMqtt();
            }
        }
        return;
    }    
    mqttClient->loop();    
    if (now % 1000 < 10) {
        processMqttQueue();
    }    
    if (timeHasElapsed(now, lastMqttStatusPublish, mqttFullConfig.status_interval * 1000UL)) {
        lastMqttStatusPublish = now;
        publishRelayStates();
    }    
    if (timeHasElapsed(now, lastMqttStatsPublish, 60000UL)) {
        lastMqttStatsPublish = now;
        publishSystemStats();
    }
}

// =============================================================================
//  MQTT Configuration Load/Save
// =============================================================================
void loadMqttFullConfig() {
    preferences.begin(NVS_NAMESPACE, true);
    size_t len = preferences.getBytes("mqttFullCfg", &mqttFullConfig, sizeof(MqttFullConfig));
    preferences.end();    
    if (len != sizeof(MqttFullConfig) || mqttFullConfig.magic != MQTT_FULL_MAGIC) {
        memset(&mqttFullConfig, 0, sizeof(MqttFullConfig));
        strcpy(mqttFullConfig.broker_ip, "192.168.1.100");
        mqttFullConfig.broker_port = 1883;
        snprintf(mqttFullConfig.client_id, 40, "ESP32_S3_Relay16_%06X", (uint32_t)(ESP.getEfuseMac() & 0xFFFFFF));
        strcpy(mqttFullConfig.base_topic, "esp32s3relay16");
        mqttFullConfig.enabled = false;
        mqttFullConfig.ha_discovery = true;
        mqttFullConfig.tls_enabled = false;
        mqttFullConfig.qos = 1;
        mqttFullConfig.keepalive = 60;
        mqttFullConfig.status_interval = 30;
        mqttFullConfig.magic = MQTT_FULL_MAGIC;
        saveMqttFullConfig();
    }
}
void saveMqttFullConfig() {
    preferences.begin(NVS_NAMESPACE, false);
    preferences.putBytes("mqttFullCfg", &mqttFullConfig, sizeof(MqttFullConfig));
    preferences.end();
}

// =============================================================================
//  MQTT API Handlers
// =============================================================================
void handleGetMqttConfig() {
    lastConnectionActivity = millis();    
    DynamicJsonDocument doc(512);
    doc["enabled"] = mqttFullConfig.enabled;
    doc["broker_ip"] = mqttFullConfig.broker_ip;
    doc["broker_port"] = mqttFullConfig.broker_port;
    doc["client_id"] = mqttFullConfig.client_id;
    doc["username"] = mqttFullConfig.username;
    doc["password"] = mqttFullConfig.password;
    doc["base_topic"] = mqttFullConfig.base_topic;
    doc["qos"] = mqttFullConfig.qos;
    doc["keepalive"] = mqttFullConfig.keepalive;
    doc["status_interval"] = mqttFullConfig.status_interval;
    doc["ha_discovery"] = mqttFullConfig.ha_discovery;
    doc["connected"] = (mqttClient && mqttClient->connected());    
    String resp;
    serializeJson(doc, resp);
    server.send(200, "application/json", resp);
}
void handleGetMqttStatus() {
    DynamicJsonDocument doc(128);
    doc["enabled"] = mqttFullConfig.enabled;
    doc["connected"] = (mqttClient && mqttClient->connected());
    doc["reconnects"] = mqttReconnectCount;
    doc["published"] = mqttPublishCount;
    doc["received"] = mqttReceiveCount;    
    String resp;
    serializeJson(doc, resp);
    server.send(200, "application/json", resp);
}
void handleSaveMqttConfig() {
    lastConnectionActivity = millis();    
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
    if (doc.containsKey("enabled") && doc.size() == 1) {
        mqttFullConfig.enabled = doc["enabled"];
        saveMqttFullConfig();
        if (mqttFullConfig.enabled) {
            lastMqttReconnect = 0;
        }
        server.send(200, "application/json", "{\"success\":true}");
        return;
    }    
    if (doc.containsKey("broker_ip"))
        strncpy(mqttFullConfig.broker_ip, doc["broker_ip"], 63);
    if (doc.containsKey("broker_port"))
        mqttFullConfig.broker_port = doc["broker_port"];
    if (doc.containsKey("client_id"))
        strncpy(mqttFullConfig.client_id, doc["client_id"], 39);
    if (doc.containsKey("username"))
        strncpy(mqttFullConfig.username, doc["username"], 31);
    if (doc.containsKey("password"))
        strncpy(mqttFullConfig.password, doc["password"], 31);
    if (doc.containsKey("base_topic"))
        strncpy(mqttFullConfig.base_topic, doc["base_topic"], 49);
    if (doc.containsKey("qos"))
        mqttFullConfig.qos = constrain((int)doc["qos"], 0, 2);
    if (doc.containsKey("keepalive"))
        mqttFullConfig.keepalive = doc["keepalive"];
    if (doc.containsKey("status_interval"))
        mqttFullConfig.status_interval = doc["status_interval"];
    if (doc.containsKey("ha_discovery"))
        mqttFullConfig.ha_discovery = doc["ha_discovery"];
    if (doc.containsKey("enabled"))
        mqttFullConfig.enabled = doc["enabled"];    
    saveMqttFullConfig();
    responseCache.valid = false;    
    if (mqttFullConfig.enabled && mqttClient) {
        if (mqttClient->connected()) {
            mqttClient->disconnect();
        }
        lastMqttReconnect = 0;
    }    
    server.send(200, "application/json", "{\"success\":true,\"restarted\":true}");
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
.msection{display:flex;gap:2px;margin-top:5px;flex-wrap:wrap}
.month{width:34px;height:22px;border-radius:3px;border:1px solid #CFD8DC;display:flex;align-items:center;justify-content:center;font-size:10px;font-weight:600;cursor:pointer;background:#FAFAFA;transition:.15s;user-select:none}
.month:hover{border-color:#4FC3F7;background:#E1F5FE}
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
.btn-mqtt-on{background:linear-gradient(135deg,#00897B 0%,#00695C 100%);color:#fff;box-shadow:0 2px 4px rgba(0,137,123,0.3)}
.btn-mqtt-on:hover{background:linear-gradient(135deg,#00ACC1 0%,#00897B 100%);filter:brightness(1.05)}
.btn-mqtt-off{background:linear-gradient(135deg,#757575 0%,#616161 100%);color:#fff;box-shadow:0 2px 4px rgba(117,117,117,0.3)}
.btn-mqtt-off:hover{background:linear-gradient(135deg,#9E9E9E 0%,#757575 100%);filter:brightness(1.05)}
@media(max-width:500px){.grid{grid-template-columns:1fr}.ibar{grid-template-columns:1fr}.input-row{flex-direction:column}.day{width:24px;height:22px;font-size:10px}.mday{width:22px;height:20px;font-size:9px}.month{width:28px;height:20px;font-size:9px}}
)css";

// =============================================================================
//  HTML PAGES
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
<a href="/mqtt">MQTT</a>
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

function monthMaskToStr(months){
  if(months===0) return '';
  if(months===0x0FFF) return 'All months';
  let s='';
  for(let i=0;i<12;i++) if(months&(1<<i)) s+=M[i]+' ';
  return s.trim()||'None';
}

function nightBadge(sc){
  if(!sc.enabled)return'';
  const a=sc.startHour*3600+sc.startMinute*60+sc.startSecond;
  const b=sc.stopHour*3600+sc.stopMinute*60+sc.stopSecond;
  const ds=dayMaskToStr(sc.days);
  const ms=monthDayMaskToStr(sc.monthDays||0);
  const months=monthMaskToStr(sc.months||0);
  let info=ds;
  if(ms) info+=' | Days:'+ms;
  if(months) info+=' | '+months;
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
    if (e.key === 'Enter') { e.preventDefault(); saveNameEdit(relayIdx, input.value); }
    else if (e.key === 'Escape') { cancelEdit(); }
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
  if (newName.length === 0) { newName = 'Relay ' + (relayIdx + 1); }
  const nameSpan = document.getElementById('name_' + relayIdx);
  relays[relayIdx].name = newName;
  if (nameSpan) { nameSpan.textContent = newName; nameSpan.style.display = ''; }
  if (editingInput) editingInput.remove();
  editingRelay = -1;
  editingInput = null;
  fetch('/api/relay/name', {method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({relay:relayIdx,name:newName})})
  .then(r=>r.json()).then(d=>{if(d.success)toast('Name saved!');else toast('Failed to save name',false);})
  .catch(()=>toast('Error saving name',false));
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
      const monthBits = sc2.months || 0x0FFF;
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
<div class="sched-section">Months of Year</div>
<div class="msection" id="month_${i}_${s}">`;
      for(let m=0;m<12;m++){
        const mask = 1<<m;
        html+=`<div class="month${(monthBits&mask)?' on':''}" onclick="toggleMonth(${i},${s},${m})" title="${M[m]}">${M[m]}</div>`;
      }
      html+=`</div></div>`;
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
function toggleMonth(ri,si,monthIdx){
  const mask = 1<<monthIdx;
  if(!relays[ri].schedules[si].months) relays[ri].schedules[si].months = 0x0FFF;
  relays[ri].schedules[si].months ^= mask;
  const monthEl = document.getElementById('month_'+ri+'_'+si).children[monthIdx];
  if(monthEl) monthEl.className = 'month' + ((relays[ri].schedules[si].months & mask)?' on':'');
  const nb=document.getElementById('nb_'+ri+'_'+si);
  if(nb)nb.innerHTML=nightBadge(relays[ri].schedules[si]);
}
function uf(ri,si,field,val){
  const sc=relays[ri].schedules[si];
  if(field==='en'){sc.enabled=val;const el=document.getElementById('si_'+ri+'_'+si);if(el)el.className='si'+(val?' act':'');}
  else if(field==='start'){const t=fromTS(val);sc.startHour=t.h;sc.startMinute=t.m;sc.startSecond=t.s;}
  else if(field==='stop'){const t=fromTS(val);sc.stopHour=t.h;sc.stopMinute=t.m;sc.stopSecond=t.s;}
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
<a href="/mqtt">MQTT</a>
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
<small>When disabled, ESP32-S3 runs in AP-only mode (no internet/NTP). Relay schedules using internal RTC/DS3231 will still work.</small>
</div>
<hr>
<div id="stationConfig">
<div class="fg"><label>Network SSID</label>
<div class="input-row"><input type="text" id="ssid" placeholder="Enter wifi name or scan" required>
<button class="btn bscan" id="scanBtn" onclick="startScan()" style="white-space:nowrap">&#x1F4F6; Scan</button></div></div>
<div class="netlist" id="netlist"></div>
<div class="fg"><label>Password</label><input type="password" id="pw" placeholder="Enter password"></div>
<button class="btn bsave" onclick="saveWiFi()">&#x1F4BE; Save &amp; Connect</button>
</div></div>
</main>
<div id="toast"></div>
<style>
.btn-sta-on{background:linear-gradient(135deg,#1565C0 0%,#0D47A1 100%);color:#fff;box-shadow:0 2px 4px rgba(21,101,192,0.3)}
.btn-sta-on:hover{background:linear-gradient(135deg,#1E88E5 0%,#1565C0 100%);filter:brightness(1.05)}
.btn-sta-off{background:linear-gradient(135deg,#C62828 0%,#B71C1C 100%);color:#fff;box-shadow:0 2px 4px rgba(198,40,40,0.3)}
.btn-sta-off:hover{background:linear-gradient(135deg,#E53935 0%,#C62828 100%);filter:brightness(1.05)}
</style>
<script>
function toast(m,ok=true){const t=document.getElementById('toast');t.textContent=m;t.className='show '+(ok?'ok':'er');clearTimeout(t._t);t._t=setTimeout(()=>t.className='',3000);}
function tick(){fetch('/api/time').then(r=>r.json()).then(d=>{document.getElementById('clk').textContent=d.time||'--:--:--';const w=document.querySelector('.wd'),t=document.querySelector('.td');if(w)w.className='dot '+(d.wifi?'g':'r');if(t){let tc='y';if(d.timeSource==='ntp')tc='g';else if(d.timeSource==='browser')tc='b';else if(d.timeSource==='rtc')tc='b';else tc='y';t.className='dot '+tc;}}).catch(()=>{});}
setInterval(tick,1000);tick();
function updateStaButton(enabled){const btn=document.getElementById('staToggleBtn');if(enabled){btn.textContent='\u2713 ON';btn.className='btn btn-sta-on'}else{btn.textContent='\u2717 OFF';btn.className='btn btn-sta-off'}}
function loadWiFiStatus(){
  fetch('/api/wifi').then(r=>r.json()).then(d=>{
    document.getElementById('ssid').value=d.ssid||'';
    const staEnabled = d.sta_enabled !== undefined ? d.sta_enabled : true;
    updateStaButton(staEnabled);
    const stationDiv=document.getElementById('stationConfig');if(stationDiv){stationDiv.style.opacity=staEnabled?'1':'0.5';const inputs=stationDiv.querySelectorAll('input,button');inputs.forEach(inp=>inp.disabled=!staEnabled)}
    const s=document.getElementById('status');s.style.display='';
    if(staEnabled&&d.connected){s.innerHTML='<span style="font-weight:700">\u2713 Connected</span> to: <strong>'+d.ssid+'</strong> ('+d.ip+') &nbsp;'+rssiBar(d.rssi||0)+' '+d.rssi+'dBm';s.className='alert ai'}
    else if(staEnabled&&!d.connected){s.innerHTML='<span style="font-weight:700">\u26A0 Not Connected</span><br>Configure SSID/password above.';s.className='alert aw'}
    else{s.innerHTML='<span style="font-weight:700">\u26D4 WiFi Station Disabled</span><br>ESP32-S3 running in AP-only mode.';s.className='alert aw'}
  }).catch(()=>{});
}
function rssiBar(rssi){const b=rssi>=-50?4:rssi>=-60?3:rssi>=-70?2:1;let s='<span class="bars">';for(let i=1;i<=4;i++)s+='<span class="bar'+(i<=b?' on':'')+('" style="height:'+(i*3+2)+'px"></span>');return s+'</span>'}
function toggleStation(){
  const btn=document.getElementById('staToggleBtn');
  const wasEnabled=btn.textContent==='\u2713 ON';
  const willEnable=!wasEnabled;
  btn.disabled=true;btn.textContent='...';
  fetch('/api/wifi',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({sta_enabled:willEnable})})
  .then(r=>r.json()).then(d=>{
    btn.disabled=false;
    if(d.success){updateStaButton(willEnable);toast('WiFi Station '+(willEnable?'ON':'OFF'));if(!willEnable)toast('NTP time sync unavailable until re-enabled',false);const stationDiv=document.getElementById('stationConfig');if(stationDiv){stationDiv.style.opacity=willEnable?'1':'0.5';const inputs=stationDiv.querySelectorAll('input,button');inputs.forEach(inp=>inp.disabled=!willEnable)}loadWiFiStatus()}
    else{toast('Failed to toggle',false);updateStaButton(wasEnabled)}
  }).catch(()=>{btn.disabled=false;toast('Error',false);updateStaButton(wasEnabled)});
}
let scanTimer=null,scanning=false;
function startScan(){
  const staEnabled=document.getElementById('staToggleBtn').textContent==='\u2713 ON';
  if(!staEnabled){toast('Please enable WiFi Station Mode first',false);return}
  if(scanning)return;
  scanning=true;
  document.getElementById('scanBtn').textContent='\u23F3 Scanning\u2026';document.getElementById('scanBtn').disabled=true;
  const nl=document.getElementById('netlist');nl.style.display='block';nl.innerHTML='<div style="text-align:center;color:#90A4AE;padding:12px">\uD83D\uDCE1 Scanning...</div>';
  fetch('/api/wifi/scan',{method:'POST'}).then(()=>{scanTimer=setInterval(pollScan,2500)}).catch(()=>endScan());
}
function pollScan(){fetch('/api/wifi/scan').then(r=>r.json()).then(d=>{if(!d.scanning){clearInterval(scanTimer);endScan();renderNets(d.networks||[])}}).catch(()=>{clearInterval(scanTimer);endScan()})}
function endScan(){scanning=false;document.getElementById('scanBtn').textContent='\uD83D\uDCF6 Scan';document.getElementById('scanBtn').disabled=false}
function renderNets(nets){
  const nl=document.getElementById('netlist');
  if(!nets.length){nl.innerHTML='<div style="color:#90A4AE;text-align:center;padding:8px">No networks found</div>';return}
  const frag=document.createDocumentFragment();
  const hdr=document.createElement('div');hdr.className='net-hdr';hdr.textContent='Available Networks';frag.appendChild(hdr);
  nets.sort((a,b)=>b.rssi-a.rssi).forEach(n=>{
    const d=document.createElement('div');d.className='netitem';
    d.innerHTML='<span class="ns">'+n.ssid+'</span><span class="nr">'+n.rssi+'dBm</span>'+rssiBar(n.rssi)+'<span>'+(n.enc?'\uD83D\uDD12':'')+'</span>';
    d.addEventListener('click',()=>{document.getElementById('ssid').value=n.ssid;document.getElementById('pw').focus()});
    frag.appendChild(d);
  });
  nl.innerHTML='';nl.appendChild(frag);
}
function saveWiFi(){
  const staEnabled=document.getElementById('staToggleBtn').textContent==='\u2713 ON';
  if(!staEnabled){toast('Enable WiFi Station Mode first',false);return}
  const ssid=document.getElementById('ssid').value.trim();
  if(!ssid){toast('SSID required',false);return}
  const btn=document.querySelector('.bsave');btn.disabled=true;btn.textContent='Saving...';
  fetch('/api/wifi',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid,password:document.getElementById('pw').value})})
  .then(r=>r.json()).then(d=>{btn.disabled=false;btn.innerHTML='&#x1F4BE; Save &amp; Connect';if(d.success){toast('Saved!');setTimeout(()=>window.location.href='/',3000)}else toast('Failed: '+d.error,false)})
  .catch(()=>{btn.disabled=false;btn.innerHTML='&#x1F4BE; Save &amp; Connect';toast('Error',false)});
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
<a href="/mqtt">MQTT</a>
<a href="/system">System</a>
</nav>
<div class="hdr-r"><span class="dot wd"></span><span class="dot td"></span>&nbsp;<span id="clk">--:--:--</span></div>
</header>
<main>
<p class="ptitle">Time &amp; NTP Settings</p>
<div class="card fcrd">
<div id="timeStatus" class="alert ai" style="display:none;margin-bottom:12px"></div>
<div class="fg"><label>NTP Server</label><input type="text" id="srv" placeholder="time.google.com" required>
<small>Fallbacks: time.google.com &rarr; time.windows.com &rarr; time.cloudflare.com &rarr; time.facebook.com</small></div>
<div class="fg"><label>GMT Offset (seconds)</label><input type="number" id="gmt" required></div>
<div class="fg"><label>Daylight Saving (seconds)</label><input type="number" id="dst" value="0"></div>
<div class="fg"><label>Auto Sync (hours, 1&ndash;24)</label><input type="number" id="shi" min="1" max="24" value="1"></div>
<div style="display:flex;gap:8px;flex-wrap:wrap">
<button class="btn bsave" style="flex:1;margin-top:0" onclick="save()">&#x1F4BE; Save Settings</button>
<button class="btn bsync" id="sbtn" onclick="sync()" style="padding:9px 16px;border-radius:6px;font-size:13px;font-weight:600;margin-top:8px;white-space:nowrap">&#x1F504; Sync NTP</button>
<button class="btn bwarn" id="bsbtn" onclick="syncFromBrowser()" style="padding:9px 16px;border-radius:6px;font-size:13px;font-weight:600;margin-top:8px;white-space:nowrap">&#x1F310; Sync Browser</button>
</div>
<small style="display:block;margin-top:12px;color:#90A4AE">Use <strong>Sync Browser</strong> when NTP unreachable. <strong>DS3231 RTC</strong> maintains time across power cycles.</small>
<div class="fg" style="margin-top:12px;border-top:1px solid #ECEFF1;padding-top:12px">
<label style="display:flex;align-items:center;gap:8px;cursor:pointer;font-size:13px;font-weight:600;color:#1A1A2E;text-transform:none">
<input type="checkbox" id="ds3231Master" style="width:18px;height:18px;cursor:pointer;accent-color:#7B1FA2" onchange="toggleDS3231Master()">
<span>DS3231 as Time Master (high accuracy)</span>
</label>
<small>When enabled, DS3231 RTC is the primary time source and corrects internal RTC every hour. Disable to use internal RTC as primary.</small>
</div>
</div>
</main>
<div id="toast"></div>
<script>
function toast(m,ok=true){const t=document.getElementById('toast');t.textContent=m;t.className='show '+(ok?'ok':'er');clearTimeout(t._t);t._t=setTimeout(()=>t.className='',3000);}
function tick(){fetch('/api/time').then(r=>r.json()).then(d=>{document.getElementById('clk').textContent=d.time||'--:--:--';const w=document.querySelector('.wd'),t=document.querySelector('.td');if(w)w.className='dot '+(d.wifi?'g':'r');if(t){let tc='y';if(d.timeSource==='ntp')tc='g';else if(d.timeSource==='browser')tc='b';else if(d.timeSource==='rtc')tc='b';else tc='y';t.className='dot '+tc;}updateTimeStatus(d)}).catch(()=>{})}
function updateTimeStatus(d){
  const s=document.getElementById('timeStatus');s.style.display='block';
  let icon='',msg='',cls='ai';
  if(d.timeSource==='ntp'){icon='\u2705';msg='Time: <strong>NTP Server</strong> \u2014 High accuracy';cls='ai'}
  else if(d.timeSource==='browser'){icon='\uD83C\uDF10';msg='Time: <strong>Browser Sync</strong> \u2014 Moderate';cls='aw'}
  else if(d.timeSource==='rtc'){icon='\uD83D\uDCBF';msg='Time: <strong>DS3231 RTC</strong> \u2014 Battery-backed';cls='ai'}
  else{icon='\u26A0\uFE0F';msg='Time: <strong>None</strong> \u2014 Please sync';cls='aw'}
  let rtcInfo='';
  if(d.rtcPresent){rtcInfo='<br><small>\u2705 DS3231 RTC detected';if(d.rtcSynced)rtcInfo+=' | Last sync: '+d.rtcSyncAge+'s ago';rtcInfo+='</small>';}
  else rtcInfo='<br><small>\u26A0\uFE0F DS3231 RTC not detected</small>';
  s.innerHTML=icon+' '+msg+rtcInfo;s.className='alert '+cls;
}
setInterval(tick,1000);tick();
fetch('/api/ntp').then(r=>r.json()).then(d=>{
  document.getElementById('srv').value=d.ntpServer||'time.google.com';
  document.getElementById('gmt').value=d.gmtOffset||28800;
  document.getElementById('dst').value=d.daylightOffset||0;
  document.getElementById('shi').value=d.syncHours||1;
  document.getElementById('ds3231Master').checked=d.ds3231Master||false;
}).catch(()=>{});
function toggleDS3231Master(){
  const checked=document.getElementById('ds3231Master').checked;
  fetch('/api/time/ds3231-master',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({enabled:checked})})
  .then(r=>r.json()).then(d=>{if(d.success)toast('DS3231 master mode '+(d.enabled?'enabled':'disabled'));else toast('Failed',false)})
  .catch(()=>toast('Error',false));
}
function save(){
  const h=parseInt(document.getElementById('shi').value);
  if(h<1||h>24){toast('Sync 1\u201324 h',false);return}
  fetch('/api/ntp',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ntpServer:document.getElementById('srv').value,gmtOffset:parseInt(document.getElementById('gmt').value),daylightOffset:parseInt(document.getElementById('dst').value),syncHours:h})})
  .then(r=>r.json()).then(d=>{if(d.success)toast('NTP settings saved!');else toast('Failed: '+d.error,false)}).catch(()=>toast('Error',false));
}
function sync(){const b=document.getElementById('sbtn');b.disabled=true;b.textContent='Syncing\u2026';fetch('/api/ntp/sync',{method:'POST'}).then(r=>r.json()).then(d=>{b.disabled=false;b.innerHTML='&#x1F504; Sync Now';if(d.success)toast('Time synced!');else toast('Sync failed',false)}).catch(()=>{b.disabled=false;b.innerHTML='&#x1F504; Sync Now';toast('Error',false)})}
function syncFromBrowser(){
  const b=document.getElementById('bsbtn');b.disabled=true;b.textContent='Syncing\u2026';
  const utcEpoch=Math.floor(Date.now()/1000);
  fetch('/api/time/browser-sync',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({utc_epoch:utcEpoch})})
  .then(r=>r.json()).then(d=>{b.disabled=false;b.innerHTML='&#x1F310; Sync from Browser';if(d.success){document.getElementById('clk').textContent=d.local_time;let msg='Synced: '+d.local_time;if(d.rtc_synced)msg+=' | DS3231 updated';toast(msg);tick()}else toast('Sync failed: '+(d.error||'Unknown'),false)})
  .catch(()=>{b.disabled=false;b.innerHTML='&#x1F310; Sync from Browser';toast('Error',false)});
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
<a href="/mqtt">MQTT</a>
<a href="/system">System</a>
</nav>
<div class="hdr-r"><span class="dot wd"></span><span class="dot td"></span>&nbsp;<span id="clk">--:--:--</span></div>
</header>
<main>
<p class="ptitle">Access Point Settings</p>
<div class="card fcrd">
<div class="alert aw">&#x26A0;&#xFE0F; Changing SSID, Password, Channel or Visibility will restart the AP and disconnect all clients.</div>
<div class="fg"><label>AP SSID</label><input type="text" id="ssid" maxlength="31" required></div>
<div class="fg"><label>AP Password</label><input type="password" id="pw" minlength="8" placeholder="Enter wifi password"></div>
<div class="fg"><label>Channel (1&ndash;13)</label>
<select id="ch"><option value="1">1</option><option value="2">2</option><option value="3">3</option><option value="4">4</option><option value="5">5</option><option value="6">6 (default)</option><option value="7">7</option><option value="8">8</option><option value="9">9</option><option value="10">10</option><option value="11">11</option><option value="12">12</option><option value="13">13</option></select></div>
<div class="fg"><label>SSID Visibility</label><select id="hidden"><option value="0">Visible</option><option value="1">Hidden</option></select></div>
<button class="btn bsave" onclick="save()">&#x1F4BE; Save Settings</button>
</div>
</main>
<div id="toast"></div>
<script>
function toast(m,ok=true){const t=document.getElementById('toast');t.textContent=m;t.className='show '+(ok?'ok':'er');clearTimeout(t._t);t._t=setTimeout(()=>t.className='',3000);}
function tick(){fetch('/api/time').then(r=>r.json()).then(d=>{document.getElementById('clk').textContent=d.time||'--:--:--';const w=document.querySelector('.wd'),t=document.querySelector('.td');if(w)w.className='dot '+(d.wifi?'g':'r');if(t){let tc='y';if(d.timeSource==='ntp')tc='g';else if(d.timeSource==='browser')tc='b';else if(d.timeSource==='rtc')tc='b';else tc='y';t.className='dot '+tc;}}).catch(()=>{})}
setInterval(tick,1000);tick();
fetch('/api/ap').then(r=>r.json()).then(d=>{document.getElementById('ssid').value=d.ap_ssid||'';document.getElementById('ch').value=d.ap_channel||6;document.getElementById('hidden').value=d.ap_hidden?'1':'0'}).catch(()=>{});
function save(){
  const pw=document.getElementById('pw').value;
  if(pw.length>0&&pw.length<8){toast('Password must be 8+ chars',false);return}
  fetch('/api/ap',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ap_ssid:document.getElementById('ssid').value,ap_password:pw,ap_channel:parseInt(document.getElementById('ch').value),ap_hidden:document.getElementById('hidden').value==='1'})})
  .then(r=>r.json()).then(d=>{if(d.success){toast('Saved!');if(d.restarted)toast('AP restarted');setTimeout(()=>location.reload(),4000)}else toast('Failed: '+d.error,false)}).catch(()=>toast('Error',false));
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
<a href="/mqtt">MQTT</a>
<a href="/system">System</a>
</nav>
<div class="hdr-r"><span class="dot wd"></span><span class="dot td"></span>&nbsp;<span id="clk">--:--:--</span></div>
</header>
<main>
<p class="ptitle">GPIO Pin Configuration</p>
<div class="card fcrd">
<div class="alert ai">Configure which GPIO pins control your relays and set active level. Changes take effect immediately.</div>
<div class="fg"><label>Global Active Level Mode</label>
<select id="globalMode" onchange="saveGlobalMode()"><option value="0">Per-Relay (Individual)</option><option value="1">Global: All Active LOW</option><option value="2">Global: All Active HIGH</option></select></div>
<hr>
<div class="fg"><label>Add New Relay Pin</label><div class="input-row"><select id="newPin"><option value="">Select GPIO...</option></select><button class="btn bsave" onclick="addPin()" style="margin-top:0;width:auto;padding:9px 20px">+ Add Relay</button></div></div>
<hr>
<div id="pinList"><h3 style="margin-bottom:10px">Configured Relays (<span id="relayCount">0</span>)</h3><div id="pins"></div></div>
<div style="margin-top:16px;display:flex;gap:8px"><button class="btn bwarn" onclick="resetDefaults()" style="padding:9px 18px">&#x1F504; Reset to Default</button></div>
</div>
</main>
<div id="toast"></div>
<script>
function toast(m,ok=true){const t=document.getElementById('toast');t.textContent=m;t.className='show '+(ok?'ok':'er');clearTimeout(t._t);t._t=setTimeout(()=>t.className='',3000);}
function tick(){fetch('/api/time').then(r=>r.json()).then(d=>{document.getElementById('clk').textContent=d.time||'--:--:--';const w=document.querySelector('.wd'),t=document.querySelector('.td');if(w)w.className='dot '+(d.wifi?'g':'r');if(t){let tc='y';if(d.timeSource==='ntp')tc='g';else if(d.timeSource==='browser')tc='b';else if(d.timeSource==='rtc')tc='b';else tc='y';t.className='dot '+tc;}}).catch(()=>{})}
setInterval(tick,1000);tick();
// change gpio pins
const DEFAULT_PINS=[1,2,3,4,5,6,7,10,11,12,13,14,15,16,17,18];
let gpioData=null;
function saveGlobalMode(){const mode=parseInt(document.getElementById('globalMode').value);fetch('/api/gpio/global-mode',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({mode})}).then(r=>r.json()).then(d=>{if(d.success){toast('Global mode updated!');loadGPIO()}else toast('Failed: '+d.error,false)}).catch(()=>toast('Error',false))}
function loadGPIO(){
  fetch('/api/gpio').then(r=>r.json()).then(d=>{
    gpioData=d;document.getElementById('relayCount').textContent=d.count;
    fetch('/api/gpio/global-mode').then(r=>r.json()).then(data=>{if(data.mode!==undefined)document.getElementById('globalMode').value=data.mode}).catch(()=>{});
    let pinsHtml='';
    const globalMode=parseInt(document.getElementById('globalMode').value);
    const isGlobalMode=(globalMode===1||globalMode===2);
    if(isGlobalMode){pinsHtml='<div style="background:#FFF3E0;padding:10px;border-radius:7px;margin-bottom:12px;border-left:4px solid #F9A825"><strong>\u26A0\uFE0F Global Mode Active</strong><br><small>Individual settings overridden.</small></div>'}
    for(let i=0;i<d.count;i++){const activeLow=d.activeLow?d.activeLow[i]:true;pinsHtml+=`<div style="display:flex;align-items:center;gap:10px;padding:10px;border:1px solid #E3E8EF;border-radius:7px;margin-bottom:6px;background:#FAFAFA"><span style="font-weight:700;min-width:60px">Relay ${i+1}</span><span style="flex:1">GPIO <strong>${d.pins[i]}</strong></span><span style="flex:1;font-size:12px">Active: <strong>${activeLow?'LOW':'HIGH'}</strong></span><button class="btn ${activeLow?'bon-b':'boff-b'}" onclick="toggleActiveLow(${i})" style="padding:5px 10px;font-size:11px" ${isGlobalMode?'disabled':''}>${activeLow?'Set HIGH':'Set LOW'}</button><button class="btn boff-b" onclick="deletePin(${i})" style="padding:5px 10px;font-size:11px">&#x1F5D1; Remove</button></div>`}
    if(d.count===0)pinsHtml+='<div style="color:#90A4AE;text-align:center;padding:20px">No relays configured.</div>';
    if(d.count>=16)pinsHtml+='<div style="color:#E65100;text-align:center;padding:10px;font-size:12px">Maximum 16 relays reached.</div>';
    document.getElementById('pins').innerHTML=pinsHtml;
    const select=document.getElementById('newPin');select.innerHTML='<option value="">Select GPIO...</option>';
    const usedPins=new Set(d.pins.slice(0,d.count));
    d.availablePins.forEach(pin=>{if(!usedPins.has(pin))select.innerHTML+=`<option value="${pin}">GPIO ${pin}</option>`});
    if(select.options.length===1)select.innerHTML+='<option value="" disabled>All pins in use</option>';
  }).catch(()=>toast('Error',false));
}
function toggleActiveLow(index){fetch('/api/gpio/toggle-active-low',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({index})}).then(r=>r.json()).then(d=>{if(d.success){toast('Active: '+(d.activeLow?'LOW':'HIGH'));loadGPIO()}else toast('Failed: '+d.error,false)}).catch(()=>toast('Error',false))}
function addPin(){const pin=parseInt(document.getElementById('newPin').value);if(!pin)return;fetch('/api/gpio/add',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({pin})}).then(r=>r.json()).then(d=>{if(d.success){toast('Relay added on GPIO '+pin);loadGPIO()}else toast('Failed: '+d.error,false)}).catch(()=>toast('Error',false))}
function deletePin(index){if(!confirm('Remove Relay '+(index+1)+'?'))return;fetch('/api/gpio/delete',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({index})}).then(r=>r.json()).then(d=>{if(d.success){toast('Removed!');loadGPIO()}else toast('Failed: '+d.error,false)}).catch(()=>toast('Error',false))}
function resetDefaults(){if(!confirm('Reset to default 16 GPIO pins?'))return;fetch('/api/gpio/save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({pins:DEFAULT_PINS})}).then(r=>r.json()).then(d=>{if(d.success){toast('Reset!');loadGPIO()}else toast('Failed: '+d.error,false)}).catch(()=>toast('Error',false))}
loadGPIO();
</script></body></html>)raw";

const char mqtt_html[] PROGMEM = R"raw(<!DOCTYPE html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>MQTT</title>
<link rel="stylesheet" href="/style.css"></head><body>
<header>
<nav>
<a href="/">Relays</a>
<a href="/wifi">WiFi</a>
<a href="/ntp">Time</a>
<a href="/ap">AP</a>
<a href="/gpio">GPIO</a>
<a href="/mqtt" class="cur">MQTT</a>
<a href="/system">System</a>
</nav>
<div class="hdr-r"><span class="dot wd"></span><span class="dot td"></span><span id="mqttDot" class="dot"></span>&nbsp;<span id="clk">--:--:--</span></div>
</header>
<main>
<p class="ptitle">MQTT Configuration</p>
<div class="card fcrd">
<div id="status" class="alert ai" style="display:none"></div>
<div class="fg">
<label style="display:flex;align-items:center;justify-content:space-between;cursor:pointer;margin-bottom:8px">
<span style="font-size:14px;font-weight:600">MQTT Client</span>
<button id="mqttToggleBtn" class="btn" style="min-width:90px;padding:8px 16px;font-weight:700;transition:all 0.2s" onclick="toggleMqtt()"></button>
</label>
<small>Connects to MQTT broker for remote control. Compatible with Home Assistant, Node-RED, OpenHAB.</small>
</div>
<hr>
<div id="mqttConfig">
<div class="fg"><label>Broker IP / Hostname</label><input type="text" id="broker" placeholder="192.168.1.100" required></div>
<div class="fg"><label>Broker Port</label><input type="number" id="port" min="1" max="65535" value="1883" required></div>
<div class="fg"><label>Client ID</label><input type="text" id="clientId" placeholder="ESP32_S3_Relay16" required></div>
<div class="fg"><label>Username (optional)</label><input type="text" id="user" placeholder="Leave blank if no auth"></div>
<div class="fg"><label>Password (optional)</label><input type="password" id="pass" placeholder="Leave blank if no auth"></div>
<div class="fg"><label>Base Topic</label><input type="text" id="topic" placeholder="esp32s3relay16" required></div>
<div class="fg"><label>QoS (0-2)</label><select id="qos"><option value="0">0 - At most once</option><option value="1" selected>1 - At least once</option><option value="2">2 - Exactly once</option></select></div>
<div class="fg"><label>Keepalive (seconds)</label><input type="number" id="keepalive" min="10" max="3600" value="60"></div>
<div class="fg"><label>Status Publish Interval (seconds)</label><input type="number" id="interval" min="5" max="3600" value="30"></div>
<div class="fg"><label style="display:flex;align-items:center;gap:8px;cursor:pointer;font-size:13px;font-weight:600;color:#1A1A2E;text-transform:none"><input type="checkbox" id="haDiscovery" checked style="width:18px;height:18px;cursor:pointer;accent-color:#1565C0"><span>Enable Home Assistant Auto-Discovery</span></label>
<small>Automatically creates entities in Home Assistant via MQTT Discovery protocol. When enabled, the device will appear in Home Assistant automatically.</small>
</div>
<button class="btn bsave" onclick="saveMqtt()">&#x1F4BE; Save MQTT Settings</button>
<hr>
<div style="background:#FAFAFA;border-radius:8px;padding:12px;margin-top:12px">
<p style="font-weight:700;margin-bottom:8px">MQTT Topic Reference</p>
<div style="font-size:12px;color:#546E7A;line-height:1.8">
<strong>Commands (subscribe):</strong><br>
<code id="tRelaySet">.../relay/1/set</code> &larr; "ON" / "OFF" / "TOGGLE"<br>
<code id="tRelayToggle">.../relay/1/toggle</code> &larr; any payload<br>
<code id="tRelayPulse">.../relay/1/pulse</code> &larr; "5000" (ms)<br>
<code id="tRelayAll">.../relay/all/set</code> &larr; "ON" / "OFF"<br>
<code id="tSchedSet">.../relay/1/schedule/set</code> &larr; JSON<br>
<code id="tSysCmd">.../system/command</code> &larr; "reboot" / "status"<br><br>
<strong>State (publish):</strong><br>
<code id="tRelayState">.../relay/1/state</code> &rarr; "ON" / "OFF"<br>
<code id="tRelayAvail">.../relay/1/available</code> &rarr; "online"<br>
<code id="tSchedGet">.../relay/1/schedule</code> &rarr; JSON<br>
<code id="tStats">.../system/stats</code> &rarr; JSON<br>
<code id="tStatus">.../status</code> &rarr; "online" / "offline" (LWT)
</div></div></div></div>
</main>
<div id="toast"></div>
<script>
function toast(m,ok=true){const t=document.getElementById('toast');t.textContent=m;t.className='show '+(ok?'ok':'er');clearTimeout(t._t);t._t=setTimeout(()=>t.className='',3000)}
function tick(){fetch('/api/time').then(r=>r.json()).then(d=>{document.getElementById('clk').textContent=d.time||'--:--:--';const w=document.querySelector('.wd'),t=document.querySelector('.td');if(w)w.className='dot '+(d.wifi?'g':'r');if(t){let tc='y';if(d.timeSource==='ntp')tc='g';else if(d.timeSource==='browser')tc='b';else if(d.timeSource==='rtc')tc='b';else tc='y';t.className='dot '+tc}}).catch(()=>{});fetch('/api/mqtt/status').then(r=>r.json()).then(d=>{const dot=document.getElementById('mqttDot');if(dot)dot.className='dot '+(d.connected?'g':d.enabled?'y':'r')}).catch(()=>{})}
setInterval(tick,3000);tick()
function updateMqttButton(enabled){const btn=document.getElementById('mqttToggleBtn');if(enabled){btn.textContent='\u2713 ON';btn.className='btn btn-mqtt-on'}else{btn.textContent='\u2717 OFF';btn.className='btn btn-mqtt-off'}}
function updateTopicRefs(){const topic=document.getElementById('topic').value||'esp32s3relay16';document.getElementById('tRelaySet').textContent=topic+'/relay/1/set';document.getElementById('tRelayToggle').textContent=topic+'/relay/1/toggle';document.getElementById('tRelayPulse').textContent=topic+'/relay/1/pulse';document.getElementById('tRelayAll').textContent=topic+'/relay/all/set';document.getElementById('tSchedSet').textContent=topic+'/relay/1/schedule/set';document.getElementById('tSysCmd').textContent=topic+'/system/command';document.getElementById('tRelayState').textContent=topic+'/relay/1/state';document.getElementById('tRelayAvail').textContent=topic+'/relay/1/available';document.getElementById('tSchedGet').textContent=topic+'/relay/1/schedule';document.getElementById('tStats').textContent=topic+'/system/stats';document.getElementById('tStatus').textContent=topic+'/status'}
document.getElementById('topic').addEventListener('input',updateTopicRefs)
function loadMqttConfig(){fetch('/api/mqtt').then(r=>r.json()).then(d=>{document.getElementById('broker').value=d.broker_ip||'';document.getElementById('port').value=d.broker_port||1883;document.getElementById('clientId').value=d.client_id||'';document.getElementById('user').value=d.username||'';document.getElementById('pass').value=d.password||'';document.getElementById('topic').value=d.base_topic||'esp32s3relay16';document.getElementById('qos').value=d.qos||1;document.getElementById('keepalive').value=d.keepalive||60;document.getElementById('interval').value=d.status_interval||30;document.getElementById('haDiscovery').checked=d.ha_discovery!==false;updateMqttButton(d.enabled);updateTopicRefs();const s=document.getElementById('status');s.style.display='';if(d.connected){s.innerHTML='\u2705 <strong>Connected</strong> to broker';s.className='alert ai'}else if(d.enabled){s.innerHTML='\u26A0\uFE0F <strong>Enabled, not connected</strong>';s.className='alert aw'}else{s.innerHTML='\u26D4 <strong>MQTT Disabled</strong>';s.className='alert aw'};const cfg=document.getElementById('mqttConfig');cfg.style.opacity=d.enabled?'1':'0.5';const inputs=cfg.querySelectorAll('input,select,button:not(#mqttToggleBtn)');inputs.forEach(i=>i.disabled=!d.enabled)}).catch(()=>{})}
function toggleMqtt(){const btn=document.getElementById('mqttToggleBtn');const wasEnabled=btn.textContent==='\u2713 ON';const willEnable=!wasEnabled;btn.disabled=true;btn.textContent='...';fetch('/api/mqtt',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({enabled:willEnable})}).then(r=>r.json()).then(d=>{btn.disabled=false;if(d.success){updateMqttButton(willEnable);toast('MQTT '+(willEnable?'ON':'OFF'));loadMqttConfig()}else{toast('Failed',false);updateMqttButton(wasEnabled)}}).catch(()=>{btn.disabled=false;toast('Error',false);updateMqttButton(wasEnabled)})}
function saveMqtt(){const btn=document.querySelector('.bsave');btn.disabled=true;btn.textContent='Saving...';fetch('/api/mqtt',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({broker_ip:document.getElementById('broker').value,broker_port:parseInt(document.getElementById('port').value),client_id:document.getElementById('clientId').value,username:document.getElementById('user').value,password:document.getElementById('pass').value,base_topic:document.getElementById('topic').value,qos:parseInt(document.getElementById('qos').value),keepalive:parseInt(document.getElementById('keepalive').value),status_interval:parseInt(document.getElementById('interval').value),ha_discovery:document.getElementById('haDiscovery').checked})}).then(r=>r.json()).then(d=>{btn.disabled=false;btn.innerHTML='&#x1F4BE; Save MQTT Settings';if(d.success){toast('MQTT settings saved!');if(d.restarted)toast('Reconnecting...');loadMqttConfig()}else toast('Failed: '+d.error,false)}).catch(()=>{btn.disabled=false;btn.innerHTML='&#x1F4BE; Save MQTT Settings';toast('Error',false)})}
loadMqttConfig()
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
<a href="/mqtt">MQTT</a>
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
<div class="ibox"><div class="l">Browser Sync</div><div class="v" id="sbs">&hellip;</div></div>
<div class="ibox"><div class="l">NTP Server</div><div class="v" id="sns" style="font-size:12px">&hellip;</div></div>
<div class="ibox"><div class="l">Chip Model</div><div class="v" id="sch">&hellip;</div></div>
<div class="ibox"><div class="l">mDNS Hostname</div><div class="v" id="smdns">&hellip;</div></div>
<div class="ibox"><div class="l">Relay Count</div><div class="v" id="src">&hellip;</div></div>
<div class="ibox"><div class="l">GMT Offset</div><div class="v" id="sgmt" style="font-size:13px">&hellip;</div></div>
<div class="ibox"><div class="l">Drift Comp</div><div class="v" id="sdrift" style="font-size:13px">&hellip;</div></div>
<div class="ibox"><div class="l">GPIO Mode</div><div class="v" id="sactmode" style="font-size:12px">&hellip;</div></div>
<div class="ibox"><div class="l">DS3231 RTC</div><div class="v" id="srtc">&hellip;</div></div>
<div class="ibox"><div class="l">DS3231 Sync</div><div class="v" id="srtcsync">&hellip;</div></div>
<div class="ibox"><div class="l">WiFi Station</div><div class="v" id="stawifi">&hellip;</div></div>
<div class="ibox"><div class="l">DS3231 Master</div><div class="v" id="sds3231master" style="font-size:12px">&hellip;</div></div>
</div>
<div class="card fcrd">
<p style="font-weight:700;margin-bottom:12px">Device Control</p>
<div style="display:flex;gap:8px;flex-wrap:wrap">
<button class="btn bwarn" onclick="rst()" style="padding:9px 18px;border-radius:6px;font-size:13px;font-weight:600">&#x1F504; Verify Services</button>
<button class="btn bdanger" onclick="fct()" style="padding:9px 18px;border-radius:6px;font-size:13px;font-weight:600">&#x26A0; Factory Reset</button>
</div>
<p style="color:#90A4AE;font-size:12px;margin-top:10px">Factory reset clears all settings.<br>Hold <strong>BOOT button</strong> 5 seconds for hardware reset.</p>
</div>
</main>
<div id="toast"></div>
<script>
function toast(m,ok=true){const t=document.getElementById('toast');t.textContent=m;t.className='show '+(ok?'ok':'er');clearTimeout(t._t);t._t=setTimeout(()=>t.className='',3000)}
function tick(){fetch('/api/time').then(r=>r.json()).then(d=>{document.getElementById('clk').textContent=d.time||'--:--:--';const w=document.querySelector('.wd'),t=document.querySelector('.td');if(w)w.className='dot '+(d.wifi?'g':'r');if(t){let tc='y';if(d.timeSource==='ntp')tc='g';else if(d.timeSource==='browser')tc='b';else if(d.timeSource==='rtc')tc='b';else tc='y';t.className='dot '+tc}}).catch(()=>{})}
setInterval(tick,1000);tick()
function fmtUp(s){const h=Math.floor(s/3600),m=Math.floor((s%3600)/60),ss=s%60;return h+'h '+m+'m '+ss+'s'}
function fmtAge(s){if(s===4294967295||s<0)return'Never';if(s<60)return'Just now';if(s<3600)return Math.floor(s/60)+' min ago';const h=Math.floor(s/3600);return h+'h '+Math.floor((s%3600)/60)+'m ago'}
function loadSys(){fetch('/api/system').then(r=>r.json()).then(d=>{document.getElementById('sip').textContent=d.wifiConnected?d.ip:'(not connected)';document.getElementById('sap').textContent=d.ap_ip;document.getElementById('shp').textContent=(d.freeHeap/1024).toFixed(1)+' KB';document.getElementById('sup').textContent=fmtUp(d.uptime);document.getElementById('srs').textContent=d.wifiConnected?(d.rssi+'dBm'):'\u2014';const tsrc=d.timeSource||'None';let ts='';if(tsrc==='NTP')ts='color:#2E7D32';else if(tsrc==='Browser')ts='color:#1565C0';else if(tsrc==='RTC')ts='color:#7B1FA2';else ts='color:#C62828';document.getElementById('stsrc').style.cssText='font-size:13px;font-weight:700;'+ts;document.getElementById('stsrc').textContent=tsrc;document.getElementById('sutc').textContent=d.utcEpoch||'\u2014';document.getElementById('snt').textContent=d.ntpSyncAge>=0?fmtAge(d.ntpSyncAge):'Never';document.getElementById('sbs').textContent=d.browserSyncAge>=0?fmtAge(d.browserSyncAge):'Never';document.getElementById('sns').textContent=d.ntpServer||'\u2014';document.getElementById('sch').textContent=d.chipModel||'ESP32S3';document.getElementById('smdns').textContent=d.mdnsStarted?d.mdnsHostname+'.local':'Not running';document.getElementById('src').textContent=d.relayCount+' / '+d.maxRelays;document.getElementById('sgmt').textContent=(d.gmtOffset/3600).toFixed(1)+'h';document.getElementById('sdrift').textContent=d.driftComp?d.driftComp.toFixed(4):'1.0000';document.getElementById('sactmode').textContent=d.globalActiveMode===1?'Global LOW':d.globalActiveMode===2?'Global HIGH':'Per-Relay';document.getElementById('srtc').textContent=d.rtcPresent?'\u2705 Present':'\u274C Not detected';document.getElementById('srtcsync').textContent=d.rtcSyncAge>=0?fmtAge(d.rtcSyncAge):'Never';document.getElementById('stawifi').textContent=d.staEnabled?(d.wifiConnected?'\u2705 Connected':'\u26A0 Disconnected'):'\u274C Disabled';document.getElementById('sds3231master').textContent=d.ds3231Master?'\u2705 Enabled (Master)':'\u274C Disabled (Internal RTC)';}).catch(()=>{})}
loadSys();setInterval(loadSys,5000)
function rst(){if(!confirm('Verify all services?'))return;fetch('/api/reset',{method:'POST'}).then(r=>r.json()).then(d=>{if(d.success)toast('Services verified!');else toast('Failed',false)}).catch(()=>toast('Error',false))}
function fct(){if(!confirm('FACTORY RESET - ALL settings erased. Continue?'))return;fetch('/api/factory-reset',{method:'POST'}).then(r=>r.json()).then(d=>{if(d.success)toast('Factory reset complete');else toast('Failed',false)}).catch(()=>{});setTimeout(()=>window.location.href='/',7000)}
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
        loadMqttFullConfig();        
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
void syncInternalRTC(time_t rawUtcEpoch) {
    if (!VALID_UNIX_TIME(rawUtcEpoch)) return;    
    unsigned long nowMicros = micros();
    unsigned long nowMillis = millis();    
    internalEpoch = rawUtcEpoch;
    internalMillisAtLastSync = nowMillis;
    rtcMicrosAtLastSync = nowMicros;
    lastRTCRebase = nowMillis;
    rtcInitialized = true;
    if (extConfig.ds3231_master) {
        timeSource = TIME_SOURCE_RTC;
    } else if (timeSource == TIME_SOURCE_BROWSER) {
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
//  NTP SYNC
// =============================================================================
void tryNTPSync() {
    if (!wifiConnected || !extConfig.sta_enabled) {  
        ntpAsyncState = NTP_STATE_IDLE;
        return;
    }    
    unsigned long now = millis();    
    switch (ntpAsyncState) {
        case NTP_STATE_IDLE:
            if (!timeHasElapsed(now, lastNTPAttempt, NTP_RETRY_INTERVAL)) return;            
            lastNTPAttempt = now;
            ntpAsyncCurrentServer = ntpServerIndex;
            ntpAsyncState = NTP_STATE_CONNECTING;
        case NTP_STATE_CONNECTING:
            timeClient.setPoolServerName(NTP_SERVERS[ntpAsyncCurrentServer]);
            timeClient.setTimeOffset(0);
            timeClient.begin();
            ntpAsyncPhaseStart = now;
            ntpAsyncState = NTP_STATE_WAITING;
            break;            
        case NTP_STATE_WAITING:
            if (timeClient.update()) {
                time_t rawUtcEpoch = timeClient.getEpochTime();
                if (VALID_UNIX_TIME(rawUtcEpoch)) {
                    timeSource = TIME_SOURCE_NTP;
                    syncInternalRTC(rawUtcEpoch);
                    ntpServerIndex = ntpAsyncCurrentServer;
                    ntpFailCount = 0;
                    health.ntpFailures = 0;
                    ntpAsyncState = NTP_STATE_IDLE;
                    updateScheduleCache();
                    return;
                }
            }
            if (timeHasElapsed(now, ntpAsyncPhaseStart, NTP_SERVER_TIMEOUT)) {
                ntpAsyncCurrentServer = (ntpAsyncCurrentServer + 1) % NUM_NTP_SERVERS;                
                if (ntpAsyncCurrentServer == ntpServerIndex) {
                    ntpFailCount++;
                    health.ntpFailures++;
                    ntpAsyncState = NTP_STATE_IDLE;
                    return;
                }
                ntpAsyncState = NTP_STATE_CONNECTING;
            }
            break;
    }
    yield();
    server.handleClient();
}

// =============================================================================
//  BROWSER TIME SYNC
// =============================================================================
void handleBrowserTimeSync() {
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
    time_t browserUtcEpoch = doc["utc_epoch"];    
    if (browserUtcEpoch < 1577836800 || browserUtcEpoch > MAX_UNIX_TIME) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid epoch time. Expected between 2020-2106.\"}");
        return;
    }    
    timeSource = TIME_SOURCE_BROWSER;  
    syncInternalRTC(browserUtcEpoch);    
    timeSource = TIME_SOURCE_BROWSER;
    lastBrowserSync = millis();
    lastNTPSync = 0;
    if (rtcPresent && rtcInitialized && internalEpoch > 0) {
        if (rtc.begin()) {
            DateTime dt(internalEpoch);
            rtc.adjust(dt);
            rtcTimeValid = true;
            lastRTCDSync = millis();
        }
    }
    saveRTCState();
    updateScheduleCache();
    criticalStateDirty = true;    
    time_t localEpoch = getLocalEpoch(browserUtcEpoch);
    struct tm* ti = gmtime(&localEpoch);
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
    respDoc["utc_epoch"] = browserUtcEpoch;
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
//  DS3231 MASTER TOGGLE HANDLER
// =============================================================================
void handleDS3231MasterToggle() {
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
    bool enabled = doc["enabled"];
    extConfig.ds3231_master = enabled ? 1 : 0;
    saveExtConfig();
    if (enabled && rtcPresent && rtcTimeValid) {
        syncInternalRTCFromDS3231();
    }    
    server.send(200, "application/json", "{\"success\":true,\"enabled\":" + String(enabled ? "true" : "false") + "}");
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
    time_t epoch = getCurrentEpoch();
    if (epoch < MIN_UNIX_TIME) return;    
    time_t localEpoch = getLocalEpoch(epoch);
    struct tm* ti = gmtime(&localEpoch);
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
            if (!(relayConfigs[i].schedule.days[s] & cachedTodayBit)) continue;
            uint32_t monthDayMask = relayConfigs[i].schedule.monthDays[s];
            if (monthDayMask != 0 && !(monthDayMask & (1 << (cachedMonthDay - 1)))) continue;
            uint16_t monthMask = relayConfigs[i].schedule.months[s];
            if (monthMask != 0 && !(monthMask & (1 << cachedMonth))) continue;            
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
    time_t epoch = getCurrentEpoch();
    if (epoch < MIN_UNIX_TIME) return;
    time_t localEpoch = getLocalEpoch(epoch);
    struct tm* ti = gmtime(&localEpoch);
    if (!ti) return;    
    int cur = ti->tm_hour * 3600 + ti->tm_min * 60 + ti->tm_sec;
    uint8_t todayBit = cachedTodayBit;
    int monthDay = cachedMonthDay;
    int month = cachedMonth;      
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
            if (!(relayConfigs[i].schedule.days[s] & todayBit)) continue;           
            uint32_t monthDayMask = relayConfigs[i].schedule.monthDays[s];
            if (monthDayMask != 0 && !(monthDayMask & (1 << (monthDay - 1)))) continue;
            uint16_t monthMask = relayConfigs[i].schedule.months[s];
            if (monthMask != 0 && !(monthMask & (1 << month))) continue;            
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
        MDNS.addServiceTxt("http", "tcp", "version", "v9");
        MDNS.addServiceTxt("http", "tcp", "channels", String(gpioConfig.count).c_str());
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
    Serial.begin(115200);      
    initRTC();    
    loadGPIOConfig();
    loadMqttFullConfig();
    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
    for (int i = 0; i < gpioConfig.count; i++) {
        pinMode(getRelayPin(i), OUTPUT);
        setRelayOutput(i, false); 
        lastRelayOutputs[i] = false;
    }
    relayOutputsInitialized = true;    
    for (int i = 0; i < MAX_RELAYS; i++) {
        for (int s = 0; s < 8; s++) {
            relayConfigs[i].schedule.enabled[s] = false;
            relayConfigs[i].schedule.days[s] = DAY_ALL;
            relayConfigs[i].schedule.monthDays[s] = 0;
            relayConfigs[i].schedule.months[s] = MONTH_ALL;  
        }
        relayConfigs[i].manualOverride = false;
        relayConfigs[i].manualState    = false;
        snprintf(relayConfigs[i].name, 16, "Relay %d", i + 1);
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
    timeClient.begin();    
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
    if (rtcPresent && rtcInitialized && internalEpoch > 0) {
        if (timeHasElapsed(now, lastRTCDSync, DS3231_SYNC_INTERVAL)) {
            lastRTCDSync = now;
            if (rtc.begin()) {
                DateTime dt(getCurrentEpoch());
                rtc.adjust(dt);
                rtcTimeValid = true;
            }
        }
    }
    autoSaveInternalRTC();    
    if (extConfig.ds3231_master && rtcPresent && rtcTimeValid) {
        if (timeHasElapsed(now, lastDS3231SyncInternal, DS3231_SYNC_INTERVAL_MASTER)) {
            lastDS3231SyncInternal = now;
            syncInternalRTCFromDS3231();
        }
    }    
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
    handleMqtt();
    yield();
}

// =============================================================================
//  CONFIGURATION
// =============================================================================
void initDefaults() {
    memset(&sysConfig, 0, sizeof(SystemConfig));
    sysConfig.magic           = EEPROM_MAGIC;
    sysConfig.version         = EEPROM_VERSION;
    strcpy(sysConfig.ap_ssid,     "ESP32_S3_16CH_Timer_Switch");
    strcpy(sysConfig.ap_password, "ESP32-admin");
    strcpy(sysConfig.ntp_server,  "time.google.com");
    sysConfig.gmt_offset      = 28800;
    sysConfig.daylight_offset = 0;
    sysConfig.last_rtc_epoch  = 0;
    sysConfig.rtc_drift       = 1.0f;  
    strcpy(sysConfig.hostname, "esp32s3");    
    for (int i = 0; i < MAX_RELAYS; i++) {
        memset(&relayConfigs[i], 0, sizeof(RelayConfig));
        for (int s = 0; s < 8; s++) {
            relayConfigs[i].schedule.days[s] = DAY_ALL;
            relayConfigs[i].schedule.monthDays[s] = 0;
            relayConfigs[i].schedule.months[s] = MONTH_ALL;  
        }
        snprintf(relayConfigs[i].name, 16, "Relay %d", i + 1);
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
    len = preferences.getBytes("relayConfigs", relayConfigs, sizeof(RelayConfig) * MAX_RELAYS);
    if (len != sizeof(RelayConfig) * MAX_RELAYS) {
        for (int i = 0; i < MAX_RELAYS; i++) {
            memset(&relayConfigs[i], 0, sizeof(RelayConfig));
            for (int s = 0; s < 8; s++) {
                relayConfigs[i].schedule.days[s] = DAY_ALL;
                relayConfigs[i].schedule.monthDays[s] = 0;
                relayConfigs[i].schedule.months[s] = MONTH_ALL;  // NEW: default all months
            }
            snprintf(relayConfigs[i].name, 16, "Relay %d", i + 1);
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
        extConfig.ds3231_master  = 1;  
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
        if (extConfig.ds3231_master > 1) {
            extConfig.ds3231_master = 1;
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
    server.on("/mqtt",   HTTP_GET, []() { server.send_P(200, "text/html", mqtt_html);   });
    server.on("/system", HTTP_GET, []() { server.send_P(200, "text/html", system_html); });
    server.on("/style.css", HTTP_GET, []() { server.send_P(200, "text/css", style_css); });
    server.on("/api/relays",       HTTP_GET,  handleGetRelays);
    server.on("/api/relay/manual", HTTP_POST, handleManualControl);
    server.on("/api/relay/reset",  HTTP_POST, handleResetManual);
    server.on("/api/relay/save",   HTTP_POST, handleSaveRelay);
    server.on("/api/relay/name",   HTTP_POST, handleRelayName);
    server.on("/api/time", HTTP_GET, handleGetTime);
    server.on("/api/time/browser-sync", HTTP_POST, handleBrowserTimeSync);
    server.on("/api/time/ds3231-master", HTTP_POST, handleDS3231MasterToggle);
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
        server.send(200, "application/json", "{\"mode\":" + String(extConfig.global_active_mode) + "}");
    });
    server.on("/api/gpio/global-mode", HTTP_POST, handleGlobalActiveMode);
    server.on("/api/mqtt",        HTTP_GET,  handleGetMqttConfig);
    server.on("/api/mqtt",        HTTP_POST, handleSaveMqttConfig);
    server.on("/api/mqtt/status", HTTP_GET,  handleGetMqttStatus);
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
        server.send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
    });
    server.on("/library/test/success.html", HTTP_GET, []() {
        server.send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
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
            sch["months"] = relayConfigs[i].schedule.months[s];  
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
    StaticJsonDocument<4096> doc;
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
        relayConfigs[relay].schedule.months[s]      = sch["months"] | 0;  
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
    time_t utcEp = getCurrentEpoch();
    if (utcEp > MIN_UNIX_TIME) {
        time_t localEp = getLocalEpoch(utcEp);
        struct tm* t = gmtime(&localEp);
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
                  ",\"rtcSyncAge\":" + String(rtcSyncAge) + 
                  ",\"ds3231Master\":" + String(extConfig.ds3231_master ? "true" : "false") + "}";
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
        strncpy(sysConfig.sta_ssid, ssid, 31); 
        sysConfig.sta_ssid[31] = '\0';
        if (pw && strlen(pw) > 0) { 
            strncpy(sysConfig.sta_password, pw, 63); 
            sysConfig.sta_password[63] = '\0'; 
        } else { 
            sysConfig.sta_password[0] = '\0'; 
        }
        saveConfiguration();
        if (extConfig.sta_enabled) {
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
        "{\"ntpServer\":\"%s\",\"gmtOffset\":%d,\"daylightOffset\":%d,\"syncHours\":%d,\"globalActiveMode\":%d,\"ds3231Master\":%d}",
        sysConfig.ntp_server,
        sysConfig.gmt_offset,
        sysConfig.daylight_offset,
        extConfig.ntp_sync_hours,
        extConfig.global_active_mode,
        extConfig.ds3231_master
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
    bool ssidChanged = false, passChanged = false, channelChanged = false, hiddenChanged = false;    
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
    if (ssidChanged || passChanged || channelChanged || hiddenChanged) {
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
    doc["utcEpoch"] = (uint32_t)getCurrentEpoch();    
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
    doc["ds3231Master"] = extConfig.ds3231_master ? true : false;  
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
    loadMqttFullConfig();
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
        memset(&relayConfigs[i], 0, sizeof(RelayConfig));
        for (int s = 0; s < 8; s++) {
            relayConfigs[i].schedule.days[s] = DAY_ALL;
            relayConfigs[i].schedule.monthDays[s] = 0;
            relayConfigs[i].schedule.months[s] = MONTH_ALL;
        }
        snprintf(relayConfigs[i].name, 16, "Relay %d", i + 1);
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
    memset(&relayConfigs[gpioConfig.count], 0, sizeof(RelayConfig));
    for (int s = 0; s < 8; s++) {
        relayConfigs[gpioConfig.count].schedule.days[s] = DAY_ALL;
        relayConfigs[gpioConfig.count].schedule.monthDays[s] = 0;
        relayConfigs[gpioConfig.count].schedule.months[s] = MONTH_ALL;
    }
    snprintf(relayConfigs[gpioConfig.count].name, 16, "Relay %d", gpioConfig.count + 1);    
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
    memset(&relayConfigs[gpioConfig.count - 1], 0, sizeof(RelayConfig));
    for (int s = 0; s < 8; s++) {
        relayConfigs[gpioConfig.count - 1].schedule.days[s] = DAY_ALL;
        relayConfigs[gpioConfig.count - 1].schedule.monthDays[s] = 0;
        relayConfigs[gpioConfig.count - 1].schedule.months[s] = MONTH_ALL;
    }
    snprintf(relayConfigs[gpioConfig.count - 1].name, 16, "Relay %d", gpioConfig.count);
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
