/*

  Aig.cpp


*/

#include "Aig.h"

uint8_t Aig::m_compt(0);

Aig::Aig() : m_posDroit(1500),
             m_posDevie(1500),
             m_minPos(800),
             m_maxPos(2400),
             m_servoPin(UNUSED_PIN),
             m_aPointe(true),
             m_estDroit(true),
             m_run(false),
             m_sens(HIGH),
             m_curPos(0),
             m_speed(AIG_SPEED) {};

Aig::~Aig() {};

void Aig::setup()
{
  attach(m_servoPin);
  m_curPos = m_posDroit;
  move(m_posDroit);
  m_estDroit = true;
  vTaskDelay(pdMS_TO_TICKS(1000));
}

void Aig::move(const uint16_t pos)
{
  if ((pos > m_minPos) && (pos < m_maxPos))
    writeMicroseconds(pos);
}

void Aig::speed(uint16_t speed)
{
  if (speed > 0 && speed <= 10000)
    m_speed = speed;
  else
    m_speed = 5000;
}

void Aig::posDroit(const uint16_t pos)
{
  if (pos > m_minPos && pos < m_maxPos)
    m_posDroit = pos;
}

void Aig::posDevie(const uint16_t pos)
{
  if (pos > m_minPos && pos < m_maxPos)
    m_posDevie = pos;
}

void Aig::curPos(const uint16_t curPos)
{
  if (curPos > m_minPos && curPos < m_maxPos)
    m_curPos = curPos;
}

void Aig::ID(const uint8_t id) { m_id = id; }
uint8_t Aig::ID() const { return m_id; }
void Aig::estDroit(const bool pos) { m_estDroit = pos; }
bool Aig::estDroit() const { return m_estDroit; }
void Aig::pin(const byte pin) { m_servoPin = pin; }
uint8_t Aig::pin() const { return m_servoPin; }
uint16_t Aig::speed() const { return m_speed; }
uint16_t Aig::posDroit() const { return m_posDroit; }
uint16_t Aig::posDevie() const { return m_posDevie; }
uint16_t Aig::curPos() const { return m_curPos; }
bool Aig::sens() const { return m_sens; }
void Aig::sens(const bool sens) { m_sens = sens; }
bool Aig::isRunning() const { return m_run; }
void Aig::run(const bool run) { m_run = run; }
void Aig::nodePdroitIdx(const uint8_t nodePdroitIdx) { m_nodePdroitIdx = nodePdroitIdx; }
uint8_t Aig::nodePdroitIdx() const { return m_nodePdroitIdx; }
void Aig::nodePdevieIdx(const uint8_t nodePdevieIdx) { m_nodePdevieIdx = nodePdevieIdx; }
uint8_t Aig::nodePdevieIdx() const { return m_nodePdevieIdx; }

void Aig::taskGoTo(void *p)
{
  Aig *a = static_cast<Aig *>(p);
  if (!a) vTaskDelete(NULL);

  // Bornage vitesse
  if (a->m_speed < 1000 || a->m_speed > 10000)
    a->m_speed = 5000;

  // Déterminer la cible : si on est sur droit, aller sur dévié, sinon aller sur droit
  const uint16_t target = (a->m_curPos == a->m_posDroit) ? a->m_posDevie : a->m_posDroit;

  // Déterminer le sens d'incrémentation pour aller vers target
  // (si target > curPos => +1 sinon -1)
  const int8_t step = (target > a->m_curPos) ? +1 : -1;

  a->run(true);

  for (;;)
  {
    if (!a->isRunning())
    {
      LOG_INFO("Position : %d\n", a->m_curPos);
      LOG_INFO("Speed : %d\n", a->m_speed);
      vTaskDelete(NULL);
    }

    // Avance d’un micro-pas
    a->m_curPos = (uint16_t)((int)a->m_curPos + step);
    a->writeMicroseconds(a->m_curPos);

    // Arrivé ?
    if (a->m_curPos == target)
    {
      a->m_estDroit = (target == a->m_posDroit);
      a->run(false);
      continue;
    }

    delayMicroseconds(a->m_speed);
  }
}
