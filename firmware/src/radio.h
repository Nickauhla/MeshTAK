#pragma once
#include <stddef.h>
#include <stdint.h>

namespace radio {

typedef void (*RxCallback)(const uint8_t *data, size_t len, int16_t rssi, int8_t snr);

bool begin(RxCallback cb);

// Ré-applique settings::cfg (fréquence, BW, SF, CR, puissance) sans redémarrer.
bool applyConfig();

// À appeler dans la boucle principale : remonte les paquets reçus.
void poll();

// Émission bloquante (quelques dizaines à quelques centaines de ms).
// `airtimeMsOut` reçoit le temps d'antenne réel, utilisé par le régulateur de duty-cycle.
bool send(const uint8_t *data, size_t len, uint32_t &airtimeMsOut);

uint32_t timeOnAirMs(size_t len);
bool ready();

}  // namespace radio
