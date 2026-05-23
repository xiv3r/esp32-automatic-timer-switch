# ESP8266
👉 https://github.com/xiv3r/esp8266-automatic-timer-switch

# Requirements
- ESP32 38P Pins
- DS3231 RTC Module (optional)
- 1 -> 16 Channel 5V Relay
- Female Dupont Wire
- Stable Wifi for NTP/RTC sync
- 5v 3-5a Power supply
  
`Optional`
- 5v UPS (Maintain Power and Timer)

# Libraries
- ArduinoJson
- Preferences (build-in)
- NTPClient
- RTClib 1.14.1

# Installation
- Download the [flasher](https://github.com/xiv3r/esp32-automatic-timer-switch/releases/tag/flasher) and firmware and then flash.

- offset address
```
bootloader: 0x1000
partition: 0x8000
firmware: 0x10000
```

# WiFi Key
- WiFi SSID: `ESP32_16CH_Timer_Switch`
- Password: `ESP32-admin`

# Activation
- Go to `wifi settings` and connect to your home wifi after the NTP is synchronized everything will work

# Relay Naming 
- Double click relay name to edit

# GMT offset
> ⚠️ Set to your country time
- Search your country `gmt offsets in seconds` and paste it in the Time -> GMT Offset

# Access
° Direct Access
- mDNS:`esp32-16ch-timer.local`
- Captive Portal: Auto redirect
- Gateway:`192.168.4.1`
- WAN:`192.168.1.123`
  
° Global:`Enable Port Forwarding on your router to access anywhere`

# Reset
- Hold BOOT button for 5 seconds

# Restart
- Press EN button

# 16 CHANNEL GPIO Connection 
```
RELAY  |  ESP32 38P
VCC  _____ 5VIN 
IN1  _____ 15  Relay 1
IN2  _____ 2   Relay 2
IN3  _____ 4   Relay 3
IN4  _____ 5   Relay 4
IN5  _____ 18  Relay 5
IN6  _____ 19  Relay 6
IN7  _____ 3   Relay 7
IN8  _____ 1   Relay 8
IN9  _____ 23  Relay 9
IN10 _____ 13  Relay 10
IN11 _____ 14  Relay 11
IN12 _____ 27  Relay 12
IN13 _____ 26  Relay 13
IN14 _____ 25  Relay 14
IN15 _____ 33  Relay 15
IN16 _____ 32  Relay 16
GND  _____ GND
```
# DS3231 GPIO Connection 
```
DS3231 | ESP32
VCC → 3.3V
SDA → 21
SCL → 22
GND → GND
```

<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/libraries/img1.png">
<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/libraries/img2.png">
<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/libraries/img3.png">
  
# Full Features
