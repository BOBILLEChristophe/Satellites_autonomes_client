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
  uint16_t m_reserved;
  bool m_acces;
  uint16_t m_locoAddr;
  byte m_masqueAig;
  byte m_signal; // ???
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
  void reserved(uint16_t);
  uint16_t reserved();
  void acces(bool);
  bool acces();
  void locoAddr(uint16_t);
  uint16_t locoAddr();
  void masqueAig(byte);
  byte masqueAig();
  // byte signal();     // ???
  // void signal(byte); // ???
};

class Node : public Aig
{
  friend class Discovery;

private:
  uint16_t m_id;
  bool m_busy;
  uint16_t m_reserved;
  byte m_masqueAig;
  uint8_t m_SP1_idx;
  uint8_t m_SM1_idx;
  bool m_SP2_acces;
  bool m_SP2_busy;
  bool m_SM2_acces;
  bool m_SM2_busy;
  byte m_masqueAigSP2;
  byte m_masqueAigSM2;
  uint8_t m_maxSpeed;
  uint8_t m_sensMarche;

public:
  Node();
  NodePeriph *nodeP[nodePsize];
  Aig *aig[aigSize];
  Loco loco;
  Sensor sensor[sensorSize];
  Signal *signal[signalSize];
  void ID(uint16_t);
  uint16_t ID();
  void busy(bool);
  bool busy();
  void reserved(uint16_t);
  uint16_t reserved();
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
  void maxSpeed(uint8_t);
  uint8_t maxSpeed();
  void sensMarche(uint8_t);
  uint8_t sensMarche();
};
