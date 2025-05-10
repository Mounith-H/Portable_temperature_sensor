#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>
#include <time.h>
#include <max6675.h>
#include <GyverOLED.h>
#include "NetworkManager.h"
#include "display_helper.h"
#include "splashScreen.h"

/*
HARDWARE CONNECTIONS:

MAX6675 Thermocouple:
- SO  → GPIO12 (D6)  // Data from sensor
- CS  → GPIO16 (D0)  // Chip select
- SCK → GPIO14 (D5)  // Clock signal

OLED Display (I2C):
- SDA → GPIO4  (D2)
- SCL → GPIO5  (D1)

Peripherals:
- Buzzer   → GPIO0  (D3)  // Alarm when temperature exceeds threshold
- Interrupt → GPIO2  (D4)  // External trigger signal

Communication:
- Software Serial: RX→GPIO13 (D7), TX→GPIO15 (D8)  // For external comms
- Hardware Serial: USB port (115200 baud)          // For debugging

Power Requirements:
- All components operate at 3.3V
- Connect all GND pins to common ground
*/

// External communication port (separate from debug serial)
const int SOFT_RX = 13;  // GPIO13 (D7)
const int SOFT_TX = 15;  // GPIO15 (D8)
SoftwareSerial softSerial(SOFT_RX, SOFT_TX);

// MAX6675 thermocouple interface pins
const int thermoDO = 12;   // Data out (SO/MISO)
const int thermoCS = 16;   // Chip select
const int thermoCLK = 14;  // Clock signal
MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);

// Output pins
const int buzzerPin = 0;     // Alarm buzzer
const int interruptPin = 2;  // External trigger signal

// Wi-Fi & MQTT configuration (fill these in)
const char* ssid         = "********";        // FIXME: replace with your wifi SSID
const char* password     = "********";        // FIXME: replace with your wifi password
const char* mqtt_server  = "192.168.137.1";   // FIXME: replace with your mqtt server IP
const int   mqtt_port    = 1883;              // replace with your mqtt server port default is 1883
const char* mqtt_user    = "***test***";      // FIXME: replace with your mqtt username
const char* mqtt_pass    = "***test***";      // FIXME: replace with your mqtt password

WiFiClient espClient;
PubSubClient mqttClient(espClient);
NetworkManager networkManager(ssid, password);

// OLED display definitions (I2C PCB: SDA=GPIO4, SCL=GPIO5)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_WHITE 1  // Color constant for drawing in white
#define OLED_BLACK 0  // Color constant for drawing in black
int currentX, currentY, currentScale; // Track cursor position and text scale

// Initialize OLED display with SSD1306 driver (128x32 resolution)
// using buffered mode for smoother updates, at I2C address 0x3C
GyverOLED<SSD1306_128x32, OLED_BUFFER> display(0x3C);

// OLED Display Settings
unsigned long mainDisplayUpdateInterval = 1000; // Update display every 1 second
unsigned long mainLastDisplayUpdateInterval = 0;

// WiFi and MQTT state tracking
bool mqttWasConnected = false;
unsigned long lastDisplayUpdate = 0;
const unsigned long displayUpdateInterval = 500; // Update network status on display every 1 seconds
bool updateDisplayStatus = false;

// Time configuration
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 19800;  // GMT +5:30 for IST // FIXME: Update gmt offset of your location
const int   daylightOffset_sec = 0;
String outputMode = "normal";  // Default output mode

// Data configuration
unsigned long sendInterval  = 1000; // ms between sends
double        thresholdTemp = 80.00;   // °C setpoint
unsigned long lastSendTime  = 0;

// Buzzer control
bool buzzerEnabled = true;        // Global flag to enable/disable buzzer
unsigned long buzzerSilenceUntil = 0;  // Timestamp until when the buzzer should be silenced

// MQTT activity tracking
unsigned long lastMqttUpload = 0;    // Last time data was uploaded
unsigned long lastMqttDownload = 0;  // Last time data was downloaded
const unsigned long mqttActivityIndicatorDuration = 100; // How long to show upload/download activity (ms)

// Global variables
static double tempValue = 0;
bool otherUpdate = true;

// Forward declarations
void mqttCallback(char* topic, byte* payload, unsigned int length);
bool mqttReconnect(int maxAttempts = 3);
void playBuzzerAlarm(double temperature, double threshold); // Control buzzer based on temperature
void updateNetworkDisplay(); // Update network status on display
void logodisplay(); // Display logo on OLED
void displayUpdate(); // Update display with temperature and settings
void serialHandler(); // Handle incoming serial data

