/*

   Node.h


*/

#ifndef __NODE_H__
#define __NODE_H__

#include <Arduino.h>

#include "Aig.h"
#include "Config.h"
#include "Loco.h"
//#include "RFID.h"
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
  bool m_acces;
  uint16_t m_locoAddr;
  byte m_masqueAig;

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
  void acces(bool);
  bool acces();
  void locoAddr(uint16_t);
  uint16_t locoAddr();
  void masqueAig(byte);
  byte masqueAig();
};

class Node : public Aig
{
  friend class Discovery;

private:
  uint8_t m_id;
  bool m_busy;
  byte m_masqueAig;
  uint8_t m_SP1_idx;
  uint8_t m_SM1_idx;
  bool m_SP2_acces;
  bool m_SP2_busy;
  byte m_masqueAigSP2;
  byte m_masqueAigSM2;
  //uint16_t m_SP1_loco;
  //uint16_t m_SM1_loco;

public:
  Node();                        
  //Node(const Node &);            
  ~Node();                       
  //Node &operator=(const Node &);
  //static void testMemory(void *);
  NodePeriph *nodeP[nodePsize];
  Aig *aig[aigSize];
  Loco loco;
  Sensor sensor[sensorSize];
  Signal *signal[aigSize];
  // Rfid *rfid;
  void ID(uint8_t);
  uint8_t ID();
  void busy(bool);
  bool busy();
  void masqueAig(byte);
  byte masqueAig();
  void masqueAigSP2(byte);
  byte masqueAigSP2();
  void masqueAigSM2(byte);
  byte masqueAigSM2();
  //void setup();
  static void aigGoTo(void *);
  void ciblesSignaux();
  void aigRun(byte);
  void SP1_idx(uint8_t);
  uint8_t SP1_idx();
  void SM1_idx(uint8_t);
  uint8_t SM1_idx();
  void SP2_acces(bool);
  bool SP2_acces();
  void SP2_busy(bool);
  bool SP2_busy();
};

#endif
