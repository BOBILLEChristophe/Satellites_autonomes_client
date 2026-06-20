

#include "TrafficManager.h"

uint16_t TrafficManager::signalValue[2] = {0, 0};

static constexpr uint8_t SENSOR_HORAIRE = 0;
static constexpr uint8_t SENSOR_ANTIHOR = 1;

static constexpr uint8_t SENS_INCONNU = 0;
static constexpr uint8_t SENS_HORAIRE = 1;
static constexpr uint8_t SENS_ANTIHOR = 2;

static constexpr uint16_t SIGNAL_ORANGE = 0;
static constexpr uint16_t SIGNAL_ROUGE = 1;
static constexpr uint16_t SIGNAL_VERT = 2;
static constexpr uint16_t SIGNAL_CARRE = 3;
static constexpr uint16_t SIGNAL_RALENTISSEMENT = 4;
static constexpr uint16_t SIGNAL_RRALENTISSEMENT = 5;

static uint16_t blockedAddr = 0;
static uint8_t blockedDccDirection = 0;
static uint8_t blockedNetworkDirection = 0;

/////////////////////////////////////////////////////////////////////////////////

static const char *stateName(TrafficState s)
{
    switch (s)
    {
    case TrafficState::FREE:
        return "FREE";
    case TrafficState::RESERVED:
        return "RESERVED";
    case TrafficState::OCCUPIED_UNKNOWN:
        return "OCCUPIED_UNKNOWN";
    case TrafficState::OCCUPIED_KNOWN:
        return "OCCUPIED_KNOWN";
    case TrafficState::RUNNING:
        return "RUNNING";
    case TrafficState::SPEED_LIMITED:
        return "SPEED_LIMITED";
    case TrafficState::SLOWING:
        return "SLOWING";
    case TrafficState::STOPPING:
        return "STOPPING";
    case TrafficState::STOPPED:
        return "STOPPED";
    case TrafficState::ERROR:
        return "ERROR";
    default:
        return "?";
    }
}

static const char *eventName(TrafficEvent e)
{
    switch (e)
    {
    case TrafficEvent::NONE:
        return "NONE";
    case TrafficEvent::BUSY_ON:
        return "BUSY_ON";
    case TrafficEvent::BUSY_OFF:
        return "BUSY_OFF";
    case TrafficEvent::RESERVATION_REQUEST:
        return "RESERVATION_REQUEST";
    case TrafficEvent::LOCO_IDENTIFIED:
        return "LOCO_IDENTIFIED";
    case TrafficEvent::INFOS_LOCO_OK:
        return "INFOS_LOCO_OK";
    case TrafficEvent::SPEED_ABOVE_LIMIT:
        return "SPEED_ABOVE_LIMIT";
    case TrafficEvent::SPEED_OK:
        return "SPEED_OK";
    case TrafficEvent::NEXT_FREE:
        return "NEXT_FREE";
    case TrafficEvent::NEXT_BUSY:
        return "NEXT_BUSY";
    case TrafficEvent::NEXT_RESERVED_BY_ME:
        return "NEXT_RESERVED_BY_ME";
    case TrafficEvent::NEXT_RESERVED_BY_OTHER:
        return "NEXT_RESERVED_BY_OTHER";
    case TrafficEvent::BRAKE_SENSOR:
        return "BRAKE_SENSOR";
    case TrafficEvent::STOP_SENSOR:
        return "STOP_SENSOR";
    case TrafficEvent::SPEED_ZERO:
        return "SPEED_ZERO";
    case TrafficEvent::MANUAL_COMMAND_ALLOWED:
        return "MANUAL_COMMAND_ALLOWED";
    case TrafficEvent::MANUAL_COMMAND_DANGEROUS:
        return "MANUAL_COMMAND_DANGEROUS";
    case TrafficEvent::TIMEOUT:
        return "TIMEOUT";
    case TrafficEvent::ERROR_EVENT:
        return "ERROR_EVENT";
    default:
        return "?";
    }
}

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

