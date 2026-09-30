#include "Railcom.h"

namespace
{

    // -----------------------------------------------------------------------------
    // Configuration RailCom / UART
    // -----------------------------------------------------------------------------

    constexpr uint32_t RAILCOM_BAUD_RATE = 250000;

    constexpr int RAILCOM_RX_BUFFER_SIZE = 256;
    constexpr int RAILCOM_EVENT_QUEUE_SIZE = 20;

    // FIFO matériel ESP32 classique : 128 octets.
    // Une rafale RailCom normale sera normalement délivrée par timeout.
    constexpr uint8_t RAILCOM_RX_FIFO_THRESHOLD = 120;

    // Timeout en périodes de symbole UART.
    // À 250 kbit/s, 8N1 : environ 40 µs par symbole.
    // 2 symboles donnent donc environ 80 µs.
    constexpr uint8_t RAILCOM_RX_TIMEOUT_SYMBOLS = 2;

    // Nombre de reconstructions identiques consécutives nécessaires.
    constexpr uint8_t ADDRESS_CONFIRMATIONS = 5;

    // Une adresse validée disparaît après 1 seconde
    // sans nouveau groupe valide de confirmations.
    constexpr uint32_t RAILCOM_LOSS_TIMEOUT_MS = 1000;

    // -----------------------------------------------------------------------------
    // Table de décodage RailCom 4-out-of-8
    //
    // Valeurs :
    //   0..63  = données RailCom
    //   64..66 = mots de contrôle
    //   255    = symbole invalide
    // -----------------------------------------------------------------------------

    constexpr uint8_t decodeArray[256] = {

        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 64,
        255, 255, 255, 255, 255, 255, 255, 51, 255, 255, 255, 52, 255, 53, 54, 255,
        255, 255, 255, 255, 255, 255, 255, 58, 255, 255, 255, 59, 255, 60, 55, 255,
        255, 255, 255, 63, 255, 61, 56, 255, 255, 62, 57, 255, 255, 255, 255, 255,

        255, 255, 255, 255, 255, 255, 255, 36, 255, 255, 255, 35, 255, 34, 33, 255,
        255, 255, 255, 31, 255, 30, 32, 255, 255, 29, 28, 255, 27, 255, 255, 255,
        255, 255, 255, 25, 255, 24, 26, 255, 255, 23, 22, 255, 21, 255, 255, 255,
        255, 37, 20, 255, 19, 255, 255, 255, 50, 255, 255, 255, 255, 255, 255, 255,

        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 14, 255, 13, 12, 255,
        255, 255, 255, 10, 255, 9, 11, 255, 255, 8, 7, 255, 6, 255, 255, 255,
        255, 255, 255, 4, 255, 3, 5, 255, 255, 2, 1, 255, 0, 255, 255, 255,
        255, 15, 16, 255, 17, 255, 255, 255, 18, 255, 255, 255, 255, 255, 255, 255,

        255, 255, 255, 255, 255, 43, 48, 255, 255, 42, 47, 255, 49, 255, 255, 255,
        255, 41, 46, 255, 45, 255, 255, 255, 44, 255, 255, 255, 255, 255, 255, 255,
        255, 66, 40, 255, 39, 255, 255, 255, 38, 255, 255, 255, 255, 255, 255, 255,
        65, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};

    // Contrôles de cohérence déjà utilisés dans la v4.7.

    static_assert(
        decodeArray[0x99] == 8,
        "RailCom 4/8 : erreur decode 0x99");

    static_assert(
        decodeArray[0x8E] == 12,
        "RailCom 4/8 : erreur decode 0x8E");

    static_assert(
        decodeArray[0xA3] == 4,
        "RailCom 4/8 : erreur decode 0xA3");

    static_assert(
        decodeArray[0xAC] == 0,
        "RailCom 4/8 : erreur decode 0xAC");

} // namespace

// =============================================================================
// Constructeur
// =============================================================================

Railcom::Railcom(gpio_num_t rxPin,
                 uart_port_t uartNum) :

                                        m_rxPin(rxPin),
                                        m_uartNum(uartNum)
{
}

// =============================================================================
// begin()
// =============================================================================

