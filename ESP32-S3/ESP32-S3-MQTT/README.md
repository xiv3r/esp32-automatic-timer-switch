### Download the Firmware and Flash
https://github.com/xiv3r/esp32-automatic-timer-switch/releases/tag/esp32s3-mqtt

# 16 Channel GPIO Connection
```
2x8CH | ESP32-S3 N16R8
VCC _____ 5V
IN8 _____ GPIO 4  Relay 1
IN7 _____ GPIO 5  Relay 2
IN6 _____ GPIO 6  Relay 3
IN5 _____ GPIO 7  Relay 4
IN4 _____ GPIO 11 Relay 5
IN3 _____ GPIO 12 Relay 6
IN2 _____ GPIO 13 Relay 7
IN1 _____ GPIO 14 Relay 8
GND _____ GND

VCC _____ 5V
IN1 _____ GPIO 1  Relay 9
IN2 _____ GPIO 2  Relay 10
IN3 _____ GPIO 42 Relay 11
IN4 _____ GPIO 41 Relay 12
IN5 _____ GPIO 47 Relay 13
IN6 _____ GPIO 21 Relay 14
IN7 _____ GPIO 20 Relay 15
IN8 _____ GPIO 19 Relay 16
GND _____ GND
```

# DS3231 GPIO Connection
```
DS3231  |  ESP32-S3 N16R8
VCC _____ 3.3V
SDA _____ GPIO 8
SCL _____ GPIO 9
GND _____ GND
```
