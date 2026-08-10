#include "settings.h"

#include <Arduino.h>
#include <Preferences.h>
#include <string.h>

namespace settings {

proto::Config cfg;
uint8_t squadKey[crypto::KEY_LEN];
bool hasSquadKey = false;
uint8_t devicePriv[crypto::X25519_LEN];
uint8_t devicePub[crypto::X25519_LEN];
uint8_t deviceFp[crypto::FP_LEN];
uint16_t epoch = 0;
uint32_t addrMask = 0;

static Preferences prefs;
static const char *NS = "meshradio";

static uint32_t clampU32(uint32_t v, uint32_t lo, uint32_t hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

void begin() {
    prefs.begin(NS, false);

    // 1. Identité durable du boîtier.
    if (prefs.getBytesLength("privKey") != crypto::X25519_LEN) {
        crypto::x25519GeneratePrivate(devicePriv);
        prefs.putBytes("privKey", devicePriv, crypto::X25519_LEN);
        Serial.println("[settings] nouvelle identité X25519 générée");
    } else {
        prefs.getBytes("privKey", devicePriv, crypto::X25519_LEN);
    }
    crypto::x25519PublicFromPrivate(devicePriv, devicePub);
    crypto::fingerprint(devicePub, deviceFp);

    // 2. Compteur de démarrage — incrémenté À CHAQUE boot, avant toute émission.
    epoch = prefs.getUShort("epoch", 0) + 1;
    prefs.putUShort("epoch", epoch);

    // 3. Indicatif par défaut, dérivé de l'empreinte.
    snprintf(cfg.callsign, sizeof(cfg.callsign), "TB-%02X%02X", deviceFp[0], deviceFp[1]);
    if (prefs.isKey("callsign")) {
        strncpy(cfg.callsign, prefs.getString("callsign", cfg.callsign).c_str(), 16);
    }

    // 4. Escouade.
    cfg.squad = prefs.getUChar("squad", 0);
    cfg.addr = prefs.getUChar("addr", proto::ADDR_UNASSIGNED);
    cfg.role = prefs.getUChar("role", proto::ROLE_PLAYER);
    cfg.state = prefs.getUChar("state", proto::SQUAD_ALONE);
    addrMask = prefs.getULong("addrMask", 0);
    if (prefs.isKey("squadName")) {
        strncpy(cfg.squadName, prefs.getString("squadName", "").c_str(), 12);
    }
    // On ne sonde la clé que si l'état dit qu'il devrait y en avoir une : interroger
    // une entrée NVS absente fait écrire une ligne d'erreur trompeuse à la console.
    hasSquadKey = cfg.state != proto::SQUAD_ALONE &&
                  prefs.getBytesLength("squadKey") == crypto::KEY_LEN;
    if (hasSquadKey) prefs.getBytes("squadKey", squadKey, crypto::KEY_LEN);

    // Une adhésion en attente n'a pas de sens après un redémarrage : on repart seul.
    if (cfg.state == proto::SQUAD_PENDING) cfg.state = proto::SQUAD_ALONE;

    // 5. Radio.
    cfg.freqHz = prefs.getULong("freqHz", cfg.freqHz);
    cfg.sf = prefs.getUChar("sf", cfg.sf);
    cfg.txPower = (int8_t)prefs.getChar("txPower", cfg.txPower);
    cfg.posIntervalS = prefs.getUChar("posInt", cfg.posIntervalS);
    cfg.dutyPercent = prefs.getUChar("duty", cfg.dutyPercent);

    prefs.end();
}

void saveRadio() {
    prefs.begin(NS, false);
    prefs.putString("callsign", cfg.callsign);
    prefs.putULong("freqHz", cfg.freqHz);
    prefs.putUChar("sf", cfg.sf);
    prefs.putChar("txPower", cfg.txPower);
    prefs.putUChar("posInt", cfg.posIntervalS);
    prefs.putUChar("duty", cfg.dutyPercent);
    prefs.end();
}

void saveSquad() {
    prefs.begin(NS, false);
    prefs.putUChar("squad", cfg.squad);
    prefs.putUChar("addr", cfg.addr);
    prefs.putUChar("role", cfg.role);
    prefs.putUChar("state", cfg.state);
    prefs.putString("squadName", cfg.squadName);
    prefs.putULong("addrMask", addrMask);
    if (hasSquadKey) {
        prefs.putBytes("squadKey", squadKey, crypto::KEY_LEN);
    } else {
        prefs.remove("squadKey");
    }
    prefs.end();
}

void clearSquad() {
    memset(squadKey, 0, sizeof(squadKey));
    hasSquadKey = false;
    cfg.squad = 0;
    cfg.addr = proto::ADDR_UNASSIGNED;
    cfg.role = proto::ROLE_PLAYER;
    cfg.state = proto::SQUAD_ALONE;
    memset(cfg.squadName, 0, sizeof(cfg.squadName));
    addrMask = 0;
    saveSquad();
}

uint8_t allocateAddress() {
    for (uint8_t a = 2; a <= proto::ADDR_MAX; a++) {
        if (!(addrMask & (1u << a))) {
            addrMask |= (1u << a);
            return a;
        }
    }
    return 0;  // escouade pleine
}

bool applyFrom(const proto::Config &in) {
    const bool radioChanged =
        in.freqHz != cfg.freqHz || in.sf != cfg.sf || in.txPower != cfg.txPower;

    memset(cfg.callsign, 0, sizeof(cfg.callsign));
    memcpy(cfg.callsign, in.callsign, 16);

    // Bornes de sécurité : une valeur aberrante venue du téléphone ne doit pas
    // rendre la radio muette ni sortir de la bande.
    cfg.freqHz = clampU32(in.freqHz, 863000000u, 870000000u);
    cfg.sf = (uint8_t)clampU32(in.sf, 5, 12);
    cfg.txPower = (int8_t)(in.txPower < -9 ? -9 : (in.txPower > 22 ? 22 : in.txPower));
    cfg.posIntervalS = (uint8_t)clampU32(in.posIntervalS, 2, 255);
    cfg.dutyPercent = (uint8_t)clampU32(in.dutyPercent, 1, 100);

    saveRadio();
    return radioChanged;
}

void saveGpsProbe(uint8_t rxPin, uint8_t txPin, uint32_t baud) {
    prefs.begin(NS, false);
    prefs.putUChar("gpsRx", rxPin);
    prefs.putUChar("gpsTx", txPin);
    prefs.putULong("gpsBaud", baud);
    prefs.end();
}

bool loadGpsProbe(uint8_t &rxPin, uint8_t &txPin, uint32_t &baud) {
    prefs.begin(NS, true);
    const bool ok = prefs.isKey("gpsBaud");
    if (ok) {
        rxPin = prefs.getUChar("gpsRx", 9);
        txPin = prefs.getUChar("gpsTx", 8);
        baud = prefs.getULong("gpsBaud", 38400);
    }
    prefs.end();
    return ok;
}

}  // namespace settings
