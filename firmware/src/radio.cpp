#include "radio.h"

#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>

#include "board_pins.h"
#include "protocol.h"
#include "settings.h"

namespace radio {

// Sync word « réseau privé » : isole notre mesh des réseaux LoRaWAN/Meshtastic voisins.
static const uint8_t SYNC_WORD = 0x12;
// Le module SX1262 des T-Beam LilyGO utilise un TCXO piloté par DIO3 en 1,8 V
// et DIO2 comme commutateur d'antenne. Sans ces deux réglages : aucune émission.
static const float TCXO_VOLTAGE = 1.8f;
// Largeur de bande et codage sont figés : 250 kHz occupe exactement la sous-bande
// g3 (869,400-869,650 MHz) et 4/5 est le débit maximal. Seuls la fréquence, le SF
// et la puissance restent réglables depuis l'app.
static const float BANDWIDTH_KHZ = 250.0f;
static const uint8_t CODING_RATE = 5;

static SPIClass s_spi(FSPI);
static SX1262 s_radio = new Module(RADIO_CS, RADIO_DIO1, RADIO_RST, RADIO_BUSY, s_spi);
static volatile bool s_irqFlag = false;
static RxCallback s_cb = nullptr;
static bool s_ready = false;

static void IRAM_ATTR onDio1() { s_irqFlag = true; }

bool applyConfig() {
    if (!s_ready) return false;

    int st = s_radio.setFrequency(settings::cfg.freqHz / 1e6f);
    if (st == RADIOLIB_ERR_NONE) st = s_radio.setSpreadingFactor(settings::cfg.sf);
    if (st == RADIOLIB_ERR_NONE) st = s_radio.setOutputPower(settings::cfg.txPower);
    s_radio.startReceive();

    if (st != RADIOLIB_ERR_NONE) {
        Serial.printf("[radio] applyConfig erreur %d\n", st);
        return false;
    }
    Serial.printf("[radio] %.3f MHz BW=%.0f kHz SF%u CR4/%u %d dBm\n", settings::cfg.freqHz / 1e6f,
                  BANDWIDTH_KHZ, settings::cfg.sf, CODING_RATE, settings::cfg.txPower);
    return true;
}

bool begin(RxCallback cb) {
    s_cb = cb;
    s_spi.begin(RADIO_SCK, RADIO_MISO, RADIO_MOSI, RADIO_CS);

    int st = s_radio.begin(settings::cfg.freqHz / 1e6f, BANDWIDTH_KHZ, settings::cfg.sf,
                           CODING_RATE, SYNC_WORD, settings::cfg.txPower, 8 /* préambule */,
                           TCXO_VOLTAGE, false /* régulateur DC-DC */);
    if (st != RADIOLIB_ERR_NONE) {
        Serial.printf("[radio] SX1262 begin() a échoué : %d\n", st);
        return false;
    }

    s_radio.setDio2AsRfSwitch(true);
    s_radio.setCurrentLimit(140.0f);
    s_radio.setCRC(2);
    s_radio.setPacketReceivedAction(onDio1);

    st = s_radio.startReceive();
    if (st != RADIOLIB_ERR_NONE) {
        Serial.printf("[radio] startReceive() a échoué : %d\n", st);
        return false;
    }

    s_ready = true;
    Serial.printf("[radio] SX1262 OK — %.3f MHz SF%u\n", settings::cfg.freqHz / 1e6f,
                  settings::cfg.sf);
    return true;
}

void poll() {
    if (!s_ready || !s_irqFlag) return;
    s_irqFlag = false;

    uint8_t buf[proto::MAX_FRAME];
    const size_t n = s_radio.getPacketLength();
    if (n == 0 || n > sizeof(buf)) {
        s_radio.startReceive();
        return;
    }

    const int st = s_radio.readData(buf, n);
    const int16_t rssi = (int16_t)s_radio.getRSSI();
    const int8_t snr = (int8_t)s_radio.getSNR();
    s_radio.startReceive();

    if (st == RADIOLIB_ERR_NONE && s_cb) s_cb(buf, n, rssi, snr);
}

uint32_t timeOnAirMs(size_t len) {
    if (!s_ready) return 0;
    return (uint32_t)(s_radio.getTimeOnAir(len) / 1000ULL);
}

bool send(const uint8_t *data, size_t len, uint32_t &airtimeMsOut) {
    if (!s_ready) return false;
    airtimeMsOut = timeOnAirMs(len);

    const int st = s_radio.transmit((uint8_t *)data, len);
    // L'émission bloquante déclenche aussi DIO1 : on évite un faux « paquet reçu ».
    s_irqFlag = false;
    s_radio.startReceive();

    if (st != RADIOLIB_ERR_NONE) {
        Serial.printf("[radio] transmit() erreur %d\n", st);
        return false;
    }
    return true;
}

bool ready() { return s_ready; }

}  // namespace radio
