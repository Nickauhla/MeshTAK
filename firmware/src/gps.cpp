#include "gps.h"

#include <Arduino.h>
#include <TinyGPSPlus.h>

#include "board_pins.h"
#include "settings.h"

namespace gps {

static TinyGPSPlus s_gps;
static char s_desc[32] = "non détecté";
static bool s_ready = false;

// Combinaisons sondées : {broche RX de l'ESP32, broche TX de l'ESP32}.
static const uint8_t PIN_SETS[2][2] = {{GPS_PIN_A, GPS_PIN_B}, {GPS_PIN_B, GPS_PIN_A}};
static const uint32_t BAUDS[] = {38400, 9600, 115200};

// Écoute `ms` millisecondes et renvoie true si au moins une phrase NMEA a passé
// le checksum : c'est la seule preuve fiable que la liaison est correcte.
static bool listenForNmea(uint32_t ms) {
    TinyGPSPlus probe;
    const uint32_t start = millis();
    while (millis() - start < ms) {
        while (Serial1.available()) {
            probe.encode((char)Serial1.read());
            if (probe.passedChecksum() > 0) return true;
        }
        delay(5);
    }
    return false;
}

static bool tryPort(uint8_t rx, uint8_t tx, uint32_t baud, uint32_t listenMs) {
    Serial1.end();
    delay(20);
    Serial1.begin(baud, SERIAL_8N1, rx, tx);
    delay(50);
    while (Serial1.available()) Serial1.read();  // on vide les octets tronqués
    return listenForNmea(listenMs);
}

void begin() {
    // Le L76K a besoin d'un niveau haut sur sa broche WAKEUP ; sans effet sur le M10.
    pinMode(GPS_WAKEUP, OUTPUT);
    digitalWrite(GPS_WAKEUP, HIGH);
    pinMode(GPS_PPS, INPUT);
    delay(100);

    // 1) Combinaison mémorisée lors d'un démarrage précédent.
    uint8_t rx = 0, tx = 0;
    uint32_t baud = 0;
    if (settings::loadGpsProbe(rx, tx, baud) && tryPort(rx, tx, baud, 1500)) {
        snprintf(s_desc, sizeof(s_desc), "rx=%u tx=%u @%lu", rx, tx, (unsigned long)baud);
        Serial.printf("[gps] port mémorisé OK : %s\n", s_desc);
        s_ready = true;
        return;
    }

    // 2) Balayage complet.
    Serial.println("[gps] détection du port GNSS…");
    for (uint8_t p = 0; p < 2; p++) {
        for (uint8_t b = 0; b < sizeof(BAUDS) / sizeof(BAUDS[0]); b++) {
            const uint8_t trx = PIN_SETS[p][0], ttx = PIN_SETS[p][1];
            if (tryPort(trx, ttx, BAUDS[b], 1500)) {
                snprintf(s_desc, sizeof(s_desc), "rx=%u tx=%u @%lu", trx, ttx,
                         (unsigned long)BAUDS[b]);
                Serial.printf("[gps] détecté : %s\n", s_desc);
                settings::saveGpsProbe(trx, ttx, BAUDS[b]);
                s_ready = true;
                return;
            }
        }
    }

    // 3) Échec : on retombe sur la configuration la plus probable pour que le
    //    reste du firmware fonctionne quand même (radio + messages).
    Serial.println("[gps] AUCUNE trame NMEA — vérifier le rail ALDO4 et l'antenne GNSS");
    Serial1.end();
    Serial1.begin(38400, SERIAL_8N1, GPS_PIN_A, GPS_PIN_B);
}

void poll() {
    while (Serial1.available()) s_gps.encode((char)Serial1.read());
}

bool hasFix() {
    return s_ready && s_gps.location.isValid() && s_gps.location.age() < 10000 &&
           s_gps.satellites.isValid() && s_gps.satellites.value() >= 3;
}

int32_t lat() { return (int32_t)(s_gps.location.lat() * 1e7); }
int32_t lon() { return (int32_t)(s_gps.location.lng() * 1e7); }

int16_t alt() {
    if (!s_gps.altitude.isValid()) return 0;
    const double m = s_gps.altitude.meters();
    return (m < -32000 || m > 32000) ? 0 : (int16_t)m;
}

uint8_t sats() { return s_gps.satellites.isValid() ? (uint8_t)s_gps.satellites.value() : 0; }

float hdop() {
    if (!s_gps.hdop.isValid()) return 0.0f;
    const double h = s_gps.hdop.hdop();
    return (h <= 0 || h > 99) ? 0.0f : (float)h;
}

const char *portDescription() { return s_desc; }

}  // namespace gps
