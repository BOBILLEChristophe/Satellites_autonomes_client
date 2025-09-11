/*

  Discovery.cpp


*/
#include "Discovery.h"

//    Node p00;     // Le satellite qui est dans le sens horaire (sans aiguille ou aiguilles 0 et 1 droites)
//    Node p01;     // Le satellite qui est dans le sens horaire (aiguille 0 déviée - si il y en a une, aiguille 2 droite)
//    Node p10;     // Le satellite qui est dans le sens horaire (aiguille 0 droite - aiguille 1 déviée)
//    Node p11;     // Le satellite qui est dans le sens horaire (aiguille 0 déviée - aiguille 2 déviée)
//    Node m00;     // Le satellite qui est dans le sens antihoraire (sans aiguille ou aiguilles 3 et 4 droites)
//    Node m01;     // Le satellite qui est dans le sens antihoraire (aiguille 3 déviée et, si il y en a une, aiguille 5 droite)
//    Node m10;     // Le satellite qui est dans le sens antihoraire (aiguille 3 droite - aiguille 4 déviée)
//    Node m11;     // Le satellite qui est dans le sens antihoraire (aiguille 3 déviée - aiguille 5 déviée)

byte Discovery::m_switchAig{0};
byte Discovery::m_btnState{0};
byte Discovery::m_ID_satPeriph{UNUSED_ID};
byte Discovery::m_comptAig{0};
bool Discovery::m_stopProcess{false};

void Discovery::ID_satPeriph(byte val) { m_ID_satPeriph = val; }
byte Discovery::ID_satPeriph() { return m_ID_satPeriph; }
void Discovery::comptAig(byte val) { m_comptAig = val; }
byte Discovery::comptAig() { return m_comptAig; }
void Discovery::btnState(byte val) { m_btnState = val; }
byte Discovery::btnState() { return m_btnState; }
void Discovery::stopProcess(bool stop) { m_stopProcess = stop; }
const gpio_num_t Discovery::m_aigPin[] = {AIG_PIN_SIGNAL_0, AIG_PIN_SIGNAL_1, AIG_PIN_SIGNAL_2, AIG_PIN_SIGNAL_3};
const gpio_num_t Discovery::m_pinIn[] = {BTN_SAT_MOINS, BTN_SAT_PLUS, INTER_DEV_2, INTER_DEV_1}; // 35 -> btn-, 36 -> btn+, 39 -> switch 2 , 34 -> switch 1
const gpio_num_t Discovery::m_pinLed = {LED_PIN_DISCOV};
Node *Discovery::node = nullptr;

// SemaphoreHandle_t Discovery::xSemaphore = NULL;

void Discovery::begin(Node *nd)
{
  node = nd;
  //--- Initialisation des boutons et switches
  for (byte i = 0; i < 4; i++)
    pinMode(m_pinIn[i], INPUT);
  pinMode(m_pinLed, OUTPUT);

  TaskHandle_t discoveryProcessHandle = nullptr;
  xTaskCreatePinnedToCore(process, "Process", 4 * 1024, (void *)node, 7, NULL, 1);
  xTaskCreatePinnedToCore(createAigEtCibles, "CreateAiguilles", 4 * 1024, (void *)node, 2, NULL, 0);
}

void Discovery::process(void *p)
{
  bool btnMoinsState{LOW};
  bool btnPlusState{LOW};
  bool ledAllumee{LOW};

  auto clignoterLED = [&]()
  {
    if (ledAllumee)
      digitalWrite(m_pinLed, LOW);
    else
      digitalWrite(m_pinLed, HIGH);
    ledAllumee = !ledAllumee;
  };

  auto allumerLED = [&]()
  {
    digitalWrite(m_pinLed, HIGH);
  };

  auto eteindreLED = [&]()
  {
    digitalWrite(m_pinLed, LOW);
  };

  auto btnPush = [&](uint8_t btnNum)
  {
    // Envoi sur le bus CAN de l'ID du satellite, commande 0xC0
    CanMsg::sendMsg(0, 0xC0, 0, node->ID(), UNUSED_ID, 0);

    if (m_ID_satPeriph < 253)
    {
      if (node->nodeP[btnNum] == nullptr)
        node->nodeP[btnNum] = new NodePeriph;
      node->nodeP[btnNum]->ID(m_ID_satPeriph);
      allumerLED();
      m_ID_satPeriph = UNUSED_ID;
    }
    else
      clignoterLED();
  };

  Node *node;
  node = (Node *)p;

  TickType_t xLastWakeTime;
  xLastWakeTime = xTaskGetTickCount();

  for (;;)
  {
    for (byte i = 0; i < 4; i++) // Lecture de l'état des BP et des switches
    {
      if (!digitalRead(m_pinIn[i]))
        m_btnState |= (1 << i); // m_btnState est une variable qui stocke l'etat des boutons et switches
                                // sous forme binaire
      else
        m_btnState &= ~(1 << i);
    }

    switch (m_btnState & 0x03) // Concerne les seuls BP
    {
    case 0x01: // Btn - actif
      btnPush(m_btnState >> 2);
      break;

    case 0x02: // Btn + actif
      btnPush((m_btnState >> 2) + 4);
      break;

    case 0x03: // Les deux boutons sont appuyés
      // --- Reset : delete tous les noeuds periph, les aiguilles et les signaux
      if (m_comptAig > 0)
      {
        for (byte i = 0; i < 5; i++)
        {
          clignoterLED();
          vTaskDelay(pdMS_TO_TICKS(100));
        }
      }
      for (byte i = 0; i < nodePsize; i++) // noeuds periph
      {
        if (node->nodeP[i] != nullptr)
        {
          delete node->nodeP[i];
          node->nodeP[i] = nullptr;
        }
      }
      for (byte i = 0; i < aigSize; i++) // aig
      {
        if (node->aig[i] != nullptr)
        {
          delete node->aig[i];
          node->aig[i] = nullptr;
        }
      }
      for (byte i = 0; i < signalSize; i++) // signaux
      {
        if (node->signal[i] != nullptr)
        {
          delete node->signal[i];
          node->signal[i] = nullptr;
        }
      }
      m_comptAig = 0;
      allumerLED();
      break;

    default:
      eteindreLED();
      break;
    }
    // debug.printf("[Discovery %d] : process runing\n", __LINE__);
    CanMsg::sendMsg(0, 0xC1, 0, node->ID(), UNUSED_ID, 0, node->masqueAig()); // Envoi du masqueAig sur le bus CAN
    if (m_stopProcess)
    {
      vTaskDelete(NULL);
    }
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100)); // toutes les x ms
  }
}

