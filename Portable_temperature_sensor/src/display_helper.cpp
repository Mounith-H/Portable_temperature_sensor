#include "display_helper.h"
#include "NetworkManager.h"

// External variables defined elsewhere
extern GyverOLED<SSD1306_128x32, OLED_BUFFER> display;
extern int currentX; // Current cursor position X
extern int currentY; // Current cursor position Y
extern int currentScale; // Current text scale

// Function to set text cursor position
void setTextCursor(int x, int y) {
  display.setCursor(x, y);
  currentX = x;
  currentY = y;
}

// Function to set text scale
void setTextScale(int scale) {
  display.setScale(scale);
  currentScale = scale;
}

// Function to print text (const char* version) 
void printText(const char* text) {
  // Skip any leading spaces (which otherwise show as white rectangles)
  const char* ptr = text;
  while (*ptr == ' ') ptr++;
  
  // Get current position from our tracking variables
  int x = currentX;
  int y = currentY;
  int scale = currentScale;  // Use the current text scale
  
  // Print the text character by character, skipping spaces
  while (*ptr) {
    if (*ptr != ' ') {
      // Save current position
      display.setCursor(x, y);
      display.print(*ptr);
      x += 6 * scale; // Move forward to character width (6 pixels * scale)
    } else {
      // For spaces, just move the cursor forward
      x += 6 * scale; // Space width (same as character width)
    }
    ptr++;
  }
  
  // Update our position tracking
  currentX = x;
  currentY = y;
}

// Function to print text (String version)
void printText(const String& text) {
  printText(text.c_str());
}

// Function to display temperature with specific formatting
void displayTemperature(double value, int x, int y, int scale, bool addDegreeSymbol) {
  // Format temperature to one decimal place
  char tempStr[10];
  sprintf(tempStr, "%.1f", value);
  
  // Set cursor and scale
  setTextCursor(x, y);
  setTextScale(scale);
  
  // Print formatted temperature
  display.print(tempStr);
  
  // Add degree symbol if requested
  if (addDegreeSymbol) {
    int strWidth = strlen(tempStr) * 6 * scale; // Approximate width
    
    // Add degree symbol
    setTextScale(scale > 1 ? scale - 1 : 1); // Smaller scale for symbol
    display.setCursor(x + strWidth, y);
    display.print((char)247); // ° symbol
    display.print("C");
    
    // Restore original scale
    setTextScale(scale);
  }
}

