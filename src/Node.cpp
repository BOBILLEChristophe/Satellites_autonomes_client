/*

   Node.cpp


*/

#include "Node.h"

//    Node p00;     // Le satellite qui est dans le sens horaire (sans aiguille ou aiguilles 0 et 1 droites)
//    Node p01;     // Le satellite qui est dans le sens horaire (aiguille 0 déviée - si il y en a une, aiguille 2 droite)
//    Node p10;     // Le satellite qui est dans le sens horaire (aiguille 0 droite - aiguille 1 déviée)
//    Node p11;     // Le satellite qui est dans le sens horaire (aiguille 0 déviée - aiguille 2 déviée)
//    Node m00;     // Le satellite qui est dans le sens antihoraire (sans aiguille ou aiguilles 3 et 4 droites)
//    Node m01;     // Le satellite qui est dans le sens antihoraire (aiguille 3 déviée et, si il y en a une, aiguille 5 droite)
//    Node m10;     // Le satellite qui est dans le sens antihoraire (aiguille 3 droite - aiguille 4 déviée)
//    Node m11;     // Le satellite qui est dans le sens antihoraire (aiguille 3 déviée - aiguille 5 déviée)

/*-------------------------------------------------------------
                           NodePeriph
--------------------------------------------------------------*/

uint8_t NodePeriph::comptInst = 0;

NodePeriph::NodePeriph() // Constructeur
    : m_id(UNUSED_ID),
      m_busy(false),
      m_reservedBy(0),
      m_acces(true),
      m_locoAddr(1), // DCC addresse 0 broadcast
      m_masqueAig(0x00)
// m_signal(0)
{
  ++comptInst;
}

NodePeriph::~NodePeriph() // Destructeur
{
  --comptInst;
}

void NodePeriph::ID(uint8_t id) { m_id = id; }
uint8_t NodePeriph::ID() { return m_id; }
void NodePeriph::busy(bool busy) { m_busy = busy; }
bool NodePeriph::busy() { return m_busy; }
void NodePeriph::reservedBy(uint16_t locoAddr) { m_reservedBy = locoAddr; };
uint16_t NodePeriph::reservedBy() { return m_reservedBy; };
void NodePeriph::acces(bool acces) { m_acces = acces; }
bool NodePeriph::acces() { return m_acces; }
void NodePeriph::locoAddr(uint16_t addr) { m_locoAddr = addr; }
uint16_t NodePeriph::locoAddr() { return m_locoAddr; }
void NodePeriph::masqueAig(byte masqueAig) { m_masqueAig = masqueAig; }
byte NodePeriph::masqueAig() { return m_masqueAig; }

/*-------------------------------------------------------------
                           Node
--------------------------------------------------------------*/

// Constructor
Node::Node()
    : m_id(UNUSED_ID),
      m_busy(false),
      m_reservedBy(0),
      m_masqueAig(0x00),
      m_SP1_idx(0),
      m_SM1_idx(0),
      m_SP2_acces(true),
      m_SP2_busy(false),
      m_SM2_acces(true),
      m_SM2_busy(false),
      m_masqueAigSP2(0x00),
      m_masqueAigSM2(0x00),
      m_maxSpeed(1000),
      m_sensMarche(0),
      m_pendingLocoCmd(false),
      m_pendingLocoAddr(0),
      m_pendingLocoSpeed(0),
      m_ackLocoCmd(false),
      m_pendingLocoSentAt(0),
      m_pendingLocoRetry(0),
      m_trafficState(0),
      m_reservedSens(0),
      m_reservedAt(0)
{
  for (byte i = 0; i < nodePsize; i++)
    this->nodeP[i] = nullptr;
  for (byte i = 0; i < aigSize; i++)
    this->aig[i] = nullptr;
  for (byte i = 0; i < signalSize; i++)
    this->signal[i] = nullptr;

  sensor[0].setup(CAPT_PONCT_ANTIHOR_PIN, CAPT_PONCT_TEMPO, INPUT_PULLUP);
  sensor[1].setup(CAPT_PONCT_HORAIRE_PIN, CAPT_PONCT_TEMPO, INPUT_PULLUP);
  // sensor[2].setup(DETECT_PRES_CONSO_COURANT_PIN, 50, INPUT_PULLUP);
  Loco loco;
  Loco reservedLoco;
}

// Node::~Node() {} // Destructeur

void Node::ID(uint16_t id) { m_id = id; }
uint16_t Node::ID() { return m_id; }
void Node::busy(bool busy) { m_busy = busy; }
bool Node::busy() { return m_busy; }
void Node::reservedBy(uint16_t addLoco) { m_reservedBy = addLoco; }
uint16_t Node::reservedBy() { return m_reservedBy; }
void Node::clearReservation() { m_reservedBy = 1; }
void Node::masqueAig(byte masqueAig) { m_masqueAig = masqueAig; }
byte Node::masqueAig() { return m_masqueAig; }
void Node::masqueAigSP2(byte masqueAigSP2) { m_masqueAigSP2 = masqueAigSP2; }
byte Node::masqueAigSP2() { return m_masqueAigSP2; }
void Node::masqueAigSM2(byte masqueAigSM2) { m_masqueAigSM2 = masqueAigSM2; }
byte Node::masqueAigSM2() { return m_masqueAigSM2; }
void Node::SP1_idx(uint8_t idx) { m_SP1_idx = idx; }
uint8_t Node::SP1_idx() { return m_SP1_idx; }
void Node::SM1_idx(uint8_t idx) { m_SM1_idx = idx; }
uint8_t Node::SM1_idx() { return m_SM1_idx; }
void Node::SP2_acces(bool acces) { m_SP2_acces = acces; }
bool Node::SP2_acces() { return m_SP2_acces; }
void Node::SP2_busy(bool busy) { m_SP2_busy = busy; }
bool Node::SP2_busy() { return m_SP2_busy; }
void Node::SM2_acces(bool acces) { m_SM2_acces = acces; }
bool Node::SM2_acces() { return m_SM2_acces; }
void Node::SM2_busy(bool busy) { m_SM2_busy = busy; }
bool Node::SM2_busy() { return m_SM2_busy; }
void Node::maxSpeed(uint16_t maxSpeed) { m_maxSpeed = maxSpeed; }
uint16_t Node::maxSpeed() const { return m_maxSpeed; }
void Node::sensMarche(uint8_t sensMarche) { m_sensMarche = sensMarche; }
uint8_t Node::sensMarche() { return m_sensMarche; }
void Node::trafficState(uint8_t state) { m_trafficState = state; }
uint8_t Node::trafficState() const { return m_trafficState; }
void Node::reservedSens(uint8_t sens) { m_reservedSens = sens; }
uint8_t Node::reservedSens() const { return m_reservedSens; }
void Node::reservedAt(uint32_t t) { m_reservedAt = t; }
uint32_t Node::reservedAt() const { return m_reservedAt; }

void Node::aigRun(byte idx)
{
  // Sécurité index + pointeur
  if (idx >= aigSize)
    return;
  if (this->aig[idx] == nullptr)
    return;

  // Si déjà en cours, on sort
  if (this->aig[idx]->isRunning())
  {
    LOG_INFO("Manoeuvre en cours !");
    return;
  }
  // Si posDroit == posDevie, aucun mouvement utile
  if (this->aig[idx]->posDroit() == this->aig[idx]->posDevie())
    return;

  // Lance la tâche : param = Aig*
  BaseType_t ok = xTaskCreate(
      Aig::taskGoTo,
      "AigGoTo",
      2048,
      (void *)this->aig[idx],
      1,
      NULL);
}