/*

   Node.h


*/

#pragma once

#include <Arduino.h>

#include "Aig.h"
#include "Config.h"
#include "Loco.h"
#ifdef RFID
#include "RFID.h"
#endif
#include "Sensor.h"
#include "Signal.h"

class NodePeriph
{
protected:
  uint16_t m_id;
  bool m_busy;
  bool m_reserved;

  uint16_t m_reservedBySat;
  uint16_t m_reservedByLoco;

  bool m_acces;
  uint16_t m_locoAddr;
  uint8_t m_masqueAig;
  uint8_t m_typeCible;

public:
  NodePeriph();
  ~NodePeriph();

  static uint8_t comptInst;

  void ID(uint16_t);
  uint16_t ID();

  void busy(bool);
  bool busy();

  void reserved(bool);
  bool reserved();

  void reservedBySat(uint16_t);
  uint16_t reservedBySat() const;

  void reservedByLoco(uint16_t);
  uint16_t reservedByLoco() const;

  bool reservedFor(uint16_t satId, uint16_t locoAddr) const;

  void acces(bool);
  bool acces();

  void locoAddr(uint16_t);
  uint16_t locoAddr();

  void masqueAig(uint8_t);
  uint8_t masqueAig();
};

class Reserved
{
private:
  bool m_reserved;
  uint32_t m_reservedAt;
  uint16_t m_reservedBySat;
  uint16_t m_reservedByLoco;

public:
  Reserved();
  bool reservation(const uint16_t, const uint16_t);
  bool reserved() const;
  uint16_t reservedBySat() const;
  uint16_t reservedByLoco() const;
  void refresh();
};

class Node
{
  friend class Discovery;

private:
  uint16_t m_id;
  bool m_busy;
  uint8_t m_masqueAig;
  uint8_t m_masqueAigSP2;
  uint8_t m_masqueAigSM2;
  uint16_t m_maxSpeed;
  uint8_t m_sensMarche;

public:
  Node();
  NodePeriph *nodeP[nodePsize];
  NodePeriph *SP1;
  NodePeriph *SM1;
  NodePeriph *SP2;
  NodePeriph *SM2;
  Loco loco;
  Reserved reserved;
  Aig *aig[aigSize];
  Signal *signal[signalSize];
  Sensor sensor[sensorSize];
  void ID(uint16_t);
  uint16_t ID();
  void busy(bool);
  bool busy();
  void masqueAig(uint8_t);
  uint8_t masqueAig();
  void masqueAigSP2(uint8_t);
  uint8_t masqueAigSP2();
  void masqueAigSM2(uint8_t);
  uint8_t masqueAigSM2();
  static void aigGoTo(void *);
  void aigRun(uint8_t);
  void maxSpeed(uint16_t);
  uint16_t maxSpeed() const;
  bool setAccessFrom(uint16_t);
};
