/*

  CanMsg.cpp


*/

#include "CanMsg.h"

void CanMsg::setup(Node *node)
{
#ifdef DEBUG
  debug.printf("[CanMsg %d] : setup\n", __LINE__);
#endif
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
    debug.printf("canReceiveMsg free memory = % d bytes\n", canReceiveMsg);
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
      const uint8_t commande = (uint8_t)((frameIn.id & 0x1fe0000) >> 17); // Code de la commande
      const uint16_t idSatExpediteur = (uint16_t)frameIn.id & 0xffff;     // ID du satellite qui envoie
      const bool response = (frameIn.id & 0x10000) >> 16;                 // Reponse
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
        case 0x0F:
          ACAN_ESP32::can.tryToSend(frameIn);
          break;
        }
      }
      else
      {
        switch (commande) // commande appelee
        {
        case 0xB3: // fn : Reponse à demande de test du bus CAN
          if (frameIn.data[0])
            Settings::sMainReady(true);
          break;
        case 0xB5: // fn : Reponse à demande d'identifiant (0xB4)
          if (node->ID() == UNUSED_ID)
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
        case 0xBF: // fn : Enreistrement des données en mémoire flash
#ifdef SAUV_BY_MAIN
#ifdef DEBUG
          debug.printf("------ Rec->sauvegarde distante\n");
#endif
          Settings::writeFile();
#else
          debug.printf("Sauvegarde automatique desactivee.\n");
#endif
          break;

        case 0xC0: // fn : Réception de l'ID d'un satellite
          Discovery::ID_satPeriph(idSatExpediteur);
          break;

        case 0xC1:
          /*****************************************************************************************************
           * reception periodique des data envoyees par les sat pendant le processus de decouverte
           ******************************************************************************************************/

          // debug.printf("[CanMsg %d] : commande 0xC1, ID exped %d \n", __LINE__, idSatExpediteur);

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

          // if (frameIn.data[0])
          // {
          //   Serial.print("Sat : ");
          //   Serial.print(idSatExpediteur);
          //   Serial.print(" busy = ");
          //   Serial.println(frameIn.data[0]);
          // }

          // Serial.println(node->nodeP[node->SP1_idx()]->ID());

          /*****************************************************************************************************
           * reception periodique des data envoyees par les sat en exploitation (GestionReseau.cpp ligne 42)
           ******************************************************************************************************/

          // NB : node->SP1_idx() et node->SM1_idx() sont renseignés à GestionReseau.cpp ligne 111
          // en fonction de la position des aiguilles

          if (node->nodeP[node->SP1_idx()] != nullptr)
          {
            if (idSatExpediteur == node->nodeP[node->SP1_idx()]->ID()) // L'expediteur est-il le SP1 de ce sat ?
            {
              // debug.printf("[CanMsg %d] node->SP1_idx()->ID() : %d\n", __LINE__, node->nodeP[node->SP1_idx()]->ID());
              //   L'expediteur est le SP1 de ce sat / ce sat est-il le SM1 de l'expediteur ?
              if (node->ID() == frameIn.data[2])
              {
                // Cela veut dire que le SP1 de ce sat est accessible
                node->nodeP[node->SP1_idx()]->acces(true);
                node->SP2_acces(frameIn.data[3]);
                node->SP2_busy(frameIn.data[4]);
              }
              else
                node->nodeP[node->SP1_idx()]->acces(false); // Le SP1 de ce sat n'est pas accessible

              node->nodeP[node->SP1_idx()]->busy(frameIn.data[0]);
              node->nodeP[node->SP1_idx()]->reserved(frameIn.data[1]);

#ifdef DEBUG
              // debug.printf("[CanMsg %d] Sat %d busy : %d\n", __LINE__, node->nodeP[node->SP1_idx()]->ID(), node->nodeP[node->SP1_idx()]->busy());
              // debug.printf("[CanMsg %d] Sat %d acces : %d\n", __LINE__, node->nodeP[node->SP1_idx()]->ID(),  node->nodeP[node->SP1_idx()]->acces());
              // debug.printf("[CanMsg %d] node->SP2_busy : %d\n", __LINE__, node->SP2_busy());
              // debug.printf("[CanMsg %d] node->SP2_acces : %d\n", __LINE__, node->SP2_acces());
#endif
            }
          }

          if (node->nodeP[node->SM1_idx()] != nullptr)
          {
            if (idSatExpediteur == node->nodeP[node->SM1_idx()]->ID()) // L'expediteur est-il le SM1 de ce sat ?
            {
              // debug.printf("[CanMsg %d] node->SM1_idx()->ID() : %d\n", __LINE__, node->nodeP[node->SM1_idx()]->ID());
              //   L'expediteur est le SM1 de ce sat / ce sat est-il le SP1 de l'expediteur ?
              if (node->ID() == frameIn.data[1])
              {
                // Cela veut dire que le SM1 de ce sat est accessible
                node->nodeP[node->SM1_idx()]->acces(true);
                node->SM2_acces(frameIn.data[5]);
                node->SM2_busy(frameIn.data[6]);
              }
              else
                node->nodeP[node->SM1_idx()]->acces(false); // Le SM1 de ce sat n'est pas accessible

              node->nodeP[node->SM1_idx()]->busy(frameIn.data[0]);
              node->nodeP[node->SM1_idx()]->reserved(frameIn.data[1]);

#ifdef DEBUG
              // debug.printf("[CanMsg %d] Sat %d busy : %d\n", __LINE__, node->nodeP[node->SM1_idx()]->ID(), node->nodeP[node->SM1_idx()]->busy());
              // debug.printf("[CanMsg %d] Sat %d acces : %d\n", __LINE__, node->nodeP[node->SM1_idx()]->ID(), node->nodeP[node->SM1_idx()]->acces());
              // debug.printf("[CanMsg %d] node->SM2_busy : %d\n", __LINE__, node->SM2_busy());
              // debug.printf("[CanMsg %d] node->SM2_acces : %d\n", __LINE__, node->SM2_acces());
#endif
            }
          }
          break;

        case 0xE3:
          /*****************************************************************************************************
           * reservation du canton pour loco en approche sur canton précédent
           ******************************************************************************************************/

            //debug.printf("[CanMsg %d] frameIn.data[0] : %d\n", __LINE__, frameIn.data[0]);
          if (node->ID() == frameIn.data[0])
          {
            //debug.printf("[CanMsg %d] frameIn.data[0] : %d\n", __LINE__, frameIn.data[0]);
            if (node->reserved() == 0) // Si le canton n'est pas déjà reservé
            {
              node->reserved((frameIn.data[1] << 8) + frameIn.data[2]);
              debug.printf("[CanMsg %d] node->reserved : %d\n", __LINE__, node->reserved());
            }
          }
          break;

        case 0xE5:
          /*****************************************************************************************************
           * reception de l'adresse de la locomotive
           ******************************************************************************************************/
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

void CanMsg::sendMsg(CANMessage &frame)
{
  // #ifdef DEBUG
  //   if (0 == ACAN_ESP32::can.tryToSend(frame))
  //     debug.printf("Echec envoi message CAN\n");
  //   else
  //     debug.printf("Envoi commande 0x%0X\n", (frame.id & 0x1FE0000) >> 17);
  // #else
  ACAN_ESP32::can.tryToSend(frame);
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

// Nouvelle messagerie
void CanMsg::sendMsg(byte prio, byte cmde, byte resp, uint16_t thisNodeId)
{
  CANMessage frame;
  frame = formatMsg(frame, prio, cmde, resp, thisNodeId);
  frame.len = 0;
  CanMsg::sendMsg(frame);
}

void CanMsg::sendMsg(byte prio, byte cmde, byte resp, uint16_t thisNodeId, byte data0)
{
  CANMessage frame;
  frame = formatMsg(frame, prio, cmde, resp, thisNodeId);
  frame.len = 1;
  frame.data[0] = data0;
  CanMsg::sendMsg(frame);
}

void CanMsg::sendMsg(byte prio, byte cmde, byte resp, uint16_t thisNodeId, byte data0, byte data1)
{
  CANMessage frame;
  frame = formatMsg(frame, prio, cmde, resp, thisNodeId);
  frame.len = 2;
  frame.data[0] = data0;
  frame.data[1] = data1;
  CanMsg::sendMsg(frame);
}

void CanMsg::sendMsg(byte prio, byte cmde, byte resp, uint16_t thisNodeId, byte data0, byte data1, byte data2)
{
  CANMessage frame;
  frame = formatMsg(frame, prio, cmde, resp, thisNodeId);
  frame.len = 3;
  frame.data[0] = data0;
  frame.data[1] = data1;
  frame.data[2] = data2;
  CanMsg::sendMsg(frame);
}

void CanMsg::sendMsg(byte prio, byte cmde, byte resp, uint16_t thisNodeId, byte data0, byte data1, byte data2, byte data3)
{
  CANMessage frame;
  frame = formatMsg(frame, prio, cmde, resp, thisNodeId);
  frame.len = 4;
  frame.data[0] = data0;
  frame.data[1] = data1;
  frame.data[2] = data2;
  frame.data[3] = data3;
  CanMsg::sendMsg(frame);
}

void CanMsg::sendMsg(byte prio, byte cmde, byte resp, uint16_t thisNodeId, byte data0, byte data1, byte data2, byte data3, byte data4)
{
  CANMessage frame;
  frame = formatMsg(frame, prio, cmde, resp, thisNodeId);
  frame.len = 5;
  frame.data[0] = data0;
  frame.data[1] = data1;
  frame.data[2] = data2;
  frame.data[3] = data3;
  frame.data[4] = data4;
  CanMsg::sendMsg(frame);
}

void CanMsg::sendMsg(byte prio, byte cmde, byte resp, uint16_t thisNodeId, byte data0, byte data1, byte data2, byte data3, byte data4, byte data5)
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
  CanMsg::sendMsg(frame);
}

void CanMsg::sendMsg(byte prio, byte cmde, byte resp, uint16_t thisNodeId, byte data0, byte data1, byte data2, byte data3, byte data4, byte data5, byte data6)
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
  CanMsg::sendMsg(frame);
}

void CanMsg::sendMsg(byte prio, byte cmde, byte resp, uint16_t thisNodeId, byte data0, byte data1, byte data2, byte data3, byte data4, byte data5, byte data6, byte data7)
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
  CanMsg::sendMsg(frame);
}
