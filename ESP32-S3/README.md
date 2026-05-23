# Requirements
- 5V 1-16 Channel Relay
- ESP32-S3 N16R8 (16mb flash 8mb ram)
- DS3231 RTC Module
- F-F Dupont Wires
- Stabe Wifi connection
- 5V 3-5A Power Supply

# Libraries
- ArduinoJson
- Preferences (built-in)
- NTPClient
- [RTCLib 1.14.1](https://codeload.github.com/adafruit/RTClib/zip/refs/tags/1.14.1)

# Installation
- Download the Firmware and Flash

- Offsets address
```
bootloader: 0x0
partitions: 0x8000
firmware  : 0x10000
```
# Wifi Key
- Wifi Name:`ESP32_S3_16CH_Timer_Switch`
- Password:`ESP32-admin`

# Setup
- Go to `192.168.4.1 –> wifi` then connect your ESP32-S3 to your Home Wifi

# Access
° Direct Access
- mDNS:`esp32-s3-16ch-timer.local`
- Captive Portal: Auto redirect
- Gateway:`192.168.4.1`
- WAN:`192.168.1.123`
  
° Global:`Enable esp32 s3 Port Forwarding on your router to access anywhere`

# Reset
- Hold BOOT button for 5 seconds

# GPIO Connections
```
16CH | ESP32-S3 N16R8
VCC  _____ 5V
IN1  _____ 1   Relay 1
IN2  _____ 2   Relay 2
IN3  _____ 3   Relay 3
IN4  _____ 4   Relay 4
IN5  _____ 5   Relay 5
IN6  _____ 6   Relay 6
IN7  _____ 7   Relay 7
IN8  _____ 8   Relay 8
IN9  _____ 9   Relay 9
IN10 _____ 10  Relay 10
IN11 _____ 11  Relay 11
IN12 _____ 12  Relay 12
IN13 _____ 13  Relay 13
IN14 _____ 14  Relay 14
IN15 _____ 15  Relay 15
IN16 _____ 16  Relay 16
GND  _____ GND
```

# DS3231 RTC Module GPIO
```
RTC  |  ESP32-S3 N16R8 
SDA  → 17
SCL  → 18
VCC  → 3.3V
GND  → GND
```
