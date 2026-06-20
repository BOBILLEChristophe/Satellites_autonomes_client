/*

  Storage.h


*/

#pragma once
#include <Arduino.h>
#include <SPIFFS.h>
#include "Debug.h"

class Storage {
public:
  static bool begin(bool formatOnFail = false);
};

