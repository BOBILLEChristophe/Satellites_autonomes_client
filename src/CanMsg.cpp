/*

  CanMsg.cpp


*/

#include "CanMsg.h"

static constexpr uint8_t SENS_INCONNU = 0;
static constexpr uint8_t SENS_HORAIRE = 1;
static constexpr uint8_t SENS_ANTIHOR = 2;

static const char *sensName(uint8_t sens)
{
  switch (sens)
  {
  case SENS_HORAIRE:
    return "HORAIRE";
  case SENS_ANTIHOR:
    return "ANTIHOR";
  default:
    return "INCONNU";
  }
}

static const char *dccDirName(uint8_t dir)
{
  switch (dir)
  {
  case 1:
    return "FWD";
  case 2:
    return "REV";
  default:
    return "INCONNU";
  }
}

void CanMsg::setup(Node *node)
{
  TaskHandle_t canReceiveHandle = NULL;
  xTaskCreatePinnedToCore(canReceiveMsg, "CanReceiveMsg", 4 * 1024, (void *)node, 6, &canReceiveHandle, 0); // Création de la tâches pour le traitement
#ifdef TEST_MEMORY_TASK
  xTaskCreate(testMemory, "TestMemory", 2 * 1024, (void *)canReceiveHandle, 2, NULL); // Création de la tâches pour le traitement
#endif
}

