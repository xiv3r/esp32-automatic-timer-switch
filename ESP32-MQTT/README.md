### Download the Firmware and Flash

https://github.com/xiv3r/esp32-automatic-timer-switch/releases/tag/esp32-mqtt

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
