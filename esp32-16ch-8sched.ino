#include <WiFi.h>                
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <WebServer.h>       
#include <DNSServer.h>
#include <EEPROM.h>
#include <ArduinoJson.h>

// EEPROM Configuration
#define EEPROM_SIZE 2048
#define EEPROM_MAGIC 0x1234
#define EEPROM_VERSION 2 

// DNS and Web Server
DNSServer dnsServer;
WebServer server(80);
const byte DNS_PORT = 53;

// NTP Configuration
const char* ntpServer = "ph.pool.ntp.org";
long gmtOffset_sec = 28800;  // Default: Philippines GMT+8:00
int daylightOffset_sec = 0;

// WiFi Station Configuration
String sta_ssid = "";
String sta_password = "";

// NTP Client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer, gmtOffset_sec, daylightOffset_sec);

// Internal RTC variables
unsigned long internalMillisAtLastNTPSync = 0;
time_t internalEpoch = 0;
bool rtcInitialized = false;
float driftCompensation = 1.0;

// Relay Configuration
#define NUM_RELAYS 16

// ESP32 GPIO mapping
const int relayPins[NUM_RELAYS] = {
  16,  // D0
  17,  // D1
  18,  // D2
  19,  // D3
  21,  // D4
  22,  // D5
  23,  // D6
  25,  // D7
  26,  // D8
  27,  // RX / GPIO3
  32,  // TX / GPIO1
  33,  // D11
  14,  // D12
  15,  // D13
  4,   // D14
  5    // D15
};
const bool relayActiveLow = true; // Set true for active LOW relays

// Timer Schedule Structure
struct TimerSchedule {
  uint8_t startHour[8];
  uint8_t startMinute[8];
  uint8_t startSecond[8];
  uint8_t stopHour[8];
  uint8_t stopMinute[8];
  uint8_t stopSecond[8];
  bool enabled[8];
};

struct RelayConfig {
  TimerSchedule schedule;
  bool manualOverride;
  bool manualState;
} relayConfigs[NUM_RELAYS];

// System Configuration
struct SystemConfig {
  uint16_t magic;
  uint8_t version;
  char sta_ssid[32];
  char sta_password[64];
  char ap_ssid[32];
  char ap_password[32];
  char ntp_server[48];
  long gmt_offset;
  int daylight_offset;
  time_t last_rtc_epoch;
  float rtc_drift;
};

SystemConfig sysConfig;

// AP Configuration variables
char ap_ssid[32] = "ESP32_16CH_Timer_Switch";
char ap_password[32] = "ESP32-admin";

bool wifiConnected = false;
unsigned long lastNTPSync = 0;
const unsigned long NTP_SYNC_INTERVAL = 10000; // 10 seconds 

// Function prototypes
time_t getCurrentEpoch();
void syncInternalRTC();
void loadRTCState();
void saveRTCState();
void loadConfiguration();
void saveConfiguration();
void processRelaySchedules();
void setupWebServer();
void restartAP();