#ifdef TEST_MEMORY_TASK
void CanMsg::testMemory(void *pvParameters)
{
  UBaseType_t canReceiveMsg = 0;
  for (;;)
  {
    TaskHandle_t canReceiveHandle;
    canReceiveHandle = pvParameters;
    canReceiveMsg = uxTaskGetStackHighWaterMark(canReceiveHandle);
    LOG_DEBUG("canReceiveMsg free memory = %d bytes", canReceiveMsg);
    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
}
#endif

/*--------------------------------------
  Reception CAN
  --------------------------------------*/

void CanMsg::canReceiveMsg(void *pvParameters)
{
  Node *node;
  node = (Node *)pvParameters;

  TickType_t xLastWakeTime;
  xLastWakeTime = xTaskGetTickCount();

  for (;;)
  {
    CANMessage frameIn;
    if (ACAN_ESP32::can.receive(frameIn))
    {
      // 28     25 24      17 16 15           0
      //+---------+----------+--+--------------+
      //| priorité| commande |R | expéditeur   |
      //+---------+----------+--+--------------+
      const uint8_t priorite = (frameIn.id >> 25) & 0x0F;   // priorité
      const uint8_t commande = (frameIn.id >> 17) & 0xFF;   // Code de la commande
      const bool response = ((frameIn.id >> 16) & 0x01);    // Reponse
      const uint16_t idSatExpediteur = frameIn.id & 0xFFFF; // ID de l'expediteur

#ifdef DEBUG
      // debug.printf("\n[CanMsg %d]------ Expediteur %d : commande 0x%0X\n", __LINE__, idSatExpediteur, commande);
#endif
      if (frameIn.rtr) // Remote frame
      {
#ifdef DEBUG
        debug.printf("[CanMsg %d Frame de remote \n", __LINE__);
#endif
        switch (commande)
        {
        case 0xB0:
          ACAN_ESP32::can.tryToSend(frameIn);
          break;
        }
      }
      else
      {
        switch (commande) // commande appelee
        {
        case 0x04: // ACK de laBox pour la commande de vitesse avec bit response = 1
        {
          if (response && frameIn.len >= 6)
          {
            uint16_t ackAddr =
                ((uint16_t)frameIn.data[2] << 8) |
                frameIn.data[3];

            uint16_t ackSpeed =
                ((uint16_t)frameIn.data[4] << 8) |
                frameIn.data[5];

            if (node->m_pendingLocoCmd &&
                ackAddr == node->m_pendingLocoAddr &&
                ackSpeed == node->m_pendingLocoSpeed)
            {
              node->m_ackLocoCmd = true;
              node->m_pendingLocoCmd = false;

              // LOG_INFO("ACK validé loco %u vitesse %u", ackAddr, ackSpeed);
            }
          }
          break;
        }
          //         case 0xAA: // Retour commande throttle depuis LaBox / DCC-EX
          //         {
          //           if (frameIn.len < 7)
          //           {
          // #ifdef DEBUG
          //             debug.printf("[CanMsg %d] Commande 0xAA invalide : len=%d\n", __LINE__, frameIn.len);
          // #endif
          //             break;
          //           }
          //           // Loc-ID reçue : 00 00 C0 xx
          //           uint32_t locId =
          //               ((uint32_t)frameIn.data[0] << 24) |
          //               ((uint32_t)frameIn.data[1] << 16) |
          //               ((uint32_t)frameIn.data[2] << 8) |
          //               ((uint32_t)frameIn.data[3]);

          //           // Extraction adresse DCC depuis Loc-ID Märklin/DCC
          //           uint16_t cab =
          //               ((uint16_t)(frameIn.data[2] & 0x3F) << 8) |
          //               frameIn.data[3];

          //           uint16_t speed =
          //               ((uint16_t)frameIn.data[4] << 8) |
          //               frameIn.data[5];

          //           uint8_t direction = 0;
          //           if (frameIn.data[6] == 0)
          //             direction = 1;
          //           else if (frameIn.data[6] == 1)
          //             direction = 2;

          // #ifdef DEBUG
          //           debug.printf("[CanMsg %d] CMD 0xAA throttle : locId=0x%08lX cab=%u speed=%u dir=%d\n",
          //                        __LINE__,
          //                        (unsigned long)locId,
          //                        cab,
          //                        speed,
          //                        direction);
          // #endif

          //           // Mise à jour de la loco locale du canton
          //           node->loco.address(cab);
          //           node->loco.speed(speed);

          //           // À adapter selon ta convention actuelle :
          //           // 1 = horaire / forward
          //           // 2 = antihoraire / reverse
          //           node->loco.direction(direction);

          //           break;
          //         }

        case 0xAB: // Requête/réponse base locomotives
        {
          if (!response)
          {
            LOG_INFO("Requête base loco reçue 0xAB");
            break;
          }

          if (frameIn.len < 7)
          {
            LOG_ERROR("Réponse base loco 0xAB invalide len=%d", frameIn.len);
            break;
          }

          uint16_t addr =
              ((uint16_t)(frameIn.data[2] & 0x3F) << 8) |
              frameIn.data[3];

          if (addr <= 1)
            break;

          uint16_t speed =
              ((uint16_t)frameIn.data[4] << 8) |
              frameIn.data[5];

          uint8_t direction = frameIn.data[6]; // 0 = FWD, 1 = REV

          TrafficState state = static_cast<TrafficState>(node->trafficState());

          uint8_t dccDir = (direction == 0) ? 1 : 2;

          if (state == TrafficState::RESERVED)
          {
            if (addr != node->reservedBy())
              break;

            node->reservedLoco.address(addr);
            node->reservedLoco.speed(speed);
            node->reservedLoco.direction(dccDir);

            LOG_INFO("Reponse base loco 0xAB reservedLoco : addr=%u speed=%u dccDir=%s netSens=%s",
                     addr,
                     speed,
                     dccDirName(node->reservedLoco.direction()),
                     sensName(node->reservedSens()));
            break;
          }

          if (node->loco.address() > 1 && addr != node->loco.address())
            break;

          node->loco.address(addr);
          node->loco.speed(speed);
          node->loco.direction(dccDir);

          LOG_INFO("Reponse base loco 0xAB loco : addr=%u speed=%u dccDir=%s netSens=%s",
                   addr,
                   speed,
                   dccDirName(node->loco.direction()),
                   sensName(node->loco.sens()));
        }

        case 0xB2: // fn : Reponse à demande de test du bus CAN
          LOG_INFO("Reponse demande de test du bus CAN");
          if (response && frameIn.data[0])
            Settings::mainReady(true);
          break;
        case 0xB4: // fn : Reponse à demande d'identifiant (0xB4)
          if (response && node->ID() == UNUSED_ID)
            node->ID(frameIn.data[0]);
          break;
        case 0xBC: // Reset ESP32
          ESP.restart();
          break;
        case 0xBD: // Activation  - desactivation du WiFi
          Serial.print("desactivation du WiFi : ");
          Serial.println(frameIn.data[0]);
          Settings::wifiOn(frameIn.data[0]);
          Serial.print("desactivation du WiFi : ");
          Serial.println(Settings::wifiOn());
          Settings::writeFile();
          delay(1000);
          ESP.restart();
          break;
        case 0xBE: // Activation  - desactivation du mode Discovery
          if (frameIn.data[0])
          {
            Settings::discoveryOn(true);
            Settings::writeFile();
            delay(1000);
            ESP.restart();
          }
          else
          {
            Settings::discoveryOn(false);
            Discovery::stopProcess(true);
          }
          Settings::writeFile();
          break;
        case 0xBF: // fn : Enregistrement des données en mémoire flash
#ifdef SAUV_BY_MAIN
          LOG_INFO("[CanMsg.cpp %d] ------ Rec->sauvegarde distante", __LINE__);
          Settings::writeFile();
#else
          LOG_INFO("[CanMsg.cpp %d] ------ Sauvegarde automatique desactivee.", __LINE__);
#endif
          break;

        case 0xC0: // fn : Réception de l'ID d'un satellite
          Discovery::ID_satPeriph(idSatExpediteur);
          break;

        case 0xC1: // reception periodique des data envoyees par les sat pendant le processus de decouverte

          LOG_INFO("[CanMsg.cpp %d] commande 0xC1, ID exped %d ", __LINE__, idSatExpediteur);

          for (auto el : node->nodeP)
          {
            if (el != nullptr)
            {
              if (idSatExpediteur == el->ID()) // Si l'expediteur est un SP1 ou un SM1
              {
                el->masqueAig(frameIn.data[0]);
                // Serial.print("el.id = ");Serial.println(el->ID());
                // Serial.print("el.masqueAig = ");Serial.println(el->masqueAig());
              }
            }
          }
          break;

        case 0xE0:
        {
          if (frameIn.len < 7)
            break;

          NodePeriph *periph = nullptr;
          uint8_t periphIdx = UNUSED_ID;

          for (uint8_t i = 0; i < nodePsize; i++)
          {
            if (node->nodeP[i] != nullptr &&
                node->nodeP[i]->ID() == idSatExpediteur)
            {
              periph = node->nodeP[i];
              periphIdx = i;
              break;
            }
          }

          if (periph == nullptr)
          {
            // LOG_INFO("0xE0 ignore : expediteur inconnu %u", idSatExpediteur);
            break;
          }

          periph->busy(frameIn.data[0]);

          // LOG_INFO("0xE0 maj sat %u idx=%u busy=%u",
          //          idSatExpediteur,
          //          periphIdx,
          //          frameIn.data[0]);

          // Si l'expéditeur est mon SP1, alors son SP1 devient mon SP2
          if (periphIdx == node->SP1_idx())
          {
            node->SP2_acces(frameIn.data[3]);
            node->SP2_busy(frameIn.data[4]);
          }

          // Si l'expéditeur est mon SM1, alors son SM1 devient mon SM2
          if (periphIdx == node->SM1_idx())
          {
            node->SM2_acces(frameIn.data[5]);
            node->SM2_busy(frameIn.data[6]);
          }

          break;
        }

        case 0xE3:
        {
          if (frameIn.len < 4)
            break;

          const uint8_t targetId = frameIn.data[0];

          const uint16_t addr =
              ((uint16_t)frameIn.data[1] << 8) |
              frameIn.data[2];

          const uint8_t sens = frameIn.data[3];

          if (node->ID() != targetId)
            break;

          // if (node->busy())
          // {
          //   LOG_ERROR("Reservation refusee : canton occupe target=%u loco=%u occupant=%u",
          //             targetId,
          //             addr,
          //             node->loco.address());
          //   break;
          // }

          if (node->reservedBy() <= 1 || node->reservedBy() == addr)
          {
            node->reservedBy(addr);
            node->reservedSens(sens);
            node->reservedAt(millis());

            // node->trafficState((uint8_t)TrafficState::RESERVED);
            if (!node->busy())
            {
              node->trafficState((uint8_t)TrafficState::RESERVED);
            }

            LOG_INFO("Reservation canton acceptee target=%u loco=%u sens=%u busy=%u state=%u",
                     targetId,
                     addr,
                     sens,
                     node->busy(),
                     node->trafficState());

            CanMsg::sendMsg(1, 0xAB, 0, node->ID(),
                            0x00, 0x00,
                            (uint8_t)(0xC0 | ((addr >> 8) & 0x3F)),
                            (uint8_t)(addr & 0xFF));
          }
          else
          {
            LOG_INFO("Reservation refusee loco %u reserveBy=%u busy=%u",
                     addr,
                     node->reservedBy(),
                     node->busy());
          }

          break;
        }

        case 0xE5: // reception de l'adresse de la locomotive sur SP1 ou SM1
          if (node->nodeP[node->SP1_idx()] != nullptr)
          {
            if (idSatExpediteur == node->nodeP[node->SP1_idx()]->ID()) // Si l'expediteur est SP1
            {
              node->nodeP[node->SP1_idx()]->locoAddr((frameIn.data[0] << 8) + frameIn.data[1]);
            }
          }
          if (node->nodeP[node->SM1_idx()] != nullptr)
          {
            if (idSatExpediteur == node->nodeP[node->SM1_idx()]->ID()) // Si l'expediteur est SM1
            {
              node->nodeP[node->SM1_idx()]->locoAddr((frameIn.data[0] << 8) + frameIn.data[1]);
            }
          }
          break;

        case 0xE9:
          /*****************************************************************************************************
           * reception d'une commande d'aiguillage
           ******************************************************************************************************/

          if (node->ID() == frameIn.data[0])
            node->aigRun(frameIn.data[1]);
          break;
        }
      }
    }
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));
  }
}

