/*

  Loco.h


*/

#ifndef __LOCO_H__
#define __LOCO_H__

#include <Arduino.h>

class Loco
{

private:
  uint16_t m_address;
  uint8_t m_railMode;  // 0 indéterminé - 2 = 2R - 3 = 3R                     // 0 inderterminé - 2 2R - 3 3R
  uint8_t m_direction; // 0 inderterminé - 1 avant - 2 arrière
  uint16_t m_speed;
  uint8_t m_sens; // 0 inderterminé - 1 horaire - 2 anti-horaire
  uint16_t m_targetSpeed;

public:
  Loco(); // Constructor
  void address(uint16_t);
  uint16_t address() const;
  void railMode(uint8_t mode);
  uint8_t railMode() const;
  void direction(uint8_t);
  uint8_t direction() const;
  void sens(uint8_t);
  uint8_t sens() const;
  void speed(uint16_t);
  uint16_t speed() const;
  void ralentis(uint16_t);
  void stop();
  void targetSpeed(uint16_t);
  uint16_t targetSpeed() const;
};

#endif
