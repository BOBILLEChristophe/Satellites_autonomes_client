/*

  ConsoCourant.h



*/

#ifndef __CONSO_COURANT__
#define __CONSO_COURANT__

#include <Arduino.h>
#include "Config.h"

class ConsoCourant
{
private:
  gpio_num_t m_pinIn;

public:
  ConsoCourant();
  ~ConsoCourant();
  Node *m_node;
  void setup(Node *, const gpio_num_t);
  static void IRAM_ATTR loop(void *pvParameters);
};

ConsoCourant::ConsoCourant() {};
ConsoCourant::~ConsoCourant() {};

void ConsoCourant::setup(Node *node, const gpio_num_t pinIn)
{
  m_node = node;
  m_pinIn = pinIn;
  pinMode(m_pinIn, INPUT_PULLUP);
  xTaskCreatePinnedToCore(this->loop, "loop", 2 * 1024, this, 10, NULL, 1);
}

void IRAM_ATTR ConsoCourant::loop(void *p)
{
  TickType_t xLastWakeTime;
  xLastWakeTime = xTaskGetTickCount();
  ConsoCourant *pThis = (ConsoCourant *)p;

  for (;;)
  {
    if (digitalRead(pThis->m_pinIn))
      pThis->m_node->busy(false);
    else
      pThis->m_node->busy(true);
#ifdef debug
    static bool oldBusy = false;
    if (oldBusy != pThis->m_node->busy())
    {
      debug.printf("[ConsoCourant %d] Busy = %s \n", __LINE__, pThis->m_node->busy() ? "true" : "false");
      oldBusy = pThis->m_node->busy();
    }
#endif
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(200)); // toutes les x ms
  }
}
#endif
