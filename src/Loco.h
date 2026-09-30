/*

  Loco.h


*/

#ifndef __LOCO_H__
#define __LOCO_H__

#include <Arduino.h>
#include "Debug.h"

class Loco
{

private:
  uint16_t m_address;
  uint8_t m_railMode;          // 0 indéterminé - 2 = 2R - 3 = 3R
  uint16_t m_speed;
  uint16_t m_speedMax;
  uint16_t m_speedRalenti;
  uint16_t m_oldSpeed;
  uint8_t m_networkDirection;   // 0 = inconnu, 1 = HORAIRE, 2 = ANTIHORAIRE

  uint8_t m_envoiSpeedCmd;

public:
  Loco(); // Constructor
  void address(const uint16_t);
  uint16_t address() const;
  void railMode(const uint8_t);
  uint8_t railMode() const;
  void speed(const uint16_t);
  uint16_t speed() const;
  void oldSpeed(const uint16_t);
  uint16_t oldSpeed() const;
  void networkDirection(const uint8_t);
  uint8_t networkDirection() const;
  void sRalenti();
  uint16_t gRalenti()const;
  void stop();
  void e_stop();
  void envoiSpeedCmd(const bool);
  bool envoiSpeedCmd();
};

#endif
