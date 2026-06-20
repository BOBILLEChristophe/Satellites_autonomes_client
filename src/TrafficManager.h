/*

  TrafficManager.h


*/

#pragma once

#include <Arduino.h>
#include "CanMsg.h"
#include "Debug.h"
#include "Node.h"
#include "Settings.h"
#include "SignauxCmd.h"

// enum class BlockState : uint8_t
// {
//     Free,
//     OccupiedUnknown,
//     Running,
//     Slowing,
//     Stopping,
//     Stopped,
//     Error
// };

// enum class TrafficEvent : uint8_t
// {
//     None,
//     BlockBecameFree,
//     BlockBecameOccupied,
//     LocoDetected,
//     SpeedAboveLimit,
//     SpeedOk,
//     NextBlockOccupied,
//     BrakeSensorReached,
//     StopSensorReached,
//     ManualRestartForbidden,
//     Error
// };

/********************************************************/

enum class TrafficState : uint8_t
{
    FREE,
    RESERVED,
    OCCUPIED_UNKNOWN,
    OCCUPIED_KNOWN,
    RUNNING,
    SPEED_LIMITED,
    SLOWING,
    STOPPING,
    STOPPED,
    ERROR
};

enum class TrafficEvent : uint8_t
{
    NONE,
    BUSY_ON,
    BUSY_OFF,
    RESERVATION_REQUEST,
    LOCO_IDENTIFIED,
    INFOS_LOCO_OK,
    SPEED_ABOVE_LIMIT,
    SPEED_OK,
    NEXT_FREE,
    NEXT_BUSY,
    NEXT_RESERVED_BY_ME,
    NEXT_RESERVED_BY_OTHER,
    BRAKE_SENSOR,
    STOP_SENSOR,
    SPEED_ZERO,
    MANUAL_COMMAND_ALLOWED,
    MANUAL_COMMAND_DANGEROUS,
    TIMEOUT,
    ERROR_EVENT
};




/********************************************************/

struct TrafficContext
{
    bool busy = false;

    uint16_t locoAddr = 0;
    uint16_t locoSpeed = 0;
    uint16_t maxSpeed = 0;
    uint8_t locoDirection = 0;
    uint8_t networkDirection = 0;
    bool sensorHoraire = false;
    bool sensorAntiHor = false;
};

class TrafficManager
{
public:
    TrafficManager() = delete;
    static void setup(Node *node);
    static void IRAM_ATTR loopTask(void *p);

private:
    static uint16_t signalValue[2];
    static void signauxTask(void *p);

#ifdef TEST_MEMORY_TASK
    static void testMemory(void *p);
#endif

    static void readContext(Node *node, TrafficContext &ctx);
    static bool speedClose(uint16_t a, uint16_t b);
    static TrafficEvent detectEvent(Node *node, TrafficContext &ctx);
    static void handleState(Node *node, TrafficContext &ctx, TrafficEvent event);
    static void enterError(Node *node, const char *reason);
};