#pragma once
#include "protocol.h"

namespace blelink {

typedef void (*FrameHandler)(const proto::Frame &f);

void begin(const char *deviceName, FrameHandler handler);

// Draine les octets reçus par la pile BLE et appelle le handler (contexte boucle principale).
void poll();

bool connected();

/** Nom annoncé en BLE — celui que l'utilisateur doit reconnaître dans la liste. */
const char *name();

// Envoie une trame vers le téléphone (encodage SLIP + fragmentation MTU).
void sendFrame(const proto::Frame &f);

void sendLog(const char *text);

}  // namespace blelink
