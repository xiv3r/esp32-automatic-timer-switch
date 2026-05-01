# Requirements
- 5V 1-24 Channel Relay
- ESP32-S3 N16R8 Wroom-1 Dev Module
- DS3231 RTC Module
- 2pcs Dupont Wires
- 5V 5-8A Power Supply

# Libraries
- ArduinoJson
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
- Wifi Name:`ESP32_S3_24CH_Timer_Switch`

- Password:`ESP32S3-admin`

# Setup
- Go to `192.168.4.1 –> wifi` then connect your ESP32-S3 to your Home Wifi

# Access
° Direct Access
- mDNS:`esp32-s3-24ch-timer.local`
- Captive Portal: Auto redirect
- Gateway:`192.168.4.1`
- WAN:`192.168.1.123`
  
° Global:`Enable esp32 s3 Port Forwarding on your router to access anywhere`

# GPIO Connections
```
24CH | ESP32-S3 N16R8
VCC  => 5V
IN1  => 4
IN2  => 5
IN3  => 6
IN4  => 7
IN5  => 15
IN6  => 16
IN7  => 17
IN8  => 18
IN9  => 10
IN10 => 11
IN11 => 12
IN12 => 13
IN13 => 14
IN14 => 1
IN15 => 2
IN16 => 42
IN17 => 41
IN18 => 40
IN19 => 39
IN20 => 38
IN21 => 45
IN22 => 48
IN23 => 47
IN24 => 21
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
