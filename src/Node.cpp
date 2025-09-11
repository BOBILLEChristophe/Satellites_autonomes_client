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
      m_reserved(0),
      m_acces(true),
      m_locoAddr(0),
      m_masqueAig(0x00),
      m_signal(0)
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
void NodePeriph::reserved(uint16_t locoAddr) { m_reserved = locoAddr; };
uint16_t NodePeriph::reserved() { return m_reserved; };
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
      m_reserved(0),
      m_masqueAig(0x00),
      m_SP1_idx(0),
      m_SM1_idx(0),
      m_SP2_acces(true),
      m_SP2_busy(false),
      m_SM2_acces(true),
      m_SM2_busy(false),
      m_masqueAigSP2(0x00),
      m_masqueAigSM2(0x00),
      m_maxSpeed(128),
      m_sensMarche(0)
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
}

// Node::~Node() {} // Destructeur

void Node::ID(uint16_t id) { m_id = id; }
uint16_t Node::ID() { return m_id; }
void Node::busy(bool busy) { m_busy = busy; }
bool Node::busy() { return m_busy; }
void Node::reserved(uint16_t add_loco) { m_reserved = add_loco; };
uint16_t Node::reserved() { return m_reserved; };
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
void Node::maxSpeed(uint8_t maxSpeed) { m_maxSpeed = maxSpeed; }
uint8_t Node::maxSpeed() { return m_maxSpeed; }
void Node::sensMarche(uint8_t sensMarche) { m_sensMarche = sensMarche; }
uint8_t Node::sensMarche() { return m_sensMarche; }

void Node::aigRun(byte idx)
{
  if (!(this->aig[idx]->isRunning()))
  {
    if (this->aig[idx]->posDroit() != this->aig[idx]->posDevie())
    {
      Node *pThis = (Node *)this->aig[idx];
      TaskHandle_t aigGoToHandle = NULL;
      xTaskCreate(this->aigGoTo, "goTo", 2 * 1024, pThis, 4, &aigGoToHandle); // Création de la tâche
    }
  }
#ifdef DEBUG
  else
    debug.println("Manoeuvre en cours !");
#endif
}

void Node::aigGoTo(void *p)
{
  Node *pThis = (Node *)p;

  if ((pThis->m_posDroit < pThis->m_posDevie) && (pThis->m_curPos == pThis->m_posDevie))
    pThis->m_sens = 0;
  if ((pThis->m_posDroit < pThis->m_posDevie) && (pThis->m_curPos == pThis->m_posDroit))
    pThis->m_sens = 1;
  if ((pThis->m_posDevie < pThis->m_posDroit) && (pThis->m_curPos == pThis->m_posDevie))
    pThis->m_sens = 1;
  if ((pThis->m_posDevie < pThis->m_posDroit) && (pThis->m_curPos == pThis->m_posDroit))
    pThis->m_sens = 0;

  if (pThis->m_speed < 1000 && pThis->m_speed > 10000)
    pThis->m_speed = 5000;

  pThis->run(true);

  for (;;)
  {
    if (pThis->isRunning())
    {
      if (pThis->m_sens == 0)
        pThis->m_curPos--;
      else
        pThis->m_curPos++;
      pThis->writeMicroseconds(pThis->m_curPos);
      if (pThis->m_curPos == pThis->m_posDevie || pThis->m_curPos == pThis->m_posDroit)
        pThis->run(false);
      if (pThis->m_curPos == pThis->m_posDroit)
        pThis->m_estDroit = true;
      else if (pThis->m_curPos == pThis->m_posDevie)
        pThis->m_estDroit = false;
      delayMicroseconds(pThis->m_speed);
    }
    else
    {
#ifdef DEBUG
      debug.printf("m_curPos : %d\n", pThis->m_curPos);
      debug.printf("m_speed : %d\n", pThis->m_speed);
      debug.println("vTaskDelete");
#endif
      vTaskDelete(NULL);
    }
  }
}