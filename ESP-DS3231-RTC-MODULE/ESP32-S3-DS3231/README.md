# Requirements
- 5V 1-16 Channel Relay
- ESP32-S3 N16R8 Wroom-1 Dev Module
- DS3231 RTC Module
- 2pcs Dupont Wires
- 5V 5-8A Power Supply

# Libraries
- ArduinoJson
- Preferences 
- NTPClient
- [RTCLib 1.14.1](https://codeload.github.com/adafruit/RTClib/zip/refs/tags/1.14.1)

# Installation
- Download the Firmware and Flash
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

# GPIO Connections
```
16CH | ESP32-S3 N16R8
VCC  => 5V
IN1  => 1
IN2  => 2
IN3  => 3
IN4  => 4
IN5  => 5
IN6  => 6
IN7  => 7
IN8  => 10
IN9  => 11
IN10 => 12
IN11 => 13
IN12 => 14
IN13 => 15
IN14 => 16
IN15 => 17
IN16 => 18
GND  => GND
```

# DS3231 RTC Module GPIO
```
RTC  |  ESP32-S3 N16R8 
SDA  → 8
SCL  → 9
VCC → 3.3V
GND → GND
```

# Full Features
