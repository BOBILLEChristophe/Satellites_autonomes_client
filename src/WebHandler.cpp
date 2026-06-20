/*

   WebHandler.cpp


*/

#include "WebHandler.h"

WebHandler::WebHandler() : _server(nullptr), _ws(nullptr) {}

void WebHandler::init(Node *node, uint16_t webPort)
{
  _server = new AsyncWebServer(webPort);
  _ws = new AsyncWebSocket("/ws");
  _ws->onEvent(std::bind(&WebHandler::WsEvent, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));

  WebHandler::route();

  _server->addHandler(_ws);
  _server->begin();

  this->node = node;
}

void WebHandler::loop()
{
  //-> Ajout 16/06/26 22:51
  static uint32_t lastNotify = 0;

  if (millis() - lastNotify >= 500)
  {
    lastNotify = millis();

    if (_ws != nullptr)
    {
      notifyClients();
    }
  }
  //-> End ajout

  _ws->cleanupClients();
}

void WebHandler::WsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  switch (type)
  {
  case WS_EVT_CONNECT:
#ifdef DEBUG
    debug.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
#endif
    WebHandler::notifyClients();
    break;
  case WS_EVT_DISCONNECT:
#ifdef DEBUG
    debug.printf("WebSocket client #%u disconnected\n", client->id());
#endif
    break;

  case WS_EVT_ERROR:
    //
    break;
  case WS_EVT_PONG:
    //
    break;
  case WS_EVT_DATA:

#ifdef DEBUG
    debug.printf("[WebHandler %d] : WebSocket len %d\n", __LINE__, len);
    debug.printf("[WebHandler %d] : WebSocket data %d\n", __LINE__, data);
#endif

    StaticJsonDocument<4066> doc1; // Memory pool
    DeserializationError error = deserializeJson(doc1, data);

    if (error) // Check for errors in parsing
    {
#ifdef DEBUG
      debug.println("Parsing failed");
#endif
    }
    else
    {
      String message = (char *)data;

      if (message.indexOf("servoSettings") >= 0)
      {
        const char *servoId = doc1["servoSettings"][0];
        const uint16_t servoValue = doc1["servoSettings"][1];
        const uint8_t servoName = doc1["servoSettings"][2];
#ifdef DEBUG
        debug.printf("servoId %s\n", servoId);
        debug.printf("servoValue %d\n", servoValue);
        debug.printf("servoName %d\n", servoName);
#endif

        if ('0' == servoId[2])
        {
          node->aig[servoName]->posDroit(servoValue);
          node->aig[servoName]->curPos(servoValue);
          node->aig[servoName]->move(servoValue);
        }
        else if ('1' == servoId[2])
        {
          node->aig[servoName]->posDevie(servoValue);
          node->aig[servoName]->curPos(servoValue);
          node->aig[servoName]->move(servoValue);
        }
        else if ('2' == servoId[2])
        {
          // node->aig[servoName]->sSpeed((1 / (float)servoValue) * 10000);
          uint16_t speed = 11000 - (servoValue * 1000);
          node->aig[servoName]->speed(speed);
#ifdef DEBUG
          debug.printf("speed %d\n", node->aig[servoName]->speed());
          debug.println("------------------------------");
          debug.printf("servo    : %d\n", servoName);
          debug.printf("servoValue : %d\n", servoValue);
          debug.println("------------------------------");
#endif
        }
      }
      // Test de déplacement "en vraie grandeur" de l'aiguille
      if (message.indexOf("servoTest") >= 0)
      {
        const uint8_t servoName = doc1["servoTest"][0];
#ifdef DEBUG
        debug.println("servoTest");
        debug.printf("servo     : %d\n", servoName);
        debug.printf("gPosDroit : %d\n", node->aig[servoName]->posDroit());
        debug.printf("gPosDevie : %d\n", node->aig[servoName]->posDevie());
#endif
        node->aigRun(servoName);
      }
      if (message.indexOf("wifi_on") >= 0)
      {
        const bool wifi_on = doc1["wifi_on"][0];
        if (wifi_on)
          Settings::wifiOn(true);
        else
          Settings::wifiOn(false);
        Settings::writeFile();
        delay(1000);
        ESP.restart();
#ifdef DEBUG
        debug.printf("[discovery_on %d] : wifi_on %d\n", __LINE__, wifi_on);
#endif
      }

      if (message.indexOf("discovery_on") >= 0)
      {
        const bool discovery_on = doc1["discovery_on"][0];
        if (discovery_on)
        {
          Settings::discoveryOn(true);
        }
        else
        {
          Settings::discoveryOn(false);
          Discovery::stopProcess(true);
        }
        Settings::writeFile();

#ifdef DEBUG
        debug.printf("[discovery_on %d] : discovery_on %d\n", __LINE__, discovery_on);
#endif
      }

      if (message.indexOf("maxSpeed") >= 0)
      {
        const uint8_t maxSpeed = doc1["maxSpeed"][0];
        node->maxSpeed(maxSpeed);
#ifdef DEBUG
        debug.printf("[maxSpeed %d] : maxSpeed %d\n", __LINE__, maxSpeed);
#endif
      }

      if (message.indexOf("save") >= 0)
      {
#ifdef DEBUG
        debug.println("save");
#endif
        Settings::writeFile();
      }

      if (message.indexOf("restartEsp") >= 0)
      {
#ifdef DEBUG
        debug.println("restartEsp");
#endif
        ESP.restart();
      }
    }
    break;
  }
}

