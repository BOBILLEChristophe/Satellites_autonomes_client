

#include "TrafficManager.h"

uint16_t TrafficManager::signalValue[2] = {0, 0};

static constexpr uint8_t SENS_INCONNU = 0;
static constexpr uint8_t SENS_HORAIRE = 1;
static constexpr uint8_t SENS_ANTIHOR = 2;

static constexpr uint16_t SIGNAL_ORANGE = 0;
static constexpr uint16_t SIGNAL_ROUGE = 1;
static constexpr uint16_t SIGNAL_VERT = 2;
static constexpr uint16_t SIGNAL_CARRE = 3;
static constexpr uint16_t SIGNAL_RALENTISSEMENT = 4;
static constexpr uint16_t SIGNAL_RRALENTISSEMENT = 5;

void TrafficManager::setup(Node *node)
{
    TaskHandle_t loopTaskHandle = nullptr;

    xTaskCreatePinnedToCore(
        TrafficManager::signauxTask,
        "SignauxTask",
        2 * 1024,
        node,
        4,
        nullptr,
        0);

    xTaskCreatePinnedToCore(
        TrafficManager::loopTask,
        "LoopTask",
        8 * 1024,
        node,
        10,
        &loopTaskHandle,
        1);

#ifdef TEST_MEMORY_TASK
    xTaskCreate(
        TrafficManager::testMemory,
        "TestMemory",
        2 * 1024,
        (void *)loopTaskHandle,
        2,
        nullptr);
#endif
}

