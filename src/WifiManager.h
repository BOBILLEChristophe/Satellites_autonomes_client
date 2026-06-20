#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "Debug.h"

class WifiManager {
public:
  WifiManager();

  void begin(const char *, const char *);
  void loop();                 // à appeler dans loop() pour gérer les retries
  bool isConnected() const;
  String ip() const;

  void setReconnectIntervalMs(uint32_t ms);

private:
  const char* m_ssid;
  const char* m_password;

  uint32_t m_reconnectIntervalMs = 5000;
  uint32_t m_lastAttemptMs = 0;
  bool m_gotIpPending = false;

  bool m_connected = false;

  // Gestion des événements ESP32
  WiFiEventId_t m_evtGotIp = 0;
  WiFiEventId_t m_evtDisconnected = 0;

  void attachEvents();
  void detachEvents();
  void connectNow();
};
