/*

  Loco.cpp


*/

#include "Loco.h"

constexpr uint16_t LOCO_SPEED_MAX = 1000;
constexpr uint16_t LOCO_SPEED_ESTOP = 1;

Loco::Loco() : m_address(0),
               m_railMode(0),
               m_speed(0),
               m_speedMax(1000),
               m_speedRalenti(200),
               m_oldSpeed(0),
               m_networkDirection(0),
               m_envoiSpeedCmd(false) {};

void Loco::address(uint16_t address) { m_address = address; }
uint16_t Loco::address() const { return m_address; }
void Loco::railMode(uint8_t mode)
{
  if (mode == 0 || mode == 2 || mode == 3)
    m_railMode = mode;
}
uint8_t Loco::railMode() const { return m_railMode; }
void Loco::networkDirection(const uint8_t direction) { m_networkDirection = direction; } // HORAIRE / ANTIHORAIRE
uint8_t Loco::networkDirection() const { return m_networkDirection; }  
void Loco::speed(const uint16_t speed)
{
  m_speed = speed;
  if (m_speed > LOCO_SPEED_MAX)
    m_speed = LOCO_SPEED_MAX;
  if (speed == LOCO_SPEED_ESTOP)
    m_speed = 0;
}
uint16_t Loco::speed() const { return m_speed; }
void Loco::stop() { m_speed = 0; }
void Loco::e_stop() { m_speed = 1; }
void Loco::sRalenti() { m_speed = m_speedRalenti; }
uint16_t Loco::gRalenti() const { return m_speedRalenti; }
void Loco::envoiSpeedCmd(const bool val) { m_envoiSpeedCmd = val; }
bool Loco::envoiSpeedCmd()
{
  static uint8_t compt = 0;
  compt++;
  if (m_oldSpeed != m_speed)
    m_envoiSpeedCmd = true;
  else
  {
    m_envoiSpeedCmd = false;
    compt = 0;
  }
  if (compt > 5)
  {
    LOG_ERROR("Loco envoiSpeedCmd répétition > 5");
    compt = 0;
    return false;
  }
  else
    return m_envoiSpeedCmd;
}
void Loco::oldSpeed(const uint16_t speed) { m_oldSpeed = speed; }
uint16_t Loco::oldSpeed() const { return m_oldSpeed; }
