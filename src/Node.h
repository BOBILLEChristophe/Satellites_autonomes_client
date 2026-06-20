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

// struct Liaison
// {
//   byte aig;
//   bool pos;
// };

class NodePeriph
{
protected:
  uint8_t m_id;
  bool m_busy;
  uint16_t m_reservedBy;
  bool m_acces;
  uint16_t m_locoAddr;
  byte m_masqueAig;
  // byte m_signal; // ???
  byte m_typeCible;

public:
  NodePeriph();  // Constructeur sans argument
  ~NodePeriph(); // Destructeur
  // NodePeriph(const NodePeriph &);            // Constructeur de recopie
  // NodePeriph &operator=(const NodePeriph &); // Operator d'affectation
  static uint8_t comptInst;
  void ID(uint8_t);
  uint8_t ID();
  // Liaison *liaison[2];
  void busy(bool);
  bool busy();
  void reservedBy(uint16_t);
  uint16_t reservedBy();
  void acces(bool);
  bool acces();
  void locoAddr(uint16_t);
  uint16_t locoAddr();
  void masqueAig(byte);
  byte masqueAig();
  // byte signal();     // ???
  // void signal(byte); // ???
};

class Node
{
  friend class Discovery;

private:
  uint16_t m_id;
  bool m_busy;
  uint16_t m_reservedBy;
  byte m_masqueAig;
  uint8_t m_SP1_idx;
  uint8_t m_SM1_idx;
  bool m_SP2_acces;
  bool m_SP2_busy;
  bool m_SM2_acces;
  bool m_SM2_busy;
  byte m_masqueAigSP2;
  byte m_masqueAigSM2;
  uint16_t m_maxSpeed;
  uint8_t m_sensMarche;
uint8_t m_trafficState = 0;
  uint8_t m_reservedSens = 0;
  uint32_t m_reservedAt = 0;
  ///////////////////
  //uint8_t m_trafficState = (uint8_t)TrafficState::FREE;
  ///////////////////
public:
  // ACK de la centrale aux commandes de locomotives
  bool m_pendingLocoCmd;
  uint16_t m_pendingLocoAddr;
  uint16_t m_pendingLocoSpeed;
  bool m_ackLocoCmd;
  uint32_t m_pendingLocoSentAt;
  uint8_t m_pendingLocoRetry;

  Node();
  NodePeriph *nodeP[nodePsize];
  Aig *aig[aigSize];
  Loco loco;
  Loco reservedLoco;
  Sensor sensor[sensorSize];
  Signal *signal[signalSize];
  void ID(uint16_t);
  uint16_t ID();
  void busy(bool);
  bool busy();
  void reservedBy(uint16_t);
  uint16_t reservedBy();
  void clearReservation();
  void masqueAig(byte);
  byte masqueAig();
  void masqueAigSP2(byte);
  byte masqueAigSP2();
  void masqueAigSM2(byte);
  byte masqueAigSM2();
  static void aigGoTo(void *);
  void aigRun(byte);
  void SP1_idx(uint8_t);
  uint8_t SP1_idx();
  void SM1_idx(uint8_t);
  uint8_t SM1_idx();
  void SP2_acces(bool);
  bool SP2_acces();
  void SP2_busy(bool);
  bool SP2_busy();
  void SM2_acces(bool);
  bool SM2_acces();
  void SM2_busy(bool);
  bool SM2_busy();
  void maxSpeed(uint16_t);
  uint16_t maxSpeed() const;
  void sensMarche(uint8_t);
  uint8_t sensMarche();
  // void trafficState(uint8_t state);
  // uint8_t trafficState() const;
  void reservedSens(uint8_t sens);
  uint8_t reservedSens() const;
  void reservedAt(uint32_t t);
  uint32_t reservedAt() const;
  ///////////////////////////////
  void trafficState(uint8_t state);
  uint8_t trafficState() const;
  ///////////////////////////////
};
