#pragma once
#include <stdint.h>

namespace board {

// Initialise le PMU AXP2101 et allume les rails RADIO (ALDO3) et GNSS (ALDO4).
// Sans cet appel, ni le SX1262 ni le GNSS ne sont alimentés sur le T-Beam Supreme.
bool begin();

uint8_t batteryPercent();   // 0..100, 255 si inconnu
uint16_t batteryMillivolts();
bool isCharging();
uint32_t nodeIdFromMac();

}  // namespace board
