# About
This is ESP32 smart relay controller, designed to manage 20 independent relay channels with advanced scheduling capabilities. The system operates as a standalone device with its own Wi-Fi access point and web-based control interface, making it suitable for industrial automation, home automation, greenhouse control, lighting systems, and other applications requiring precise timed control of multiple electrical circuits.


# Requirements
- ESP32 38P Pins
- 1 -> 16 Channel 5V Relay
- Female Dupont Wire
- Home Wifi for NTP/RTC sync
- 5v 5-8a Power supply
  
`Optional`
- 5v50Ah DC Battery (Maintain Power and Timer)

# Arduino Libraries
- ArduinoJson
- Preferences
- NTPClient

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

# Relay Name
- Double click relay name to edit

# Access
° Direct Access
- mDNS:`esp32-16ch-timer.local`
- Captive Portal: Auto redirect
- Gateway:`192.168.4.1`
- WAN:`192.168.1.123`
  
° Global:`Enable Port Forwarding on your router to access anywhere`

# 16 Channel GPIO Connection 
```
RELAY    ESP32 38P
VCC  _____ 5VIN 
IN1  _____ 15
IN2  _____ 2
IN3  _____ 4
IN4  _____ 5
IN5  _____ 18
IN6  _____ 3
IN7  _____ 1
IN8  _____ 23
IN9  _____ 13
IN10 _____ 12
IN11 _____ 14
IN12 _____ 27
IN13 _____ 26
IN14 _____ 25
IN15 _____ 33
IN16 _____ 32
GND  _____ GND
```

<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/libraries/">
<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/libraries/">

<details><summary>
  
# Full Features
</summary>


</details>