static void clearReservation(Node *node)
{
    if (node == nullptr)
        return;

    node->reservedBy(0);
    node->reservedSens(SENS_INCONNU);
    node->reservedAt(0);

    node->reservedLoco.address(1);
    node->reservedLoco.speed(0);
    node->reservedLoco.targetSpeed(1000);
    node->reservedLoco.direction(0);
    node->reservedLoco.sens(SENS_INCONNU);

    LOG_INFO("Reservation effacee");
}

void TrafficManager::enterError(Node *node, const char *reason)
{
    node->trafficState((uint8_t)TrafficState::ERROR);
    LOG_ERROR("TrafficManager ERROR : %s", reason);
}

static bool speedClose(uint16_t a, uint16_t b)
{
    constexpr uint16_t SPEED_TOLERANCE = 20;

    return (a > b)
               ? ((a - b) <= SPEED_TOLERANCE)
               : ((b - a) <= SPEED_TOLERANCE);
}

static bool forceStop(Node *node, uint16_t addr)
{
    // Direction = avant, vitesse = 0;
    CanMsg::sendMsg(0, 0x05, 0, node->ID(),
                    0x00, 0x00,
                    (uint8_t)((addr >> 8) & 0xFF),
                    (uint8_t)(addr & 0xFF),
                    node->loco.direction());

    bool ok = CanMsg::sendMsg(0, 0x04, 0, node->ID(),
                              0x00, 0x00,
                              (uint8_t)((addr >> 8) & 0xFF),
                              (uint8_t)(addr & 0xFF),
                              0x00,
                              0x00);

    if (ok)
        node->loco.targetSpeed(0);

    return ok;
}

/////////////////////////////////////////////////////////////////////////////////

static bool commandSpeedIfNeeded(Node *node, uint16_t addr, uint16_t targetSpeed)
{
    if (speedClose(node->loco.targetSpeed(), targetSpeed))
        return false;

    const uint8_t dir = node->loco.direction(); // 1=FWD, 2=REV

    // 1) Envoyer d'abord la direction connue par la base loco
    CanMsg::sendMsg(0, 0x05, 0, node->ID(),
                    0x00, 0x00,
                    (uint8_t)((addr >> 8) & 0xFF),
                    (uint8_t)(addr & 0xFF),
                    dir);

    // 2) Puis envoyer la vitesse
    bool ok = CanMsg::sendMsg(0, 0x04, 0, node->ID(),
                              0x00, 0x00,
                              (uint8_t)((addr >> 8) & 0xFF),
                              (uint8_t)(addr & 0xFF),
                              (uint8_t)((targetSpeed >> 8) & 0xFF),
                              (uint8_t)(targetSpeed & 0xFF));

    if (ok)
        node->loco.targetSpeed(targetSpeed);

    return ok;
}

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

bool TrafficManager::speedClose(uint16_t a, uint16_t b)
{
    constexpr uint16_t SPEED_TOLERANCE = 20; // 2 %
    return (a > b) ? ((a - b) <= SPEED_TOLERANCE)
                   : ((b - a) <= SPEED_TOLERANCE);
}

void TrafficManager::readContext(Node *node, TrafficContext &ctx)
{
    ctx.busy = node->busy();
    // LOG_INFO("busy=%u", node->busy());
    ctx.locoAddr = node->loco.address();
    ctx.locoSpeed = node->loco.speed();
    ctx.maxSpeed = node->maxSpeed();

    ctx.locoDirection = node->loco.direction();
    ctx.networkDirection = node->loco.sens();

    ctx.sensorHoraire = node->sensor[SENSOR_HORAIRE].state();
    ctx.sensorAntiHor = node->sensor[SENSOR_ANTIHOR].state();
}

