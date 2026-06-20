/*

  Loco.cpp


*/

#include "Loco.h"

// Loco::Loco() : m_address(1),
//                m_direction(0),
//                m_speed(0),
//                m_sens(0),
// {};

Loco::Loco() : m_address(1),
               m_direction(0),
               m_speed(0),
               m_sens(0),
               m_targetSpeed(0)
{};

void Loco::address(uint16_t address) { m_address = address; }
uint16_t Loco::address() const { return m_address; }
void Loco::railMode(uint8_t mode)
{
  if (mode == 0 || mode == 2 || mode == 3)
    m_railMode = mode;
}
uint8_t Loco::railMode() const { return m_railMode; }
void Loco::direction(uint8_t direction) { m_direction = direction; }
uint8_t Loco::direction() const { return m_direction; }
void Loco::sens(uint8_t sens) { m_sens = sens; }
uint8_t Loco::sens() const { return m_sens; }
void Loco::speed(uint16_t speed) { m_speed = speed; }
uint16_t Loco::speed() const { return m_speed; }
void Loco::stop() { m_speed = 0; }
void Loco::ralentis(uint16_t speed) { m_speed = speed; }
void Loco::targetSpeed(uint16_t v) { m_targetSpeed = v; }
uint16_t Loco::targetSpeed() const { return m_targetSpeed; }

