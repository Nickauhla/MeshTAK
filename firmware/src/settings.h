#pragma once
#include <stdint.h>

#include "crypto.h"
#include "protocol.h"

namespace settings {

extern proto::Config cfg;

/** Clé d'escouade — ne quitte JAMAIS le device (ni par BLE, ni en clair par radio). */
extern uint8_t squadKey[crypto::KEY_LEN];
extern bool hasSquadKey;

/** Identité durable du boîtier, générée au premier démarrage. */
extern uint8_t devicePriv[crypto::X25519_LEN];
extern uint8_t devicePub[crypto::X25519_LEN];
extern uint8_t deviceFp[crypto::FP_LEN];

/** Compteur de démarrage : garantit l'unicité des vecteurs d'initialisation. */
extern uint16_t epoch;

/** Adresses déjà attribuées par le chef (bit i = adresse i occupée). */
extern uint32_t addrMask;

void begin();
void saveRadio();
void saveSquad();
void clearSquad();

/** Plus petite adresse libre de 2 à 31, ou 0 si l'escouade est pleine. */
uint8_t allocateAddress();

/** Applique une configuration venue de l'app (champs en lecture seule ignorés). */
bool applyFrom(const proto::Config &incoming);

void saveGpsProbe(uint8_t rxPin, uint8_t txPin, uint32_t baud);
bool loadGpsProbe(uint8_t &rxPin, uint8_t &txPin, uint32_t &baud);

}  // namespace settings
