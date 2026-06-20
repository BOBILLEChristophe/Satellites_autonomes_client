

/*

copyright (c) 2022 christophe.bobille - LOCODUINO - www.locoduino.org

v 0.11.8 : Ajout de la détection de présence par consommation de courant
v 0.11.9 : Correction de divers petits bugs après essais sur réseau
v 0.12.0 : Plusieurs bugs corrigés pour la signalisation
v 0.12.1 : Modification importantes des structures de message CAN
v 0.13.0 : Mise à jour importante Ajout de fonctionnalités
v 0.13.1 : Correction d'un bug sur les commandes d'aiguille
v 0.13.2 : Petits ajustements
v 0.14.0 : Evolutions majeures pour la détection et l'envoi de commandes CAN a laBox
v 0.14.1 : Introduction de l'information "canton reservé" (Node::m_reserved)

*/

//---  Test si ESP32
#ifndef ARDUINO_ARCH_ESP32
#error "Select an ESP32 board"
#endif

#define PROJECT "Satellites autonomes (client)"
#define VERSION "v 0.21.1"
#define AUTHOR "christophe BOBILLE : christophe.bobille@gmail.com"

//--- Fichiers inclus
#include <Arduino.h>
#include <ArduinoOTA.h>
#include "CanMsg.h"
#include "CanManager.h"
#include "Config.h"
#ifdef CHIP_INFO
#include "ChipInfo.h"
#endif
#include "CurrentConsumpt.h"
#include "Debug.h"
#include "Discovery.h"
#include "TrafficManager.h"
#include "Node.h"
#include "Railcom.h"
#include "Settings.h"
#include "SignauxCmd.h"
#include "Storage.h"
#include "WebHandler.h"
#include "WifiManager.h"
#include "freertos/queue.h"

// Instances
Node node;
Railcom railcom(RAILCOM_RX, RAILCOM_TX);
// TrafficManager trafficManager(&node);
WifiManager wifi;
WebHandler webHandler;
CurrentConsumpt currentConsumpt;

// Var globale
bool wifiOn;

/*-------------------------------------------------------------
                           setup
--------------------------------------------------------------*/

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    ;
  delay(100);

//--- Infos ESP32 (desactivable)
#ifdef CHIP_INFO
  ChipInfo::print();
#endif

  Serial.printf("\nProject   :    %s", PROJECT);
  Serial.printf("\nVersion   :    %s", VERSION);
  Serial.printf("\nAuteur    :    %s", AUTHOR);
  Serial.printf("\nFichier   :    %s", __FILE__);
  Serial.printf("\nCompiled  :    %s", __DATE__);
  Serial.printf(" - %s\n\n", __TIME__);
  Serial.printf("-----------------------------------\n\n");

  Storage::begin(true); // Montage du système de fichiers SPIFFS
  vTaskDelay(pdMS_TO_TICKS(100));
  Settings::setup(&node);
  vTaskDelay(pdMS_TO_TICKS(100));
  //--- Configure ESP32 CAN
  CanManager::setup();
  vTaskDelay(pdMS_TO_TICKS(100));
  CanMsg::setup(&node);
  vTaskDelay(pdMS_TO_TICKS(100));

  bool err = 0;
  if (err == Settings::begin())
  {
    Serial.printf("-----------------------------------\n");
    Serial.printf("ID Node : %d\n", node.ID());
    Serial.printf("-----------------------------------\n\n");
  }
  else
  {
    Serial.printf("[Settings] : Echec de la configuration\n");
    return;
  }

  //Settings::wifiOn(true);

  if (Settings::discoveryOn()) // Si option validee, lancement de la méthode pour le procecuss de decouverte
  {
    Discovery::begin(&node);
    Settings::wifiOn(true);
  }
  else
  {
    // Settings::wifiOn(false);
    for (byte i = 0; i < signalSize; i++)
    {
      if (node.signal[i] == nullptr)
        node.signal[i] = new Signal;
      node.signal[i]->setup();
    }
    railcom.begin();
    SignauxCmd::setup();
    TrafficManager::setup(&node);
    currentConsumpt.setup(&node, CONSO_COURANT_PIN);
  }
  //--- Wifi et web serveur
  if (Settings::wifiOn()) // Si option validee
  {
    wifi.begin(Settings::ssid, Settings::password);
    webHandler.init(&node, 80);
  }

  Serial.printf(Settings::discoveryOn() ? "[Discovery] : on\n" : "[Discovery] : off\n");
  Serial.printf(Settings::wifiOn() ? "[Wifi] : on\n" : "Wifi : off\n");
  Serial.printf("-----------------------------------\n");
  Serial.printf("[Main %d] : End setup\n\n", __LINE__);
  Serial.printf("-----------------------------------\n\n");
#ifndef debug
  vTaskDelay(pdMS_TO_TICKS(1000));
  Serial.end(); // Desactivation de Serial
  Serial.println("Ne doit pas s'afficher !");
#endif
  wifiOn = Settings::wifiOn();

  ArduinoOTA.setHostname("satellite_client");
  ArduinoOTA.onStart([]() { LOG_INFO("OTA start"); });
  ArduinoOTA.onEnd([]() { LOG_INFO("OTA end"); });
  ArduinoOTA.onError([](ota_error_t error){ LOG_ERROR("OTA error %u", error); });
  ArduinoOTA.begin();

} // ->End setup

/*-------------------------------------------------------------
                           loop
--------------------------------------------------------------*/

void loop()
{
  static uint16_t oldAddress = 0;

  //******************** Ecouteur page web **********************************

  if (wifiOn)          // Si option validée
  {
    ArduinoOTA.handle();
    webHandler.loop(); // ecoute des ports web 80 et 81
  }

  if (!Settings::discoveryOn()) // Si option non validée
  {
    //************************* Railcom ****************************************
    // if (railcom.address() && node.busy())
    // {
    node.loco.address(railcom.address());
    // }
    if (node.loco.address() != oldAddress)
    {
      if (node.loco.address() > 1)
        LOG_INFO("Railcom - Adresse de loco : %d", node.loco.address());
      else
        LOG_INFO("Railcom - Pas de loco");
      oldAddress = node.loco.address();
    }
  }
  //**************************************************************************
  vTaskDelay(pdMS_TO_TICKS(50));
} // ->End loop
