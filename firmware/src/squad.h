#pragma once
#include "protocol.h"

namespace squad {

void begin();

/** Réémission périodique des demandes d'adhésion et expiration. */
void tick();

/** Commande venue de l'app (créer / demander / accepter / refuser / quitter). */
void handleCommand(const proto::Frame &f);

/**
 * Annonce à l'app l'empreinte de CE device. Émise à la demande de configuration,
 * donc à chaque connexion : l'app peut ainsi l'afficher pour la vérification de
 * vive voix, sans que la clé privée quitte jamais le firmware.
 */
void notifyIdentity();

/** Demande d'adhésion reçue par radio — traitée uniquement par le chef. */
void handleJoinRequest(const proto::Frame &f);

/** Attribution reçue par radio — traitée uniquement par un candidat en attente. */
void handleJoinGrant(const proto::Frame &f);

}  // namespace squad
