# Requirements
- ESP32 30P/38P Pins
- 8-Channel Relay
- 10 Dupont Wire
- Home Wifi for NTP/RTC sync
- 5vDC Battery (Maintain Power and Timer)(optional)

# Arduino Libraries
- ArduinoJson
- NTPClient

# Features 
- Captive Portal
- Custom Wifi settings
- WAN gateway access
- Manual and Automatic Switch
- Control Doesn't works if time is not synchronized
- Lightweight and More Responsive Web User Interface 
- ESP32 NTP/RTC Auto synchronization
- Wifi Client mode for NTP/RTC time synchronization
- Support 1--8 Channel Relay
- Each Relay have 4 start and stop schedule a total of 8 schedules
- Persistent data (Survive Power loss)
- Anti-Reset Protection
- Works Offline after NTP is synchronized
- 99% RTC+NTP accuracy rate

# Suitable
- Home Automation
- Farms
- Livestocks 
- Buildings
- Water pumping station
- Greenhouses
- Automatic Garden Sprinkler System 
- So much more...

# Note
- In case the ntp server is inactive to maintain the relay switch time precision control add a 5vdc battery for esp32 and separate the 5vdc relay power supply adaptor.

# Installation
- Download the [flasher](https://github.com/xiv3r/esp32-automatic-timer-switch/releases/tag/flasher) and [firmware](https://github.com/xiv3r/esp32-automatic-timer-switch/raw/refs/heads/main/esp32-8ch-timer-switch-firmware-0x0.bin) then flash using `0x0` offset

# WiFi Key
`Note: Take note your wifi credentials reset doesn't supported (anti-reset protection)` 
- WiFi SSID: `ESP32_8CH_Timer_Switch`
- Password: `ESP32-admin`

# Activation
- Go to `wifi settings` and connect to your home wifi after the NTP is synchronized everything will works.

# Schematics
```
RELAY     ESP32 38P
VCC _____ 5VIN 
IN1 _____ D32 GPIO
IN2 _____ D33 GPIO
IN3 _____ D25 GPIO
IN4 _____ D26 GPIO
IN5 _____ D27 GPIO
IN6 _____ D14 GPIO
IN7 _____ D16 GPIO
IN8 _____ D17 GPIO
GND _____ GND
```
<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/img/esp32_dashboard.jpg">
