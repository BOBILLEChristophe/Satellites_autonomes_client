/*

   Settings.cpp


*/

#include "Settings.h"

bool Settings::WIFI_ON = true;
bool Settings::DISCOVERY_ON = true;
String Settings::ssid_str;
String Settings::password_str;
char Settings::ssid[30] = {};
char Settings::password[30] = {};
bool Settings::isMainReady = false;
Node *Settings::node = nullptr;
void Settings::mainReady(bool val) { Settings::isMainReady = val; }
bool Settings::discoveryOn() { return DISCOVERY_ON; }
void Settings::discoveryOn(bool val)
{
  Settings::DISCOVERY_ON = val;
  // Settings::writeFile();
}
bool Settings::wifiOn() { return WIFI_ON; }
void Settings::wifiOn(bool val)
{
  Settings::WIFI_ON = val;
}

/*-------------------------------------------------------------
                           setup
--------------------------------------------------------------*/

void Settings::setup(Node *nd)
{
  node = nd;  // Injection de la dépendance Node
  readFile(); // Chargement des paramètres sauvegardés (settings.json)
}

/*-------------------------------------------------------------
                           begin
--------------------------------------------------------------*/

bool Settings::begin()
{
  //--- Test de la présence de la carte Main
  uint8_t countReset = 1;
  do
  {
    CanMsg::sendMsg(1, 0xB2, 0, node->ID());
    vTaskDelay(pdMS_TO_TICKS(1000));
    LOG_INFO("Connexion CAN avec la carte Main %d/10", countReset);
    if (countReset++ == 10)
    {
      LOG_ERROR("Echec de la connexion CAN avec la carte Main");
      LOG_ERROR("Redemarrage dans 5 secondes");
      delay(5000);
      esp_restart();
    }
  } while (!isMainReady);

  // Identifiant du Node
  if (node->ID() == UNUSED_ID)
    LOG_ERROR("Le satellite ne possède pas d'identifiant");

  while (UNUSED_ID == node->ID()) // L'identifiant n'est pas en mémoire
  {
    //--- Requete identifiant
    CanMsg::sendMsg(1, 0xB3, 0, node->ID());
    vTaskDelay(pdMS_TO_TICKS(1000));
    if (node->ID() != UNUSED_ID)
      writeFile(); // Sauvegarde de donnees en flash
    else
      LOG_INFO(".");
  }
  LOG_INFO("End settings");
  return 0;
} //--- End begin

// /*-------------------------------------------------------------
//                            readFile
// --------------------------------------------------------------*/