void setup() {  
  // Initialize both serial ports
  Serial.begin(115200);     // Hardware Serial for debug
  softSerial.begin(9600);   // Software Serial for communication with external device
  Serial.println("Debug: Serial ports initialized");
  
  // Configure output pins for buzzer and interrupt signals
  pinMode(buzzerPin, OUTPUT);
  pinMode(interruptPin, OUTPUT);
  digitalWrite(interruptPin, LOW); // Initialize interrupt signal as inactive
  
  noTone(buzzerPin);              // Initialize buzzer in silent state
  // MAX6675 automatically initializes when the object is created
  // I2C init for OLED
  Wire.begin(4, 5);
  Wire.setClock(400000);  // Set I2C clock speed to 400kHz (fast mode)
  display.init();                 // Initialize the display
  display.clear();                // Clear the display buffer - necessary to start with a clean black background
  display.setScale(1);            // Set text size scaling to 1 (equivalent to setTextSize)
  display.autoPrintln(true);      // Enable automatic line breaks
  display.invertText(false);      // Ensure text isn't inverted (text is white on black background)
  display.textMode(BUF_REPLACE);  // Ensure text writes in replace mode (not overlaid)

  logodisplay();  // Display animated logo on OLED

  // Initialize network manager (non-blocking)
  networkManager.begin();
  networkManager.startConnection();  // Define version and device info
  const String VERSION = "1.1.0";
  const String DEVICE_ID = "ESP-" + String(ESP.getChipId(), HEX);

  // Show startup message
  display.clear();
  display.setScale(2);
  display.setCursor(57, 0);
  display.print("MEL");
  display.setScale(1);
  display.setCursor(0, 2);
  display.print("Temp Sensor v");
  display.print(VERSION);
  display.setCursor(0, 3); // line 1 (GyverOLED uses line-based cursor position)
  display.print("ID: ");
  display.print(DEVICE_ID);
  display.update(); // update display (equivalent to display.display())

  // Print startup info to Serial  
  Serial.println("\n=========================");
  Serial.println("Temperature Sensor v" + VERSION);
  Serial.println("Device ID: " + DEVICE_ID);
  Serial.println("Sensor: MAX6675 (Digital)");
  Serial.println("Connecting to WiFi: " + String(ssid));
  Serial.println("MQTT Server: " + String(mqtt_server) + ":" + String(mqtt_port));
  
  // Test MAX6675 reading
  float initialTemp = thermocouple.readCelsius();
  Serial.print("Initial temperature reading: ");
  Serial.print(initialTemp);
  Serial.println("°C");
  Serial.println("=========================");
  
  // Configure time (it will sync once WiFi is available)
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  // MQTT setup (connection will happen in loop)
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);

  delay(1500);
  display.clear();
  display.line(0,16,128,16,OLED_WHITE);
  display.update(); // Clear display after startup message
}

// Update network status display without blocking
// Shows connection status and activity indicators through small dots on display
void updateNetworkDisplay() {
  // MQTT upload activity indicator in top-right corner (data sent to broker)
  if (millis() - lastMqttUpload < mqttActivityIndicatorDuration) {display.dot(127, 0, OLED_WHITE); display.update(127, 0, 127, 0);} 
  else {display.dot(127, 0, OLED_BLACK);  display.update(127, 0, 127, 0);} 

  // Return if it's not time for the next status update
  if (millis() - lastDisplayUpdate < displayUpdateInterval) {
   return;  // Not time to update yet
  }
  lastDisplayUpdate = millis();
  updateDisplayStatus = !(updateDisplayStatus);  // Toggle blink state

  // Only update status indicators, not full display
  if (millis() - lastSendTime > sendInterval / 4) {
    ConnectionState state = networkManager.getState();
    bool mqttConnected = mqttClient.connected();
    
    // Make sure we're not overlapping with other display operations
    display.textMode(BUF_REPLACE);  // Ensure text writes in replace mode (not overlaid)

    // Draw the status indicators one by one
    // MQTT status indicator
    if (mqttConnected) {
      display.dot(127, 4, OLED_WHITE);  // MQTT dot
    } else if (state == CONN_CONNECTED) {
      // MQTT disconnected but WiFi connected - blink
      if (!updateDisplayStatus) display.dot(127, 4, OLED_BLACK); 
      else display.dot(127, 4, OLED_WHITE); 
    } else if ((state == CONN_DISCONNECTED) || (state == CONN_CONNECTION_FAILED)) display.dot(127, 4, OLED_BLACK);

    // WiFi status indicator
    if (state == CONN_CONNECTED) {
      display.dot(127, 8, OLED_WHITE);  // WiFi dot
    } else if (state == CONN_CONNECTING) {
      // Blinking WiFi indicator 
      if (!updateDisplayStatus) display.dot(127, 8, OLED_BLACK); 
      else display.dot(127, 8, OLED_WHITE); 
    } else if ((state == CONN_DISCONNECTED) || (state == CONN_CONNECTION_FAILED)) display.dot(127, 8, OLED_BLACK);

    // Use partial update to only refresh the status indicator area
    display.update(127, 4, 127, 8);  // Update only the status indicator region
  }
}

