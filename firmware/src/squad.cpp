// -----------------------------------------------------------------------------
// Création d'escouade et adhésion validée par le chef.
//
// Il n'y a aucun mot de passe : une escouade est une clé aléatoire de 16 octets,
// créée par son chef et transmise uniquement aux candidats qu'il valide, chiffrée
// pour eux seuls via X25519. C'est ce qui permet la révocation — faire tourner la
// clé et la redistribuer à tous sauf à l'exclu.
// -----------------------------------------------------------------------------
#include "squad.h"

#include <Arduino.h>
#include <string.h>

#include "blelink.h"
#include "crypto.h"
#include "mesh.h"
#include "settings.h"

namespace squad {

// Disposition de la charge utile d'un JOIN_GRANT (cf. shared/PROTOCOL.md §5.3).
static const size_t GRANT_FP = 0;
static const size_t GRANT_PUB = 4;
static const size_t GRANT_NONCE = 36;
static const size_t GRANT_CIPHER = 48;
static const size_t GRANT_PLAIN_LEN = 30;  // clé(16) + adresse(1) + longueur(1) + nom(12)
static const size_t GRANT_TAG = GRANT_CIPHER + GRANT_PLAIN_LEN;
static const size_t GRANT_LEN = GRANT_TAG + crypto::TAG_LEN;

static const uint32_t REQUEST_PERIOD_MS = 5000;
static const uint32_t REQUEST_TIMEOUT_MS = 60000;
static const uint32_t CANDIDATE_TTL_MS = 180000;

struct Candidate {
    uint8_t pub[crypto::X25519_LEN];
    uint8_t fp[crypto::FP_LEN];
    char callsign[17];
    uint32_t lastSeenMs;
    bool used;
};
static const size_t CANDIDATES_N = 8;
static Candidate s_candidates[CANDIDATES_N];

static char s_wantedName[13];
static uint32_t s_nextRequestMs = 0;
static uint32_t s_requestDeadlineMs = 0;

// --- Utilitaires -------------------------------------------------------------
static uint8_t squadIdFromName(const char *name) {
    char upper[13] = {0};
    const size_t n = strnlen(name, 12);
    for (size_t i = 0; i < n; i++) upper[i] = (char)toupper((unsigned char)name[i]);
    uint8_t h[32];
    crypto::sha256((const uint8_t *)upper, n, h);
    return h[0];
}

static void notifyApp(uint8_t event, const uint8_t fp[4], const char *callsign) {
    proto::Frame f;
    f.type = proto::T_JOIN_EVENT;
    f.flags = proto::F_LOCAL;
    f.squad = settings::cfg.squad;
    f.src = settings::cfg.addr;
    f.len = (uint8_t)proto::encodeJoinEvent(event, fp, callsign, f.payload);
    blelink::sendFrame(f);
}

void notifyIdentity() { notifyApp(proto::JOIN_EV_IDENTITY, settings::deviceFp, settings::cfg.callsign); }

static Candidate *findCandidate(const uint8_t fp[4]) {
    for (size_t i = 0; i < CANDIDATES_N; i++) {
        if (s_candidates[i].used && memcmp(s_candidates[i].fp, fp, crypto::FP_LEN) == 0) {
            return &s_candidates[i];
        }
    }
    return nullptr;
}

// --- Actions -----------------------------------------------------------------
static void createSquad(const char *name) {
    memset(s_candidates, 0, sizeof(s_candidates));
    crypto::randomBytes(settings::squadKey, crypto::KEY_LEN);
    settings::hasSquadKey = true;

    settings::cfg.squad = squadIdFromName(name);
    settings::cfg.addr = 1;
    settings::cfg.role = proto::ROLE_LEADER;
    settings::cfg.state = proto::SQUAD_LEADER;
    memset(settings::cfg.squadName, 0, sizeof(settings::cfg.squadName));
    strncpy(settings::cfg.squadName, name, 12);
    settings::addrMask = (1u << 1);
    settings::saveSquad();

    Serial.printf("[squad] escouade « %s » créée — canal %u, je suis #1 (chef)\n", name,
                  settings::cfg.squad);
    mesh::sendConfigState();
    mesh::broadcastNodeInfo();
}

static void startJoinRequest(const char *name) {
    memset(s_wantedName, 0, sizeof(s_wantedName));
    strncpy(s_wantedName, name, 12);

    settings::hasSquadKey = false;
    settings::cfg.squad = squadIdFromName(name);
    settings::cfg.addr = proto::ADDR_UNASSIGNED;
    settings::cfg.role = proto::ROLE_PLAYER;
    settings::cfg.state = proto::SQUAD_PENDING;
    memset(settings::cfg.squadName, 0, sizeof(settings::cfg.squadName));
    strncpy(settings::cfg.squadName, name, 12);

    s_nextRequestMs = millis();
    s_requestDeadlineMs = millis() + REQUEST_TIMEOUT_MS;

    Serial.printf("[squad] demande d'adhésion à « %s » (canal %u)\n", name, settings::cfg.squad);
    mesh::sendConfigState();
}

static void sendJoinRequest() {
    proto::Frame f;
    f.type = proto::T_JOIN_REQUEST;
    f.flags = 0;  // en clair : le candidat n'a pas encore la clé
    f.ttl = 1;    // acte de proximité, jamais relayé
    f.squad = settings::cfg.squad;

    const size_t nameLen = strnlen(settings::cfg.callsign, 16);
    memcpy(f.payload, settings::devicePub, crypto::X25519_LEN);
    f.payload[32] = (uint8_t)nameLen;
    memcpy(f.payload + 33, settings::cfg.callsign, nameLen);
    f.len = (uint8_t)(33 + nameLen);

    mesh::sendClear(f, 0);
}

static void acceptCandidate(const uint8_t fp[4]) {
    Candidate *c = findCandidate(fp);
    if (!c) return;

    const uint8_t addr = settings::allocateAddress();
    if (addr == 0) {
        Serial.println("[squad] escouade pleine : 31 membres maximum");
        return;
    }
    settings::saveSquad();

    uint8_t shared[crypto::X25519_LEN], wrap[crypto::KEY_LEN];
    if (!crypto::x25519Shared(settings::devicePriv, c->pub, shared)) return;
    crypto::joinWrapKey(shared, wrap);

    // Contenu chiffré : clé d'escouade + adresse attribuée + nom.
    uint8_t plain[GRANT_PLAIN_LEN] = {0};
    memcpy(plain, settings::squadKey, crypto::KEY_LEN);
    plain[16] = addr;
    const size_t nameLen = strnlen(settings::cfg.squadName, 12);
    plain[17] = (uint8_t)nameLen;
    memcpy(plain + 18, settings::cfg.squadName, nameLen);

    proto::Frame f;
    f.type = proto::T_JOIN_GRANT;
    f.flags = 0;
    f.ttl = 1;
    f.squad = settings::cfg.squad;
    memcpy(f.payload + GRANT_FP, c->fp, crypto::FP_LEN);
    memcpy(f.payload + GRANT_PUB, settings::devicePub, crypto::X25519_LEN);
    crypto::randomBytes(f.payload + GRANT_NONCE, crypto::NONCE_LEN);

    if (!crypto::seal(wrap, f.payload + GRANT_NONCE, nullptr, 0, plain, GRANT_PLAIN_LEN,
                      f.payload + GRANT_CIPHER, f.payload + GRANT_TAG)) {
        return;
    }
    f.len = GRANT_LEN;

    // Émis trois fois : la trame n'est pas relayée et un candidat qui la rate
    // devrait tout recommencer.
    mesh::sendClear(f, 0);
    mesh::sendClear(f, 1200);
    mesh::sendClear(f, 2600);

    Serial.printf("[squad] %s validé — adresse #%u attribuée\n", c->callsign, addr);
    c->used = false;
}

// --- Entrées -----------------------------------------------------------------
void begin() {
    memset(s_candidates, 0, sizeof(s_candidates));
    memset(s_wantedName, 0, sizeof(s_wantedName));
}

void tick() {
    if (settings::cfg.state != proto::SQUAD_PENDING) return;

    const uint32_t now = millis();
    if ((int32_t)(now - s_requestDeadlineMs) >= 0) {
        settings::cfg.state = proto::SQUAD_ALONE;
        memset(settings::cfg.squadName, 0, sizeof(settings::cfg.squadName));
        settings::cfg.squad = 0;
        Serial.println("[squad] demande expirée, aucune réponse du chef");
        notifyApp(proto::JOIN_EV_REFUSED, settings::deviceFp, "");
        mesh::sendConfigState();
        return;
    }
    if ((int32_t)(now - s_nextRequestMs) >= 0) {
        s_nextRequestMs = now + REQUEST_PERIOD_MS;
        sendJoinRequest();
    }
}

void handleCommand(const proto::Frame &f) {
    uint8_t cmd = 0, fp[crypto::FP_LEN] = {0};
    char name[13] = {0};
    if (!proto::decodeJoinCmd(f.payload, f.len, cmd, fp, name, sizeof(name))) return;

    switch (cmd) {
        case proto::JOIN_CMD_CREATE:
            if (name[0]) createSquad(name);
            break;
        case proto::JOIN_CMD_REQUEST:
            if (name[0]) startJoinRequest(name);
            break;
        case proto::JOIN_CMD_ACCEPT:
            if (settings::cfg.state == proto::SQUAD_LEADER) acceptCandidate(fp);
            break;
        case proto::JOIN_CMD_REFUSE: {
            // Aucune trame de refus n'est émise : elle ne serait pas authentifiable,
            // et le silence est indistinguable d'un chef hors de portée. Le candidat
            // conclut de lui-même à l'expiration.
            Candidate *c = findCandidate(fp);
            if (c) c->used = false;
            break;
        }
        case proto::JOIN_CMD_LEAVE:
            settings::clearSquad();
            memset(s_candidates, 0, sizeof(s_candidates));
            Serial.println("[squad] escouade quittée");
            mesh::sendConfigState();
            break;
    }
}

void handleJoinRequest(const proto::Frame &f) {
    if (settings::cfg.state != proto::SQUAD_LEADER) return;
    if (f.squad != settings::cfg.squad) return;
    if (f.len < 33) return;

    const uint8_t nameLen = f.payload[32];
    if (33 + (size_t)nameLen > f.len) return;

    uint8_t fp[crypto::FP_LEN];
    crypto::fingerprint(f.payload, fp);

    Candidate *c = findCandidate(fp);
    if (c) {  // déjà connu : la demande est réémise toutes les 5 s
        c->lastSeenMs = millis();
        return;
    }

    for (size_t i = 0; i < CANDIDATES_N; i++) {
        if (s_candidates[i].used) continue;
        c = &s_candidates[i];
        memcpy(c->pub, f.payload, crypto::X25519_LEN);
        memcpy(c->fp, fp, crypto::FP_LEN);
        memset(c->callsign, 0, sizeof(c->callsign));
        memcpy(c->callsign, f.payload + 33, nameLen > 16 ? 16 : nameLen);
        c->lastSeenMs = millis();
        c->used = true;

        Serial.printf("[squad] demande d'adhésion de %s (empreinte %02X%02X%02X%02X)\n",
                      c->callsign, fp[0], fp[1], fp[2], fp[3]);
        notifyApp(proto::JOIN_EV_PENDING, fp, c->callsign);
        return;
    }
    Serial.println("[squad] trop de demandes en attente, celle-ci est ignorée");
}

void handleJoinGrant(const proto::Frame &f) {
    if (settings::cfg.state != proto::SQUAD_PENDING) return;
    if (f.len < GRANT_LEN) return;
    if (memcmp(f.payload + GRANT_FP, settings::deviceFp, crypto::FP_LEN) != 0) return;

    uint8_t shared[crypto::X25519_LEN], wrap[crypto::KEY_LEN], plain[GRANT_PLAIN_LEN];
    if (!crypto::x25519Shared(settings::devicePriv, f.payload + GRANT_PUB, shared)) return;
    crypto::joinWrapKey(shared, wrap);

    if (!crypto::open(wrap, f.payload + GRANT_NONCE, nullptr, 0, f.payload + GRANT_CIPHER,
                      GRANT_PLAIN_LEN, f.payload + GRANT_TAG, plain)) {
        Serial.println("[squad] attribution reçue mais indéchiffrable — ignorée");
        return;
    }

    memcpy(settings::squadKey, plain, crypto::KEY_LEN);
    settings::hasSquadKey = true;
    settings::cfg.addr = plain[16];
    settings::cfg.squad = f.squad;
    settings::cfg.state = proto::SQUAD_MEMBER;
    settings::cfg.role = proto::ROLE_PLAYER;
    memset(settings::cfg.squadName, 0, sizeof(settings::cfg.squadName));
    memcpy(settings::cfg.squadName, plain + 18, plain[17] > 12 ? 12 : plain[17]);
    settings::saveSquad();

    Serial.printf("[squad] validé dans « %s » — je suis #%u\n", settings::cfg.squadName,
                  settings::cfg.addr);
    notifyApp(proto::JOIN_EV_KEY_RECEIVED, settings::deviceFp, settings::cfg.callsign);
    mesh::sendConfigState();
    mesh::broadcastNodeInfo();
}

}  // namespace squad
