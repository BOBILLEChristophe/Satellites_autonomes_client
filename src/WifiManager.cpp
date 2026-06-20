#include "WifiManager.h"

WifiManager::WifiManager() {}

void WifiManager::setReconnectIntervalMs(uint32_t ms)
{
  m_reconnectIntervalMs = ms;
}

void WifiManager::begin(const char *ssid, const char *password)
{
  m_ssid = ssid;
  m_password = password;
  // m_ssid = "Livebox-BC90";
  // m_password = "V9b7qzKFxdQfbMT4Pa";

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(false);

  attachEvents();
  connectNow();
}

void WifiManager::loop()
{
  if (m_gotIpPending)
  {
    m_gotIpPending = false;
    LOG_INFO("wifi connected - IP = %s", WiFi.localIP().toString().c_str());
  }

  if (isConnected())
    return;

  const uint32_t now = millis();

  if (now - m_lastAttemptMs >= m_reconnectIntervalMs)
    connectNow();
}

bool WifiManager::isConnected() const
{
  return m_connected && WiFi.status() == WL_CONNECTED;
}

String WifiManager::ip() const
{
  if (!isConnected())
    return String("");

  return WiFi.localIP().toString();
}

void WifiManager::connectNow()
{
  m_lastAttemptMs = millis();

  LOG_INFO("wifi connecting...");

  // Ne PAS utiliser WiFi.disconnect(true, true)
  // car cela peut casser/réinitialiser la pile réseau.
  WiFi.disconnect(false, false);
  delay(100);

  WiFi.begin(m_ssid, m_password);
}

void WifiManager::attachEvents()
{
  m_evtGotIp = WiFi.onEvent(
      [this](WiFiEvent_t, WiFiEventInfo_t)
      {
        m_connected = true;
        m_gotIpPending = true; // log différé dans loop()
      },
      ARDUINO_EVENT_WIFI_STA_GOT_IP);

  m_evtDisconnected = WiFi.onEvent(
      [this](WiFiEvent_t, WiFiEventInfo_t info)
      {
        m_connected = false;
        LOG_ERROR("wifi disconnected. Reason = %u",
                  (unsigned)info.wifi_sta_disconnected.reason);
      },
      ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
}

void WifiManager::detachEvents()
{
  if (m_evtGotIp)
    WiFi.removeEvent(m_evtGotIp);

  if (m_evtDisconnected)
    WiFi.removeEvent(m_evtDisconnected);

  m_evtGotIp = 0;
  m_evtDisconnected = 0;
}