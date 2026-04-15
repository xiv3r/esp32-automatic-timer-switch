#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <sys/time.h>   // Added for settimeofday()
#include <time.h>       // Added for time() and localtime()
#include <stdint.h>     // For explicit integer types

// EEPROM Configuration
#define EEPROM_SIZE 2048
#define EEPROM_MAGIC 0x1234
#define EEPROM_VERSION 2    // Incremented for new time storage field

// WiFi AP Configuration - Changed from const to variables
char ap_ssid[32] = "ESP32_8CH_Smart_Switch";
char ap_password[64] = "ESP32-admin";

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

// Relay Configuration
#define NUM_RELAYS 8
const int relayPins[NUM_RELAYS] = {32, 33, 25, 26, 27, 14, 16, 17}; // Adjust pins as needed
const bool relayActiveLow = true; // Set true for active LOW relays

// Timer Schedule Structure
struct TimerSchedule {
  uint8_t startHour[4];
  uint8_t startMinute[4];
  uint8_t startSecond[4];
  uint8_t stopHour[4];
  uint8_t stopMinute[4];
  uint8_t stopSecond[4];
  bool enabled[4];
};

struct RelayConfig {
  TimerSchedule schedule;
  bool manualOverride;
  bool manualState;
} relayConfigs[NUM_RELAYS];

// System Configuration - Extended to include AP settings and RTC persistence
struct SystemConfig {
  uint16_t magic;
  uint8_t version;
  char sta_ssid[32];
  char sta_password[64];
  char ap_ssid[32];
  char ap_password[64];
  char ntp_server[48];
  long gmt_offset;
  int daylight_offset;
  time_t last_ntp_sync_epoch;   // NEW: Last successful NTP epoch saved to EEPROM
};

SystemConfig sysConfig;
bool wifiConnected = false;
unsigned long lastNTPSync = 0;
const unsigned long NTP_SYNC_INTERVAL = 1000; // 1 ms