bool Railcom::begin()
{
    // Protection contre un second appel accidentel.
    if (m_taskHandle != nullptr)
    {
        return true;
    }

    // -------------------------------------------------------------------------
    // Configuration UART
    // -------------------------------------------------------------------------

    uart_config_t uartConfig = {};

    uartConfig.baud_rate = RAILCOM_BAUD_RATE;
    uartConfig.data_bits = UART_DATA_8_BITS;
    uartConfig.parity = UART_PARITY_DISABLE;
    uartConfig.stop_bits = UART_STOP_BITS_1;
    uartConfig.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uartConfig.source_clk = UART_SCLK_APB;

    if (uart_param_config(m_uartNum, &uartConfig) != ESP_OK)
    {
        return false;
    }

    // RX uniquement.
    // Aucun GPIO TX n'est nécessaire.

    if (uart_set_pin(
            m_uartNum,
            UART_PIN_NO_CHANGE,
            m_rxPin,
            UART_PIN_NO_CHANGE,
            UART_PIN_NO_CHANGE) != ESP_OK)
    {
        return false;
    }

    // -------------------------------------------------------------------------
    // Installation du driver UART ESP-IDF
    // -------------------------------------------------------------------------

    if (uart_driver_install(
            m_uartNum,
            RAILCOM_RX_BUFFER_SIZE,
            0, // Pas de buffer TX
            RAILCOM_EVENT_QUEUE_SIZE,
            &m_uartQueue,
            0) != ESP_OK)
    {
        m_uartQueue = nullptr;
        return false;
    }

    // -------------------------------------------------------------------------
    // Configuration réception
    // -------------------------------------------------------------------------

    if ((uart_set_rx_full_threshold(
             m_uartNum,
             RAILCOM_RX_FIFO_THRESHOLD) != ESP_OK) ||
        (uart_set_rx_timeout(
             m_uartNum,
             RAILCOM_RX_TIMEOUT_SYMBOLS) != ESP_OK))
    {
        uart_driver_delete(m_uartNum);

        m_uartQueue = nullptr;

        return false;
    }

    uart_flush_input(m_uartNum);

    // -------------------------------------------------------------------------
    // État initial RailCom
    // -------------------------------------------------------------------------

    resetParser();

    m_address = 0;
    m_lastValidatedAddress = 0;
    m_lastValidatedReceptionMs = 0;

    // -------------------------------------------------------------------------
    // Tâche RailCom
    // -------------------------------------------------------------------------

    const BaseType_t result = xTaskCreatePinnedToCore(

        taskEntry,
        "RailComUART",
        4096,
        this,
        5,
        &m_taskHandle,
        1);

    if (result != pdPASS)
    {
        m_taskHandle = nullptr;
        uart_driver_delete(m_uartNum);
        m_uartQueue = nullptr;
        return false;
    }

    return true;
}

// =============================================================================
// address()
// =============================================================================

uint16_t Railcom::address() const
{
    return m_address;
}

// =============================================================================
// isPresent()
// =============================================================================

bool Railcom::isPresent() const
{
    return m_address != 0;
}

// =============================================================================
// taskEntry()
//
// Fonction statique servant de passerelle entre FreeRTOS et l'objet C++.
// =============================================================================

void Railcom::taskEntry(void *parameter)
{
    Railcom *self =
        static_cast<Railcom *>(parameter);

    self->task();

    // Normalement jamais atteint.
    vTaskDelete(nullptr);
}

// =============================================================================
// task()
//
// Réception UART entièrement événementielle.
// Le réveil toutes les 50 ms ne sert qu'à vérifier la perte RailCom.
// =============================================================================

void Railcom::task()
{
    uart_event_t event;

    for (;;)
    {
        if (xQueueReceive(
                m_uartQueue,
                &event,
                pdMS_TO_TICKS(50)) == pdTRUE)
        {
            switch (event.type)
            {

                // -----------------------------------------------------------------
                // Données reçues
                // -----------------------------------------------------------------

            case UART_DATA:
            {
                size_t remaining = event.size;

                while (remaining > 0)
                {
                    uint8_t temp[32];

                    const size_t requested =
                        (remaining < sizeof(temp))
                            ? remaining
                            : sizeof(temp);

                    const int received =
                        uart_read_bytes(
                            m_uartNum,
                            temp,
                            requested,
                            0);

                    if (received <= 0)
                    {
                        break;
                    }

                    for (int i = 0; i < received; ++i)
                    {
                        feedByte(temp[i]);
                    }

                    remaining -=
                        static_cast<size_t>(received);
                }

                break;
            }

                // -----------------------------------------------------------------
                // Débordement UART
                // -----------------------------------------------------------------

            case UART_FIFO_OVF:
            case UART_BUFFER_FULL:

                uart_flush_input(m_uartNum);

                xQueueReset(m_uartQueue);

                resetParser();

                break;

                // -----------------------------------------------------------------
                // Erreurs UART
                // -----------------------------------------------------------------

            case UART_FRAME_ERR:
            case UART_PARITY_ERR:
            case UART_BREAK:

                // On abandonne simplement une éventuelle
                // paire RailCom incomplète.

                m_waitingSecondSymbol = false;

                break;

            default:

                break;
            }
        }

        // Vérification périodique de présence.
        checkLoss();
    }
}

// =============================================================================
// feedByte()
//
// Parseur continu du flux UART.
//
// Un événement UART_DATA n'est PAS considéré comme une frontière RailCom.
// Une paire peut parfaitement être coupée entre deux événements UART.
// =============================================================================

void Railcom::feedByte(uint8_t raw)
{
    const uint8_t decoded =
        decodeArray[raw];

    // 64..66 = mots de contrôle RailCom
    // 255    = symbole invalide
    //
    // Ces valeurs constituent un séparateur naturel.

    if (decoded > 63)
    {
        m_waitingSecondSymbol = false;

        return;
    }

    // -------------------------------------------------------------------------
    // Recherche du premier symbole
    // -------------------------------------------------------------------------

    if (!m_waitingSecondSymbol)
    {
        const uint8_t identifier =
            decoded >> 2;

        // Canal 1 :
        //
        // ID 1 = ADR1
        // ID 2 = ADR2

        if ((identifier == 1) ||
            (identifier == 2))
        {
            m_firstDecodedSymbol = decoded;

            m_waitingSecondSymbol = true;
        }

        return;
    }

    // -------------------------------------------------------------------------
    // Deuxième symbole
    // -------------------------------------------------------------------------

    processAddressDatagram(
        m_firstDecodedSymbol,
        decoded);

    m_waitingSecondSymbol = false;
}