TrafficEvent TrafficManager::detectEvent(Node *node, TrafficContext &ctx)
{

    const TrafficState state = static_cast<TrafficState>(node->trafficState());

    if (state == TrafficState::RESERVED)
    {
        if (!ctx.busy)
            return TrafficEvent::NONE;

        return TrafficEvent::BUSY_ON;
    }

    // if (ctx.busy && ctx.locoAddr > 1 && ctx.networkDirection == 0)
    // {
    //     if (ctx.sensorAntiHor && !ctx.sensorHoraire)
    //     {
    //         node->loco.sens(SENS_HORAIRE);
    //         ctx.networkDirection = SENS_HORAIRE;
    //         LOG_INFO("Sens reseau detecte : HORAIRE");
    //     }
    //     else if (ctx.sensorHoraire && !ctx.sensorAntiHor)
    //     {
    //         node->loco.sens(SENS_ANTIHOR);
    //         ctx.networkDirection = SENS_ANTIHOR;
    //         LOG_INFO("Sens reseau detecte : ANTIHOR");
    //     }
    // }

    if (ctx.busy && ctx.locoAddr > 1 && ctx.networkDirection == SENS_INCONNU)
    {
        if (ctx.sensorAntiHor && !ctx.sensorHoraire)
        {
            node->loco.sens(SENS_HORAIRE);
            ctx.networkDirection = SENS_HORAIRE;
            LOG_INFO("Sens reseau detecte par capteur : HORAIRE");
        }
        else if (ctx.sensorHoraire && !ctx.sensorAntiHor)
        {
            node->loco.sens(SENS_ANTIHOR);
            ctx.networkDirection = SENS_ANTIHOR;
            LOG_INFO("Sens reseau detecte par capteur : ANTIHORAIRE");
        }
        else if (ctx.locoDirection == 1)
        {
            node->loco.sens(SENS_HORAIRE);
            ctx.networkDirection = SENS_HORAIRE;
            LOG_INFO("Sens reseau deduit de dir DCC : HORAIRE");
        }
        else if (ctx.locoDirection == 2)
        {
            node->loco.sens(SENS_ANTIHOR);
            ctx.networkDirection = SENS_ANTIHOR;
            LOG_INFO("Sens reseau deduit de dir DCC : ANTIHORAIRE");
        }
        else
        {
            LOG_INFO("Sens reseau impossible : H=%u AH=%u dir=%u",
                     ctx.sensorHoraire,
                     ctx.sensorAntiHor,
                     ctx.locoDirection);
        }
    }

    if (static_cast<TrafficState>(node->trafficState()) == TrafficState::STOPPED &&
        ctx.busy &&
        ctx.locoAddr > 1 &&
        ctx.locoSpeed > 20)
    {
        // Même loco, même direction que celle qui venait vers le canton occupé :
        // redémarrage interdit.
        if (ctx.locoAddr == blockedAddr &&
            ctx.locoDirection == blockedDccDirection)
        {
            return TrafficEvent::MANUAL_COMMAND_DANGEROUS;
        }

        // Direction inverse : autorisée.
        return TrafficEvent::LOCO_IDENTIFIED;
    }

    if (!ctx.busy)
        return TrafficEvent::BUSY_OFF;

    if (ctx.locoAddr <= 1)
        return TrafficEvent::BUSY_ON;

    if (ctx.busy && ctx.locoAddr > 1)
    {
        uint8_t nextIdx = UNUSED_ID;

        if (ctx.networkDirection == 1) // SENS_HORAIRE
            nextIdx = node->SP1_idx();
        else if (ctx.networkDirection == 2) // ANTI-HORAIRE
            nextIdx = node->SM1_idx();

        if (nextIdx != UNUSED_ID && node->nodeP[nextIdx] != nullptr)
        {
            if (
                node->nodeP[nextIdx]->busy() ||
                (node->nodeP[nextIdx]->reservedBy() > 1 &&
                 node->nodeP[nextIdx]->reservedBy() != ctx.locoAddr))
            {
                LOG_INFO("Next canton reserveBy=%u", node->nodeP[nextIdx]->reservedBy());

                // sens horaire : premier capteur = SENS_HORAIRE, second = SENS_ANTIHOR
                if (ctx.networkDirection == 1)
                {
                    if (ctx.sensorAntiHor)
                        return TrafficEvent::STOP_SENSOR;

                    if (ctx.sensorHoraire)
                        return TrafficEvent::BRAKE_SENSOR;
                }

                // sens anti-horaire : premier capteur = SENS_ANTIHOR, second = SENS_HORAIRE
                if (ctx.networkDirection == 2)
                {
                    if (ctx.sensorHoraire)
                        return TrafficEvent::STOP_SENSOR;

                    if (ctx.sensorAntiHor)
                        return TrafficEvent::BRAKE_SENSOR;
                }
                return TrafficEvent::NEXT_BUSY;
            }
        }
    }

    if (ctx.locoSpeed > ctx.maxSpeed && !speedClose(ctx.locoSpeed, ctx.maxSpeed))
        return TrafficEvent::SPEED_ABOVE_LIMIT;

    if (static_cast<TrafficState>(node->trafficState()) == TrafficState::SPEED_LIMITED &&
        ctx.locoSpeed <= ctx.maxSpeed)
    {
        return TrafficEvent::SPEED_OK;
    }

    constexpr uint32_t RESERVATION_TIMEOUT_MS = 1500;

    if (node->reservedBy() > 1 && !node->busy() &&
        millis() - node->reservedAt() > RESERVATION_TIMEOUT_MS)
    {
        clearReservation(node);
        node->trafficState((uint8_t)TrafficState::FREE);
    }

    return TrafficEvent::LOCO_IDENTIFIED;
}