// HTML Pages
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 16-Channel Relay Smart Switch</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial; margin: 10px; background: #f0f0f0; }
        .container { max-width: 1200px; margin: 0 auto; }
        .header { background: #2196F3; color: white; padding: 15px; border-radius: 5px; margin-bottom: 20px; }
        .relay-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; }
        .relay-card { background: white; padding: 15px; border-radius: 5px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        .relay-title { font-size: 18px; font-weight: bold; margin-bottom: 10px; color: #333; }
        .schedule { margin-bottom: 10px; padding: 10px; background: #f9f9f9; border-radius: 3px; }
        .schedule h4 { margin: 0 0 10px 0; color: #666; }
        .time-input { display: inline-block; margin: 5px; }
        .time-input input { width: 60px; padding: 5px; border: 1px solid #ddd; border-radius: 3px; }
        .time-input label { display: block; font-size: 12px; color: #666; }
        .button { background: #2196F3; color: white; border: none; padding: 8px 15px; border-radius: 3px; cursor: pointer; margin: 2px; }
        .button.save { background: #4CAF50; }
        .button.manual { background: #ff9800; }
        .status { display: inline-block; padding: 5px 10px; border-radius: 3px; font-weight: bold; }
        .status.on { background: #4CAF50; color: white; }
        .status.off { background: #f44336; color: white; }
        .nav { margin-bottom: 20px; }
        .nav a { color: white; text-decoration: none; margin-right: 15px; }
        .notification { position: fixed; top: 20px; right: 20px; padding: 15px; border-radius: 5px; color: white; display: none; }
        .notification.success { background: #4CAF50; }
        .notification.error { background: #f44336; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>ESP32 16-Channel Relay Smart Switch</h1>
            <p>Author: Github.com/xiv3r</p>
            <div class="nav">
                <a href="/">Relays Settings</a>
                <a href="/wifi">WiFi Settings</a>
                <a href="/ntp">Time Settings</a>
                <a href="/ap">AP Settings</a>
                <span style="float: right;" id="currentTime">--:--:--</span>
            </div>
        </div>
        
        <div class="relay-grid" id="relayGrid"></div>
    </div>
    
    <div id="notification" class="notification"></div>
    
    <script>
        let relays = [];
        let currentTime = '';
        let isLoading = false;
        const NUM_SCHEDULES = 8;

        function showNotification(message, isSuccess = true) {
            const notification = document.getElementById('notification');
            notification.textContent = message;
            notification.className = 'notification ' + (isSuccess ? 'success' : 'error');
            notification.style.display = 'block';
            setTimeout(() => {
                notification.style.display = 'none';
            }, 3000);
        }
        
        function loadRelays() {
            if (isLoading) return;
            
            fetch('/api/relays')
                .then(response => response.json())
                .then(data => {
                    relays = data;
                    renderRelays();
                })
                .catch(error => {
                    console.error('Error loading relays:', error);
                    showNotification('Error loading relay data', false);
                });
        }
        
        function renderRelays() {
            const grid = document.getElementById('relayGrid');
            grid.innerHTML = '';
            
            relays.forEach((relay, index) => {
                const card = document.createElement('div');
                card.className = 'relay-card';
                
                let html = `<div class="relay-title">Relay ${index + 1} 
                    <span class="status ${relay.state ? 'on' : 'off'}">${relay.state ? 'ON' : 'OFF'}</span>
                </div>`;
                
                html += `<div style="margin-bottom: 15px;">
                    <button class="button manual" onclick="manualControl(${index}, true)">Turn ON</button>
                    <button class="button manual" onclick="manualControl(${index}, false)">Turn OFF</button>
                    <button class="button" onclick="resetManual(${index})">Auto</button>
                </div>`;
                
                for (let s = 0; s < NUM_SCHEDULES; s++) {
                    const schedule = relay.schedules[s];
                    html += `<div class="schedule">
                        <h4>Schedule ${s + 1} 
                            <input type="checkbox" id="enable_${index}_${s}" ${schedule.enabled ? 'checked' : ''} 
                                onchange="updateScheduleField(${index}, ${s}, 'enabled', this.checked)">
                        </h4>
                        <div class="time-input">
                            <label>Start</label>
                            <input type="number" min="0" max="23" value="${schedule.startHour}" 
                                id="startHour_${index}_${s}"
                                onchange="updateScheduleField(${index}, ${s}, 'startHour', this.value)"> :
                            <input type="number" min="0" max="59" value="${schedule.startMinute}" 
                                id="startMinute_${index}_${s}"
                                onchange="updateScheduleField(${index}, ${s}, 'startMinute', this.value)"> :
                            <input type="number" min="0" max="59" value="${schedule.startSecond}" 
                                id="startSecond_${index}_${s}"
                                onchange="updateScheduleField(${index}, ${s}, 'startSecond', this.value)">
                        </div>
                        <div class="time-input">
                            <label>Stop</label>
                            <input type="number" min="0" max="23" value="${schedule.stopHour}" 
                                id="stopHour_${index}_${s}"
                                onchange="updateScheduleField(${index}, ${s}, 'stopHour', this.value)"> :
                            <input type="number" min="0" max="59" value="${schedule.stopMinute}" 
                                id="stopMinute_${index}_${s}"
                                onchange="updateScheduleField(${index}, ${s}, 'stopMinute', this.value)"> :
                            <input type="number" min="0" max="59" value="${schedule.stopSecond}" 
                                id="stopSecond_${index}_${s}"
                                onchange="updateScheduleField(${index}, ${s}, 'stopSecond', this.value)">
                        </div>
                    </div>`;
                }
                
                html += `<button class="button save" onclick="saveRelay(${index})">Save Relay ${index + 1}</button>`;
                card.innerHTML = html;
                grid.appendChild(card);
            });
        }
        
        function updateScheduleField(relayIndex, scheduleIndex, field, value) {
            if (field === 'enabled') {
                relays[relayIndex].schedules[scheduleIndex][field] = value;
            } else {
                let numValue = parseInt(value);
                if (isNaN(numValue)) return;
                
                if (field.includes('Hour')) {
                    numValue = Math.max(0, Math.min(23, numValue));
                } else if (field.includes('Minute') || field.includes('Second')) {
                    numValue = Math.max(0, Math.min(59, numValue));
                }
                
                relays[relayIndex].schedules[scheduleIndex][field] = numValue;
                
                const inputId = field + '_' + relayIndex + '_' + scheduleIndex;
                document.getElementById(inputId).value = numValue;
            }
        }
        
        function manualControl(relayIndex, state) {
            fetch('/api/relay/manual', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({relay: relayIndex, state: state})
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    showNotification(`Relay ${relayIndex + 1} turned ${state ? 'ON' : 'OFF'}`);
                    loadRelays();
                } else {
                    showNotification('Failed to control relay', false);
                }
            })
            .catch(error => {
                console.error('Error:', error);
                showNotification('Error controlling relay', false);
            });
        }
        
        function resetManual(relayIndex) {
            fetch('/api/relay/reset', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({relay: relayIndex})
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    showNotification(`Relay ${relayIndex + 1} set to Auto mode`);
                    loadRelays();
                } else {
                    showNotification('Failed to reset relay', false);
                }
            })
            .catch(error => {
                console.error('Error:', error);
                showNotification('Error resetting relay', false);
            });
        }
        
        function saveRelay(relayIndex) {
            isLoading = true;
            
            fetch('/api/relay/save', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({
                    relay: relayIndex, 
                    schedules: relays[relayIndex].schedules
                })
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    showNotification(`Relay ${relayIndex + 1} saved successfully!`);
                    setTimeout(() => {
                        loadRelays();
                        isLoading = false;
                    }, 500);
                } else {
                    showNotification(`Failed to save Relay ${relayIndex + 1}`, false);
                    isLoading = false;
                }
            })
            .catch(error => {
                console.error('Error:', error);
                showNotification('Error saving relay settings', false);
                isLoading = false;
            });
        }
        
        function updateTime() {
            fetch('/api/time')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('currentTime').textContent = data.time;
                })
                .catch(error => {
                    console.error('Error fetching time:', error);
                });
        }
        
        loadRelays();
        
        setInterval(() => {
            if (!isLoading) {
                loadRelays();
            }
        }, 60000);
        
        setInterval(updateTime, 1000);
        updateTime();
    </script>
</body>
</html>
)rawliteral";

const char wifi_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>WiFi Settings</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial; margin: 10px; background: #f0f0f0; }
        .container { max-width: 600px; margin: 0 auto; }
        .header { background: #2196F3; color: white; padding: 15px; border-radius: 5px; margin-bottom: 20px; }
        .card { background: white; padding: 20px; border-radius: 5px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        .form-group { margin-bottom: 15px; }
        label { display: block; margin-bottom: 5px; color: #666; }
        input[type="text"], input[type="password"], select { width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 3px; box-sizing: border-box; }
        .button { background: #4CAF50; color: white; border: none; padding: 10px 20px; border-radius: 3px; cursor: pointer; margin-right: 10px; margin-top: 5px; }
        .button.secondary { background: #607D8B; }
        .nav { margin-bottom: 20px; }
        .nav a { color: white; text-decoration: none; margin-right: 15px; }
        .notification { position: fixed; top: 20px; right: 20px; padding: 15px; border-radius: 5px; color: white; display: none; z-index: 1000; }
        .notification.success { background: #4CAF50; }
        .notification.error { background: #f44336; }
        .notification.info { background: #2196F3; }
        #scanResults { margin-top: 10px; max-height: 200px; overflow-y: auto; border: 1px solid #ddd; border-radius: 3px; padding: 5px; display: none; }
        .scan-item { padding: 5px; cursor: pointer; border-bottom: 1px solid #eee; }
        .scan-item:hover { background: #f0f0f0; }
        .scan-item .signal { float: right; color: #666; font-size: 12px; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>WiFi Station Settings</h1>
            <div class="nav">
                <a href="/">Relays Settings</a>
                <a href="/wifi">WiFi Settings</a>
                <a href="/ntp">Time Settings</a>
                <a href="/ap">AP Settings</a>
            </div>
        </div>
        
        <div class="card">
            <form onsubmit="saveWiFi(event)">
                <div class="form-group">
                    <label>SSID:</label>
                    <input type="text" id="ssid" required>
                    <button type="button" class="button secondary" onclick="scanWiFi()" style="margin-top:5px;">Scan for Networks</button>
                    <select id="networkSelect" style="margin-top:5px; display:none;" size="5" onchange="selectNetwork()">
                        <option value="">-- Select a network --</option>
                    </select>
                    <div id="scanningIndicator" style="display:none; margin-top:5px;">Scanning...</div>
                </div>
                <div class="form-group">
                    <label>Password:</label>
                    <input type="password" id="password">
                </div>
                <button type="submit" class="button">Save WiFi Settings</button>
            </form>
        </div>
    </div>
    
    <div id="notification" class="notification"></div>
    
    <script>
        let scanTimeout = null;
        
        function showNotification(message, isSuccess = true, isInfo = false) {
            const notification = document.getElementById('notification');
            notification.textContent = message;
            notification.className = 'notification ' + (isInfo ? 'info' : (isSuccess ? 'success' : 'error'));
            notification.style.display = 'block';
            setTimeout(() => {
                notification.style.display = 'none';
            }, 3000);
        }
        
        function loadWiFi() {
            fetch('/api/wifi')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('ssid').value = data.ssid || '';
                })
                .catch(error => {
                    console.error('Error loading WiFi settings:', error);
                });
        }
        
        function saveWiFi(event) {
            event.preventDefault();
            const data = {
                ssid: document.getElementById('ssid').value,
                password: document.getElementById('password').value
            };
            
            fetch('/api/wifi', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify(data)
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    showNotification('WiFi settings saved! Device will restart...');
                    setTimeout(() => {
                        window.location.href = '/';
                    }, 3000);
                } else {
                    showNotification('Failed to save WiFi settings', false);
                }
            })
            .catch(error => {
                console.error('Error:', error);
                showNotification('Error saving WiFi settings', false);
            });
        }
        
        function scanWiFi() {
            const selectEl = document.getElementById('networkSelect');
            const indicator = document.getElementById('scanningIndicator');
            const scanBtn = event.target;
            
            selectEl.innerHTML = '<option value="">-- Select a network --</option>';
            selectEl.style.display = 'none';
            indicator.style.display = 'block';
            scanBtn.disabled = true;
            
            fetch('/api/wifi/scan')
                .then(response => response.json())
                .then(data => {
                    if (data.status === 'scanning') {
                        showNotification('Scanning for networks...', true, true);
                        pollScanResults();
                    } else if (data.networks) {
                        displayScanResults(data.networks);
                    } else {
                        showNotification('Failed to scan', false);
                        resetScanUI();
                    }
                })
                .catch(error => {
                    console.error('Error:', error);
                    showNotification('Error scanning networks', false);
                    resetScanUI();
                });
        }
        
        function pollScanResults() {
            if (scanTimeout) clearTimeout(scanTimeout);
            
            fetch('/api/wifi/scan')
                .then(response => response.json())
                .then(data => {
                    if (data.status === 'scanning') {
                        scanTimeout = setTimeout(pollScanResults, 1000);
                    } else if (data.networks) {
                        displayScanResults(data.networks);
                    } else {
                        showNotification('Scan failed', false);
                        resetScanUI();
                    }
                })
                .catch(error => {
                    console.error('Error polling scan:', error);
                    showNotification('Error during scan', false);
                    resetScanUI();
                });
        }
        
        function displayScanResults(networks) {
            const selectEl = document.getElementById('networkSelect');
            const indicator = document.getElementById('scanningIndicator');
            const scanBtn = document.querySelector('button[onclick="scanWiFi()"]');
            
            selectEl.innerHTML = '<option value="">-- Select a network --</option>';
            
            if (networks.length === 0) {
                selectEl.innerHTML += '<option value="" disabled>No networks found</option>';
            } else {
                networks.sort((a, b) => b.rssi - a.rssi);
                
                networks.forEach(net => {
                    const option = document.createElement('option');
                    option.value = net.ssid;
                    option.textContent = net.display || `${net.ssid} (${net.rssi}dBm) ${net.encryption === 'Open' ? '(Open)' : ''}`;
                    option.dataset.encryption = net.encryption;
                    selectEl.appendChild(option);
                });
            }
            
            selectEl.style.display = 'block';
            indicator.style.display = 'none';
            if (scanBtn) scanBtn.disabled = false;
            
            showNotification(`Found ${networks.length} network(s)`, true, true);
        }
        
        function resetScanUI() {
            document.getElementById('networkSelect').style.display = 'none';
            document.getElementById('scanningIndicator').style.display = 'none';
            const scanBtn = document.querySelector('button[onclick="scanWiFi()"]');
            if (scanBtn) scanBtn.disabled = false;
        }
        
        function selectNetwork() {
            const selectEl = document.getElementById('networkSelect');
            const selectedOption = selectEl.options[selectEl.selectedIndex];
            if (selectedOption.value) {
                document.getElementById('ssid').value = selectedOption.value;
                selectEl.style.display = 'none';
            }
        }
        
        loadWiFi();
    </script>
</body>
</html>
)rawliteral";

const char ntp_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Time Settings</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial; margin: 10px; background: #f0f0f0; }
        .container { max-width: 600px; margin: 0 auto; }
        .header { background: #2196F3; color: white; padding: 15px; border-radius: 5px; margin-bottom: 20px; }
        .card { background: white; padding: 20px; border-radius: 5px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        .form-group { margin-bottom: 15px; }
        label { display: block; margin-bottom: 5px; color: #666; }
        input[type="text"], input[type="number"] { width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 3px; box-sizing: border-box; }
        .button { background: #4CAF50; color: white; border: none; padding: 10px 20px; border-radius: 3px; cursor: pointer; margin-right: 10px; }
        .nav { margin-bottom: 20px; }
        .nav a { color: white; text-decoration: none; margin-right: 15px; }
        .notification { position: fixed; top: 20px; right: 20px; padding: 15px; border-radius: 5px; color: white; display: none; }
        .notification.success { background: #4CAF50; }
        .notification.error { background: #f44336; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>Time Settings</h1>
            <div class="nav">
                <a href="/">Relays Settings</a>
                <a href="/wifi">WiFi Settings</a>
                <a href="/ntp">Time Settings</a>
                <a href="/ap">AP Settings</a>
            </div>
        </div>
        
        <div class="card">
            <form onsubmit="saveNTP(event)">
                <div class="form-group">
                    <label>NTP Server:</label>
                    <input type="text" id="ntpServer" value="ph.pool.ntp.org" required>
                </div>
                <div class="form-group">
                    <label>GMT Offset (seconds):</label>
                    <input type="number" id="gmtOffset" value="28800" required>
                </div>
                <div class="form-group">
                    <label>Daylight Offset (seconds):</label>
                    <input type="number" id="daylightOffset" value="0">
                </div>
                <button type="submit" class="button">Save NTP Settings</button>
                <button type="button" class="button" onclick="syncNTP()">Sync Time Now</button>
            </form>
        </div>
    </div>
    
    <div id="notification" class="notification"></div>
    
    <script>
        function showNotification(message, isSuccess = true) {
            const notification = document.getElementById('notification');
            notification.textContent = message;
            notification.className = 'notification ' + (isSuccess ? 'success' : 'error');
            notification.style.display = 'block';
            setTimeout(() => {
                notification.style.display = 'none';
            }, 3000);
        }
        
        function loadNTP() {
            fetch('/api/ntp')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('ntpServer').value = data.ntpServer || 'ph.pool.ntp.org';
                    document.getElementById('gmtOffset').value = data.gmtOffset || 28800;
                    document.getElementById('daylightOffset').value = data.daylightOffset || 0;
                })
                .catch(error => {
                    console.error('Error loading NTP settings:', error);
                });
        }
        
        function saveNTP(event) {
            event.preventDefault();
            const data = {
                ntpServer: document.getElementById('ntpServer').value,
                gmtOffset: parseInt(document.getElementById('gmtOffset').value),
                daylightOffset: parseInt(document.getElementById('daylightOffset').value)
            };
            
            fetch('/api/ntp', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify(data)
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    showNotification('NTP settings saved successfully!');
                } else {
                    showNotification('Failed to save NTP settings', false);
                }
            })
            .catch(error => {
                console.error('Error:', error);
                showNotification('Error saving NTP settings', false);
            });
        }
        
        function syncNTP() {
            const button = event.target;
            button.disabled = true;
            button.textContent = 'Syncing...';
            
            fetch('/api/ntp/sync', {method: 'POST'})
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        showNotification('Time synchronized successfully!');
                    } else {
                        showNotification('Failed to sync time. Check WiFi connection.', false);
                    }
                    button.disabled = false;
                    button.textContent = 'Sync Time Now';
                })
                .catch(error => {
                    console.error('Error:', error);
                    showNotification('Error syncing time', false);
                    button.disabled = false;
                    button.textContent = 'Sync Time Now';
                });
        }
        
        loadNTP();
    </script>
</body>
</html>
)rawliteral";

const char ap_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>AP Settings</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial; margin: 10px; background: #f0f0f0; }
        .container { max-width: 600px; margin: 0 auto; }
        .header { background: #2196F3; color: white; padding: 15px; border-radius: 5px; margin-bottom: 20px; }
        .card { background: white; padding: 20px; border-radius: 5px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        .form-group { margin-bottom: 15px; }
        label { display: block; margin-bottom: 5px; color: #666; }
        input[type="text"], input[type="password"] { width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 3px; box-sizing: border-box; }
        .button { background: #4CAF50; color: white; border: none; padding: 10px 20px; border-radius: 3px; cursor: pointer; }
        .nav { margin-bottom: 20px; }
        .nav a { color: white; text-decoration: none; margin-right: 15px; }
        .notification { position: fixed; top: 20px; right: 20px; padding: 15px; border-radius: 5px; color: white; display: none; }
        .notification.success { background: #4CAF50; }
        .notification.error { background: #f44336; }
        .note { background: #fff3cd; border: 1px solid #ffeaa7; padding: 10px; border-radius: 3px; margin-bottom: 15px; color: #856404; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>Access Point Settings</h1>
            <div class="nav">
                <a href="/">Relays Settings</a>
                <a href="/wifi">WiFi Settings</a>
                <a href="/ntp">Time Settings</a>
                <a href="/ap">AP Settings</a>
            </div>
        </div>
        
        <div class="card">
            <div class="note">
                <strong>Note:</strong> Changing AP settings will disconnect all currently connected devices. 
                You will need to reconnect to the new AP SSID after saving.
            </div>
            <form onsubmit="saveAP(event)">
                <div class="form-group">
                    <label>AP SSID (Network Name):</label>
                    <input type="text" id="ap_ssid" maxlength="31" required>
                </div>
                <div class="form-group">
                    <label>AP Password (min 8 characters or leave blank for open network):</label>
                    <input type="password" id="ap_password" minlength="8">
                </div>
                <button type="submit" class="button">Save AP Settings</button>
            </form>
        </div>
    </div>
    
    <div id="notification" class="notification"></div>
    
    <script>
        function showNotification(message, isSuccess = true) {
            const notification = document.getElementById('notification');
            notification.textContent = message;
            notification.className = 'notification ' + (isSuccess ? 'success' : 'error');
            notification.style.display = 'block';
            setTimeout(() => {
                notification.style.display = 'none';
            }, 3000);
        }
        
        function loadAP() {
            fetch('/api/ap')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('ap_ssid').value = data.ap_ssid || 'ESP32_16CH_Timer_Switch';
                    document.getElementById('ap_password').value = data.ap_password || '';
                })
                .catch(error => {
                    console.error('Error loading AP settings:', error);
                });
        }
        
        function saveAP(event) {
            event.preventDefault();
            const password = document.getElementById('ap_password').value;
            if (password.length > 0 && password.length < 8) {
                showNotification('Password must be at least 8 characters or blank', false);
                return;
            }
            
            const data = {
                ap_ssid: document.getElementById('ap_ssid').value,
                ap_password: password
            };
            
            fetch('/api/ap', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify(data)
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    showNotification('AP settings saved! AP will restart. Reconnect to the new network.');
                    setTimeout(() => {
                        window.location.reload();
                    }, 3000);
                } else {
                    showNotification('Failed to save AP settings', false);
                }
            })
            .catch(error => {
                console.error('Error:', error);
                showNotification('Error saving AP settings', false);
            });
        }
        
        loadAP();
    </script>
</body>
</html>
)rawliteral";

// RTC FUNCTIONS 
time_t getCurrentEpoch() {
    if (wifiConnected) {
        time_t ntpTime = timeClient.getEpochTime();
        if (ntpTime > 100000) {
            return ntpTime;
        }
    }
    if (rtcInitialized) {
        unsigned long now = millis();
        unsigned long elapsed;
        if (now >= internalMillisAtLastNTPSync) {
            elapsed = now - internalMillisAtLastNTPSync;
        } else {
            elapsed = (0xFFFFFFFF - internalMillisAtLastNTPSync) + now + 1;
        }
        time_t rtcEpoch = internalEpoch + (time_t)((elapsed / 1000.0) * driftCompensation);
        return rtcEpoch;
    }
    return 0;
}

void syncInternalRTC() {
    if (wifiConnected) {
        time_t ntpEpoch = timeClient.getEpochTime();
        if (ntpEpoch > 100000) {
            unsigned long now = millis();
            if (rtcInitialized) {
                unsigned long elapsed;
                if (now >= internalMillisAtLastNTPSync) {
                    elapsed = now - internalMillisAtLastNTPSync;
                } else {
                    elapsed = (0xFFFFFFFF - internalMillisAtLastNTPSync) + now + 1;
                }
                if (elapsed > 10000) {
                    float actualDrift = (float)(ntpEpoch - internalEpoch) / (elapsed / 1000.0);
                    driftCompensation = driftCompensation * 0.95 + actualDrift * 0.05;
                }
            }
            internalEpoch = ntpEpoch;
            internalMillisAtLastNTPSync = now;
            rtcInitialized = true;
            saveRTCState();
            Serial.printf("RTC synced: epoch=%lu, drift=%.6f\n", internalEpoch, driftCompensation);
        }
    }
}

void saveRTCState() {
    sysConfig.last_rtc_epoch = internalEpoch;
    sysConfig.rtc_drift = driftCompensation;
    EEPROM.put(0, sysConfig);
    EEPROM.commit();
}

void loadRTCState() {
    if (sysConfig.last_rtc_epoch > 100000) {
        internalEpoch = sysConfig.last_rtc_epoch;
        driftCompensation = sysConfig.rtc_drift;
        internalMillisAtLastNTPSync = millis();
        rtcInitialized = true;
        Serial.printf("RTC state loaded from EEPROM: %lu\n", internalEpoch);
    }
}

void restartAP() {
    WiFi.softAPdisconnect(true);
    delay(500);
    if (strlen(sysConfig.ap_password) > 0) {
        WiFi.softAP(sysConfig.ap_ssid, sysConfig.ap_password);
    } else {
        WiFi.softAP(sysConfig.ap_ssid);
    }
    Serial.println("AP restarted with new settings");
    Serial.print("AP SSID: ");
    Serial.println(sysConfig.ap_ssid);
}

void setup() {
    Serial.begin(115200);
    
    EEPROM.begin(EEPROM_SIZE);
    
    for (int i = 0; i < NUM_RELAYS; i++) {
        pinMode(relayPins[i], OUTPUT);
        digitalWrite(relayPins[i], relayActiveLow ? HIGH : LOW);
    }
    
    for (int i = 0; i < NUM_RELAYS; i++) {
        for (int s = 0; s < 8; s++) {
            relayConfigs[i].schedule.enabled[s] = false;
        }
        relayConfigs[i].manualOverride = false;
        relayConfigs[i].manualState = false;
    }
    
    loadConfiguration();
    loadRTCState();
    
    if (strlen(sysConfig.sta_ssid) > 0) {
        WiFi.begin(sysConfig.sta_ssid, sysConfig.sta_password);
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            wifiConnected = true;
            Serial.println("\nConnected to WiFi");
            Serial.println(WiFi.localIP());
            
            timeClient.setPoolServerName(sysConfig.ntp_server);
            timeClient.setTimeOffset(sysConfig.gmt_offset);
            timeClient.begin();
            if (timeClient.update()) {
                Serial.println("NTP time synchronized");
                syncInternalRTC();
            }
        }
    }
    
    WiFi.mode(WIFI_AP_STA);
    if (strlen(sysConfig.ap_password) > 0) {
        WiFi.softAP(sysConfig.ap_ssid, sysConfig.ap_password);
    } else {
        WiFi.softAP(sysConfig.ap_ssid);
    }
    
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    
    setupWebServer();
    
    Serial.println("AP IP address: " + WiFi.softAPIP().toString());
}

void loop() {
    dnsServer.processNextRequest();
    server.handleClient();
    
    if (wifiConnected && millis() - lastNTPSync > NTP_SYNC_INTERVAL) {
        if (timeClient.update()) {
            lastNTPSync = millis();
            syncInternalRTC();
            Serial.println("NTP time synchronized with internal RTC");
        }
    }
    
    processRelaySchedules();
    
    delay(10);
}

void setupWebServer() {
    server.on("/", HTTP_GET, []() {
        server.send_P(200, "text/html", index_html);
    });
    
    server.on("/wifi", HTTP_GET, []() {
        server.send_P(200, "text/html", wifi_html);
    });
    
    server.on("/ntp", HTTP_GET, []() {
        server.send_P(200, "text/html", ntp_html);
    });
    
    server.on("/ap", HTTP_GET, []() {
        server.send_P(200, "text/html", ap_html);
    });
    
    server.on("/api/relays", HTTP_GET, handleGetRelays);
    server.on("/api/relay/manual", HTTP_POST, handleManualControl);
    server.on("/api/relay/reset", HTTP_POST, handleResetManual);
    server.on("/api/relay/save", HTTP_POST, handleSaveRelay);
    server.on("/api/time", HTTP_GET, handleGetTime);
    server.on("/api/wifi", HTTP_GET, handleGetWiFi);
    server.on("/api/wifi", HTTP_POST, handleSaveWiFi);
    server.on("/api/wifi/scan", HTTP_GET, handleWiFiScan);
    server.on("/api/ntp", HTTP_GET, handleGetNTP);
    server.on("/api/ntp", HTTP_POST, handleSaveNTP);
    server.on("/api/ntp/sync", HTTP_POST, handleSyncNTP);
    server.on("/api/ap", HTTP_GET, handleGetAP);
    server.on("/api/ap", HTTP_POST, handleSaveAP);
    
    server.onNotFound([]() {
        if (server.hostHeader() != WiFi.softAPIP().toString()) {
            server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
            server.send(302, "text/plain", "");
        } else {
            server.send(404, "text/plain", "Not Found");
        }
    });
    
    server.begin();
}

void processRelaySchedules() {
    unsigned long currentEpoch = getCurrentEpoch();
    if (currentEpoch < 100000) return;
    
    struct tm *timeinfo = localtime((time_t*)&currentEpoch);
    
    for (int i = 0; i < NUM_RELAYS; i++) {
        if (relayConfigs[i].manualOverride) {
            digitalWrite(relayPins[i], 
                relayActiveLow ? !relayConfigs[i].manualState : relayConfigs[i].manualState);
            continue;
        }
        
        bool shouldBeOn = false;
        
        for (int s = 0; s < 8; s++) {
            if (!relayConfigs[i].schedule.enabled[s]) continue;
            
            int startSeconds = relayConfigs[i].schedule.startHour[s] * 3600 + 
                             relayConfigs[i].schedule.startMinute[s] * 60 + 
                             relayConfigs[i].schedule.startSecond[s];
            int stopSeconds = relayConfigs[i].schedule.stopHour[s] * 3600 + 
                            relayConfigs[i].schedule.stopMinute[s] * 60 + 
                            relayConfigs[i].schedule.stopSecond[s];
            int currentSeconds = timeinfo->tm_hour * 3600 + 
                               timeinfo->tm_min * 60 + 
                               timeinfo->tm_sec;
            
            if (startSeconds < stopSeconds) {
                if (currentSeconds >= startSeconds && currentSeconds < stopSeconds) {
                    shouldBeOn = true;
                    break;
                }
            } else {
                if (currentSeconds >= startSeconds || currentSeconds < stopSeconds) {
                    shouldBeOn = true;
                    break;
                }
            }
        }
        
        digitalWrite(relayPins[i], relayActiveLow ? !shouldBeOn : shouldBeOn);
    }
}

void loadConfiguration() {
    EEPROM.get(0, sysConfig);
    
    if (sysConfig.magic != EEPROM_MAGIC || sysConfig.version != EEPROM_VERSION) {
        if (sysConfig.magic == EEPROM_MAGIC && sysConfig.version == 1) {
            Serial.println("Migrating EEPROM from version 1 to 2 (4->8 schedules)");
            
            struct OldRelayConfig {
                struct {
                    uint8_t startHour[4];
                    uint8_t startMinute[4];
                    uint8_t startSecond[4];
                    uint8_t stopHour[4];
                    uint8_t stopMinute[4];
                    uint8_t stopSecond[4];
                    bool enabled[4];
                } schedule;
                bool manualOverride;
                bool manualState;
            } oldRelayConfig;
            
            int addr = sizeof(SystemConfig);
            for (int i = 0; i < NUM_RELAYS; i++) {
                EEPROM.get(addr, oldRelayConfig);
                addr += sizeof(OldRelayConfig);
                
                for (int s = 0; s < 4; s++) {
                    relayConfigs[i].schedule.startHour[s] = oldRelayConfig.schedule.startHour[s];
                    relayConfigs[i].schedule.startMinute[s] = oldRelayConfig.schedule.startMinute[s];
                    relayConfigs[i].schedule.startSecond[s] = oldRelayConfig.schedule.startSecond[s];
                    relayConfigs[i].schedule.stopHour[s] = oldRelayConfig.schedule.stopHour[s];
                    relayConfigs[i].schedule.stopMinute[s] = oldRelayConfig.schedule.stopMinute[s];
                    relayConfigs[i].schedule.stopSecond[s] = oldRelayConfig.schedule.stopSecond[s];
                    relayConfigs[i].schedule.enabled[s] = oldRelayConfig.schedule.enabled[s];
                }
                for (int s = 4; s < 8; s++) {
                    relayConfigs[i].schedule.startHour[s] = 0;
                    relayConfigs[i].schedule.startMinute[s] = 0;
                    relayConfigs[i].schedule.startSecond[s] = 0;
                    relayConfigs[i].schedule.stopHour[s] = 0;
                    relayConfigs[i].schedule.stopMinute[s] = 0;
                    relayConfigs[i].schedule.stopSecond[s] = 0;
                    relayConfigs[i].schedule.enabled[s] = false;
                }
                relayConfigs[i].manualOverride = oldRelayConfig.manualOverride;
                relayConfigs[i].manualState = oldRelayConfig.manualState;
            }
            
            sysConfig.version = EEPROM_VERSION;
            saveConfiguration();
            Serial.println("Migration completed.");
        } else {
            Serial.println("Initializing default configuration (version 2)");
            sysConfig.magic = EEPROM_MAGIC;
            sysConfig.version = EEPROM_VERSION;
            strcpy(sysConfig.sta_ssid, "");
            strcpy(sysConfig.sta_password, "");
            strcpy(sysConfig.ap_ssid, "ESP32_16CH_Timer_Switch");
            strcpy(sysConfig.ap_password, "ESP32-admin");
            strcpy(sysConfig.ntp_server, "ph.pool.ntp.org");
            sysConfig.gmt_offset = 28800;
            sysConfig.daylight_offset = 0;
            sysConfig.last_rtc_epoch = 0;
            sysConfig.rtc_drift = 1.0;
            saveConfiguration();
        }
    } else {
        int addr = sizeof(SystemConfig);
        for (int i = 0; i < NUM_RELAYS; i++) {
            EEPROM.get(addr, relayConfigs[i]);
            addr += sizeof(RelayConfig);
        }
        Serial.println("Configuration loaded (version 2)");
    }
    
    strcpy(ap_ssid, sysConfig.ap_ssid);
    strcpy(ap_password, sysConfig.ap_password);
}

void saveConfiguration() {
    EEPROM.put(0, sysConfig);
    
    int addr = sizeof(SystemConfig);
    for (int i = 0; i < NUM_RELAYS; i++) {
        EEPROM.put(addr, relayConfigs[i]);
        addr += sizeof(RelayConfig);
    }
    
    if (EEPROM.commit()) {
        Serial.println("Configuration saved successfully");
    } else {
        Serial.println("ERROR: Failed to save configuration");
    }
}

// API Handlers
void handleGetRelays() {
    DynamicJsonDocument doc(8192);
    JsonArray array = doc.to<JsonArray>();
    
    for (int i = 0; i < NUM_RELAYS; i++) {
        JsonObject relay = array.createNestedObject();
        relay["state"] = digitalRead(relayPins[i]) == (relayActiveLow ? LOW : HIGH);
        
        JsonArray schedules = relay.createNestedArray("schedules");
        for (int s = 0; s < 8; s++) {
            JsonObject schedule = schedules.createNestedObject();
            schedule["startHour"] = relayConfigs[i].schedule.startHour[s];
            schedule["startMinute"] = relayConfigs[i].schedule.startMinute[s];
            schedule["startSecond"] = relayConfigs[i].schedule.startSecond[s];
            schedule["stopHour"] = relayConfigs[i].schedule.stopHour[s];
            schedule["stopMinute"] = relayConfigs[i].schedule.stopMinute[s];
            schedule["stopSecond"] = relayConfigs[i].schedule.stopSecond[s];
            schedule["enabled"] = relayConfigs[i].schedule.enabled[s];
        }
    }
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleManualControl() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"No data\"}");
        return;
    }
    
    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (error) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
    }
    
    int relay = doc["relay"];
    bool state = doc["state"];
    
    if (relay >= 0 && relay < NUM_RELAYS) {
        relayConfigs[relay].manualOverride = true;
        relayConfigs[relay].manualState = state;
        saveConfiguration();
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
    
    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (error) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
    }
    
    int relay = doc["relay"];
    
    if (relay >= 0 && relay < NUM_RELAYS) {
        relayConfigs[relay].manualOverride = false;
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
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (error) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
    }
    
    int relay = doc["relay"];
    JsonArray schedules = doc["schedules"].as<JsonArray>();
    
    if (relay >= 0 && relay < NUM_RELAYS && schedules.size() >= 8) {
        for (int s = 0; s < 8; s++) {
            relayConfigs[relay].schedule.startHour[s] = schedules[s]["startHour"];
            relayConfigs[relay].schedule.startMinute[s] = schedules[s]["startMinute"];
            relayConfigs[relay].schedule.startSecond[s] = schedules[s]["startSecond"];
            relayConfigs[relay].schedule.stopHour[s] = schedules[s]["stopHour"];
            relayConfigs[relay].schedule.stopMinute[s] = schedules[s]["stopMinute"];
            relayConfigs[relay].schedule.stopSecond[s] = schedules[s]["stopSecond"];
            relayConfigs[relay].schedule.enabled[s] = schedules[s]["enabled"];
        }
        saveConfiguration();
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid relay or schedules\"}");
    }
}

void handleGetTime() {
    DynamicJsonDocument doc(128);
    char timeStr[9];
    
    time_t currentEpoch = getCurrentEpoch();
    
    if (currentEpoch > 100000) {
        struct tm *timeinfo = localtime(&currentEpoch);
        sprintf(timeStr, "%02d:%02d:%02d", 
                timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    } else {
        strcpy(timeStr, "--:--:--");
    }
    
    doc["time"] = timeStr;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleGetWiFi() {
    DynamicJsonDocument doc(256);
    doc["ssid"] = sysConfig.sta_ssid;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSaveWiFi() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"No data\"}");
        return;
    }
    
    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (error) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
    }
    
    const char* ssid = doc["ssid"];
    const char* password = doc["password"];
    
    if (ssid && strlen(ssid) < 32) {
        strcpy(sysConfig.sta_ssid, ssid);
        if (password) {
            strcpy(sysConfig.sta_password, password);
        } else {
            sysConfig.sta_password[0] = '\0';
        }
        saveConfiguration();
        
        server.send(200, "application/json", "{\"success\":true}");
        delay(1000);
        ESP.restart();
    } else {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid SSID\"}");
    }
}

void handleWiFiScan() {
    int n = WiFi.scanComplete();
    
    if (n == WIFI_SCAN_FAILED) {
        WiFi.scanNetworks(true);
        server.send(202, "application/json", "{\"status\":\"scanning\"}");
        return;
    }
    
    if (n == WIFI_SCAN_RUNNING) {
        server.send(202, "application/json", "{\"status\":\"scanning\"}");
        return;
    }
    
    DynamicJsonDocument doc(4096);
    JsonArray networks = doc.createNestedArray("networks");
    
    for (int i = 0; i < n; ++i) {
        JsonObject net = networks.createNestedObject();
        net["ssid"] = WiFi.SSID(i);
        net["rssi"] = WiFi.RSSI(i);
        net["encryption"] = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Open" : "Secured";
        char display[64];
        snprintf(display, sizeof(display), "%s (%ddBm) %s", 
                 WiFi.SSID(i).c_str(), WiFi.RSSI(i), 
                 (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Open" : "");
        net["display"] = display;
    }
    
    WiFi.scanDelete();
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleGetNTP() {
    DynamicJsonDocument doc(256);
    doc["ntpServer"] = sysConfig.ntp_server;
    doc["gmtOffset"] = sysConfig.gmt_offset;
    doc["daylightOffset"] = sysConfig.daylight_offset;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSaveNTP() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"No data\"}");
        return;
    }
    
    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (error) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
    }
    
    const char* ntp_server = doc["ntpServer"];
    
    if (ntp_server && strlen(ntp_server) < 48) {
        strcpy(sysConfig.ntp_server, ntp_server);
        sysConfig.gmt_offset = doc["gmtOffset"];
        sysConfig.daylight_offset = doc["daylightOffset"];
        saveConfiguration();
        
        if (wifiConnected) {
            timeClient.setPoolServerName(sysConfig.ntp_server);
            timeClient.setTimeOffset(sysConfig.gmt_offset);
            timeClient.update();
        }
        
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid NTP server\"}");
    }
}

void handleSyncNTP() {
    if (wifiConnected) {
        if (timeClient.update()) {
            syncInternalRTC();
            server.send(200, "application/json", "{\"success\":true}");
        } else {
            server.send(400, "application/json", "{\"success\":false,\"error\":\"Failed to sync\"}");
        }
    } else {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"WiFi not connected\"}");
    }
}

void handleGetAP() {
    DynamicJsonDocument doc(256);
    doc["ap_ssid"] = sysConfig.ap_ssid;
    doc["ap_password"] = sysConfig.ap_password;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSaveAP() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"No data\"}");
        return;
    }
    
    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (error) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
    }
    
    const char* ap_ssid_new = doc["ap_ssid"];
    const char* ap_password_new = doc["ap_password"];
    
    if (ap_ssid_new && strlen(ap_ssid_new) > 0 && strlen(ap_ssid_new) < 32) {
        strcpy(sysConfig.ap_ssid, ap_ssid_new);
        strcpy(ap_ssid, ap_ssid_new);
        
        if (ap_password_new && strlen(ap_password_new) > 0) {
            strcpy(sysConfig.ap_password, ap_password_new);
            strcpy(ap_password, ap_password_new);
        } else {
            sysConfig.ap_password[0] = '\0';
            ap_password[0] = '\0';
        }
        
        saveConfiguration();
        
        server.send(200, "application/json", "{\"success\":true}");
        
        delay(100);
        restartAP();
    } else {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid AP SSID\"}");
    }
}