#ifdef TEST_MEMORY_TASK
void TrafficManager::testMemory(void *pvParameters)
{
    TaskHandle_t loopTaskHandle = (TaskHandle_t)pvParameters;

    for (;;)
    {
        UBaseType_t freeStack = uxTaskGetStackHighWaterMark(loopTaskHandle);
        LOG_DEBUG("TrafficManager loopTask free memory = %d bytes", freeStack);
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
#endif

void TrafficManager::signauxTask(void *p)
{
    Node *node = (Node *)p;

    TickType_t xLastWakeTime = xTaskGetTickCount();
    uint16_t oldValue[2] = {0xFFFF, 0xFFFF};

    for (;;)
    {
        for (uint8_t i = 0; i < 2; i++)
        {
            if (node->signal[i] != nullptr && oldValue[i] != signalValue[i])
            {
                const uint16_t mask = node->signal[i]->affiche(signalValue[i]);
                SignauxCmd::affiche(mask);

                oldValue[i] = signalValue[i];

                LOG_INFO("[TrafficManager %d] signal[%u] value=%u masque=0b%s",
                         __LINE__,
                         i,
                         signalValue[i],
                         String(mask, BIN).c_str());
            }
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(500));
    }
}

void TrafficManager::loopTask(void *pvParameters)
{
    Node *node = (Node *)pvParameters;

    uint32_t lastReservationSendMs = 0;
    uint16_t lastReservationDestId = 0;
    uint16_t lastReservationLocoAddr = 0;

    TickType_t xLastWakeTime = xTaskGetTickCount();
    // TickType_t xLastLocChangeTime = xTaskGetTickCount();
    const TickType_t locoDelay = pdMS_TO_TICKS(1000);
    TickType_t xLastSignalChangeTime[2] = {xTaskGetTickCount(), xTaskGetTickCount()};
    const TickType_t signalDelay = pdMS_TO_TICKS(1000);

    bool oldBusy = node->busy();

    for (;;)
    {
        node->reserved.refresh(); // MAJ des reservations

        const bool currentBusy = node->busy();
        const bool busyOn = !oldBusy && currentBusy;

        if (busyOn)
        {
            if (node->loco.address() == 0 &&
                node->reserved.reserved())
            {
                node->loco.address(
                    node->reserved.reservedByLoco());

                LOG_INFO("BUSY_ON : train %u identifié par réservation du satellite %u",
                         node->loco.address(),
                         node->reserved.reservedBySat());
            }
        }

        /*************************************************************************************
         * Canton libre
         ************************************************************************************/
        if (node->busy() == false)
        {
            node->sensor[SENSOR_HORAIRE].state(LOW); // Desactivation des capteurs ponctuels si aucune loco reconnue
            node->sensor[SENSOR_ANTIHOR].state(LOW);
            node->loco.address(0);                     // Reset de l'adresse
            node->loco.speed(0);                       // ... de la vitesse ...
            node->loco.oldSpeed(0);                    // ... de la précédente vitesse ...
            node->loco.networkDirection(SENS_INCONNU); // ... de la direction ...
            node->loco.envoiSpeedCmd(false);           // ... répétition de commande ...
            node->loco.railMode(0);                    // ... du mode d'alimentation.
        }

        /*************************************************************************************
         * Canton occupé
         ************************************************************************************/
        else
        /*************************************************************************************
         * Demande infos locomotive + direction réseau
         ************************************************************************************/
        {
            if (node->loco.address() > 0)
            {
                const uint16_t addr = node->loco.address();
                const uint32_t nowMs = millis();
                static uint32_t lastLocoInfoRequestMs = 0;

                if (nowMs - lastLocoInfoRequestMs > locoDelay)
                {
                    CanMsg::sendMsg(1, 0xAB, 0, node->ID(),
                                    0x00,
                                    0x00,
                                    (uint8_t)(0xC0 | ((addr >> 8) & 0x3F)),
                                    (uint8_t)(addr & 0xFF));

                    lastLocoInfoRequestMs = nowMs;
                }
            }

            /*************************************************************************************
             * Adaptation de la vitesse de la locomotive au canton
             ************************************************************************************/
            if (node->loco.speed() > node->maxSpeed())
            {
                node->loco.speed(node->maxSpeed());
            }
        }

        /*************************************************************************************
         * Recherche SP1 / SM1
         ************************************************************************************/

        auto searchSat = [node](bool satPos) -> uint8_t
        {
            uint8_t idxA = 0;
            uint8_t idxS = 0;

            if (satPos == 1)
            {
                idxA = 3;
                idxS = 4;
            }

            uint8_t idx = idxS;

            if (node->aig[0 + idxA] != nullptr) // aiguille 0 ou 3
            {
                if (node->aig[0 + idxA]->estDroit()) // aiguille 0 ou 3 droite
                {
                    // p00 ou m00
                    idx = 0 + idxS;                     // idx = 0 ou 4
                    if (node->aig[1 + idxA] != nullptr) // aiguille 1 ou 4
                    {
                        // p10 ou m10
                        if (!node->aig[1 + idxA]->estDroit()) // aiguille 1 ou 4 déviée
                            idx = 2 + idxS;                   // idx = 2 ou 6
                    }
                }
                else // aiguille 0 ou 3 déviéé
                {
                    // p01 ou m01
                    idx = 1 + idxS;                     // idx = 1 ou 5
                    if (node->aig[2 + idxA] != nullptr) // aiguille 2 ou 5
                    {
                        if (!node->aig[2 + idxA]->estDroit())
                            idx = 3 + idxS; // idx = 3 ou 7
                    }
                }
            }
            return idx;
        };

        /*************************************************************************************
         * Envoi état du satellite
         ************************************************************************************/

        // Pour ce satellite, on recherche quel sont les cantons SP1 et SM1
        // en fonction de la position des aiguilles
        auto *sp1 = node->nodeP[searchSat(0)]; // côté horaire
        auto *sm1 = node->nodeP[searchSat(1)]; // côté anti-horaire

        node->SP1 = sp1;
        node->SM1 = sm1;

        uint8_t etats = 0;
        etats |= ((uint8_t)node->busy() << 0);
        etats |= ((uint8_t)node->reserved.reserved() << 1);
        if (sp1 != nullptr)
        {
            etats |= ((uint8_t)sp1->busy() << 2);
            etats |= ((uint8_t)sp1->acces() << 3);
        }
        if (sm1 != nullptr)
        {
            etats |= ((uint8_t)sm1->busy() << 4);
            etats |= ((uint8_t)sm1->acces() << 5);
        }

        const uint16_t sp1Id = (sp1 != nullptr) ? sp1->ID() : 0;
        const uint16_t sm1Id = (sm1 != nullptr) ? sm1->ID() : 0;

        const uint16_t addr =
            node->busy() ? node->loco.address() : 0;

        CanMsg::sendMsg(1, 0x40, 0, node->ID(),
                        (uint8_t)(sp1Id >> 8),
                        (uint8_t)(sp1Id & 0xFF),
                        (uint8_t)(sm1Id >> 8),
                        (uint8_t)(sm1Id & 0xFF),
                        etats,
                        (uint8_t)(addr >> 8),
                        (uint8_t)(addr & 0xFF));

        /*************************************************************************************
         * Réservation du canton suivant
         ************************************************************************************/

        if (node->busy() && node->loco.address() > 0)
        {
            const uint8_t networkDirection = node->loco.networkDirection();

            if (networkDirection != SENS_INCONNU)
            {
                NodePeriph *nodeDest =
                    (networkDirection == SENS_HORAIRE) ? node->SP1 : node->SM1;

                if (nodeDest != nullptr && nodeDest->ID() != 0)
                {
                    const uint32_t nowMs = millis();
                    const uint16_t destId = nodeDest->ID();
                    const uint16_t locoAddr = node->loco.address();

                    // Envoi immédiat si la destination ou la loco change.
                    const bool changed =
                        destId != lastReservationDestId ||
                        locoAddr != lastReservationLocoAddr;

                    // Sinon renouvellement toutes les secondes.
                    if (changed ||
                        (uint32_t)(nowMs - lastReservationSendMs) >= reservationSendDelayMs)
                    {
                        CanMsg::sendMsg(
                            1, 0x41, 0, node->ID(),
                            (uint8_t)(destId >> 8),
                            (uint8_t)(destId & 0xFF),
                            (uint8_t)(locoAddr >> 8),
                            (uint8_t)(locoAddr & 0xFF));

                        lastReservationSendMs = nowMs;
                        lastReservationDestId = destId;
                        lastReservationLocoAddr = locoAddr;
                    }
                }
            }
        }
        else
        {
            // Permettra un nouvel envoi immédiat lors de la prochaine apparition d'un train.
            lastReservationDestId = 0;
            lastReservationLocoAddr = 0;
        }

        // /*************************************************************************************
        //  * Signalisation
        //  ************************************************************************************/

        // ---- Etape 1 : calcul de signalValue[0] (horaire) et signalValue[1] (anti-horaire) ----

        for (uint8_t i = 0; i < 2; i++)
        {
            NodePeriph *sat1 = (i == 0) ? node->SP1 : node->SM1;
            NodePeriph *sat2 = (i == 0) ? node->SP2 : node->SM2;

            if (sat1 != nullptr && sat1->ID() > 0)
            {
                const uint16_t myAddr = node->loco.address();

                // Le canton suivant est BUSY, mais c'est mon propre train
                // qui l'occupe déjà.
                const bool busyByMyTrain =
                    sat1->busy() &&
                    myAddr > 0 &&
                    sat1->locoAddr() == myAddr;

                // Le canton suivant est réservé par mon satellite
                // pour ma locomotive.
                const bool reservedForMe =
                    sat1->reservedFor(node->ID(), myAddr);

                /*
                 * Règles :
                 *
                 * BUSY par mon propre train     -> autorisé
                 * BUSY par un autre train       -> interdit
                 * FREE réservé pour moi         -> autorisé
                 * FREE réservé pour un autre    -> interdit
                 * FREE non réservé              -> autorisé
                 */
                bool accessForbidden = false;

                if (sat1->busy())
                {
                    accessForbidden = !busyByMyTrain;
                }
                else if (sat1->reserved())
                {
                    accessForbidden = !reservedForMe;
                }

                if (sat1->acces())
                {
                    if (accessForbidden)
                    {
                        signalValue[i] = SIGNAL_CARRE;
                    }
                    else
                    {
                        signalValue[i] = SIGNAL_VERT;

                        if (sat2 != nullptr && sat2->ID() > 0)
                        {
                            if (sat2->acces())
                                signalValue[i] =
                                    sat2->busy() ? SIGNAL_ORANGE : SIGNAL_VERT;
                            else
                                signalValue[i] = SIGNAL_ORANGE;
                        }
                    }
                }
                else
                {
                    signalValue[i] = SIGNAL_CARRE;
                }
            }
            else
            {
                signalValue[i] = SIGNAL_CARRE;
            }
        }

        // ---- Etape 2 : détection de changement, un "old" et un temporisateur PAR DIRECTION ----
        static uint16_t oldSignalValue[2] = {0xFFFF, 0xFFFF};

        for (uint8_t i = 0; i < 2; i++)
        {
            if (signalValue[i] != oldSignalValue[i])
            {
                xLastSignalChangeTime[i] = xTaskGetTickCount();
                oldSignalValue[i] = signalValue[i];
            }
        }

        // ---- Etape 3 : décision arrêt/ralentissement, indépendante par direction ----
        for (uint8_t i = 0; i < 2; i++)
        {
            if (xTaskGetTickCount() - xLastSignalChangeTime[i] > signalDelay)
            {
                const bool signalArret =
                    (signalValue[i] == SIGNAL_CARRE || signalValue[i] == SIGNAL_ROUGE);

                const bool signalRalenti =
                    (signalValue[i] == SIGNAL_RALENTISSEMENT ||
                     signalValue[i] == SIGNAL_RRALENTISSEMENT);

                if (signalArret)
                {
                    if (node->loco.networkDirection() == SENS_HORAIRE)
                    {
                        if (node->sensor[SENSOR_ANTIHOR].state())
                            node->loco.sRalenti();
                        if (node->sensor[SENSOR_HORAIRE].state())
                            node->loco.stop();
                    }
                    else if (node->loco.networkDirection() == SENS_ANTIHOR)
                    {
                        if (node->sensor[SENSOR_HORAIRE].state())
                            node->loco.sRalenti();
                        if (node->sensor[SENSOR_ANTIHOR].state())
                            node->loco.stop();
                    }
                }
                else if (signalRalenti)
                {
                    if (node->loco.networkDirection() == SENS_HORAIRE)
                    {
                        if (node->sensor[SENSOR_ANTIHOR].state() && node->loco.speed() > node->loco.gRalenti())
                            node->loco.sRalenti();
                    }
                    else if (node->loco.networkDirection() == SENS_ANTIHOR)
                    {
                        if (node->sensor[SENSOR_HORAIRE].state() && node->loco.speed() > node->loco.gRalenti())
                            node->loco.sRalenti();
                    }
                }
            }
        }
        // /*************************************************************************************
        //  * Envoie des commandes de vitesse à la centrale
        //  ************************************************************************************/

        if (node->loco.address() > 0)
        {
            if (node->loco.envoiSpeedCmd())
            {                                                    // Message à la centrale DCC++
                const uint16_t cappedSpeed = node->loco.speed(); // valeur réellement stockée, post-transformation
                const uint16_t addr = node->loco.address();
                CanMsg::sendMsg(0, 0x04, 0, node->ID(),
                                0x00,
                                0x00,
                                (uint8_t)(0xC0 | ((addr >> 8) & 0x3F)),
                                (uint8_t)(addr & 0xFF),
                                (uint8_t)((cappedSpeed >> 8) & 0xFF),
                                (uint8_t)(cappedSpeed & 0xFF));
                // #ifdef debug
                //                 debug.printf("[GestionReseau %d] Loco %d vitesse %d\n", __LINE__, node->loco.address(), node->loco.speed());
                // #endif
            }
        }
        oldBusy = currentBusy;
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));
    }
}