void Settings::readFile()
{
  File file = SPIFFS.open("/settings.json", "r");
  if (!file)
  {
    LOG_ERROR("Failed to open settings.json");
    return;
  }

  // for info *****
  debug.printf("\nInformations du fichier \"settings.json\" : \n\n");
  String content = file.readString();
  Serial.println(content);
  file.seek(0); // Rewind the file to the beginning before deserializing
  //************* */

  size_t size = file.size();
  if (size > 2 * 1024)
  {
    LOG_ERROR("File size too large");
    file.close();
    return;
  }

  // Désérialisation
  DynamicJsonDocument doc(2 * 1024);
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  vTaskDelay(pdMS_TO_TICKS(100));

  if (error)
  {
    LOG_ERROR("Failed to read file, using default configuration");
    LOG_ERROR("DeserializationError: %s\n", error.c_str());
    return;
  }
  // ---
  node->ID(doc["idNode"] | UNUSED_ID);
  LOG_INFO("- ID node : %d", node->ID());

  Discovery::comptAig(doc["comptAig"]);
  node->masqueAig(doc["masqueAig"]);
  WIFI_ON = doc["wifi_on"];
  DISCOVERY_ON = doc["discovery_on"];
  ssid_str = doc["ssid"].as<String>();
  password_str = doc["password"].as<String>();
  strlcpy(Settings::ssid, ssid_str.c_str(), sizeof(Settings::ssid));
  strlcpy(Settings::password, password_str.c_str(), sizeof(Settings::password));
  uint16_t maxSpeed = doc["maxSpeed"];
  node->maxSpeed(maxSpeed);
  LOG_INFO("- node->maxSpeed : %d\n", node->maxSpeed());
  node->sensMarche(doc["sensMarche"]);

  // Nœuds
  const char *index[] = {"p00", "p01", "p10", "p11", "m00", "m01", "m10", "m11"};
  for (byte i = 0; i < nodePsize; i++)
  {
    if (doc[index[i]] != "null")
    {
      if (node->nodeP[i] == nullptr)
        node->nodeP[i] = new NodePeriph;
      node->nodeP[i]->ID(doc[index[i]]);
      LOG_INFO("- node->nodeP[%s]->id : %d", index[i], node->nodeP[i]->ID());
    }
    else
      LOG_INFO("- node->nodeP[%s]->id : NULL", index[i]);
  }

  // Aiguilles
  for (byte i = 0; i < aigSize; i++)
  {
    // debug.printf("valeur de aig %d : %s%c\n", i, doc["aig" + String(i)]);
    if (doc["aig" + String(i)] != "null")
    {
      if (node->aig[i] == nullptr)
        node->aig[i] = new Aig;
      node->aig[i]->ID(doc["aig" + String(i) + "id"]);
      node->aig[i]->posDroit(doc["aig" + String(i) + "posDroit"]);
      node->aig[i]->posDevie(doc["aig" + String(i) + "posDevie"]);
      node->aig[i]->speed(doc["aig" + String(i) + "speed"]);
      node->aig[i]->pin(doc["aig" + String(i) + "pin"]);
      node->aig[i]->setup();

      LOG_INFO("- Creation de l'aiguille %d", i);
    }
  }

  // Signaux
  for (byte i = 0; i < signalSize; i++)
  {
    if (doc["sign" + String(i)] != "null")
    {
      if (node->signal[i] == nullptr)
        node->signal[i] = new Signal;
      node->signal[i]->type(doc["sign" + String(i) + "type"]);
      node->signal[i]->position(doc["sign" + String(i) + "position"]);

      LOG_INFO("- Creation du signal %d", i);
    }
  }
} //--- End readFile

// /*-------------------------------------------------------------
//                            writeFile
// --------------------------------------------------------------*/

void Settings::writeFile()
{
  File file = SPIFFS.open("/settings.json", "w");
  if (!file)
  {
    LOG_ERROR("Failed to open settings.json");
    return;
  }

  DynamicJsonDocument doc(2 * 1024);

  doc["idNode"] = node->ID();
  doc["comptAig"] = Discovery::comptAig();
  doc["masqueAig"] = node->masqueAig();
  doc["wifi_on"] = WIFI_ON;
  doc["discovery_on"] = DISCOVERY_ON;
  doc["ssid"] = ssid;
  doc["password"] = password;
  doc["maxSpeed"] = node->maxSpeed();
  doc["sensMarche"] = node->sensMarche();

  // Nœuds
  const String index[] = {"p00", "p01", "p10", "p11", "m00", "m01", "m10", "m11"};
  for (byte i = 0; i < nodePsize; i++)
  {
    if (node->nodeP[i] == nullptr)
      doc[index[i]] = "null";
    else
      doc[index[i]] = node->nodeP[i]->ID();
  }

  // Aiguilles
  for (byte i = 0; i < aigSize; i++)
  {
    if (node->aig[i] == nullptr)
      doc["aig" + String(i)] = "null";
    else
    {
      doc["aig" + String(i) + "id"] = node->aig[i]->ID();
      doc["aig" + String(i) + "posDroit"] = node->aig[i]->posDroit();
      doc["aig" + String(i) + "posDevie"] = node->aig[i]->posDevie();
      doc["aig" + String(i) + "speed"] = node->aig[i]->speed();
      doc["aig" + String(i) + "pin"] = node->aig[i]->pin();
    }
  }

  // Signaux
  for (byte i = 0; i < signalSize; i++)
  {
    if (node->signal[i] == nullptr)
      doc["sign" + String(i)] = "null";
    else
    {
      doc["sign" + String(i) + "type"] = node->signal[i]->type();
      doc["sign" + String(i) + "position"] = node->signal[i]->position();
    }
  }

  String output;
  serializeJson(doc, output);
  file.print(output);
  file.close();

  LOG_INFO("Sauvegarde des datas en FLASH");
} //--- End writeFile
