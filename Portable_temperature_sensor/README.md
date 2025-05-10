# Portable Temperature Sensor Project

This project uses a NodeMCU ESP8266 with MAX6675 thermocouple to create a portable temperature sensor with wireless connectivity.

## May 2025 Update

Important changes:
- Removed all OLED display functionality (will be implemented separately)
- Removed GyverOLED library dependency
- Fixed String comparison issues (changed == to .equals())
- Simplified project structure
- Removed unneeded display files

## Hardware Requirements

- NodeMCU ESP8266
- MAX6675 Thermocouple Module
- Buzzer
- Wires/Breadboard for connections

## Pin Connections

```
Temperature Sensor (MAX6675):
- SCK → GPIO14 (D5 on NodeMCU)
- CS  → GPIO16 (D0 on NodeMCU)
- SO  → GPIO12 (D6 on NodeMCU)
- VCC → 3.3V
- GND → GND

Buzzer:
- Buzzer+ → GPIO0  (D3 on NodeMCU)
- Buzzer- → GND

Interrupt Output:
- Signal → GPIO2  (D4 on NodeMCU)
```

## Software Setup

### PlatformIO

1. Open the project in PlatformIO
2. All required libraries are specified in platformio.ini
3. Build and upload to NodeMCU

### Arduino IDE

1. Make sure you have the ESP8266 board manager URLs set up
2. Install the following libraries:
   - GyverOLED
   - PubSubClient
   - EspSoftwareSerial
   - Adafruit MAX6675
3. Open main.cpp as your Arduino sketch
4. Select NodeMCU 1.0 ESP-12E Module as the board type
5. Build and upload

## Building and Uploading

### Using PlatformIO

```
cd Portable_temperature_sensor
platformio run -t upload
```

### Using Arduino IDE

1. Rename main.cpp to Portable_temperature_sensor.ino
2. Move all files into a Portable_temperature_sensor folder
3. Open the .ino file in Arduino IDE
4. Click Upload

## Troubleshooting

### Display not working
- Check the I2C connections
- Make sure SDA is connected to GPIO4 and SCL to GPIO5
- Verify 3.3V and GND connections

### Temperature readings incorrect
- Check MAX6675 connections
- Make sure the thermocouple is properly attached to MAX6675
- Verify power connections

### WiFi/MQTT not connecting
- Check the WiFi and MQTT credentials in the code
- Make sure MQTT server is reachable
- Check for proper antenna connection on NodeMCU

## Code Structure

- main.cpp: Main program file
- NetworkManager.h: WiFi connection management

Note: Display-related files (display_helper.cpp/h, display_test.h) are still in the project directory but are no longer used.