// HTML Pages (unchanged)
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 8-Channel Relay Timer</title>
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
            <h1>ESP32 8-Channel Relay Timer Switch</h1>
            <div class="nav">
                <a href="/">Relay Settings</a>
                <a href="/wifi">WiFi Settings</a>
                <a href="/ap">AP Settings</a>
                <a href="/ntp">NTP/RTC Settings</a>
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
                
                // Manual control buttons
                html += `<div style="margin-bottom: 15px;">
                    <button class="button manual" onclick="manualControl(${index}, true)">Turn ON</button>
                    <button class="button manual" onclick="manualControl(${index}, false)">Turn OFF</button>
                    <button class="button" onclick="resetManual(${index})">Auto</button>
                </div>`;
                
                // Schedules
                for (let s = 0; s < 4; s++) {
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
                
                // Validate based on field type
                if (field.includes('Hour')) {
                    numValue = Math.max(0, Math.min(23, numValue));
                } else if (field.includes('Minute') || field.includes('Second')) {
                    numValue = Math.max(0, Math.min(59, numValue));
                }
                
                relays[relayIndex].schedules[scheduleIndex][field] = numValue;
                
                // Update input field with validated value
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
                    // Reload to get updated state
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
        
        // Load data on page load
        loadRelays();
        
        // Update relays periodically but not too frequently to avoid interrupting user input
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
        input[type="text"], input[type="password"] { width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 3px; box-sizing: border-box; }
        .button { background: #4CAF50; color: white; border: none; padding: 10px 20px; border-radius: 3px; cursor: pointer; }
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
            <h1>WiFi Station Settings</h1>
            <div class="nav">
                <a href="/">Relay Settings</a>
                <a href="/wifi">WiFi Settings</a>
                <a href="/ap">AP Settings</a>
                <a href="/ntp">NTP/RTC Settings</a>
            </div>
        </div>
        
        <div class="card">
            <form onsubmit="saveWiFi(event)">
                <div class="form-group">
                    <label>SSID:</label>
                    <input type="text" id="ssid" required>
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
        function showNotification(message, isSuccess = true) {
            const notification = document.getElementById('notification');
            notification.textContent = message;
            notification.className = 'notification ' + (isSuccess ? 'success' : 'error');
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
        
        loadWiFi();
    </script>
</body>
</html>
)rawliteral";

const char ap_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Access Point Settings</title>
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
        .info { background: #e3f2fd; padding: 15px; border-radius: 3px; margin-bottom: 20px; color: #1976D2; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>Access Point Settings</h1>
            <div class="nav">
                <a href="/">Relay Settings</a>
                <a href="/wifi">WiFi Settings</a>
                <a href="/ap">AP Settings</a>
                <a href="/ntp">NTP/RTC Settings</a>
            </div>
        </div>
        
        <div class="card">
            <div class="info">
                <strong>Note:</strong> Changing AP settings will restart the device. 
                You'll need to reconnect using the new credentials.
            </div>
            <form onsubmit="saveAP(event)">
                <div class="form-group">
                    <label>AP SSID:</label>
                    <input type="text" id="ap_ssid" required minlength="1" maxlength="31">
                </div>
                <div class="form-group">
                    <label>AP Password:</label>
                    <input type="password" id="ap_password" minlength="8" maxlength="63">
                    <small style="color: #666;">Minimum 8 characters, leave blank for open network</small>
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
                    document.getElementById('ap_ssid').value = data.ap_ssid || 'ESP32_8CH_Smart_Switch';
                })
                .catch(error => {
                    console.error('Error loading AP settings:', error);
                });
        }
        
        function saveAP(event) {
            event.preventDefault();
            const password = document.getElementById('ap_password').value;
            
            if (password && password.length < 8) {
                showNotification('Password must be at least 8 characters', false);
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
                    showNotification('AP settings saved! Device will restart...');
                    setTimeout(() => {
                        window.location.href = '/';
                    }, 3000);
                } else {
                    showNotification(data.error || 'Failed to save AP settings', false);
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

const char ntp_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>NTP & RTC Settings</title>
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
            <h1>NTP & RTC Settings</h1>
            <div class="nav">
                <a href="/">Relay Settings</a>
                <a href="/wifi">WiFi Settings</a>
                <a href="/ap">AP Settings</a>
                <a href="/ntp">NTP/RTC Settings</a>
            </div>
        </div>
        
        <div class="card">
            <form onsubmit="saveNTP(event)">
                <div class="form-group">
                    <label>NTP Server:</label>
                    <input type="text" id="ntpServer" value="pool.ntp.org" required>
                </div>
                <div class="form-group">
                    <label>GMT Offset (seconds):</label>
                    <input type="number" id="gmtOffset" required>
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
                    document.getElementById('ntpServer').value = data.ntpServer || 'pool.ntp.org';
                    document.getElementById('gmtOffset').value = data.gmtOffset || 19800;
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

// Function declarations
void handleGetRelays();
void handleManualControl();
void handleResetManual();
void handleSaveRelay();
void handleGetTime();
void handleGetWiFi();
void handleSaveWiFi();
void handleGetAP();
void handleSaveAP();
void handleGetNTP();
void handleSaveNTP();
void handleSyncNTP();

void setup() {
  Serial.begin(115200);
  
  // Initialize EEPROM
  if (!EEPROM.begin(EEPROM_SIZE)) {
    Serial.println("Failed to initialize EEPROM");
  }
  
  // Initialize relay pins
  for (int i = 0; i < NUM_RELAYS; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], relayActiveLow ? HIGH : LOW);
  }
  
  // Initialize relay configs with defaults
  for (int i = 0; i < NUM_RELAYS; i++) {
    for (int s = 0; s < 4; s++) {
      relayConfigs[i].schedule.enabled[s] = false;
    }
    relayConfigs[i].manualOverride = false;
    relayConfigs[i].manualState = false;
  }
  
  // Load configuration
  loadConfiguration();
  
  // Setup WiFi
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
      
      // Initialize NTP and set system time
      timeClient.setPoolServerName(sysConfig.ntp_server);
      timeClient.setTimeOffset(sysConfig.gmt_offset);
      timeClient.begin();
      if (timeClient.update()) {
        time_t epoch = timeClient.getEpochTime();
        struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
        settimeofday(&tv, nullptr);
        // Save this epoch to EEPROM for future cold boots
        sysConfig.last_ntp_sync_epoch = epoch;
        saveConfiguration();
        Serial.println("System time set from NTP and saved to EEPROM");
      }
    }
  }
  
  // If WiFi failed and we have a saved epoch, restore it as fallback
  if (!wifiConnected && sysConfig.last_ntp_sync_epoch > 1609459200) { // after 2021
    struct timeval tv = { .tv_sec = sysConfig.last_ntp_sync_epoch, .tv_usec = 0 };
    settimeofday(&tv, nullptr);
    Serial.println("Restored time from EEPROM (last NTP sync)");
  }
  
  // Always start AP mode for configuration
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ap_ssid, ap_password);
  
  // Setup DNS server for captive portal
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  
  // Setup web server routes
  setupWebServer();
  
  Serial.println("AP IP address: " + WiFi.softAPIP().toString());
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  
  // Update NTP and set system time
  if (wifiConnected && millis() - lastNTPSync > NTP_SYNC_INTERVAL) {
    if (timeClient.update()) {
      lastNTPSync = millis();
      time_t epoch = timeClient.getEpochTime();
      struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
      settimeofday(&tv, nullptr);
      // Persist the new time to EEPROM
      sysConfig.last_ntp_sync_epoch = epoch;
      saveConfiguration();
      Serial.println("System time updated from NTP and saved to EEPROM");
    }
  }
  
  // Process relay schedules
  processRelaySchedules();
  
  delay(10);
}

