/*

  GestionReseau.cpp


*/

#include "GestionReseau.h"

uint16_t GestionReseau::signalValue[2] = {0};

void GestionReseau::setup(Node *node)
{
    xTaskCreatePinnedToCore(signauxTask, "SignauxTask", 2 * 1024, (void *)node, 8, NULL, 0); // Création de la tâches pour les signaux
    xTaskCreatePinnedToCore(loopTask, "LoopTask", 8 * 1024, (void *)node, 10, NULL, 0);      // Création de la tâches pour le traitement
    // tskIDLE_PRIORITY
}

void GestionReseau::signauxTask(void *p)
{
    Node *node;
    node = (Node *)p;
    TickType_t xLastWakeTime;
    xLastWakeTime = xTaskGetTickCount();
    uint16_t oldValue[2] = {0};

    for (;;)
    {
        for (uint8_t i = 0; i < 2; i++)
        {
            if (signalValue[i] > 0 && oldValue[i] != signalValue[i])
            {
                SignauxCmd::affiche(node->signal[i]->affiche(signalValue[i]));
                oldValue[i] = signalValue[i];
#ifdef debug
                // debug.printf("[GestionReseau %d] signal value ", __LINE__);
                // debug.println(node->signal[i]->affiche(signalValue[i]), BIN);
#endif
            }
        }
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(500));
    }
}