// Non-blocking MQTT reconnect helper
#define CLIENT_ID_LEN 24
bool mqttReconnect(int maxAttempts) {
  if (!networkManager.isConnected()) {
    return false;  // Can't connect to MQTT without WiFi
  }

  static int attempts = 0;
  static unsigned long lastAttemptTime = 0;
  const unsigned long attemptInterval = 1000;  // 1 second between attempts
  
  if (!mqttClient.connected()) {
    unsigned long now = millis();
    
    if (now - lastAttemptTime > attemptInterval) {
      lastAttemptTime = now;
      
      if (attempts < maxAttempts) {
        Serial.print("Debug: MQTT attempt ");
        Serial.println(attempts + 1);
        
        char clientId[CLIENT_ID_LEN];
        snprintf(clientId, CLIENT_ID_LEN, "NodeMCU-%lu", millis());
        if (mqttClient.connect(clientId, mqtt_user, mqtt_pass)) {
          mqttClient.subscribe("sensor/interval");
          mqttClient.subscribe("sensor/setpoint");
          mqttClient.subscribe("sensor/buzzer");  // Add buzzer control topic
          attempts = 0;  // Reset counter on success
          return true;
        } else {
          Serial.print("MQTT connection failed, state=");
          Serial.print(mqttClient.state());
          Serial.print(", attempt ");
          Serial.print(attempts + 1);
          Serial.print("/");
          Serial.println(maxAttempts);
          attempts++;
        }
      } else {
        attempts = 0;  // Reset for next time
        return false;
      }
    }
  } else {
    attempts = 0;  // Reset counter when connected
    return true;
  }
  
  return false;
}

