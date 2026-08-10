#pragma once
#include <stdint.h>

namespace gps {

// Ouvre l'UART GNSS. L'orientation RX/TX (GPIO 8/9) et le débit ne sont pas
// documentés de façon cohérente selon les sources : on sonde les combinaisons
// jusqu'à obtenir des trames NMEA valides, puis on mémorise le résultat en NVS.
void begin();
void poll();

bool hasFix();
int32_t lat();       // degrés × 1e7
int32_t lon();       // degrés × 1e7
int16_t alt();       // mètres
uint8_t sats();
float hdop();  // 0 si inconnu

const char *portDescription();  // ex. "rx=9 tx=8 @38400"

}  // namespace gps
