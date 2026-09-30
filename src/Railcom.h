#ifndef __RAILCOM_H__
#define __RAILCOM_H__

#include <Arduino.h>

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"


class Railcom
{
public:

    // UART_NUM_1 par défaut.
    // Uniquement une broche RX : RailCom ne transmet rien par cet UART.
    explicit Railcom(gpio_num_t rxPin,
                     uart_port_t uartNum = UART_NUM_1);

    // Initialise l'UART et démarre la tâche RailCom.
    // Retourne true si l'initialisation a réussi.
    bool begin();

    // Adresse DCC actuellement détectée.
    // 0 = aucune locomotive RailCom validée.
    uint16_t address() const;

    // true si une locomotive RailCom est actuellement validée.
    bool isPresent() const;


private:

    // Entrée statique nécessaire à FreeRTOS.
    static void taskEntry(void *parameter);

    // Tâche événementielle UART.
    void task();

    // Parseur du flux RailCom.
    void feedByte(uint8_t raw);

    // Traitement d'un datagramme RailCom à 2 symboles.
    void processAddressDatagram(uint8_t symbol0,
                                uint8_t symbol1);

    // Reconstruction de l'adresse DCC courte ou longue.
    uint16_t buildAddress() const;

    // Validation après plusieurs lectures identiques.
    void validateAddress(uint16_t address);

    // Détection de disparition RailCom.
    void checkLoss();

    // Remise à zéro de l'état du parseur.
    void resetParser();


    // -------------------------------------------------------------------------
    // Configuration matérielle
    // -------------------------------------------------------------------------

    const gpio_num_t m_rxPin;
    const uart_port_t m_uartNum;


    // -------------------------------------------------------------------------
    // FreeRTOS / UART
    // -------------------------------------------------------------------------

    QueueHandle_t m_uartQueue = nullptr;
    TaskHandle_t m_taskHandle = nullptr;


    // -------------------------------------------------------------------------
    // Adresse actuellement validée
    // -------------------------------------------------------------------------

    volatile uint16_t m_address = 0;


    // -------------------------------------------------------------------------
    // Reconstruction ADR1 / ADR2
    // -------------------------------------------------------------------------

    uint8_t m_adr1Data = 0;
    uint8_t m_adr2Data = 0;

    bool m_adr1Valid = false;
    bool m_adr2Valid = false;


    // -------------------------------------------------------------------------
    // Validation de l'adresse
    // -------------------------------------------------------------------------

    uint16_t m_candidateAddress = 0;
    uint8_t m_confirmationCount = 0;

    uint16_t m_lastValidatedAddress = 0;
    uint32_t m_lastValidatedReceptionMs = 0;


    // -------------------------------------------------------------------------
    // Parseur continu du flux UART
    // -------------------------------------------------------------------------

    bool m_waitingSecondSymbol = false;
    uint8_t m_firstDecodedSymbol = 0;
};

#endif