// This helper function will directly handle temperature display in the left half of the screen
void drawTemperatureScreen(double temperature, double setpoint, bool buzzerEnabled, unsigned long silenceUntil, String mode) {
  // Debug the temperature value to serial
  Serial.print("DEBUG: Drawing temperature screen with temp=");
  Serial.println(temperature);
  
  // Ensure the display is initialized properly before clearing
  display.clear();   // Clear the buffer
  display.update();  // Initial update to ensure the display is clear
  display.clear();   // Clear again for good measure
  
  // Draw vertical divider in the middle of the screen
  display.line(SCREEN_WIDTH/2, 0, SCREEN_WIDTH/2, SCREEN_HEIGHT-1, 1);
  
  // LEFT SIDE - Temperature display
  char tempStr[8];
  sprintf(tempStr, "%.1f", temperature);
  
  // MAXIMUM VISIBILITY APPROACH
  // Create a black filled rectangle for temperature display (white outline on black)
  for (int y = 2; y < SCREEN_HEIGHT-2; y++) {
    for (int x = 2; x < SCREEN_WIDTH/2-2; x++) {
      display.dot(x, y, 0); // Fill with black (0)
    }
  }
  
  // Draw border around temperature area (white border)
  display.rect(1, 1, SCREEN_WIDTH/2-2, SCREEN_HEIGHT-2, 1); 
  
  // Set text to be normal (white on black), which is default
  // Use a clearly visible scale for temperature
  display.setScale(2);  
  
  // Fixed positioning that works reliably
  int tempLength = strlen(tempStr);
  int charWidth = 12; // Width of each digit at scale 2
  
  // Center horizontally in the left half
  int centerX = (SCREEN_WIDTH/2 - tempLength * charWidth) / 2;
  if (centerX < 4) centerX = 4; // Minimum margin
  
  // Center vertically
  int centerY = (SCREEN_HEIGHT - 16) / 2; // 16 is approximate height of scale 2 text
  
  // DEBUG - print positioning information
  Serial.print("Temp string: ");
  Serial.print(tempStr);
  Serial.print(", length: ");
  Serial.print(tempLength);
  Serial.print(", position X: ");
  Serial.print(centerX);
  Serial.print(", Y: ");
  Serial.println(centerY);
  
  // Draw temperature
  display.setCursor(centerX, centerY);
  display.print(tempStr);
  
  // Add degree symbol and C with appropriate scale
  display.setScale(1); // Smaller scale for the symbols
  display.setCursor(centerX + (tempLength * charWidth), centerY + 2);
  display.print((char)247); // ° symbol
  display.setCursor(centerX + (tempLength * charWidth) + 6, centerY + 2);
  display.print('C');
  
  // Reset text to normal (black on white background) for the right side of screen
  display.invertText(false);
  
  // RIGHT SIDE - Parameters
  // Set threshold
  display.setScale(1);
  display.setCursor(SCREEN_WIDTH/2 + 3, 2);
  display.print("SET:");
  display.setCursor(SCREEN_WIDTH/2 + 25, 2);
  
  // Format setpoint
  char setStr[8];
  sprintf(setStr, "%.1f", setpoint);
  display.print(setStr);
  display.print((char)247); // Degree symbol
  display.print("C");
  
  // Add horizontal dividers on right side
  display.line(SCREEN_WIDTH/2+1, 12, SCREEN_WIDTH-1, 12, 1);
  
  // Buzzer status
  display.setCursor(SCREEN_WIDTH/2 + 3, 15);
  display.print("BUZ:");
  display.setCursor(SCREEN_WIDTH/2 + 25, 15);
  if (!buzzerEnabled) {
    display.print("OFF");
  } else if (millis() < silenceUntil) {
    display.print("MUTE");
  } else {
    display.print("ON");
  }
    // Mode indicator
  display.line(SCREEN_WIDTH/2+1, 24, SCREEN_WIDTH-1, 24, 1);
  display.setCursor(SCREEN_WIDTH/2 + 3, 26);
  display.print("MODE:");
  display.setCursor(SCREEN_WIDTH/2 + 32, 26);
  if (mode.equals("log")) {
    display.print("LOG");
  } else {
    display.print("NRM");
  }
  
  // Connection status indicators in the corners
  // WiFi indicator (bottom left)
  display.setCursor(2, SCREEN_HEIGHT-8);  if (networkManager.isConnected()) {
    display.print("W");
  } else if (networkManager.getState() == ConnectionState::CONN_CONNECTING) {
    if ((millis() / 500) % 2) {
      display.print("W");
    }
  } else {
    display.print("X");
  }
  
  // MQTT indicator (bottom right)
  display.setCursor(SCREEN_WIDTH-8, SCREEN_HEIGHT-8);
  if (mqttClient.connected()) {
    display.print("M");
  } else if (networkManager.isConnected()) {
    if ((millis() / 500) % 2) {
      display.print("M");
    }
  } else {
    display.print("X");
  }
  
  // Draw MQTT activity indicators in the top corners
  // Upload indicator (top left corner)
  if (millis() - lastMqttUpload < mqttActivityIndicatorDuration) {
    // Draw a simple arrow with lines instead of triangle
    display.line(5, 0, 5, 3, 1);   // Vertical line
    display.line(3, 2, 5, 0, 1);   // Left diagonal
    display.line(7, 2, 5, 0, 1);   // Right diagonal
  }
  
  // Download indicator (top right corner)
  if (millis() - lastMqttDownload < mqttActivityIndicatorDuration) {
    // Draw a simple arrow with lines instead of triangle
    display.line(SCREEN_WIDTH-5, 0, SCREEN_WIDTH-5, 3, 1);   // Vertical line
    display.line(SCREEN_WIDTH-3, 1, SCREEN_WIDTH-5, 3, 1);   // Right diagonal
    display.line(SCREEN_WIDTH-7, 1, SCREEN_WIDTH-5, 3, 1);   // Left diagonal
  }
  
  // Update the display
  display.update();
}
