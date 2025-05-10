#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <ESP8266WiFi.h>

enum ConnectionState {
  CONN_DISCONNECTED,
  CONN_CONNECTING,
  CONN_CONNECTED,
  CONN_CONNECTION_FAILED
};

class NetworkManager {
private:
  const char* _ssid;
  const char* _password;  unsigned long _lastAttemptTime = 0;
  unsigned long _reconnectInterval = 30000; // 30 seconds between connection attempts
  unsigned long _connectionStartTime = 0;
  unsigned long _connectionTimeout = 10000; // 10 seconds timeout for connection attempt
  ConnectionState _state = CONN_DISCONNECTED;
  bool _wasConnected = false; // Track previous connection state

public:
  NetworkManager(const char* ssid, const char* password) : _ssid(ssid), _password(password) {}

  void begin() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    _state = CONN_DISCONNECTED;
  }
  ConnectionState getState() {
    return _state;
  }

  bool isConnected() {
    return WiFi.status() == WL_CONNECTED;
  }
  void startConnection() {
    if (_state != CONN_CONNECTING) {
      WiFi.begin(_ssid, _password);
      _state = CONN_CONNECTING;
      _connectionStartTime = millis();
    }
  }
  void update() {
    // Check current WiFi status
    if (WiFi.status() == WL_CONNECTED) {
      // Detect state change from disconnected to connected
      if (_state != CONN_CONNECTED) {
        _wasConnected = false;
      }
      _state = CONN_CONNECTED;
    } else {
      // If we were previously connected, update state
      if (_state == CONN_CONNECTED) {
        _state = CONN_DISCONNECTED;
        _wasConnected = true; // Mark that we just disconnected
      }

      // If we're in connecting state, check timeout
      if (_state == CONN_CONNECTING) {
        if (millis() - _connectionStartTime > _connectionTimeout) {
          // Connection attempt timed out
          _state = CONN_CONNECTION_FAILED;
          _lastAttemptTime = millis();
        }
      }

      // If disconnected or failed, and enough time has passed, try to reconnect
      if ((_state == CONN_DISCONNECTED || _state == CONN_CONNECTION_FAILED) && 
          millis() - _lastAttemptTime > _reconnectInterval) {
        startConnection();
      }
    }
  }

  // Check if connection state just changed
  bool justConnected() {
    if (_state == CONN_CONNECTED && !_wasConnected) {
      _wasConnected = true;
      return true;
    }
    return false;
  }

  IPAddress getLocalIP() {
    return WiFi.localIP();
  }
};

#endif // NETWORK_MANAGER_H
