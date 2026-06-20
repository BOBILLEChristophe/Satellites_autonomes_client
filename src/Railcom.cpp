/*

   Railcom.cpp


  https://forum.arduino.cc/t/esp32-xtaskcreatepinnedtocore-usage-in-a-class/630469

  https://community.platformio.org/t/freertos-task-from-within-a-class-is-only-partially-working/22152

  https://forums.freertos.org/t/freertos-task-in-a-c-class/13984

*/

#include "Railcom.h"

// Identifiants des données du canal 1
const byte CH1_ADR_LOW = 1 << 2;
const byte CH1_ADR_HIGH = 1 << 3;

const byte QUEUE_1_SIZE = 10;
const byte QUEUE_2_SIZE = 3;

RingBuf<uint16_t, NB_ADDRESS_TO_COMPARE> buffer; // Instance

/* ----- Constructeur   -------------------*/

Railcom::Railcom(const gpio_num_t rxPin, const gpio_num_t txPin) : m_rxPin(rxPin),
                                                                   m_txPin(txPin),
                                                                   m_address(0)
{
  // Queue
  xQueue1 = xQueueCreate(QUEUE_1_SIZE, sizeof(uint8_t));
  xQueue2 = xQueueCreate(QUEUE_2_SIZE, sizeof(uint16_t));
  mySerial = &Serial1;
  mySerial->begin(250000, SERIAL_8N1, m_rxPin, m_txPin); // Define and start ESP32 HardwareSerial port

  const uint16_t x = 0;
  for (uint8_t i = 0; i < NB_ADDRESS_TO_COMPARE; i++) // On place des zéros dans le buffer de comparaison
    buffer.push(x);
}

void Railcom::begin()
{
  xTaskCreatePinnedToCore(this->receiveData, "ReceiveData", 4 * 1024, this, 5, NULL, 1); // Création de la tâches pour la réception
  xTaskCreatePinnedToCore(this->parseData, "ParseData", 4 * 1024, this, 7, NULL, 1);     // Création de la tâches pour le traitement
  xTaskCreatePinnedToCore(this->setAddress, "SetAddress", 2 * 1024, this, 9, NULL, 1);   // Création de la tâches pour MAJ adresse
}

/* ----- getAddress   -------------------*/

uint16_t Railcom::address() const
{
  return m_address;
}

/* ----- receiveData   -------------------*/

void IRAM_ATTR Railcom::receiveData(void *p)
{
  TickType_t xLastWakeTime;
  xLastWakeTime = xTaskGetTickCount();

  uint8_t inByte(0);
  uint8_t count(0);
  Railcom *pThis = (Railcom *)p;

  for (;;)
  {
    while (pThis->mySerial->available() > 0)
    {
      if (count == 0)
        inByte = '\0';
      else
        inByte = (uint8_t)pThis->mySerial->read();
      if (count < 3)
        xQueueSend(pThis->xQueue1, &inByte, 0);
      count++;
    }
    count = 0;
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(25)); // toutes les x ms
  }
}

/* ----- parseData   -------------------*/

void IRAM_ATTR Railcom::parseData(void *p)
{
  bool start(false);
  int16_t temp(0);
  byte inByte(0);
  uint8_t rxArray[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  uint8_t rxArrayCnt(0);
  byte dccAddr[2] = {0, 0};
  Railcom *pThis = (Railcom *)p;

  TickType_t xLastWakeTime;
  xLastWakeTime = xTaskGetTickCount();

  const byte decodeArray[] = {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 64, 255, 255, 255, 255, 255, 255, 255, 51, 255, 255, 255, 52,
                              255, 53, 54, 255, 255, 255, 255, 255, 255, 255, 255, 58, 255, 255, 255, 59, 255, 60, 55, 255, 255, 255, 255, 63, 255, 61, 56, 255, 255, 62,
                              57, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 36, 255, 255, 255, 35, 255, 34, 33, 255, 255, 255, 255, 31, 255, 30, 32, 255,
                              255, 29, 28, 255, 27, 255, 255, 255, 255, 255, 255, 25, 255, 24, 26, 255, 255, 23, 22, 255, 21, 255, 255, 255, 255, 37, 20, 255, 19, 255, 255,
                              255, 50, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 14, 255, 13, 12, 255, 255, 255, 255, 10, 255,
                              9, 11, 255, 255, 8, 7, 255, 6, 255, 255, 255, 255, 255, 255, 4, 255, 3, 5, 255, 255, 2, 1, 255, 0, 255, 255, 255, 255, 15, 16, 255, 17, 255, 255, 255,
                              18, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 43, 48, 255, 255, 42, 47, 255, 49, 255, 255, 255, 255, 41, 46, 255, 45, 255, 255,
                              255, 44, 255, 255, 255, 255, 255, 255, 255, 255, 66, 40, 255, 39, 255, 255, 255, 38, 255, 255, 255, 255, 255, 255, 255, 65, 255, 255, 255, 255,
                              255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};

  auto check_4_8_code = [&]() -> bool
  {
    if (decodeArray[inByte] < 255)
    {
      inByte = decodeArray[inByte];
      return true;
    }
    return false;
  };

  for (;;)
  {
    do
    {
      xQueueReceive(pThis->xQueue1, &inByte, pdMS_TO_TICKS(portMAX_DELAY));
      if (inByte == '\0')
        start = true;
    } while (!start);
    start = false;

    for (byte i = 0; i < 2; i++)
    {
      if (xQueueReceive(pThis->xQueue1, &inByte, pdMS_TO_TICKS(portMAX_DELAY)) == pdPASS)
      {
        if (inByte >= 0x0F && inByte <= 0xF0)
        {
          if (check_4_8_code())
          {
            rxArray[rxArrayCnt] = inByte;
            rxArrayCnt++;
          }
        }
      }
    }

    if (rxArrayCnt == 2)
    {
      if (rxArray[0] & CH1_ADR_HIGH)
        dccAddr[0] = rxArray[1] | (rxArray[0] << 6);
      if (rxArray[0] & CH1_ADR_LOW)
        dccAddr[1] = rxArray[1] | (rxArray[0] << 6);
      temp = (dccAddr[1] - 128) << 8;
      if (temp < 0)
        temp = dccAddr[0];
      else
        temp += dccAddr[0];

      bool testOk = true;
      uint16_t j = 0;
      buffer.pop(j);
      buffer.push(temp);
      do
      {
        if (buffer[j] != temp)
          testOk = false;
        j++;
      } while (testOk && j <= buffer.size());

      if (testOk)
      {
        xQueueSend(pThis->xQueue2, &temp, 0);
      }
    }
    rxArrayCnt = 0;
    for (byte i = 0; i < 2; i++)
      rxArray[i] = 0;
  }
  vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(25)); // toutes les x ms
}


/* ----- setAddress   -------------------*/

void IRAM_ATTR Railcom::setAddress(void *p)
{
  TickType_t xLastWakeTime;
  xLastWakeTime = xTaskGetTickCount();

  uint16_t address(0);
  Railcom *pThis = (Railcom *)p;

  for (;;)
  {
    address = 1;
    xQueueReceive(pThis->xQueue2, &address, pdMS_TO_TICKS(0));
    pThis->m_address = address;
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(25)); // toutes les x ms
  }
}