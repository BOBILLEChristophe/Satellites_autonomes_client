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
};