void setupWebServer() {
  // Main pages
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", index_html);
  });
  
  server.on("/wifi", HTTP_GET, []() {
    server.send_P(200, "text/html", wifi_html);
  });
  
  server.on("/ap", HTTP_GET, []() {
    server.send_P(200, "text/html", ap_html);
  });
  
  server.on("/ntp", HTTP_GET, []() {
    server.send_P(200, "text/html", ntp_html);
  });
  
  // API endpoints
  server.on("/api/relays", HTTP_GET, handleGetRelays);
  server.on("/api/relay/manual", HTTP_POST, handleManualControl);
  server.on("/api/relay/reset", HTTP_POST, handleResetManual);
  server.on("/api/relay/save", HTTP_POST, handleSaveRelay);
  server.on("/api/time", HTTP_GET, handleGetTime);
  server.on("/api/wifi", HTTP_GET, handleGetWiFi);
  server.on("/api/wifi", HTTP_POST, handleSaveWiFi);
  server.on("/api/ap", HTTP_GET, handleGetAP);
  server.on("/api/ap", HTTP_POST, handleSaveAP);
  server.on("/api/ntp", HTTP_GET, handleGetNTP);
  server.on("/api/ntp", HTTP_POST, handleSaveNTP);
  server.on("/api/ntp/sync", HTTP_POST, handleSyncNTP);
  
  // Captive portal redirect
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
  // Use system time instead of NTP client
  time_t now;
  time(&now);
  if (now < 1609459200) return; // Invalid time (before 2021-01-01)

  struct tm *timeinfo = localtime(&now);
  
  for (int i = 0; i < NUM_RELAYS; i++) {
    if (relayConfigs[i].manualOverride) {
      digitalWrite(relayPins[i], 
        relayActiveLow ? !relayConfigs[i].manualState : relayConfigs[i].manualState);
      continue;
    }
    
    bool shouldBeOn = false;
    
    for (int s = 0; s < 4; s++) {
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
        // Overnight schedule
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
  
  // Check for valid configuration
  if (sysConfig.magic != EEPROM_MAGIC) {
    Serial.println("No valid config found – initializing defaults");
    sysConfig.magic = EEPROM_MAGIC;
    sysConfig.version = EEPROM_VERSION;
    strcpy(sysConfig.sta_ssid, "");
    strcpy(sysConfig.sta_password, "");
    strcpy(sysConfig.ap_ssid, "ESP32_8CH_Smart_Switch");
    strcpy(sysConfig.ap_password, "ESP32-admin");
    strcpy(sysConfig.ntp_server, "ph.pool.ntp.org");
    sysConfig.gmt_offset = 28800;
    sysConfig.daylight_offset = 0;
    sysConfig.last_ntp_sync_epoch = 0;  // NEW: Initialize new field
    saveConfiguration();
  }
  // Handle version migration (from version 1 to 2)
  else if (sysConfig.version == 1) {
    Serial.println("Migrating configuration from version 1 to 2");
    sysConfig.version = EEPROM_VERSION;
    sysConfig.last_ntp_sync_epoch = 0;  // Initialize new field
    saveConfiguration();
  }
  
  // Copy AP settings from config to variables
  strcpy(ap_ssid, sysConfig.ap_ssid);
  strcpy(ap_password, sysConfig.ap_password);
  
  // Load relay configurations
  int addr = sizeof(SystemConfig);
  for (int i = 0; i < NUM_RELAYS; i++) {
    EEPROM.get(addr, relayConfigs[i]);
    addr += sizeof(RelayConfig);
  }
  
  Serial.println("Configuration loaded");
  Serial.printf("Last NTP sync epoch: %lu\n", sysConfig.last_ntp_sync_epoch);
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

// API Handlers (unchanged except handleSyncNTP now also saves epoch)
void handleGetRelays() {
  DynamicJsonDocument doc(4096);
  JsonArray array = doc.to<JsonArray>();
  
  for (int i = 0; i < NUM_RELAYS; i++) {
    JsonObject relay = array.createNestedObject();
    relay["state"] = digitalRead(relayPins[i]) == (relayActiveLow ? LOW : HIGH);
    
    JsonArray schedules = relay.createNestedArray("schedules");
    for (int s = 0; s < 4; s++) {
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
  
  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
    return;
  }
  
  int relay = doc["relay"];
  JsonArray schedules = doc["schedules"].as<JsonArray>();
  
  if (relay >= 0 && relay < NUM_RELAYS && schedules.size() >= 4) {
    for (int s = 0; s < 4; s++) {
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
  
  time_t now;
  time(&now);
  if (now > 1609459200) {  // Valid time after Jan 1, 2021
    struct tm *timeinfo = localtime(&now);
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

void handleGetAP() {
  DynamicJsonDocument doc(256);
  doc["ap_ssid"] = sysConfig.ap_ssid;
  // Don't send password for security
  
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
    if (ap_password_new && strlen(ap_password_new) > 0) {
      if (strlen(ap_password_new) < 8) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Password must be at least 8 characters\"}");
        return;
      }
      strcpy(sysConfig.ap_password, ap_password_new);
    } else {
      sysConfig.ap_password[0] = '\0';
    }
    saveConfiguration();
    
    server.send(200, "application/json", "{\"success\":true}");
    delay(1000);
    ESP.restart();
  } else {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid AP SSID\"}");
  }
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
      if (timeClient.update()) {
        time_t epoch = timeClient.getEpochTime();
        struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
        settimeofday(&tv, nullptr);
        // Save the updated time
        sysConfig.last_ntp_sync_epoch = epoch;
        saveConfiguration();
      }
    }
    
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid NTP server\"}");
  }
}

void handleSyncNTP() {
  if (wifiConnected) {
    if (timeClient.update()) {
      time_t epoch = timeClient.getEpochTime();
      struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
      settimeofday(&tv, nullptr);
      // Save the synced time to EEPROM
      sysConfig.last_ntp_sync_epoch = epoch;
      saveConfiguration();
      server.send(200, "application/json", "{\"success\":true}");
    } else {
      server.send(400, "application/json", "{\"success\":false,\"error\":\"Failed to sync\"}");
    }
  } else {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"WiFi not connected\"}");
  }
}
