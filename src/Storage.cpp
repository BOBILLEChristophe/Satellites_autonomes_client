/*

  Storage.cpp


*/

#include "Storage.h"

bool Storage::begin(bool formatOnFail) 
{
    static bool ok = false;
    if (ok)
        return true;

    ok = SPIFFS.begin(formatOnFail);
    if (!ok)
        LOG_ERROR("SPIFFS mount FAILED");
    else
        LOG_INFO("SPIFFS mounted OK");
    return ok;
}
