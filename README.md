## Contributors 

<a href="https://github.com/xiv3r/esp32-automatic-timer-switch/graphs/contributors">
<img src="https://contrib.rocks/image?&columns=25&max=10000&&repo=xiv3r/esp32-automatic-timer-switch" alt="contributors"/></a>

## Requirements
- ESP32 30/38P Pins
- DS3231 RTC Module (offline recommend)
- 5v 1-16 Channel Relay
- Female to Female Dupont Wire
- 5v 2-5a Power supply

`Optional`
- 5v UPS (Maintain RTC Time without DS3231 or NTP)
- Solid State Relay (SSR DC-AC) (High Load Setup)
- ESP32 Expansion Board
- Stable Wifi Connection for NTP/RTC sync (online if no ds3231)

## Arduino Libraries
- ArduinoJson
- RTClib v1.14.1

## Installation
> Download and install 
### ESP32 Win/Linux Drivers
- CH340G: https://sparks.gogo.co.nz/ch340.html
- CP2102: https://www.silabs.com/software-and-tools/usb-to-uart-bridge-vcp-drivers?tab=downloads
- FT232: https://ftdichip.com/wp-content/uploads/2025/03/CDM-v2.12.36.20-Universal-Driver-for-x64-WHQL-Certified.zip
## Flasher
### Android (otg)
- https://play.google.com/store/apps/details?id=io.serialflow.espflash
### Android Termux
- https://github.com/7wp81x/Termux-ESP-Flasher
### Windows
- https://dl.espressif.com/public/flash_download_tool.zip
### Linux
```sh
esptool --port <PORT> write_flash 0x0 esp32-dump-0x0.bin
```
### Win/Linux Browser
- https://g3gg0.github.io/esp32_flasher/flasher.html
### Flash firmware 
- Download the Firmware and Flash
- https://github.com/xiv3r/esp32-automatic-timer-switch/releases/tag/esp32
- Flash Offset
```
esp32-dump-0x0.bin: 0x0
```

## WiFi Key
- WiFi SSID: `ESP32_16CH_Timer_Switch`
- Password: `12345678`
  
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
- https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/gmt-offsets-seconds.md
  
## Access
- mDNS:`esp32-16ch-timer-switch.local`
- Captive Portal: `Auto redirect`
- Gateway:`192.168.4.1`
- WAN:`192.168.1.123`
- Global:`Enable Port Forwarding on your router to access anywhere`

## Note
- Disable Wifi Station Mode if you have a DS3231
- Avoid connecting to a non-existed open wifi network SSID to prevent hang issue. Solution turn off wifi station mode.

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

## 16 CHANNEL RELAY GPIO Connection 
```
RELAY  |  ESP32 30/38P
VCC  _____ 5VIN
IN1  _____ GPIO32  Relay 1
IN2  _____ GPIO33  Relay 2
IN3  _____ GPIO25  Relay 3
IN4  _____ GPIO26  Relay 4
IN5  _____ GPIO27  Relay 5
IN6  _____ GPIO14  Relay 6
IN7  _____ GPIO13  Relay 7
IN8  _____ GPIO23  Relay 8
IN9  _____ GPIO1   Relay 9  (TX)
IN10 _____ GPIO3   Relay 10 (RX)
IN11 _____ GPIO19  Relay 11
IN12 _____ GPIO18  Relay 12
IN13 _____ GPIO5   Relay 13
IN14 _____ GPIO4   Relay 14
IN15 _____ GPIO2   Relay 15
IN16 _____ GPIO15  Relay 16
GND  _____ GND
```

## DS3231 GPIO Connection 
```
DS3231 | ESP32 38P
VCC → 3.3V
SDA → 21
SCL → 22
GND → GND
```

<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/libraries/src1.png">
<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/libraries/src2.png">
<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/libraries/src3.png">
<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/libraries/src4.png">
<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/libraries/shot2.jpg">
<img src="https://github.com/xiv3r/esp32-automatic-timer-switch/blob/main/libraries/src5.jpg">

# Build Firmware
>  Auto Build firmware binaries using github action
- https://github.com/xiv3r/arduino
