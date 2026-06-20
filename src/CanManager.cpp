/*

  CanManager.cpp

*/

#include "CanManager.h"

void CanManager::setup()
{
    LOG_INFO("[CanManager %d] Configure ESP32 CAN", __LINE__);

    ACAN_ESP32_Settings settings(CAN_BITRATE);
    settings.mRxPin = CAN_RX;
    settings.mTxPin = CAN_TX;

    const uint32_t errorCode = ACAN_ESP32::can.begin(settings);

    LOG_INFO("[CanManager %d] Configuration CAN sans filtre", __LINE__);

    if (errorCode == 0)
    {
        LOG_INFO("[CanManager %d] Configuration CAN OK", __LINE__);
    }
    else
    {
        LOG_ERROR("[CanManager %d] Configuration CAN erreur 0x%08X", __LINE__, errorCode);
        vTaskDelay(pdMS_TO_TICKS(1000));
        return;
    }
}