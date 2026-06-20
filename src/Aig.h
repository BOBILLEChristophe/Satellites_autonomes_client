/*

  Aig.h


*/

#pragma once

#include <Arduino.h>
#include <Servo.h>
#include "Config.h"
#include "Debug.h"

class Aig : public Servo
{
protected:
  uint8_t m_id;
  bool m_active = false;
  uint16_t m_posDroit;
  uint16_t m_posDevie;
  uint16_t m_minPos;
  uint16_t m_maxPos;
  uint8_t m_servoPin;
  bool m_aPointe;
  bool m_estDroit;
  bool m_run;
  bool m_sens;
  uint16_t m_curPos;
  uint16_t m_speed;
  static uint8_t m_compt;
  uint8_t m_nodePdroitIdx;
  uint8_t m_nodePdevieIdx;

 
public:
  Aig();
  ~Aig();
  void active(bool v) { m_active = v; }
  bool active() const { return m_active; }
  void reset(); // remet champs à défaut
  void setup();
  void move(const uint16_t);
  void pin(const byte);
  uint8_t pin() const;
  void ID(const uint8_t);
  uint8_t ID() const;
  void speed(const uint16_t);
  uint16_t speed() const;
  void estDroit(const bool);
  bool estDroit() const;
  void posDroit(const uint16_t);
  void posDevie(const uint16_t);
  uint16_t posDroit() const;
  uint16_t posDevie() const;
  uint16_t curPos() const;
  void curPos(const uint16_t);
  bool sens() const;
  void sens(const bool);
  bool isRunning() const;
  void run(const bool);
  void nodePdroitIdx(const uint8_t);
  uint8_t nodePdroitIdx() const;
  void nodePdevieIdx(const uint8_t);
  uint8_t nodePdevieIdx() const;
  static void taskGoTo(void *p);
};
