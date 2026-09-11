# Requirements
- 5V 1-16 Channel Relay
- ESP32-S3 N16R8 (16mb flash 8mb ram)
- ESP32-S3 Expansion board
- DS3231 RTC Module (offline recommended)
- Female to Male Dupont Wires
- Stabe Wifi Connection (optional if no ds3231)
- 5V 2-5A Power Supply

`Optional`
- 5v UPS (Maintain Internal RTC time without DS3231 or NTP)
- SRR for high loads setup

# Libraries
- ArduinoJson
- PubSubClient
- RTCLib 1.14.1

# Installation
- Download the Firmware and Flash
- https://github.com/xiv3r/esp32-automatic-timer-switch/releases/tag/esp32s3
- Flash Offset
```
esp32s3-dump-0x0.bin: 0x0
```

# Wifi Key
- Wifi Name:`ESP32S3_16CH_Timer_Switch`
- Password:`12345678`

## Activation
> - Without ds3231 or wifi the time runs from internal rtc

° Online
- Go to `Wifi settings` and connect to your home wifi to set the rtc time automatically

° Offline
- Go to `Time settings` and tap `Sync Browser ` to set the rtc time

## Relay Naming 
> mobile mode
- Double click relay name to edit

## Set the Time (country)
> Set to your country time e.g for PH (UTC+8.0) 28800 seconds
- Search your country `gmt offsets in seconds` and paste to the Time -> GMT Offset
- e.g for negative gmt: -28800
- e.g for positive gmt:  28800
- https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/gmt-offsets-seconds.md
  
## Access
- mDNS:`esp32s3-16ch-timer-switch.local`
- Captive Portal: `Auto redirect`
- Gateway:`192.168.4.1`
- WAN:`192.168.1.123`
- Global:`Enable Port Forwarding on your router to access anywhere`

## Note
- Disable Wifi Station Mode if you have a DS3231

<details><summary>

## Isolate Relay Power
</summary>

> ⚠️ Use the Main relay power input and Avoid using VCC and GND from the relay IN GPIO Pin row

### 5V Relay
- Remove the Yellow VCC-JDVCC jumper.
- Relay JD-VCC pin: Connect to external 5V Positive wire.
- Relay GND pin: Connect to external 5V Negative wire.
- Relay VCC pin: Connect to ESP32 5V (powers the LED).

### 12V Relay
- Remove the Yellow VCC-JDVCC jumper.
- Relay JD-VCC pin: Connect to external 12V Positive wire.
- Relay GND pin: Connect to external 12V Negative wire.
- Relay VCC pin: Connect to ESP32 5V (powers the LED).

</details>

## Reset
- Hold BOOT button for 5 seconds to factory reset 

## Restart
- Press EN button to restart

# 16 Channel GPIO Connection
```
16CH   |   ESP32-S3 N16R8
VCC  _____ 5V
IN1  _____ GPIO 4   Relay 1
IN2  _____ GPIO 5   Relay 2
IN3  _____ GPIO 6   Relay 3
IN4  _____ GPIO 7   Relay 4
IN5  _____ GPIO 11  Relay 5
IN6  _____ GPIO 12  Relay 6
IN7  _____ GPIO 13  Relay 7
IN8  _____ GPIO 14  Relay 8
IN9  _____ GPIO 1   Relay 9
IN10 _____ GPIO 2   Relay 10
IN11 _____ GPIO 42  Relay 11
IN12 _____ GPIO 41  Relay 12
IN13 _____ GPIO 47  Relay 13
IN14 _____ GPIO 21  Relay 14
IN15 _____ GPIO 20  Relay 15
IN16 _____ GPIO 19  Relay 16
GND  _____ GND
```

# DS3231 GPIO Connection
```
DS3231  |  ESP32-S3 N16R8 
   SDA  → GPIO 8
   SCL  → GPIO 9
   VCC  → 3.3V
   GND  → GND
```

<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/ESP32-S3/image/esp32s3-1.jpg">
<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/ESP32-S3/image/s3-2.png">
<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/ESP32-S3/image/s3-3.png">
<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/ESP32-S3/image/s3-4.png">
<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/ESP32-S3/image/s3-5.png">