void TrafficManager::handleState(Node *node, TrafficContext &ctx, TrafficEvent event)
{
    TrafficState oldState = static_cast<TrafficState>(node->trafficState());

    if (event == TrafficEvent::BUSY_OFF)
    {
        node->sensor[SENSOR_HORAIRE].state(LOW);
        node->sensor[SENSOR_ANTIHOR].state(LOW);

        blockedAddr = 0;
        blockedDccDirection = 0;
        blockedNetworkDirection = 0;

        node->loco.sens(0);
        node->loco.speed(0);
        node->loco.targetSpeed(1000);
        node->loco.direction(0);
        node->loco.address(1);

        node->trafficState((uint8_t)TrafficState::FREE);
        return;
    }

    if (!ctx.busy &&
        static_cast<TrafficState>(node->trafficState()) != TrafficState::RESERVED)
    {
        node->trafficState((uint8_t)TrafficState::FREE);
        return;
    }

    if (ctx.busy &&
        static_cast<TrafficState>(node->trafficState()) == TrafficState::FREE)
    {
        if (ctx.locoAddr > 1)
            node->trafficState((uint8_t)TrafficState::OCCUPIED_KNOWN);
        else
            node->trafficState((uint8_t)TrafficState::OCCUPIED_UNKNOWN);
    }

    switch (static_cast<TrafficState>(node->trafficState()))
    {
    case TrafficState::FREE:
        if (event == TrafficEvent::LOCO_IDENTIFIED)
            node->trafficState((uint8_t)TrafficState::RUNNING);
        break;

    case TrafficState::RUNNING:
        if (ctx.networkDirection == 0)
        {
            node->trafficState((uint8_t)TrafficState::OCCUPIED_KNOWN);
            LOG_INFO("RUNNING annule : sens reseau inconnu loco %u", ctx.locoAddr);
            break;
        }
        if (event == TrafficEvent::SPEED_ABOVE_LIMIT)
        {
            if (commandSpeedIfNeeded(node, ctx.locoAddr, ctx.maxSpeed))
            {
                LOG_INFO("Ralentissement loco %u : %u -> %u",
                         ctx.locoAddr, ctx.locoSpeed, ctx.maxSpeed);

                node->trafficState((uint8_t)TrafficState::SPEED_LIMITED);
            }
        }
        else if (event == TrafficEvent::BRAKE_SENSOR)
        {
            if (commandSpeedIfNeeded(node, ctx.locoAddr, 200))
                LOG_INFO("Freinage loco %u : canton suivant occupe", ctx.locoAddr);

            node->trafficState((uint8_t)TrafficState::SLOWING);
        }
        break;

    case TrafficState::SLOWING:
        if (event == TrafficEvent::STOP_SENSOR)
        {
            if (commandSpeedIfNeeded(node, ctx.locoAddr, 0))
                LOG_INFO("Arret loco %u : canton suivant occupe", ctx.locoAddr);

            blockedAddr = ctx.locoAddr;
            blockedDccDirection = ctx.locoDirection; // 1=FWD, 2=REV
            blockedNetworkDirection = ctx.networkDirection;

            node->trafficState((uint8_t)TrafficState::STOPPING);
        }
        break;

    case TrafficState::STOPPING:
        if (speedClose(ctx.locoSpeed, 0))
            node->trafficState((uint8_t)TrafficState::STOPPED);
        break;

    case TrafficState::STOPPED:
        if (!ctx.busy)
        {
            node->trafficState((uint8_t)TrafficState::FREE);
        }
        else if (event == TrafficEvent::MANUAL_COMMAND_DANGEROUS)
        {
            if (forceStop(node, ctx.locoAddr))
            {
                LOG_INFO("Redemarrage interdit loco %u : canton suivant occupe", ctx.locoAddr);
            }
        }
        break;

    case TrafficState::OCCUPIED_UNKNOWN:
        if (event == TrafficEvent::LOCO_IDENTIFIED)
            node->trafficState((uint8_t)TrafficState::OCCUPIED_KNOWN);
        break;

    case TrafficState::OCCUPIED_KNOWN:
        if (ctx.networkDirection != 0)
        {
            node->trafficState((uint8_t)TrafficState::RUNNING);
        }
        break;

    case TrafficState::SPEED_LIMITED:
        if (event == TrafficEvent::SPEED_OK)
            node->trafficState((uint8_t)TrafficState::RUNNING);
        break;

    case TrafficState::RESERVED:
    {
        if (event == TrafficEvent::BUSY_ON)
        {
            const uint8_t sensReserve = node->reservedSens();

            node->loco.address(node->reservedLoco.address());
            node->loco.speed(node->reservedLoco.speed());
            node->loco.direction(node->reservedLoco.direction());
            node->loco.sens(sensReserve);

            LOG_INFO("TRANSFERT reservation -> loco : addr=%u sensReserve=%s",
                     node->loco.address(),
                     sensName(node->loco.sens()));

            clearReservation(node);
            node->trafficState((uint8_t)TrafficState::OCCUPIED_KNOWN);
        }
        break;
    }

    default:
        enterError(node, "Etat inconnu");
        break;
    }

    TrafficState newState = static_cast<TrafficState>(node->trafficState());

    if (oldState != newState)
    {
        LOG_INFO("TM transition %s + %s -> %s",
                 stateName(oldState),
                 eventName(event),
                 stateName(newState));
    }
}

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