void WebHandler::notifyClients()
{
  //-> https://m1cr0lab-esp32.github.io/remote-control-with-websocket/websocket-and-json/
  //-> https://arduinojson.org/v6/api/jsonobject/

  //-> Ajout 16/06/26 22:52
  if (_ws == nullptr)
    return;

  if (_ws->count() == 0)
    return;
  //-> End ajout

  StaticJsonDocument<1024> doc;

  doc["idNode"] = node->ID();

  // Nœuds
  String index[] = {"p00", "p01", "p10", "p11", "m00", "m01", "m10", "m11"};
  for (byte i = 0; i < 8; i++)
  {
    if (node->nodeP[i] == nullptr)
      doc[index[i]] = "null";
    else
      doc[index[i]] = node->nodeP[i]->ID();
  }

  // Locomotive
  doc["locoSpeed"] = node->loco.speed();
  if (node->loco.address() > 1)
  {
    doc["locoAddress"] = node->loco.address();
  }
  else
  {
    doc["locoAddress"] = "--";
    doc["locoSpeed"] = "--";
  }

  doc["locoRailMode"] = node->loco.railMode(); // 2R / 3R
  doc["locoNetworkDir"] = node->loco.sens();   // sens réel sur le réseau

  switch (node->loco.direction())
  {

  case 1:
    doc["locoDirection"] = "Avant";
    break;

  case 2:
    doc["locoDirection"] = "Arrière";
    break;

  default:
    doc["locoDirection"] = "--";
    break;
  }

  // Aiguilles
  for (byte i = 0; i < aigSize; i++)
  {
    if (node->aig[i] == nullptr)
    {
      doc["s" + String(i)] = "null";
      doc["s" + String(i) + "0"] = "";
      doc["s" + String(i) + "1"] = "";
      doc["s" + String(i) + "2"] = "";
    }
    else
    {
      doc["s" + String(i)] = "Actif";
      doc["s" + String(i) + "0"] = node->aig[i]->posDroit();
      doc["s" + String(i) + "1"] = node->aig[i]->posDevie();
      doc["s" + String(i) + "2"] = (11000 - node->aig[i]->speed()) / 1000;
    }
  }

  doc["wifi_on"] = Settings::wifiOn();
  doc["discovery_on"] = Settings::discoveryOn();

  doc["maxSpeed"] = node->maxSpeed();
  doc["sensMarche"] = node->sensMarche();

  if (node->signal[0] != nullptr)
    doc["cibleHoraire"] = node->signal[0]->type();
  if (node->signal[1] != nullptr)
    doc["cibleAntiHor"] = node->signal[1]->type();

  String output;
  serializeJson(doc, output);
  _ws->textAll(output);
}

void WebHandler::route()
{
  // Route for root / web page
  _server->on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/index.html", "text/html"); });

  _server->on("/w3.css", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/w3.css", "text/css"); });

  _server->on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/style.css", "text/css"); });

  _server->on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/script.js", "text/javascript"); });

  _server->on("/settings.json", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/settings.json", "text/json"); });

  _server->on("/favicon.png", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/favicon.png", "image/png"); });

  _server->on("/cible_0.jpg", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/cible_0.jpg", "image/jpg"); });

  _server->on("/cible_1.jpg", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/cible_1.jpg", "image/jpg"); });

  _server->on("/cible_2.jpg", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/cible_2.jpg", "image/jpg"); });

  _server->on("/cible_3.jpg", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/cible_3.jpg", "image/jpg"); });

  _server->onNotFound([](AsyncWebServerRequest *request)
                      {
    Serial.printf("Not found: %s!\r\n", request->url().c_str());
    request->send(404); });
}