/*--------------------------------------
  Envoi CAN
  --------------------------------------*/

bool CanMsg::sendMsg(CANMessage &frame)
{
  // #ifdef DEBUG
  //   if (0 == ACAN_ESP32::can.tryToSend(frame))
  //     debug.printf("Echec envoi message CAN\n");
  //   else
  //     debug.printf("Envoi commande 0x%0X\n", (frame.id & 0x1FE0000) >> 17);
  // #else
  return ACAN_ESP32::can.tryToSend(frame);
  // #endif
}

auto formatMsg = [](CANMessage &frame, byte prio, byte cmde, byte resp, uint16_t thisNodeId) -> CANMessage
{
  frame.id |= (uint32_t)prio << 25; // Priorite 0, 1 ou 2
  frame.id |= (uint32_t)cmde << 17; // commande appelée
  frame.id |= (uint32_t)resp << 16; // Response
  frame.id |= (uint32_t)thisNodeId; // ID expediteur
  frame.ext = true;
  return frame;
};

bool CanMsg::sendMsg(byte prio, byte cmde, byte resp, uint16_t thisNodeId)
{
  CANMessage frame;
  frame = formatMsg(frame, prio, cmde, resp, thisNodeId);
  frame.len = 0;
  return CanMsg::sendMsg(frame);
}

