#ifndef DISPLAY_HELPER_H
#define DISPLAY_HELPER_H

#include <Arduino.h>
#include <GyverOLED.h>
#include <PubSubClient.h>
#include "NetworkManager.h"

// These definitions should match those in main.cpp
#ifndef SCREEN_WIDTH
#define SCREEN_WIDTH 128
#endif
#ifndef SCREEN_HEIGHT
#define SCREEN_HEIGHT 32
#endif

// Forward declarations for external variables needed by helper functions
extern GyverOLED<SSD1306_128x32, OLED_BUFFER> display;
extern PubSubClient mqttClient;
extern NetworkManager networkManager;
extern unsigned long lastMqttUpload;
extern unsigned long lastMqttDownload;
extern const unsigned long mqttActivityIndicatorDuration;

// Text handling function declarations
void setTextCursor(int x, int y);
void setTextScale(int scale);
void printText(const char* text);
void printText(const String& text);
void displayTemperature(double value, int x, int y, int scale, bool addDegreeSymbol);

// This helper function will directly handle temperature display in the left half of the screen
void drawTemperatureScreen(double temperature, double setpoint, bool buzzerEnabled, unsigned long silenceUntil, String mode);

#endif // DISPLAY_HELPER_H
