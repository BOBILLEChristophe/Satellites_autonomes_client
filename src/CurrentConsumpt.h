/*

  CurrentConsumpt.h



*/

#ifndef __CONSO_COURANT__
#define __CONSO_COURANT__

#include <Arduino.h>
#include "Config.h"
#include "Debug.h"

class CurrentConsumpt
{
private:
  gpio_num_t m_pinIn;

public:
  CurrentConsumpt();
  ~CurrentConsumpt();
  Node *m_node;
  void setup(Node *, const gpio_num_t);
  static void IRAM_ATTR loop(void *pvParameters);
};

CurrentConsumpt::CurrentConsumpt() {};
CurrentConsumpt::~CurrentConsumpt() {};

void CurrentConsumpt::setup(Node *node, const gpio_num_t pinIn)
{
  m_node = node;
  m_pinIn = pinIn;
  pinMode(m_pinIn, INPUT_PULLUP);
  xTaskCreatePinnedToCore(this->loop, "loop", 2 * 1024, this, 10, NULL, 1);
}

void IRAM_ATTR CurrentConsumpt::loop(void *p)
{
  TickType_t xLastWakeTime;
  xLastWakeTime = xTaskGetTickCount();

  CurrentConsumpt *pThis = (CurrentConsumpt *)p;

  bool lastRead = false;
  uint8_t sameCount = 0;
  constexpr uint8_t FILTER_COUNT = 3;

  for (;;)
  {
    bool newRead = !digitalRead(pThis->m_pinIn); // actif à 0 => busy=true

    if (newRead == lastRead)
    {
      if (sameCount < FILTER_COUNT)
        sameCount++;
    }
    else
    {
      lastRead = newRead;
      sameCount = 1;
    }

    // Validation uniquement après x lectures identiques
    if (sameCount >= FILTER_COUNT)
    {
      if (pThis->m_node->busy() != newRead)
      {
        pThis->m_node->busy(newRead);
        //LOG_INFO("Busy = %s", newRead ? "true" : "false");
      }
    }
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(200));
  }
}

// void IRAM_ATTR CurrentConsumpt::loop(void *p)
// {
//   TickType_t xLastWakeTime;
//   xLastWakeTime = xTaskGetTickCount();
//   CurrentConsumpt *pThis = (CurrentConsumpt *)p;

//   for (;;)
//   {
//     if (digitalRead(pThis->m_pinIn))
//       pThis->m_node->busy(false);
//     else
//       pThis->m_node->busy(true);
// //#ifdef debug
//     static bool oldBusy = false;
//     if (oldBusy != pThis->m_node->busy())
//     {
//       //LOG_INFO("Busy = %s", pThis->m_node->busy() ? "true" : "false");
//       oldBusy = pThis->m_node->busy();
//     }
// //#endif
//     vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(200)); // toutes les x ms
//   }
// }
#endif