bool CanMsg::sendMsg(byte prio, byte cmde, byte resp, uint16_t thisNodeId, byte data0)
{
  CANMessage frame;
  frame = formatMsg(frame, prio, cmde, resp, thisNodeId);
  frame.len = 1;
  frame.data[0] = data0;
  return CanMsg::sendMsg(frame);
}

bool CanMsg::sendMsg(byte prio, byte cmde, byte resp, uint16_t thisNodeId, byte data0, byte data1)
{
  CANMessage frame;
  frame = formatMsg(frame, prio, cmde, resp, thisNodeId);
  frame.len = 2;
  frame.data[0] = data0;
  frame.data[1] = data1;
  return CanMsg::sendMsg(frame);
}

bool CanMsg::sendMsg(byte prio, byte cmde, byte resp, uint16_t thisNodeId, byte data0, byte data1, byte data2)
{
  CANMessage frame;
  frame = formatMsg(frame, prio, cmde, resp, thisNodeId);
  frame.len = 3;
  frame.data[0] = data0;
  frame.data[1] = data1;
  frame.data[2] = data2;
  return CanMsg::sendMsg(frame);
}

bool CanMsg::sendMsg(byte prio, byte cmde, byte resp, uint16_t thisNodeId, byte data0, byte data1, byte data2, byte data3)
{
  CANMessage frame;
  frame = formatMsg(frame, prio, cmde, resp, thisNodeId);
  frame.len = 4;
  frame.data[0] = data0;
  frame.data[1] = data1;
  frame.data[2] = data2;
  frame.data[3] = data3;
  return CanMsg::sendMsg(frame);
}

