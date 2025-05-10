# Portable Temperature Sensor - Implementation

This folder contains the implementation code for the Portable Temperature Sensor project.

## Project Structure

```
Portable_temperature_sensor/
├── platformio.ini        # PlatformIO configuration file
├── include/              # Header files
├── lib/                  # Project-specific libraries
├── src/                  # Source code
│   ├── main.cpp          # Main application code
│   ├── NetworkManager.h  # WiFi connection management
│   ├── display_helper.h  # Display helper functions (not currently used)
│   └── display_helper.cpp# Display implementation (not currently used)
└── test/                 # Test files
```

## Development Environment

### PlatformIO Configuration

This project uses PlatformIO for dependency management and deployment. The `platformio.ini` file is configured with:

```ini
[env:nodemcuv2]
platform = espressif8266
board = nodemcuv2
framework = arduino
monitor_speed = 115200
lib_deps = 
	knolleary/PubSubClient@^2.8
	plerup/EspSoftwareSerial@^8.2.0
	adafruit/MAX6675 library@^1.1.2
	gyverlibs/GyverOLED@^1.6.4
```

> **Note:** The GyverOLED library is still included but display functionality has been removed in the latest version. This dependency will be removed in future updates.

## Implementation Details

### Hardware Configuration

The sensor hardware is configured in `main.cpp` with the following pin assignments:

```cpp
// MAX6675 thermocouple interface pins
const int thermoDO = 12;   // Data out (SO/MISO)
const int thermoCS = 16;   // Chip select
const int thermoCLK = 14;  // Clock signal

// Output pins
const int buzzerPin = 0;     // Alarm buzzer
const int interruptPin = 2;  // External trigger signal

// Software serial port
const int SOFT_RX = 13;  // GPIO13 (D7)
const int SOFT_TX = 15;  // GPIO15 (D8)
```

### Network Connectivity

WiFi connection is managed through the `NetworkManager` class in `src/NetworkManager.h`, which provides:

- Automatic reconnection handling
- Connection state management
- Status reporting

### MQTT Integration

Temperature data is published to MQTT topics, with the following features:
- Automatic reconnection to MQTT broker
- Configurable publishing interval
- JSON-formatted messages for easier parsing

## Building and Running

### Using PlatformIO

1. Open this folder in VSCode with PlatformIO extension
2. Build the project:
   ```
   platformio run
   ```
3. Upload to NodeMCU:
   ```
   platformio run --target upload
   ```
4. Monitor serial output:
   ```
   platformio device monitor
   ```

### Using Arduino IDE

1. Rename `main.cpp` to `Portable_temperature_sensor.ino`
2. Install required libraries through the Arduino Library Manager
3. Set board to "NodeMCU 1.0 (ESP-12E Module)"
4. Upload sketch

## Configuration

To configure the device, edit the following parameters in `src/main.cpp`:

```cpp
// WiFi settings
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// MQTT settings
const char* mqtt_server = "YOUR_MQTT_SERVER";
const int mqtt_port = 1883;

// Temperature thresholds
const float tempThreshold = 80.0;  // Temperature alarm threshold in °C
```

## Known Issues

1. After power loss, device requires manual configuration

---

For full project documentation, including hardware details and usage instructions, see the [main README](../README.md) in the parent directory.
