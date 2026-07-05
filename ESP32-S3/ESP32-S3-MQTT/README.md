### Download the Firmware and Flash
https://github.com/xiv3r/esp32-automatic-timer-switch/releases/tag/esp32s3-mqtt

# 16 Channel GPIO Connection
```
16CH   |   ESP32-S3 N16R8
VCC  _____ 5V
IN1  _____ 1   Relay 1
IN2  _____ 2   Relay 2
IN3  _____ 3   Relay 3
IN4  _____ 4   Relay 4
IN5  _____ 5   Relay 5
IN6  _____ 6   Relay 6
IN7  _____ 7   Relay 7
IN8  _____ 10  Relay 8
IN9  _____ 11  Relay 9
IN10 _____ 12  Relay 10
IN11 _____ 13  Relay 11
IN12 _____ 14  Relay 12
IN13 _____ 15  Relay 13
IN14 _____ 16  Relay 14
IN15 _____ 17  Relay 15
IN16 _____ 18  Relay 16
GND  _____ GND
```

# DS3231 GPIO Connection
```
DS3231  |  ESP32-S3 N16R8 
   SDA  → 8
   SCL  → 9
   VCC  → 3.3V
   GND  → GND
```
