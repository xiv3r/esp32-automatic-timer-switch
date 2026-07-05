### Download the Firmware and Flash

https://github.com/xiv3r/esp32-automatic-timer-switch/releases/tag/esp32-mqtt

## 16 CHANNEL RELAY GPIO Connection 
```
RELAY  |  ESP32 30/38P
VCC  _____ 5VIN 
IN1  _____ 15  Relay 1
IN2  _____ 2   Relay 2
IN3  _____ 4   Relay 3
IN4  _____ 5   Relay 4
IN5  _____ 18  Relay 5
IN6  _____ 19  Relay 6
IN7  _____ 3   Relay 7
IN8  _____ 1   Relay 8
IN9  _____ 23  Relay 9
IN10 _____ 13  Relay 10
IN11 _____ 14  Relay 11
IN12 _____ 27  Relay 12
IN13 _____ 26  Relay 13
IN14 _____ 25  Relay 14
IN15 _____ 33  Relay 15
IN16 _____ 32  Relay 16
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
