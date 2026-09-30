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

NodePeriph::NodePeriph()
    : m_id(0),
      m_busy(true),
      m_reserved(false),
      m_reservedBySat(0),
      m_reservedByLoco(0),
      m_acces(false),
      m_locoAddr(0),
      m_masqueAig(0)
{
    ++comptInst;
}

NodePeriph::~NodePeriph() // Destructeur
{
  --comptInst;
}

void NodePeriph::ID(uint16_t id) { m_id = id; }
uint16_t NodePeriph::ID() { return m_id; }
void NodePeriph::busy(bool busy) { m_busy = busy; }
bool NodePeriph::busy() { return m_busy; }
void NodePeriph::reserved(bool reserved)
{
    m_reserved = reserved;
    if (!reserved)
    {
        m_reservedBySat = 0;
        m_reservedByLoco = 0;
    }
}
bool NodePeriph::reserved() { return m_reserved; }
void NodePeriph::acces(bool acces) { m_acces = acces; }
bool NodePeriph::acces() { return m_acces; }
void NodePeriph::locoAddr(uint16_t addr) { m_locoAddr = addr; }
uint16_t NodePeriph::locoAddr() { return m_locoAddr; }
void NodePeriph::masqueAig(uint8_t masqueAig) { m_masqueAig = masqueAig; }
uint8_t NodePeriph::masqueAig() { return m_masqueAig; }

void NodePeriph::reservedBySat(uint16_t sat)
{
    m_reservedBySat = sat;
}

uint16_t NodePeriph::reservedBySat() const
{
    return m_reservedBySat;
}

void NodePeriph::reservedByLoco(uint16_t loco)
{
    m_reservedByLoco = loco;
}

uint16_t NodePeriph::reservedByLoco() const
{
    return m_reservedByLoco;
}

bool NodePeriph::reservedFor(uint16_t satId, uint16_t locoAddr) const
{
    return m_reserved &&
           m_reservedBySat == satId &&
           m_reservedByLoco == locoAddr;
}

/*-------------------------------------------------------------
                           Node
--------------------------------------------------------------*/

// Constructor
Node::Node()
    : m_id(0),
      m_busy(false),
      m_masqueAig(0x00),
      SP1(nullptr),
      SM1(nullptr),
      SP2(nullptr),
      SM2(nullptr),
      m_masqueAigSP2(0x00),
      m_masqueAigSM2(0x00),
      m_maxSpeed(1000),
      m_sensMarche(0)
{

  SP2 = new NodePeriph;
  SM2 = new NodePeriph;
  SP2->ID(0);
  SM2->ID(0);
  SP2->busy(true);
  SM2->busy(true);
  SP2->acces(false);
  SM2->acces(false);

  for (uint8_t i = 0; i < nodePsize; i++)
    this->nodeP[i] = nullptr;
  for (uint8_t i = 0; i < aigSize; i++)
    this->aig[i] = nullptr;
  for (uint8_t i = 0; i < signalSize; i++)
    this->signal[i] = nullptr;

  sensor[SENSOR_ANTIHOR].setup(CAPT_PONCT_ANTIHOR_PIN, CAPT_PONCT_TEMPO, INPUT_PULLUP);
  sensor[SENSOR_HORAIRE].setup(CAPT_PONCT_HORAIRE_PIN, CAPT_PONCT_TEMPO, INPUT_PULLUP);
}

// Node::~Node() {} // Destructeur

void Node::ID(uint16_t id) { m_id = id; }
uint16_t Node::ID() { return m_id; }
void Node::busy(bool busy) { m_busy = busy; }
bool Node::busy() { return m_busy; }
void Node::masqueAig(uint8_t masqueAig) { m_masqueAig = masqueAig; }
uint8_t Node::masqueAig() { return m_masqueAig; }
void Node::masqueAigSP2(uint8_t masqueAigSP2) { m_masqueAigSP2 = masqueAigSP2; }
uint8_t Node::masqueAigSP2() { return m_masqueAigSP2; }
void Node::masqueAigSM2(uint8_t masqueAigSM2) { m_masqueAigSM2 = masqueAigSM2; }
uint8_t Node::masqueAigSM2() { return m_masqueAigSM2; }
void Node::maxSpeed(uint16_t maxSpeed) { m_maxSpeed = maxSpeed; }
uint16_t Node::maxSpeed() const { return m_maxSpeed; }

void Node::aigRun(uint8_t idx)
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

