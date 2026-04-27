# Requirements
- 5V 1-16 Channel Relay
- ESP32-C3 Dev Module
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
- Wifi Name:`ESP32_C3_16CH_Timer_Switch`

- Password:`ESP32-C3-admin`

# Setup
- Go to `192.168.4.1/wifi` then connect your ESP32-S3 to your Home Wifi

# Access
° Direct Access
- mDNS:`esp32-c3-16ch-timer.local`
- Captive Portal: Auto redirect
- Gateway:`192.168.4.1`
- WAN:`192.168.1.123`
  
° Global:`Enable esp32-c3 Port Forwarding on your router to access anywhere`

# GPIO Connections
```
16CH | ESP32-S3
VCC  => 5V
IN1  => 0
IN2  => 1
IN3  => 2
IN4  => 3
IN5  => 4
IN6  => 5
IN7  => 6
IN8  => 7
IN9  => 8
IN10 => 9
IN11 => 10
IN12 => 11
IN13 => 18
IN14 => 19
IN15 => 20
IN16 => 21
GND  => GND
```

<details><summary>

# Full Features
  
</summary>

```

```
</details>
