/*

  Sensor.h


*/


#ifndef __SENSOR__
#define __SENSOR__

#include <Arduino.h>
#include "Config.h"

class Sensor
{
private:
  gpio_num_t m_pin;
  uint32_t m_tempo;
  byte m_input;
  bool m_state;

public:
  Sensor();
  ~Sensor();
  void setup(gpio_num_t, uint32_t, byte);
  static void loop(void *);
  bool state();
  void state(bool);
};

#endif
