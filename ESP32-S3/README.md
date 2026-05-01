# Requirements
- 5V 1-26 Channel Relay
- ESP32-S3 N16R8 Wroom-1 Dev Module
- Dupont Wires
- 5V 5-10A Power Supply

`Optional`
- 5V Battery (Maintain Time)

# Libraries
- ArduinoJson
- Preferences 
- NTPClient

# Installation
- Download the Firmware and Flash
```
bootloader: 0x0
partitions: 0x8000
firmware  : 0x10000
```
# Wifi Key
- Wifi Name:`ESP32_S3_26CH_Timer_Switch`

- Password:`ESP32S3-admin`

# Setup
- Go to `192.168.4.1/wifi` then connect your ESP32-S3 to your Home Wifi

# Access
° Direct Access
- mDNS:`esp32-s3-26ch-timer.local`
- Captive Portal: Auto redirect
- Gateway:`192.168.4.1`
- WAN:`192.168.1.123`
  
° Global:`Enable esp32 s3 Port Forwarding on your router to access anywhere`

# GPIO Connections
```
16CH | ESP32-S3
VCC  => 5V
IN1  => 4
IN2  => 5
IN3  => 6
IN4  => 7
IN5  => 15
IN6  => 16
IN7  => 17
IN8  => 18
IN9  => 8
IN10 => 9
IN11 => 10
IN12 => 11
IN13 => 12
IN14 => 13
IN15 => 14
IN16 => 1
IN17 => 2
IN18 => 42
IN19 => 41
IN20 => 40
IN21 => 39
IN22 => 38
IN23 => 45
IN24 => 48
IN25 => 47
IN26 => 21
GND  => GND
```

# Full Features

