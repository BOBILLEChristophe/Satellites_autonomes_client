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

      // #ifdef DEBUG
      //       // debug.printf("\n[CanMsg %d]------ Expediteur %d : commande 0x%0X\n", __LINE__, idSatExpediteur, commande);
      // #endif

      switch (commande) // commande appelee
      {
      case 0x04: // ACK de laBox pour la commande de vitesse avec bit response = 1
      {
        break;
      }

      case 0xAA: // Retour commande throttle depuis LaBox / DCC-EX
      {
        //         if (frameIn.len < 7)
        //         {
        // #ifdef DEBUG
        //           debug.printf("[CanMsg %d] Commande 0xAA invalide : len=%d\n", __LINE__, frameIn.len);
        // #endif
        //           break;
        //         }
        //         // Loc-ID reçue : 00 00 C0 xx
        //         uint32_t locId =
        //             ((uint32_t)frameIn.data[0] << 24) |
        //             ((uint32_t)frameIn.data[1] << 16) |
        //             ((uint32_t)frameIn.data[2] << 8) |
        //             ((uint32_t)frameIn.data[3]);

        //         // Extraction adresse DCC depuis Loc-ID Märklin/DCC
        //         uint16_t cab =
        //             ((uint16_t)(frameIn.data[2] & 0x3F) << 8) |
        //             frameIn.data[3];

        //         uint16_t speed =
        //             ((uint16_t)frameIn.data[4] << 8) |
        //             frameIn.data[5];

        //         uint8_t direction = 0;
        //         if (frameIn.data[6] == 0)
        //           direction = 1;
        //         else if (frameIn.data[6] == 1)
        //           direction = 2;

        // #ifdef DEBUG
        //         debug.printf("[CanMsg %d] CMD 0xAA throttle : locId=0x%08lX cab=%u speed=%u dir=%d\n",
        //                      __LINE__,
        //                      (unsigned long)locId,
        //                      cab,
        //                      speed,
        //                      direction);
        // #endif

        //         // Mise à jour de la loco locale du canton
        //         node->loco.address(cab);
        //         node->loco.speed(speed);

        //         // 1 = horaire
        //         // 2 = antihoraire

        break;
      }

      case 0xAB: // Requête/réponse base locomotives
      {
        if (!response)
          break;

        if (frameIn.len < 7)
        {
          LOG_ERROR("Réponse base locos 0xAB invalide len=%d", frameIn.len);
          break;
        }

        uint16_t addr = (static_cast<uint16_t>(frameIn.data[2] & 0x3F) << 8) | static_cast<uint16_t>(frameIn.data[3]);

        if (addr == 0 || addr != node->loco.address())
          break;

        uint16_t speed = static_cast<uint16_t>(frameIn.data[4] << 8) | static_cast<uint16_t>(frameIn.data[5]);
        uint8_t networkDirection = frameIn.data[6]; // 1=HORAIRE, 2=ANTIHORAIRE -- sens réel, déjà résolu par la centrale

        node->loco.speed(speed);
        node->loco.oldSpeed(speed);
        node->loco.networkDirection(networkDirection);
        break;
      }

      case 0xB2: // fn : Reponse à demande de présence de la carte Main
        LOG_INFO("Reponse demande de test présence de la carte Main");
        if (idSatExpediteur == 0x0001) // ID de la carte Main
          Settings::mainReady(true);
        break;

      case 0xB3: // fn : Reponse à demande d'identifiant (0xB3)
        if (response && node->ID() == 0)
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

      case 0xE3:
      {
        // if (frameIn.len < 4)
        //   break;

        // const uint8_t targetId = frameIn.data[0];

        // const uint16_t addr =
        //     ((uint16_t)frameIn.data[1] << 8) |
        //     frameIn.data[2];

        // const uint8_t sens = frameIn.data[3];

        // if (node->ID() != targetId)
        //   break;

        // if (node->busy())
        // {
        //   LOG_ERROR("Reservation refusee : canton occupe target=%u loco=%u occupant=%u",
        //             targetId,
        //             addr,
        //             node->loco.address());
        //   break;
        // }

        // if (node->reservedBy() <= 1 || node->reservedBy() == addr)
        // {
        //   node->reservedBy(addr);
        //   node->reserved(sens);
        //   node->reservedAt(millis());

        //   // node->trafficState((uint8_t)TrafficState::RESERVED);
        //   if (!node->busy())
        //   {
        //     node->trafficState((uint8_t)TrafficState::RESERVED);
        //   }

        //   LOG_INFO("Reservation canton acceptee target=%u loco=%u sens=%u busy=%u state=%u",
        //            targetId,
        //            addr,
        //            sens,
        //            node->busy(),
        //            node->trafficState());

        //   CanMsg::sendMsg(1, 0xAB, 0, node->ID(),
        //                   0x00, 0x00,
        //                   (uint8_t)(0xC0 | ((addr >> 8) & 0x3F)),
        //                   (uint8_t)(addr & 0xFF));
        // }
        // else
        // {
        //   LOG_INFO("Reservation refusee loco %u reserveBy=%u busy=%u",
        //            addr,
        //            //node->reservedBy(),
        //            node->busy());
        // }

        break;
      }

        // De 0x40 à 0x4F, concerne l'état d'un satellite diffusé sur le réseau
      case 0x40: // Réception de l'état d'un satellite
      {
        if (frameIn.len != 7)
          continue; // Ignore ce message si la taille est incorrecte

        for (uint8_t i = 0; i < nodePsize; i++)
        {
          // Est-ce que l'expéditeur est l'un de mes périphériques directs ?
          if (node->nodeP[i] == nullptr || node->nodeP[i]->ID() != idSatExpediteur)
            continue;
          // Oui, l'expéditeur est l'un de mes périphériques
          uint8_t etats = frameIn.data[4];
          // Ce sat est-il occupé ou libre ?
          node->nodeP[i]->busy(etats & 0x01);
          // Ce sat est-il reservé ou pas ?
          node->nodeP[i]->reserved(etats & 0x02);

          const uint16_t trainAddr =
              ((uint16_t)frameIn.data[5] << 8) | frameIn.data[6];
          node->nodeP[i]->locoAddr(trainAddr);

          const uint16_t bSp1Id = static_cast<uint16_t>((frameIn.data[0] << 8) | frameIn.data[1]);
          const uint16_t bSm1Id = static_cast<uint16_t>((frameIn.data[2] << 8) | frameIn.data[3]);

          if (i < 4)
          {
            node->nodeP[i]->acces(bSm1Id == node->ID());

            if (node->SP1 == node->nodeP[i])
            {
              const uint16_t farId = bSp1Id;

              node->SP2->ID(farId);

              if (farId == 0)
              {
                node->SP2->busy(true);
                node->SP2->acces(false);
              }
              else
              {
                node->SP2->busy(etats & 0x04);
                node->SP2->acces(etats & 0x08);
              }
            }
          }
          else
          {
            node->nodeP[i]->acces(bSp1Id == node->ID());

            if (node->SM1 == node->nodeP[i])
            {
              const uint16_t farId = bSm1Id;

              node->SM2->ID(farId);

              if (farId == 0)
              {
                node->SM2->busy(true);
                node->SM2->acces(false);
              }
              else
              {
                node->SM2->busy(etats & 0x10);
                node->SM2->acces(etats & 0x20);
              }
            }
          }
        }
        break;
      }

        // De 0x41 : réservation de canton
      case 0x41:
      {
        if (frameIn.len != 4)
          continue;

        const uint16_t messageId =
            (static_cast<uint16_t>(frameIn.data[0]) << 8) |
            static_cast<uint16_t>(frameIn.data[1]);

        const uint16_t addr =
            (static_cast<uint16_t>(frameIn.data[2]) << 8) |
            static_cast<uint16_t>(frameIn.data[3]);

        /******************************************************************
         * DEMANDE DE RESERVATION
         ******************************************************************/
        if (!response)
        {
          const uint16_t targetId = messageId;

          // La demande ne m'est pas destinée
          if (targetId != node->ID())
            continue;

          const bool accepted =
              node->reserved.reservation(idSatExpediteur, addr);

          if (accepted)
          {
            CanMsg::sendMsg(
                1,
                0x41,
                1,
                node->ID(),
                static_cast<uint8_t>(idSatExpediteur >> 8),
                static_cast<uint8_t>(idSatExpediteur & 0xFF),
                static_cast<uint8_t>(addr >> 8),
                static_cast<uint8_t>(addr & 0xFF));

            LOG_INFO(
                "Reservation acceptee : canton=%u origine=%u loco=%u",
                node->ID(),
                idSatExpediteur,
                addr);

            if (!node->busy())
            {
              if (!node->setAccessFrom(idSatExpediteur))
              {
                LOG_WARN(
                    "Reservation acceptee mais route impossible vers satellite %u",
                    idSatExpediteur);
              }
            }
          }
          break;
        }

        /******************************************************************
         * CONFIRMATION DE RESERVATION
         ******************************************************************/
        const uint16_t requesterId = messageId;

        /*
         * idSatExpediteur est ici le canton QUI EST RESERVE.
         *
         * Exemple :
         *   idSatExpediteur = 3
         *   requesterId     = 2
         *   addr            = 6400
         *
         * => canton 3 réservé par satellite 2 pour loco 6400
         */

        for (uint8_t i = 0; i < nodePsize; i++)
        {
          NodePeriph *periph = node->nodeP[i];

          if (periph == nullptr)
            continue;

          if (periph->ID() != idSatExpediteur)
            continue;

          periph->reserved(true);
          periph->reservedBySat(requesterId);
          periph->reservedByLoco(addr);

          LOG_INFO(
              "Reservation voisine : canton=%u par sat=%u loco=%u",
              idSatExpediteur,
              requesterId,
              addr);

          break;
        }

        /*
         * Si je suis moi-même le demandeur, la réservation est maintenant
         * confirmée.
         *
         * Rien d'autre à faire pour l'instant.
         */
        if (requesterId == node->ID())
        {
          LOG_INFO(
              "Ma reservation confirmee : canton=%u loco=%u",
              idSatExpediteur,
              addr);
        }

        break;
      }
      case 0x42:
      {
        break;
      }
      case 0x43:
      {
        break;
      }
      case 0x44:
      {
        break;
      }
      case 0x45:
      {
        break;
      }
      case 0x46:
      {
        break;
      }
      case 0x47:
      {
        break;
      }
      case 0x48:
      {
        break;
      }
      case 0x49:
      {
        break;
      }
      case 0x4A:
      {
        break;
      }
      case 0x4B:
      {
        break;
      }
      case 0x4C:
      {
        break;
      }
      case 0x4D:
      {
        break;
      }
      case 0x4E:
      {
        break;
      }
      case 0x4F:
      {
        break;
      }

      case 0xE0:
      {
        break;
      }
      case 0xE1:
      {
        break;
      }
      case 0xE2:
      {
        break;
      }
        // case 0xE3:
        // break;

        // case 0xE4:

        // break;

      case 0xE5: // reception de l'adresse de la locomotive sur SP1 ou SM1
      {
        // if (node->nodeP[node->SP1_idx()] != nullptr)
        // {
        //   if (idSatExpediteur == node->nodeP[node->SP1_idx()]->ID()) // Si l'expediteur est SP1
        //   {
        //     node->nodeP[node->SP1_idx()]->locoAddr((frameIn.data[0] << 8) + frameIn.data[1]);
        //   }
        // }
        // if (node->nodeP[node->SM1_idx()] != nullptr)
        // {
        //   if (idSatExpediteur == node->nodeP[node->SM1_idx()]->ID()) // Si l'expediteur est SM1
        //   {
        //     node->nodeP[node->SM1_idx()]->locoAddr((frameIn.data[0] << 8) + frameIn.data[1]);
        //   }
        // }
        // break;
      }
      case 0xE6:
      {
        break;
      }
      case 0xE7:
      {
        break;
      }
      case 0xE8:
      {
        break;
      }
      case 0xE9: // reception d'une commande d'aiguillage
      {
        /*****************************************************************************************************
         * reception d'une commande d'aiguillage
         ******************************************************************************************************/

        if (node->ID() ==
            (((uint16_t)frameIn.data[0] << 8) |
             (uint16_t)frameIn.data[1]))
        {
          node->aigRun(frameIn.data[2]);
        }
        break;
      }
      case 0xEA:
      {
        break;
      }
      case 0xEB:
      {
        break;
      }
      case 0xEC:
      {
        break;
      }
      case 0xED:
      {
        break;
      }
      case 0xEE:
      {
        break;
      }
      case 0xEF:
      {
        break;
      }

      case 0xF0:
      {
        break;
      }
      case 0xF1:
      {
        break;
      }
      case 0xF2:
      {
        break;
      }
      case 0xF3:
      {
        break;
      }
      case 0xF4:
      {
        break;
      }
      case 0xF5:
      {
        break;
      }
      case 0xF6:
      {
        break;
      }
      case 0xF7:
      {
        break;
      }
      case 0xF8:
      {
        break;
      }
      case 0xF9:
      {
        break;
      }
      case 0xFA:
      {
        break;
      }
      case 0xFB:
      {
        break;
      }
      case 0xFC:
      {
        break;
      }
      case 0xFD:
      {
        break;
      }
      case 0xFE:
      {
        break;
      }
      case 0xFF:
      {
        break;
      }
      default:
      {
        LOG_ERROR("Commande 0x%0X non trouvée ", commande);
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
