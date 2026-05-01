
# Requirements
- ESP32 30P/38P Pins
- 1 -> 18 Channel Relay
- DS3232 RTC Module
- Female Dupont Wire
- Home Wifi for NTP/RTC sync
- 5v 5a Power supply

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

# GPIO Connection 
```
RELAY     ESP32 30P/38P
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