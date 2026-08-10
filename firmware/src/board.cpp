#include "board.h"

#include <Arduino.h>
#include <Wire.h>
#include <XPowersLib.h>

#include "board_pins.h"

namespace board {

static XPowersAXP2101 pmu;
static bool s_pmuOk = false;

bool begin() {
    Wire1.begin(I2C1_SDA, I2C1_SCL);

    s_pmuOk = pmu.begin(Wire1, AXP2101_SLAVE_ADDRESS, I2C1_SDA, I2C1_SCL);
    if (!s_pmuOk) {
        Serial.println("[board] AXP2101 introuvable sur le bus I2C1 (42/41)");
        return false;
    }

    // Rails indispensables (cf. board_pins.h).
    pmu.setALDO3Voltage(RAIL_MV_RADIO);
    pmu.enableALDO3();  // SX1262
    pmu.setALDO4Voltage(RAIL_MV_GNSS);
    pmu.enableALDO4();  // GNSS
    pmu.setALDO1Voltage(RAIL_MV_PERIPH);
    pmu.enableALDO1();  // écran + capteurs I2C
    pmu.setALDO2Voltage(RAIL_MV_PERIPH);
    pmu.enableALDO2();
    pmu.disableBLDO1();  // carte microSD inutilisée : on économise

    pmu.setSysPowerDownVoltage(2600);
    pmu.enableBattDetection();
    pmu.enableBattVoltageMeasure();
    pmu.enableVbusVoltageMeasure();
    pmu.enableSystemVoltageMeasure();

    // Charge 18650 : 4,2 V / 500 mA (valeur prudente pour une cellule standard).
    pmu.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);
    pmu.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_500MA);

    // Le rail radio a besoin de quelques dizaines de ms avant que le SX1262
    // réponde sur le SPI.
    delay(200);
    Serial.println("[board] AXP2101 OK — rails radio/GNSS actifs");
    return true;
}

uint8_t batteryPercent() {
    if (!s_pmuOk) return 255;
    const int p = pmu.getBatteryPercent();
    return (p < 0 || p > 100) ? 255 : (uint8_t)p;
}

uint16_t batteryMillivolts() { return s_pmuOk ? pmu.getBattVoltage() : 0; }

bool isCharging() { return s_pmuOk ? pmu.isCharging() : false; }

uint32_t nodeIdFromMac() {
    // MAC eFuse (48 bits) repliée sur 32 bits : identifiant stable, gravé en usine,
    // et unique en pratique pour la poignée de nœuds d'une partie.
    const uint64_t mac = ESP.getEfuseMac();
    return (uint32_t)(mac & 0xFFFFFFFFULL) ^ (uint32_t)(mac >> 32);
}

}  // namespace board