bool Node::setAccessFrom(uint16_t satId)
{
    // Recherche du satellite parmi les voisins physiques
    int16_t targetIdx = -1;

    for (uint8_t i = 0; i < nodePsize; i++)
    {
        if (nodeP[i] != nullptr && nodeP[i]->ID() == satId)
        {
            targetIdx = i;
            break;
        }
    }

    if (targetIdx < 0)
    {
        LOG_WARN("setAccessFrom : satellite %u non voisin", satId);
        return false;
    }

    // Côté horaire : aiguilles 0,1,2
    // Côté anti-horaire : aiguilles 3,4,5
    const uint8_t idxA =
        (targetIdx < 4) ? 0 : 3;

    const uint8_t local =
        targetIdx - ((targetIdx < 4) ? 0 : 4);

    /*
     * Vérifier d'abord que la topologie permet réellement
     * d'atteindre la destination.
     *
     * local 0 : p00/m00 -> aucune aiguille obligatoire
     * local 1 : p01/m01 -> aiguille 0/3 obligatoire
     * local 2 : p10/m10 -> aiguille 1/4 obligatoire
     * local 3 : p11/m11 -> aiguilles 0/3 ET 2/5 obligatoires
     */

    switch (local)
    {
    case 0:
        break;

    case 1:
        if (aig[idxA] == nullptr)
            return false;
        break;

    case 2:
        if (aig[idxA + 1] == nullptr)
            return false;
        break;

    case 3:
        if (aig[idxA] == nullptr ||
            aig[idxA + 2] == nullptr)
            return false;
        break;

    default:
        return false;
    }

    /*
     * Maintenant seulement, positionnement des aiguilles.
     */

    switch (local)
    {
    // p00 / m00
    case 0:

        // aig 0/3 droite si elle existe
        if (aig[idxA] != nullptr &&
            !aig[idxA]->estDroit())
        {
            aigRun(idxA);
        }

        // aig 1/4 droite si elle existe
        if (aig[idxA + 1] != nullptr &&
            !aig[idxA + 1]->estDroit())
        {
            aigRun(idxA + 1);
        }

        break;

    // p01 / m01
    case 1:

        // aig 0/3 déviée
        if (aig[idxA]->estDroit())
        {
            aigRun(idxA);
        }

        // aig 2/5 droite si elle existe
        if (aig[idxA + 2] != nullptr &&
            !aig[idxA + 2]->estDroit())
        {
            aigRun(idxA + 2);
        }

        break;

    // p10 / m10
    case 2:

        // aig 0/3 droite si elle existe
        if (aig[idxA] != nullptr &&
            !aig[idxA]->estDroit())
        {
            aigRun(idxA);
        }

        // aig 1/4 déviée
        if (aig[idxA + 1]->estDroit())
        {
            aigRun(idxA + 1);
        }

        break;

    // p11 / m11
    case 3:

        // aig 0/3 déviée
        if (aig[idxA]->estDroit())
        {
            aigRun(idxA);
        }

        // aig 2/5 déviée
        if (aig[idxA + 2]->estDroit())
        {
            aigRun(idxA + 2);
        }

        break;
    }

    LOG_INFO("setAccessFrom : route demandee vers satellite %u",
             satId);

    return true;
}

Reserved::Reserved()
    : m_reserved(false),
      m_reservedAt(0),
      m_reservedBySat(0),
      m_reservedByLoco(0)
{
}

bool Reserved::reservation(const uint16_t sat, const uint16_t loco)
{
  if (m_reserved) // Le canton est déjà reservé
  {
    if (m_reservedBySat == sat && m_reservedByLoco == loco)
    {
      // Il s'agit du même satellite et de la même locomotive
      m_reservedAt = millis(); // MAJ de l'heure de réservation
      return true;
    }
    return false;
  }
  else // Le canton n'est pas déjà réservé, la demande est acceptée
  {
    m_reserved = true;
    m_reservedBySat = sat;
    m_reservedByLoco = loco;
    m_reservedAt = millis(); // MAJ de l'heure de réservation
    return true;
  }
}

bool Reserved::reserved() const { return m_reserved; }
uint16_t Reserved::reservedBySat() const { return m_reservedBySat; }
uint16_t Reserved::reservedByLoco() const { return m_reservedByLoco; }

void Reserved::refresh()
{
    uint32_t nowMs = millis();

    if (m_reserved &&
        (uint32_t)(nowMs - m_reservedAt) > refreshDelayMs)
    {
        m_reserved = false;
        m_reservedBySat = 0;
        m_reservedByLoco = 0;
    }
}
