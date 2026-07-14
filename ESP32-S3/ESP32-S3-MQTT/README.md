### Download the Firmware and Flash
- https://github.com/xiv3r/esp32-automatic-timer-switch/releases/tag/esp32s3-mqtt

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