// Main program loop - handles network, display, serial commands, and sensor readings
// Uses non-blocking approach to ensure responsive operation
void loop() {
  // Core functionality handling
  networkManager.update();    // Update network state (non-blocking)
  updateNetworkDisplay();     // Update network status on display (non-blocking)
  serialHandler();            // Handle incoming serial data (non-blocking)
  // Check if WiFi just connected and print status
  if (networkManager.justConnected()) {
    Serial.println("\n=========================");
    Serial.print("WiFi CONNECTED to: ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(networkManager.getLocalIP());
    Serial.println("=========================");
  }
  
  // MQTT connection management (only if WiFi connected)
  if (networkManager.isConnected()) {
    if (!mqttClient.connected()) {
      // Try reconnect without blocking if not connected
      if (!mqttWasConnected || (millis() % 10000) < 10) {  // First connect or try every ~10 seconds
        bool justConnected = mqttReconnect(1);  // Quick single attempt
        
        // Check if MQTT just connected and print status
        if (justConnected && !mqttWasConnected) {          Serial.println("\n=========================");
          Serial.print("MQTT CONNECTED to broker: ");
          Serial.print(mqtt_server);
          Serial.print(":");
          Serial.println(mqtt_port);
          Serial.println("Subscribed to sensor/interval, sensor/setpoint, and sensor/buzzer");
          Serial.println("Publishing to sensor/temperature");
          Serial.println("MQTT activity indicators: TX (↑), RX (↓) in display corners");
          Serial.println("=========================");
        }
        
        mqttWasConnected = justConnected;
      }
    } else {
      mqttClient.loop();
      
      // If MQTT just became connected
      if (!mqttWasConnected) {
        Serial.println("\n=========================");
        Serial.println("MQTT connection restored");
        Serial.println("=========================");
        mqttWasConnected = true;
      }
    }  
  } 
  else if (mqttWasConnected) {
    Serial.println("\n=========================");
    Serial.println("MQTT DISCONNECTED (WiFi lost)");
    Serial.println("=========================");
    mqttWasConnected = false;  // Reset when WiFi disconnects
  }

  // Store latest temperature reading
  static double tempC = 0.0;
  
  // Periodic temperature read & send
  unsigned long now = millis();
  if (now - lastSendTime >= sendInterval) {
    lastSendTime = now;
      
    // Read MAX6675 temperature
    tempC = thermocouple.readCelsius();  // Direct reading in Celsius
    tempValue = tempC;
    // Get current time (for timestamping in log mode)
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // Format and send output based on mode    
    if (outputMode.equals("log")) {
      softSerial.printf("%02d,%02d,%04d,%02d,%02d,%02d,%.2f\n",
                        timeinfo.tm_mday,
                        timeinfo.tm_mon + 1,
                        timeinfo.tm_year + 1900,
                        timeinfo.tm_hour,
                        timeinfo.tm_min,
                        timeinfo.tm_sec,
                        tempC);
    } else if(outputMode.equals("normal")) {
      softSerial.printf("%.2f\n", tempC);
    }    
    
    if (networkManager.isConnected() && mqttClient.connected()) {
      char buf[16];
      dtostrf(tempC, 0, 1, buf);
      
      // Publish temperature and update upload indicator      
      if (mqttClient.publish("sensor/temperature", buf)) {
        lastMqttUpload = millis(); // Mark upload activity time
      }    
    }
      // Control buzzer and interrupt pin based on temperature    
    playBuzzerAlarm(tempC, thresholdTemp);
  }

  displayUpdate();            // Update display 
}

// Handle incoming serial commands from both software and hardware serial
void serialHandler() {
  if (softSerial.available() || Serial.available()) {
    // Read from Software Serial if available, else from Hardware Serial
    String cmd = (softSerial.available()) ? softSerial.readStringUntil('\n') : Serial.readStringUntil('\n');
    cmd.trim(); 
    Serial.print("Command:");
    Serial.println(cmd);   

    if (cmd.equals("log")) {
      outputMode = "log";
      Serial.println("Debug: Switched to Log mode");
    } 
    else if (cmd.equals("normal")) {
      outputMode = "normal";
      Serial.println("Debug: Switched to Normal mode");
    } 
    else if (cmd.startsWith("interval")) {
      unsigned long v = cmd.substring(9).toInt();
      if (v > 0) {
        sendInterval = v;
        Serial.printf("Debug: Interval set to %lums\n", sendInterval);
      }    
    } 
    else if (cmd.startsWith("oled")) {
      unsigned long v = cmd.substring(5).toInt();
      if (v > 0) {
        mainDisplayUpdateInterval = v;
        Serial.printf("Debug: oled update interval set to %lums\n", mainDisplayUpdateInterval);
      }
    } 
    else if (cmd.startsWith("setpoint")) {
      double v = cmd.substring(9).toFloat();
      thresholdTemp = v;
      Serial.printf("Debug: Setpoint set to %.1f°C\n", thresholdTemp);    
    } 
    else if (cmd.startsWith("buzzer")) {
      unsigned int v = cmd.substring(7).toInt();
      Serial.println("Debug: Buzzer command received: " + String(v));
      buzzerEnabled = (v==1?true:false);
      if (buzzerEnabled) Serial.println("Debug: Buzzer enabled");
      else 
      {
        noTone(buzzerPin);  // Ensure buzzer is silent
        Serial.println("Debug: Buzzer disabled");
      }
    } 
    else if (cmd.startsWith("silence")) {      
      unsigned long duration = cmd.substring(8).toInt();
      buzzerSilenceUntil = millis() + duration * 1000UL; // Convert seconds to seconds 
      Serial.printf("Debug: Buzzer silenced for %luS\n", duration);
    } 
    else if (cmd == "reset") {
      Serial.println("Debug: Reset requested (display functionality removed)");
    }
    otherUpdate = true;
  }
}

// Update OLED display with current temperature and status information
void displayUpdate() {
  if((millis() - mainLastDisplayUpdateInterval) > mainDisplayUpdateInterval) {
    mainLastDisplayUpdateInterval = millis();
    
    // First update the top part (temperature)
    display.textMode(BUF_REPLACE);
    display.clear(0, 0, 126, 15);
    display.setScale(2);
    display.setCursor(0, 0);
    display.printf("%.2fC", tempValue);
    display.update(0, 0, 126, 15);
    
    // Then update the bottom part if needed
    if(otherUpdate) {
      otherUpdate = false;

      display.rect(0, 17, 126, 31, OLED_BLACK);
      display.setScale(1);
      display.setCursor(0, 3);
      display.printf("SET:%0.2fC", thresholdTemp);
      display.setCursor(72, 3);
      display.printf("|%s |%s", (outputMode.equals("log")?"LOG":"NRM"), (buzzerEnabled?"ON":"OFF"));
      display.update(0, 17, 126, 31);
    }
  }
}

// Controls buzzer based on temperature threshold
// Generates variable pitch based on how much temperature exceeds threshold
void playBuzzerAlarm(double temperature, double threshold) {
  if (buzzerEnabled && temperature > threshold) {
    // Map temp to frequency range (1000-2000Hz): higher temp = higher pitch
    int freq = map(temperature, threshold + 5, 100, 1000, 2000);
    tone(buzzerPin, freq);
  } else {
    noTone(buzzerPin);
  }
}

// MQTT message callback - processes incoming commands from broker
// Handles settings updates (interval, setpoint) and buzzer control commands
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Mark activity time for download indicator on display
  lastMqttDownload = millis();
  
  // Debug output to serial console
  Serial.print("MQTT message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  
  // Convert binary payload to string for easier handling
  String message;
  for (unsigned int i = 0; i < length; i++) {
    char c = (char)payload[i];
    Serial.print(c);
    message += c;
  }
  Serial.println();
    // Handle specific topics
  if (String(topic).equals("sensor/interval")) {
    unsigned long interval = message.toInt();
    if (interval > 0) {
      sendInterval = interval;
      Serial.printf("Send interval updated to %lu ms\n", sendInterval);
    }
  }
  else if (String(topic).equals("sensor/setpoint")) {
    double setpoint = message.toFloat();
    thresholdTemp = setpoint;
    Serial.printf("Temperature setpoint updated to %.1f°C\n", thresholdTemp);
    } else if (String(topic).equals("sensor/buzzer")){
    // Buzzer control message - format: "on", "off", "silence <seconds>"
    if (message.equals("on")) {
      buzzerEnabled = true;
      Serial.println("Buzzer enabled");
    } else if (message.equals("off")) {
      buzzerEnabled = false;
      noTone(buzzerPin);
      Serial.println("Buzzer disabled");
    } else if (message.startsWith("silence")) {
      int seconds = message.substring(8).toInt();
      if (seconds > 0) {
        buzzerSilenceUntil = millis() + seconds * 1000UL;
        Serial.printf("Buzzer silenced for %d seconds\n", seconds);
        noTone(buzzerPin);  // Immediately silence
      }
    }
  }
  otherUpdate = true;
}

// Animated splash screen implementation using frames stored in splashScreen.h
void logodisplay() {
    // Array of logo frames
    const unsigned char* frames[] = {
        logoFrames00, logoFrames01, logoFrames02, logoFrames03, 
        logoFrames04, logoFrames05, logoFrames06, logoFrames07,
        logoFrames08, logoFrames09, logoFrames10, logoFrames11,
        logoFrames12, logoFrames13, logoFrames14, logoFrames15,
        logoFrames16, logoFrames17, logoFrames18, logoFrames19,
        logoFrames20, logoFrames21, logoFrames22, logoFrames23,
        logoFrames24, logoFrames25, logoFrames26, logoFrames27,
        logoFrames28, logoFrames29, logoFrames30, logoFrames31,
        logoFrames32, logoFrames33, logoFrames34, logoFrames35,
        logoFrames36, logoFrames37, logoFrames38, logoFrames39,
        logoFrames40, logoFrames41, logoFrames42, logoFrames43,
        logoFrames44, logoFrames45, logoFrames46, logoFrames47,
        logoFrames48, logoFrames49, logoFrames50, logoFrames51,
        logoFrames52, logoFrames53, logoFrames54, logoFrames55,
        logoFrames56, logoFrames57, logoFrames58, logoFrames59,
        logoFrames60
    };
    
    const int totalFrames = sizeof(frames) / sizeof(frames[0]);
    const int frameDelay = 16; // Delay between frames in milliseconds
    
  // Play startup sound (if buzzer is enabled)
  if (buzzerEnabled) {
    // Play short welcome tone
    tone(buzzerPin, 1000, 50); 
    delay(100);
    tone(buzzerPin, 1500, 50);
  }
  
  // Display each frame of the animation
  for(int i = 0; i < totalFrames; i++) {
      display.clear();                    // Clear the display buffer
      display.drawBitmap(0, 0, frames[i], 128, 32, OLED_WHITE); // Draw the current frame
      display.update();                   // Update the display
      delay(frameDelay);   
    
    // Optional: Play a beep on certain frames for audio feedback
    if (buzzerEnabled && i == totalFrames - 1) {
      // Hold the final frame briefly before continuing
      delay(1000);
      tone(buzzerPin, 2000, 50);  // Final frame beep
    }
  }
  
  // Clear display before moving to the next screen
  display.clear();
  display.update();
}