Compile the .ino file based on the instructions.
Do not save the file to workspace except the 4 generated .bin firmware.

# Download the sketch.ino file
wget -O esp32-sketch.ino https://raw.githubusercontent.com/xiv3r/esp32-automatic-timer-switch/refs/heads/main/esp32-sketch.ino

# Download the arduino-cli
sudo apt update
sudo apt install wget -y
wget -O arduino-cli.deb https://github.com/arduino/arduino-cli/releases/download/v1.5.1/arduino-cli_1.5.1-1_amd64.deb
sudo dpkg -i arduino-cli.deb
sudo apt --fix-broken install -y
sudo dpkg --configure -a

# Update arduino-cli core
arduino-cli config init
arduino-cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32

# Install libraries
arduino-cli lib install "ArduinoJson"
git clone --depth 1 --branch 1.14.1 https://github.com/adafruit/RTClib.git ~/Arduino/libraries/RTClib

# Compile firmware
arduino-cli compile --fqbn esp32:esp32:esp32 --clean --output-dir firmware .

then upload the 4 generated firmware for boot, partition, firmware and merged .bin to the workspace so user can download it directly.