bool CanMsg::sendMsg(byte prio, byte cmde, byte resp, uint16_t thisNodeId, byte data0, byte data1, byte data2, byte data3, byte data4)
{
  CANMessage frame;
  frame = formatMsg(frame, prio, cmde, resp, thisNodeId);
  frame.len = 5;
  frame.data[0] = data0;
  frame.data[1] = data1;
  frame.data[2] = data2;
  frame.data[3] = data3;
  frame.data[4] = data4;
  return CanMsg::sendMsg(frame);
}

bool CanMsg::sendMsg(byte prio, byte cmde, byte resp, uint16_t thisNodeId, byte data0, byte data1, byte data2, byte data3, byte data4, byte data5)
{
  CANMessage frame;
  frame = formatMsg(frame, prio, cmde, resp, thisNodeId);
  frame.len = 6;
  frame.data[0] = data0;
  frame.data[1] = data1;
  frame.data[2] = data2;
  frame.data[3] = data3;
  frame.data[4] = data4;
  frame.data[5] = data5;
  return CanMsg::sendMsg(frame);
}

bool CanMsg::sendMsg(byte prio, byte cmde, byte resp, uint16_t thisNodeId, byte data0, byte data1, byte data2, byte data3, byte data4, byte data5, byte data6)
{
  CANMessage frame;
  frame = formatMsg(frame, prio, cmde, resp, thisNodeId);
  frame.len = 7;
  frame.data[0] = data0;
  frame.data[1] = data1;
  frame.data[2] = data2;
  frame.data[3] = data3;
  frame.data[4] = data4;
  frame.data[5] = data5;
  frame.data[6] = data6;
  return CanMsg::sendMsg(frame);
}

bool CanMsg::sendMsg(byte prio, byte cmde, byte resp, uint16_t thisNodeId, byte data0, byte data1, byte data2, byte data3, byte data4, byte data5, byte data6, byte data7)
{
  CANMessage frame;
  frame = formatMsg(frame, prio, cmde, resp, thisNodeId);
  frame.len = 8;
  frame.data[0] = data0;
  frame.data[1] = data1;
  frame.data[2] = data2;
  frame.data[3] = data3;
  frame.data[4] = data4;
  frame.data[5] = data5;
  frame.data[6] = data6;
  frame.data[7] = data7;
  return CanMsg::sendMsg(frame);
}