void IRAM_ATTR GestionReseau::loopTask(void *pvParameters)
{
    Node *node;
    node = (Node *)pvParameters;

    enum : bool
    {
        horaire,
        antiHor
    };

    uint8_t index = 0;
    bool sens0 = false;
    bool sens1 = false;
    bool s2access = false;
    bool s2busy = false;

    uint16_t oldLocAddress = 0;
    uint16_t oldLocSpeed = 0;
    uint8_t comptCmdLoco = 0;

#ifdef debug
    // char *cantonName;
    // cantonName = new char[4];
#endif

    TickType_t xLastWakeTime = xTaskGetTickCount();
    /* Retarder l'execution des commandes de locomotives */
    TickType_t lastTime = xTaskGetTickCount();
    const TickType_t tempo = pdMS_TO_TICKS(1000); // Délai de 1 seconde (1000 ms)

    for (;;)
    {
        /*************************************************************************************
         * Occupation du canton
         * et etat des capteurs
         ************************************************************************************/

        // debug.printf("[GestionReseau %d] node->busy() = %d\n", __LINE__, node->busy());

        if (node->busy() == false)
        {
            node->sensor[horaire].state(LOW); // Desactivation des capteurs ponctuels si aucune loco reconnue
            node->sensor[antiHor].state(LOW);
            node->loco.speed(0);
            node->loco.sens(0);
            node->loco.address(0);
        }
        else
            // Adapte la vitesse de la loco à la vitesse maxi du canton
            if (node->maxSpeed() < node->loco.speed())
                node->loco.speed(node->maxSpeed());

        /*************************************************************************************
         * Sens de roulage des locos
         ************************************************************************************/

        if (node->sensor[horaire].state() && !node->sensor[antiHor].state())
            node->loco.sens(1); // Horaire
        if (node->sensor[antiHor].state() && !node->sensor[horaire].state())
            node->loco.sens(2); // Anti Horaire

        // Serial.println(node->loco.sens());

        /*************************************************************************************
         * Information aupres des satellites environnants
         ************************************************************************************/

        // En fonction des aiguilles qui appartiennent a ce canton et de leurs positions
        // on en recherche quel est le SP1 et le SM1
        auto rechercheSat = [node](bool satPos) -> uint8_t
        {
            uint8_t idxA = 0; // Pour horaire
            uint8_t idxS = 0;

            if (satPos == 1) // Pour anti-horaire
            {
                idxA = 3;
                idxS = 4;
            }

            uint8_t $idx = idxS;

            if (node->aig[0 + idxA] != nullptr) // L'aiguille 0 à horaire (ou 3 à anti-horaire) existe
            {
                if (node->aig[0 + idxA]->estDroit()) // L'aiguille 0 à horaire (ou 3 à anti-horaire) est droite
                {
                    $idx = 0 + idxS;
                    if (node->aig[1 + idxA] != nullptr) // L'aiguille 1 à horaire (ou 4 à anti-horaire) existe
                    {
                        if (!node->aig[1 + idxA]->estDroit()) // L'aiguille 1 à horaire (ou 4 à anti-horaire) est droite
                            $idx = 0 + idxS;
                        else
                            $idx = 1 + idxS; // L'aiguille 1 à horaire (ou 4 à anti-horaire) est déviée
                    }
                }
                else // L'aiguille 0 à horaire (ou 3 à anti-horaire) est déviée
                {
                    $idx = 1 + idxS;
                    if (node->aig[2 + idxA] != nullptr) // L'aiguille 2 à horaire (ou 5 à anti-horaire) existe
                    {
                        if (node->aig[2 + idxA]->estDroit())
                            $idx = 2 + idxS; // L'aiguille 2 à horaire (ou 5 à anti-horaire) est droite
                        else
                            $idx = 3 + idxS; // L'aiguille 2 à horaire (ou 5 à anti-horaire) est déviée
                    }
                }
            }
            return $idx;
        };
        node->SP1_idx(rechercheSat(horaire));
        node->SM1_idx(rechercheSat(antiHor));

        // debug.printf("[GestionReseau %d] node->SP1_idx = %d\n", __LINE__, node->SP1_idx());
        // debug.printf("[GestionReseau %d] node->SM1_idx = %d\n", __LINE__, node->SM1_idx());

        /*************************************************************************************
         * Envoi sur le bus CAN des informations concernant ce satellite
         ************************************************************************************/

        CanMsg::sendMsg(0, 0xE0, 0, node->ID(),
                        node->busy(),
                        (uint8_t)node->nodeP[node->SP1_idx()]->ID(),
                        (uint8_t)node->nodeP[node->SM1_idx()]->ID(),
                        node->nodeP[node->SP1_idx()]->acces(),
                        node->nodeP[node->SP1_idx()]->busy(),
                        node->nodeP[node->SM1_idx()]->acces(),
                        node->nodeP[node->SM1_idx()]->busy());

        // Reserver le canton suivant
        // if (node->loco.speed() > 0)
        // {
        if (node->loco.sens() == 1) // Horaire)
        {
            CanMsg::sendMsg(0, 0xE3, 0, node->ID(),
                            (uint8_t)node->nodeP[node->SP1_idx()]->ID(),
                            (node->loco.address() & 0xFF00) >> 8,
                            node->loco.address() & 0x00FF);
        }
        else if (node->loco.sens() == 2) // Anti Horaire
        {
            CanMsg::sendMsg(0, 0xE3, 0, node->ID(),
                            (uint8_t)node->nodeP[node->SM1_idx()]->ID(),
                            (node->loco.address() & 0xFF00) >> 8,
                            node->loco.address() & 0x00FF);
        }
        // }

        enum : uint16_t
        {
            orange,
            rouge,
            vert,
            carre,
            ralentissement,
            rRalentissement
        };

        /*************************************************************************************
         *
         ************************************************************************************/

        for (uint8_t i = 0; i < 2; i++)
        {
            s2busy = false;
            s2access = false;
            switch (i)
            {
            case 0:
                index = node->SP1_idx();
                sens0 = horaire;
                sens1 = antiHor;
                s2access = node->SP2_acces();
                s2busy = node->SP2_busy();
                // debug.printf("[GestionReseau %d] Le SP2 est acces : %s \n", __LINE__, s2access ? "true" : "false");
                // debug.printf("[GestionReseau %d] Le SP2 est busy : %s \n", __LINE__, s2busy ? "true" : "false");
#ifdef debug
                // strcpy(cantonName, "SP1");
#endif
                break;
            case 1:
                index = node->SM1_idx();
                sens0 = antiHor;
                sens1 = horaire;
                s2access = node->SM2_acces();
                s2busy = node->SM2_busy();
                // debug.printf("[GestionReseau %d] Le SM2 est acces : %s \n", __LINE__, s2access ? "true" : "false");
                // debug.printf("[GestionReseau %d] Le SM2 est busy : %s \n", __LINE__, s2busy ? "true" : "false");
#ifdef debug
                // strcpy(cantonName, "SM1");
#endif
                break;
            }

            // debug.printf("[GestionReseau %d] reserved %d \n", __LINE__, node->nodeP[index]->reserved());

            // debug.printf("[GestionReseau %d] index %d \n", __LINE__, index);
            // debug.printf("[GestionReseau %d] node->sensor[sens0].state() %d \n", __LINE__, node->sensor[sens0].state());
            // debug.printf("[GestionReseau %d] node->sensor[sens1].state() %d \n", __LINE__, node->sensor[sens1].state());

            if (node->nodeP[index] != nullptr)
            {
                if (node->nodeP[index]->acces()) // Le canton SP1/SM1 est accessible
                {
                    // debug.printf("[GestionReseau %d] Le canton %s est accessible\n", __LINE__, cantonName);
                    if (node->nodeP[index]->busy()) // Le canton SP1/SM1 est occupé
                    {
                        Serial.println(node->nodeP[index]->ID());
                        if (node->loco.address() > 0)
                        {
                            Serial.println(node->loco.address());
                            if (node->loco.address() != node->nodeP[index]->reserved()) // Est-ce que la loco qui occupe le canton SP1/SM1 est celle qui l'a reservé ?
                            {
                                // debug.printf("[GestionReseau %d] Le canton %s est accessible mais occupe\n", __LINE__, cantonName);
                                signalValue[i] = rouge;
                            }
                        }
                    }

                    else // Le canton SP1/SM1 est accessible et libre
                    {
                        // debug.printf("[GestionReseau %d] Le canton %s est accessible et libre\n", __LINE__, cantonName);
                        signalValue[i] = vert;

                        if (s2access) // Le canton SP2 est-il accessible ?
                        {
                            // debug.printf("[GestionReseau %d] SP2 accessible\n", __LINE__);
                            if (s2busy) // Le canton SP2 est-il occupé ?
                            {
                                // debug.printf("[GestionReseau %d] SP2 occupe\n", __LINE__);
                                signalValue[i] = orange;
                            }
                            else // Le canton SP2 n'est pas occupé
                            {
                                // debug.printf("[GestionReseau %d] SP2 libre\n", __LINE__);
                                signalValue[i] = vert;
                            }
                        }
                        else // Le canton SP2 n'est pas accessible
                        {
                            // debug.printf("[GestionReseau %d] SP2 non accessible\n", __LINE__);
                            signalValue[i] = orange;
                        }
                    }
                }
                else // Le canton SP1/SM1 est n'est pas accessible
                {
                    // debug.printf("[GestionReseau %d] Le canton %s n'est pas accessible\n", __LINE__, cantonName);
                    signalValue[i] = carre;
                }
            }
            else // Le canton SP1/SM1 n'existe pas
            {
                // debug.printf("[GestionReseau %d] Le canton %s n'existe pas\n", __LINE__, cantonName);
                signalValue[i] = carre;
            }

            static uint16_t oldSignalValue0 = 0;
            static uint16_t oldSignalValue1 = 0;
            if (signalValue[0] != oldSignalValue0)
            {
                lastTime = xTaskGetTickCount();
                oldSignalValue0 = signalValue[0];
            }
            if (signalValue[1] != oldSignalValue1)
            {
                lastTime = xTaskGetTickCount();
                oldSignalValue1 = signalValue[1];
            }

            if (xTaskGetTickCount() - lastTime > tempo)
            {
                switch (signalValue[i])
                {
                case carre:
                case rouge:
                    // debug.printf("[GestionReseau %d] feu rouge\n", __LINE__);
                    if (node->loco.sens() == 1) // Horaire
                    {
                        if (node->sensor[1].state())
                            node->loco.stop();
                        else
                            node->loco.speed(200); // 200/1000 = 20%
                    }

                    if (node->loco.sens() == 2) // Anti Horaire
                    {
                        if (node->sensor[0].state())
                            node->loco.stop();
                        else
                            node->loco.speed(200); // 200/1000 = 20%
                    }
                    // if (node->sensor[sens1].state())
                    // {
                    //     if (node->sensor[sens0].state())
                    //         node->loco.stop();
                    //     else if (node->loco.speed() > 200)
                    //         node->loco.speed(200); // 200/1000 = 20%
                    // }
                    break;
                case ralentissement:
                    if (node->loco.sens() == 1)
                    {
                        if (node->sensor[0].state() && node->loco.speed() > 200)
                            node->loco.speed(200); // 200/1000 = 20%
                    }
                    if (node->loco.sens() == 2)
                    {
                        if (node->sensor[1].state() && node->loco.speed() > 200)
                            node->loco.speed(200); // 200/1000 = 20%
                    }
                    break;
                case vert:
                    // debug.println("ligne 318");
                    break;
                }
                //}
            }

            // Envoi des commandes à la loco
            if (node->loco.address() > 0)
            {
                if (node->loco.speed() != oldLocSpeed)
                    comptCmdLoco = 0;

                if (comptCmdLoco < 5)
                { // Message à la centrale DCC++
                    CanMsg::sendMsg(0, 0x04, 0, node->ID(),
                                    0x00,
                                    0x00,
                                    (node->loco.address() & 0xFF00) >> 8,
                                    node->loco.address() & 0x00FF,
                                    (node->loco.speed() & 0xFF00) >> 8,
                                    node->loco.speed());
#ifdef debug
                    debug.printf("[GestionReseau %d] Loco %d vitesse %d\n", __LINE__, node->loco.address(), node->loco.speed());
#endif
                    oldLocAddress = node->loco.address();
                    oldLocSpeed = node->loco.speed();
                    comptCmdLoco++;
                }
            }
        }
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));
    }
}
