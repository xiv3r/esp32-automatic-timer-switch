# Requirements
- 5V 1-16 Channel Relay
- ESP32-S3 N16R8 (16mb flash 8mb ram)
- DS3231 RTC Module (offline recommended)
- Female to Female Dupont Wires
- Stabe Wifi Connection (opt. if no ds3231)
- 5V 2-5A Power Supply

`Optional`
- 5v UPS (Maintain Internal RTC time without DS3231 or NTP)
- ESP32-S3 Expansion board

# Libraries
- ArduinoJson
- PubSubClient
- RTCLib 1.14.1

# Installation
- Download the Firmware and Flash

https://github.com/xiv3r/esp32-automatic-timer-switch/releases/tag/esp32s3
- Flash Offset
```
esp32s3-dump-0x0.bin: 0x0
```
# Wifi Key
- Wifi Name:`ESP32_S3_16CH_Timer_Switch`
- Password:`ESP32-admin`

# Setup
> online
- Go to `192.168.4.1 –> wifi` then connect to your Home Wifi to set rtc time automatically 

> offline
- Go to `192.168.4.1 -> Time` then tap sync browser to set the rtc time 

# Access
- mDNS:`esp32-s3-16ch-timer-switch.local`
- Captive Portal:`Auto redirect`
- Gateway:`192.168.4.1`
- WAN:`192.168.1.123`
- Global:`Enable esp32 s3 Port Forwarding on your router to access anywhere`

# Reset
- Hold BOOT button for 5 seconds

# 16 Channel GPIO Connection
```
16CH   |   ESP32-S3 N16R8
VCC  _____ 5V
IN1  _____ 1   Relay 1
IN2  _____ 2   Relay 2
IN3  _____ 3   Relay 3
IN4  _____ 4   Relay 4
IN5  _____ 5   Relay 5
IN6  _____ 6   Relay 6
IN7  _____ 7   Relay 7
IN8  _____ 10  Relay 8
IN9  _____ 11  Relay 9
IN10 _____ 12  Relay 10
IN11 _____ 13  Relay 11
IN12 _____ 14  Relay 12
IN13 _____ 15  Relay 13
IN14 _____ 16  Relay 14
IN15 _____ 17  Relay 15
IN16 _____ 18  Relay 16
GND  _____ GND
```

# DS3231 GPIO Connection
```
DS3231  |  ESP32-S3 N16R8 
   SDA  → 8
   SCL  → 9
   VCC  → 3.3V
   GND  → GND
```
<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/ESP32-S3/image/s3-1.jpg">
<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/ESP32-S3/image/s3-2.png">
<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/ESP32-S3/image/s3-3.png">
<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/ESP32-S3/image/s3-4.png">
<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/ESP32-S3/image/s3-5.png">


