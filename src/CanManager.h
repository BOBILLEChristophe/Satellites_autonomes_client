/*


  CanManager.h


*/

#pragma once

#include <Arduino.h>
#include <ACAN_ESP32.h>
#include "Config.h"
#include "Debug.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Forward declaration pour éviter d'inclure Node.h ici
class Node;

// -----------------------------------------------------------------------------
// Dépendances attendues (définies dans un Config.h)
// -----------------------------------------------------------------------------
// constexpr gpio_num_t CAN_RX = GPIO_NUM_22;
// constexpr gpio_num_t CAN_TX = GPIO_NUM_23;
// constexpr uint32_t CAN_BITRATE = 1000UL * 1000UL; // 1 Mb/s

// -----------------------------------------------------------------------------
// Classe CanManager : configuration du contrôleur CAN via ACAN_ESP32
// -----------------------------------------------------------------------------

class CanManager
{
public:
  CanManager() = delete;
  static void setup();

private:
};