// =============================================================================
// processAddressDatagram()
// =============================================================================

void Railcom::processAddressDatagram(
    uint8_t symbol0,
    uint8_t symbol1)
{
    const uint8_t identifier =
        symbol0 >> 2;

    const uint8_t data =
        static_cast<uint8_t>(
            ((symbol0 & 0x03) << 6) |
            symbol1);

    switch (identifier)
    {

        // -------------------------------------------------------------------------
        // ADR1 : partie haute de l'adresse
        // -------------------------------------------------------------------------

    case 1:

        m_adr1Data = data;

        m_adr1Valid = true;

        break;

        // -------------------------------------------------------------------------
        // ADR2 : partie basse
        // -------------------------------------------------------------------------

    case 2:

        m_adr2Data = data;

        m_adr2Valid = true;

        break;

    default:

        return;
    }

    // -------------------------------------------------------------------------
    // Adresse complète disponible
    // -------------------------------------------------------------------------

    if (m_adr1Valid &&
        m_adr2Valid)
    {
        const uint16_t reconstructedAddress =
            buildAddress();

        // Les deux éléments sont consommés ensemble.

        m_adr1Valid = false;
        m_adr2Valid = false;

        validateAddress(
            reconstructedAddress);
    }
}

// =============================================================================
// buildAddress()
// =============================================================================

uint16_t Railcom::buildAddress() const
{
    // -------------------------------------------------------------------------
    // Adresse courte
    //
    // ADR1 < 128
    // L'adresse est directement contenue dans ADR2.
    // -------------------------------------------------------------------------

    if (m_adr1Data < 128)
    {
        return m_adr2Data;
    }

    // -------------------------------------------------------------------------
    // Adresse longue
    //
    // ADR1 contient les 6 bits hauts après retrait du marqueur 10xxxxxx.
    // ADR2 contient les 8 bits bas.
    // -------------------------------------------------------------------------

    return static_cast<uint16_t>(

        (static_cast<uint16_t>(
             m_adr1Data - 128)
         << 8)

        |

        m_adr2Data);
}

// =============================================================================
// validateAddress()
// =============================================================================

void Railcom::validateAddress(
    uint16_t address)
{
    // Adresse 0 interdite / absence.

    if (address == 0)
    {
        m_candidateAddress = 0;
        m_confirmationCount = 0;

        return;
    }

    // -------------------------------------------------------------------------
    // Nouvelle adresse candidate
    // -------------------------------------------------------------------------

    if (address != m_candidateAddress)
    {
        m_candidateAddress = address;

        m_confirmationCount = 1;

        return;
    }

    // -------------------------------------------------------------------------
    // Confirmation supplémentaire
    // -------------------------------------------------------------------------

    if (m_confirmationCount <
        ADDRESS_CONFIRMATIONS)
    {
        ++m_confirmationCount;
    }

    if (m_confirmationCount <
        ADDRESS_CONFIRMATIONS)
    {
        return;
    }

    // -------------------------------------------------------------------------
    // Adresse validée
    // -------------------------------------------------------------------------

    const uint32_t now =
        millis();

    if (address !=
        m_lastValidatedAddress)
    {
        m_lastValidatedAddress =
            address;

        m_address =
            address;
    }

    // Le watchdog de présence n'est rafraîchi
    // qu'après 5 reconstructions identiques consécutives.

    m_lastValidatedReceptionMs =
        now;

    // On repart de zéro :
    // il faudra à nouveau 5 confirmations
    // pour rafraîchir le watchdog.

    m_confirmationCount = 0;
}

// =============================================================================
// checkLoss()
// =============================================================================

void Railcom::checkLoss()
{
    if (m_lastValidatedAddress == 0)
    {
        return;
    }

    const uint32_t now =
        millis();

    if (static_cast<uint32_t>(
            now - m_lastValidatedReceptionMs) <
        RAILCOM_LOSS_TIMEOUT_MS)
    {
        return;
    }

    // -------------------------------------------------------------------------
    // Plus aucune série RailCom valide depuis 1 seconde
    // -------------------------------------------------------------------------

    m_address = 0;

    m_lastValidatedAddress = 0;

    m_lastValidatedReceptionMs = 0;

    resetParser();
}

// =============================================================================
// resetParser()
// =============================================================================

void Railcom::resetParser()
{
    m_adr1Data = 0;
    m_adr2Data = 0;

    m_adr1Valid = false;
    m_adr2Valid = false;

    m_candidateAddress = 0;
    m_confirmationCount = 0;

    m_waitingSecondSymbol = false;
    m_firstDecodedSymbol = 0;
}