void Discovery::createAigEtCibles(void *p) // Création des aiguilles
{
  Node *node;
  node = (Node *)p;

  TickType_t xLastWakeTime;
  xLastWakeTime = xTaskGetTickCount();

  for (;;)
  {
    node->masqueAig(0x00);
    m_comptAig = 0;
    for (Aig *el : node->aig)
      el = nullptr;
    // node->cibleHoraire(0);
    // node->cibleAntiHor(0);

    auto createAig = [&](uint8_t index, uint8_t nodP0, uint8_t nodP1)
    {
      if (m_comptAig < 4)
      {
        if (node->aig[index] == nullptr)
          node->aig[index] = new Aig;

        node->aig[index]->ID(index);
        node->aig[index]->pin(m_aigPin[m_comptAig]);
        node->aig[index]->setup();
        node->aig[index]->nodePdroitIdx(nodP0);
        node->aig[index]->nodePdevieIdx(nodP1);

        node->m_masqueAig |= (1 << index);
        m_comptAig++;
      }
    };

    const byte aigConditions[aigSize][2] = {
        {p00, p01}, {p00, p10}, {p01, p11}, {m00, m01}, {m00, m10}, {m01, m11}};

    for (uint8_t i = 0; i < aigSize; i++)
    {
      auto condition = aigConditions[i];
      if (node->nodeP[condition[0]] != nullptr && node->nodeP[condition[1]] != nullptr)
        createAig(i, condition[0], condition[1]);
    }

    uint8_t x = 0;
    uint8_t y = 0;
    uint8_t index = 0;

    for (byte i = 0; i < 2; i++)
    {
      switch (i)
      {
      case 0: // Sens horaire
        x = 0;
        y = 3;
        index = p00;
        break;
      case 1: // Sens anti-horaire
        x = 3;
        y = 0;
        index = m00;
        break;
      }

      // Serial.print("m00 : ");
      // Serial.println(m00);

      // Serial.print("masque aig p00: ");
      // Serial.println(node->nodeP[p00]->masqueAig());

      // Serial.print("masque aig m00: ");
      // Serial.println(node->nodeP[m00]->masqueAig());

      byte typeCible = 0; // 3 feux par défaut

      if ((node->masqueAig()) & (1 << x)) // Il y a une aiguille a pied a horaire du canton S0 => RRalentissement (avec Carré)
      {
        // cas 1
        typeCible = 3; // Carre + RRalentissement -> pas besoin d'aller plus loin
      }
      else // Il n'y a pas d'aiguille a pied a horaire du canton S0
      {
        // On regarde si les nodeP directement relies ont des aiguilles à pied
        if (node->nodeP[index] != nullptr)
        {                                                 // Il y a un canton SP1 a horaire du canton
          if (node->nodeP[index]->masqueAig() & (1 << y)) // On cherche si l'aiguille 3 (anti horaire) de SP1 existe
          {
            // SP1 a au moins une aiguille a anti-horaire
            // cas 2
            typeCible = 1; // -> la cible est (au moins) un carré
          }

          else // SP1 n'a pas d'aiguille a anti-horaire
          {
            if (node->nodeP[index]->masqueAig() & (1 << x)) // On cherche si l'aiguille 0 (horaire) de SP1 existe
            {
              // SP1 a au moins une aiguille a horaire
              // cas 3
              typeCible = 2; // -> la cible est un ralentissement
            }
            /*else
            {
              // SP1 n'a pas d'aiguille a horaire
              // Si SP2 a une aiguille a anti-horaire

              // Si SP2 na pas d'aiguille a anti horaire
            }*/
          }
        }
        else // Il n'y a pas de canton a horaire de node S0
        {
          // cas 4
          typeCible = 1; // Signal "Carre" Arret imperatif
        }
      }

      if (index == p00)
      {
        if (node->signal[0] == nullptr)
          node->signal[0] = new Signal;
        node->signal[0]->type(typeCible);
        //debug.printf("[Discovery %d] : Type de Cible pour sortie horaire  : %d\n", __LINE__, typeCible);
      }
      else if (index == m00)
      {
        if (node->signal[1] == nullptr)
          node->signal[1] = new Signal;
        node->signal[1]->type(typeCible);
        //debug.printf("[Discovery %d] : Type de Cible pour sortie anti-hor : %d\n", __LINE__, typeCible);
      }
    }

    if (m_stopProcess)
    {
      Settings::writeFile();
      vTaskDelay(pdMS_TO_TICKS(1000));
      ESP.restart();
    }
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(2000)); // toutes les x ms
  }
}
