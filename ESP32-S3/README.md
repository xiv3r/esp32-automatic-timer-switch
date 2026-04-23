# Requirements
- 5V 1-16 Channel Relay
- ESP32-S3 Dev Module
- 2pcs Dupont Wires
- 5V 5A Power Supply

`Optional`
- 5V Battery (Maintain Time)

# Libraries
- ArduinoJson
- NTPClient

# Installation
- Download the Firmware and Flash
```
bootloader: 0x0
partitions: 0x8000
firmware  : 0x10000
```
# Wifi Key
- Wifi Name:`ESP32_S3_16CH_Timer_Switch`

- Password:`ESP32-S3-admin`

# Setup
- Go to `192.168.4.1/wifi` then connect your ESP32-S3 to your Home Wifi

# GPIO Connections
```
16CH | ESP32-S3
VCC => 5V
IN1 => 4
IN2 => 5
IN3 => 6
IN4 => 7
IN5 => 8
IN6 => 9
IN7 => 10
IN8 => 11
IN9 => 12
IN10 => 13
IN11 => 14
IN12 => 15
IN13 => 16
IN14 => 17
IN15 => 18
IN16 => 21
GND => GND
```
