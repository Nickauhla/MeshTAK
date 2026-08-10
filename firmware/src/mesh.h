#pragma once
#include <stddef.h>
#include <stdint.h>

#include "protocol.h"

namespace mesh {

static const uint8_t DEFAULT_TTL = 3;

void begin();
void tick();

void onRadioFrame(const uint8_t *raw, size_t n, int16_t rssi, int8_t snr);
void onAppFrame(const proto::Frame &f);

/** Renseigne src/seq/epoch/squad, chiffre si la clé d'escouade est connue, met en file. */
void sendNetwork(proto::Frame &f, uint32_t delayMs = 0);

/** Idem mais SANS chiffrement — réservé aux trames d'adhésion (§5 du protocole). */
void sendClear(proto::Frame &f, uint32_t delayMs = 0);

void broadcastPosition();
void broadcastNodeInfo();
void sendConfigState();

void setPlayerStatus(uint8_t status);
uint8_t playerStatus();

uint8_t peerCount(uint32_t maxAgeMs);
int16_t lastRssi();
void fillStatus(proto::Status &s);

}  // namespace mesh
