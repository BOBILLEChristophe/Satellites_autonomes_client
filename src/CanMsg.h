/*

  CanMsg.h

  Structure des identifiants CAN : https://www.locoduino.org/IMG/png/satautonomes_messageriecan_v1.png
*/

#pragma once

#include <ACAN_ESP32.h>
#include "Config.h"
#include "Discovery.h"
#include "Settings.h"
#include "TrafficManager.h"

class CanMsg
{
public:
  CanMsg() = delete;
  static void setup(Node *);
  static void testMemory(void *);
  static void canReceiveMsg(void *);
  static bool sendMsg(CANMessage &);
  static bool sendMsg(byte, byte, byte, uint16_t);
  static bool sendMsg(byte, byte, byte, uint16_t, byte);
  static bool sendMsg(byte, byte, byte, uint16_t, byte, byte);
  static bool sendMsg(byte, byte, byte, uint16_t, byte, byte, byte);
  static bool sendMsg(byte, byte, byte, uint16_t, byte, byte, byte, byte);
  static bool sendMsg(byte, byte, byte, uint16_t, byte, byte, byte, byte, byte);
  static bool sendMsg(byte, byte, byte, uint16_t, byte, byte, byte, byte, byte, byte);
  static bool sendMsg(byte, byte, byte, uint16_t, byte, byte, byte, byte, byte, byte, byte);
  static bool sendMsg(byte, byte, byte, uint16_t, byte, byte, byte, byte, byte, byte, byte, byte);
};