void IRAM_ATTR TrafficManager::loopTask(void *pvParameters)
{
    Node *node = (Node *)pvParameters;

    uint8_t index = 0;
    bool sens0 = false;
    bool sens1 = false;
    bool s2access = false;
    bool s2busy = false;

    uint16_t oldLocoInfoRequestAddr = 0;
    uint32_t lastLocoInfoRequestMs = 0;

    TickType_t xLastWakeTime = xTaskGetTickCount();

    TickType_t lastSignalChangeTime = xTaskGetTickCount();
    const TickType_t signalDelay = pdMS_TO_TICKS(1000);

    for (;;)
    {

        /*************************************************************************************
         * Canton occupé : demande infos loco + sens de roulage
         ************************************************************************************/

        if (node->busy())
        {
            if (node->loco.address() > 1)
            {
                const uint16_t addr = node->loco.address();
                const uint32_t nowMs = millis();

                if (addr != oldLocoInfoRequestAddr || nowMs - lastLocoInfoRequestMs > 1000)
                {
                    CanMsg::sendMsg(1, 0xAB, 0, node->ID(),
                                    0x00,
                                    0x00,
                                    (uint8_t)(0xC0 | ((addr >> 8) & 0x3F)),
                                    (uint8_t)(addr & 0xFF));

                    oldLocoInfoRequestAddr = addr;
                    lastLocoInfoRequestMs = nowMs;
                }
            }

            // if (node->sensor[SENSOR_HORAIRE].state() && !node->sensor[SENSOR_ANTIHOR].state())
            //     node->loco.sens(1);

            // if (node->sensor[SENSOR_ANTIHOR].state() && !node->sensor[SENSOR_HORAIRE].state())
            //     node->loco.sens(2);
        }

        if (node->reservedBy() > 1 && !node->busy() && millis() - node->reservedAt() > 1500)
        {
            LOG_INFO("Reservation expiree loco %u", node->reservedBy());
            clearReservation(node);
        }

        /*************************************************************************************
         * Recherche SP1 / SM1
         ************************************************************************************/

        auto rechercheSat = [node](bool satPos) -> uint8_t
        {
            uint8_t idxA = 0;
            uint8_t idxS = 0;

            if (satPos == 1)
            {
                idxA = 3;
                idxS = 4;
            }

            uint8_t idx = idxS;

            if (node->aig[0 + idxA] != nullptr)
            {
                if (node->aig[0 + idxA]->estDroit())
                {
                    idx = 0 + idxS;

                    if (node->aig[1 + idxA] != nullptr)
                    {
                        if (!node->aig[1 + idxA]->estDroit())
                            idx = 0 + idxS;
                        else
                            idx = 1 + idxS;
                    }
                }
                else
                {
                    idx = 1 + idxS;

                    if (node->aig[2 + idxA] != nullptr)
                    {
                        if (node->aig[2 + idxA]->estDroit())
                            idx = 2 + idxS;
                        else
                            idx = 3 + idxS;
                    }
                }
            }

            return idx;
        };

        node->SP1_idx(rechercheSat(0)); // côté horaire / SP
        node->SM1_idx(rechercheSat(1)); // côté anti-horaire / SM

        TrafficContext ctx;
        readContext(node, ctx);

        TrafficEvent event = detectEvent(node, ctx);
        ////////////////////////////////////////////////
        uint8_t sp1Idx = node->SP1_idx();
        uint8_t sm1Idx = node->SM1_idx();

        uint16_t sp1Id = 0;
        uint16_t sm1Id = 0;

        if (sp1Idx != UNUSED_ID && node->nodeP[sp1Idx] != nullptr)
            sp1Id = node->nodeP[sp1Idx]->ID();

        if (sm1Idx != UNUSED_ID && node->nodeP[sm1Idx] != nullptr)
            sm1Id = node->nodeP[sm1Idx]->ID();

        // LOG_INFO("TM ctx state=%s event=%s busy=%u addr=%u speed=%u target=%u sens=%u dir=%u max=%u H=%u AH=%u SP1_idx=%u SP1_id=%u SM1_idx=%u SM1_id=%u",
        //          stateName((TrafficState)node->trafficState()),
        //          eventName(event),
        //          ctx.busy,
        //          ctx.locoAddr,
        //          ctx.locoSpeed,
        //          node->loco.targetSpeed(),
        //          ctx.networkDirection,
        //          ctx.locoDirection,
        //          ctx.maxSpeed,
        //          ctx.sensorHoraire,
        //          ctx.sensorAntiHor,
        //          sp1Idx,
        //          sp1Id,
        //          sm1Idx,
        //          sm1Id);
        TrafficState state = static_cast<TrafficState>(node->trafficState());
        LOG_INFO(
            "TM ctx state=%s event=%s "
            "busy=%u "
            "addr=%u speed=%u target=%u sens=%u dir=%u "
            "reservedAddr=%u reservedSpeed=%u reservedDir=%u "
            "reservedBy=%u reservedSens=%u "
            "max=%u H=%u AH=%u "
            "SP1_idx=%u SP1_id=%u "
            "SM1_idx=%u SM1_id=%u",

            stateName(state),
            eventName(event),

            ctx.busy,

            node->loco.address(),
            node->loco.speed(),
            node->loco.targetSpeed(),
            node->loco.sens(),
            node->loco.direction(),

            node->reservedLoco.address(),
            node->reservedLoco.speed(),
            node->reservedLoco.direction(),

            node->reservedBy(),
            node->reservedSens(),

            ctx.maxSpeed,
            ctx.sensorHoraire,
            ctx.sensorAntiHor,

            node->SP1_idx(),
            node->SP1_idx() != UNUSED_ID ? node->nodeP[node->SP1_idx()]->ID() : 0,

            node->SM1_idx(),
            node->SM1_idx() != UNUSED_ID ? node->nodeP[node->SM1_idx()]->ID() : 0);

        //////////////////////////////////////////////////////////////////////////////////

        handleState(node, ctx, event);

        /*************************************************************************************
         * Envoi état du satellite
         ************************************************************************************/

        if (node->nodeP[node->SP1_idx()] != nullptr &&
            node->nodeP[node->SM1_idx()] != nullptr)
        {
            CanMsg::sendMsg(1, 0xE0, 0, node->ID(),
                            node->busy(),
                            (uint8_t)node->nodeP[node->SP1_idx()]->ID(),
                            (uint8_t)node->nodeP[node->SM1_idx()]->ID(),
                            node->nodeP[node->SP1_idx()]->acces(),
                            node->nodeP[node->SP1_idx()]->busy(),
                            node->nodeP[node->SM1_idx()]->acces(),
                            node->nodeP[node->SM1_idx()]->busy());
        }

        /*************************************************************************************
         * Réservation du canton suivant
         ************************************************************************************/

        if (node->busy() && node->loco.address() > 1)
        {
            uint8_t nodeIdx = UNUSED_ID;
            // switch (1)
            switch (node->loco.sens())
            {
            case 1:
                nodeIdx = node->SP1_idx();
                break;
            case 2:
                nodeIdx = node->SM1_idx();
                break;
            default:
                break;
            }

            LOG_INFO("Reservation check sens=%u SP1_idx=%u SM1_idx=%u nodeIdx=%u addr=%u",
                     node->loco.sens(),
                     node->SP1_idx(),
                     node->SM1_idx(),
                     nodeIdx,
                     node->loco.address());

            if (nodeIdx != UNUSED_ID && node->nodeP[nodeIdx] != nullptr)
            {
                const uint16_t addr = node->loco.address();

                uint8_t reservedSens = SENS_INCONNU;

                if (nodeIdx == node->SP1_idx())
                {
                    reservedSens = SENS_HORAIRE;
                }
                else if (nodeIdx == node->SM1_idx())
                {
                    reservedSens = SENS_ANTIHOR;
                }
                else
                {
                    LOG_ERROR("Reservation impossible : nodeIdx incoherent=%u", nodeIdx);
                    return;
                }

                CanMsg::sendMsg(1, 0xE3, 0, node->ID(),
                                (uint8_t)node->nodeP[nodeIdx]->ID(),
                                (uint8_t)((addr >> 8) & 0xFF),
                                (uint8_t)(addr & 0xFF),
                                reservedSens);

                LOG_INFO("Envoi reservation 0xE3 target=%u loco=%u sensReserve=%s",
                         node->nodeP[nodeIdx]->ID(),
                         addr,
                         sensName(reservedSens));
            }
        }

        /*************************************************************************************
         * Signalisation
         ************************************************************************************/

        for (uint8_t i = 0; i < 2; i++)
        {
            const char *cantonName0 = "";
            const char *cantonName1 = "";

            s2busy = false;
            s2access = false;

            switch (i)
            {
            case 0:
                index = node->SP1_idx();
                sens0 = SENS_HORAIRE;
                sens1 = SENS_ANTIHOR;
                s2access = node->SP2_acces();
                s2busy = node->SP2_busy();
                cantonName0 = "SP1";
                cantonName1 = "SP2";
                break;

            case 1:
                index = node->SM1_idx();
                sens0 = SENS_ANTIHOR;
                sens1 = SENS_HORAIRE;
                s2access = node->SM2_acces();
                s2busy = node->SM2_busy();
                cantonName0 = "SM1";
                cantonName1 = "SM2";
                break;
            }

            if (node->nodeP[index] != nullptr)
            {
                if (node->nodeP[index]->acces())
                {
                    const bool nextBusy = node->nodeP[index]->busy();

                    const bool nextReservedByOther =
                        node->nodeP[index]->reservedBy() > 1 &&
                        node->nodeP[index]->reservedBy() != node->loco.address();

                    if (nextBusy || nextReservedByOther)
                    {
                        signalValue[i] = SIGNAL_ROUGE;
                    }
                    else
                    {
                        signalValue[i] = SIGNAL_VERT;

                        if (s2access)
                            signalValue[i] = s2busy ? SIGNAL_ORANGE : SIGNAL_VERT;
                        else
                            signalValue[i] = SIGNAL_ORANGE;
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

            static uint16_t oldSignalValue0 = 0xFFFF;
            static uint16_t oldSignalValue1 = 0xFFFF;

            if (signalValue[0] != oldSignalValue0)
            {
                lastSignalChangeTime = xTaskGetTickCount();
                oldSignalValue0 = signalValue[0];
            }

            if (signalValue[1] != oldSignalValue1)
            {
                lastSignalChangeTime = xTaskGetTickCount();
                oldSignalValue1 = signalValue[1];
            }

            if (xTaskGetTickCount() - lastSignalChangeTime > signalDelay)
            {
                const bool signalArret =
                    (signalValue[i] == SIGNAL_CARRE || signalValue[i] == SIGNAL_ROUGE);

                const bool signalRalenti =
                    (signalValue[i] == SIGNAL_RALENTISSEMENT ||
                     signalValue[i] == SIGNAL_RRALENTISSEMENT);

                if (signalArret)
                {
                    if (node->loco.sens() == 1) // SENS_HORAIRE
                    {
                        if (node->sensor[SENSOR_HORAIRE].state())
                            node->loco.speed(200);

                        if (node->sensor[SENSOR_ANTIHOR].state())
                            node->loco.stop();
                    }
                    else if (node->loco.sens() == 2) // ANTI-HORAIRE
                    {
                        if (node->sensor[SENSOR_ANTIHOR].state())
                            node->loco.speed(200);

                        if (node->sensor[SENSOR_HORAIRE].state())
                            node->loco.stop();
                    }
                }
                else if (signalRalenti)
                {
                    if (node->loco.sens() == 1) // SENS_HORAIRE
                    {
                        if (node->sensor[SENSOR_HORAIRE].state() && node->loco.speed() > 200)
                            node->loco.speed(200);
                    }
                    else if (node->loco.sens() == 2) // ANTI-HORAIRE
                    {
                        if (node->sensor[SENSOR_ANTIHOR].state() && node->loco.speed() > 200)
                            node->loco.speed(200);
                    }
                }
            }
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));
    }
}