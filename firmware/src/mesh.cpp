#include "mesh.h"

#include <Arduino.h>
#include <esp_random.h>
#include <string.h>

#include "blelink.h"
#include "board.h"
#include "crypto.h"
#include "gps.h"
#include "radio.h"
#include "settings.h"
#include "squad.h"

namespace mesh {

static uint16_t s_seq = 0;
static uint8_t s_status = proto::ST_OK;
static int16_t s_lastRssi = 0;
static int8_t s_lastSnr = 0;

// --- Déduplication : clé (escouade, adresse, seq) -----------------------------
// L'escouade fait partie de la clé, sinon le membre n° 3 d'Alpha éclipserait le
// membre n° 3 de Bravo.
static const size_t DEDUP_N = 64;
static uint32_t s_seen[DEDUP_N];
static uint8_t s_seenIdx = 0;

static bool seenBefore(uint8_t squad, uint8_t src, uint16_t seq) {
    const uint32_t key = ((uint32_t)squad << 24) | ((uint32_t)src << 16) | seq;
    for (size_t i = 0; i < DEDUP_N; i++) {
        if (s_seen[i] == key) return true;
    }
    s_seen[s_seenIdx] = key;
    s_seenIdx = (uint8_t)((s_seenIdx + 1) % DEDUP_N);
    return false;
}

// --- Table des pairs entendus -------------------------------------------------
struct Peer {
    uint8_t squad;
    uint8_t addr;
    uint32_t lastMs;
    bool used;
};
static const size_t PEERS_N = 24;
static Peer s_peers[PEERS_N];

static void notePeer(uint8_t squad, uint8_t addr) {
    size_t oldest = 0;
    for (size_t i = 0; i < PEERS_N; i++) {
        if (s_peers[i].used && s_peers[i].squad == squad && s_peers[i].addr == addr) {
            s_peers[i].lastMs = millis();
            return;
        }
        if (!s_peers[i].used) oldest = i;
        else if (s_peers[oldest].used && (int32_t)(s_peers[i].lastMs - s_peers[oldest].lastMs) < 0)
            oldest = i;
    }
    s_peers[oldest] = {squad, addr, millis(), true};
}

uint8_t peerCount(uint32_t maxAgeMs) {
    uint8_t n = 0;
    const uint32_t now = millis();
    for (size_t i = 0; i < PEERS_N; i++) {
        if (s_peers[i].used && now - s_peers[i].lastMs <= maxAgeMs) n++;
    }
    return n;
}

int16_t lastRssi() { return s_lastRssi; }

// --- Budget de temps d'antenne (fenêtre glissante de 60 s) --------------------
static uint16_t s_airtimeSec[60];
static uint8_t s_airtimeIdx = 0;
static uint32_t s_lastSecMs = 0;
static uint32_t s_nextTxMs = 0;

static void airtimeTick() {
    while ((int32_t)(millis() - s_lastSecMs) >= 1000) {
        s_lastSecMs += 1000;
        s_airtimeIdx = (uint8_t)((s_airtimeIdx + 1) % 60);
        s_airtimeSec[s_airtimeIdx] = 0;
    }
}

static void airtimeNote(uint32_t ms) {
    const uint32_t v = (uint32_t)s_airtimeSec[s_airtimeIdx] + ms;
    s_airtimeSec[s_airtimeIdx] = (uint16_t)(v > 65535 ? 65535 : v);
}

static uint8_t airtimePercent() {
    uint32_t sum = 0;
    for (size_t i = 0; i < 60; i++) sum += s_airtimeSec[i];
    return (uint8_t)(sum / 600);  // sum(ms) / 60000 ms × 100
}

// --- Trace série ---------------------------------------------------------------
static const char *typeName(uint8_t t) {
    switch (t) {
        case proto::T_POSITION: return "POS ";
        case proto::T_TEXT: return "TXT ";
        case proto::T_NODEINFO: return "INFO";
        case proto::T_ACK: return "ACK ";
        case proto::T_JOIN_REQUEST: return "JOIN";
        case proto::T_JOIN_GRANT: return "GRNT";
        case proto::T_MARKER: return "MARK";
        default: return "??? ";
    }
}

// --- File d'émission ----------------------------------------------------------
struct Pending {
    uint8_t buf[proto::MAX_FRAME];
    size_t len;
    uint32_t dueMs;
    uint8_t type;
    uint8_t squad;
    uint8_t src;
    uint16_t seq;
    bool isRelay;
    bool used;
};
static const size_t QN = 8;
static Pending s_q[QN];

static bool enqueue(const proto::Frame &f, uint32_t delayMs, bool isRelay) {
    uint8_t buf[proto::MAX_FRAME];
    const size_t n = proto::encode(f, buf, sizeof(buf));
    if (n == 0) return false;

    int slot = -1;
    // Une position obsolète du même nœud n'a plus d'intérêt : on la remplace.
    if (f.type == proto::T_POSITION) {
        for (size_t i = 0; i < QN; i++) {
            if (s_q[i].used && s_q[i].type == proto::T_POSITION && s_q[i].src == f.src &&
                s_q[i].squad == f.squad) {
                slot = (int)i;
                break;
            }
        }
    }
    if (slot < 0) {
        for (size_t i = 0; i < QN; i++) {
            if (!s_q[i].used) {
                slot = (int)i;
                break;
            }
        }
    }
    if (slot < 0) {
        Serial.println("[mesh] file d'émission pleine, trame abandonnée");
        return false;
    }

    memcpy(s_q[slot].buf, buf, n);
    s_q[slot].len = n;
    s_q[slot].dueMs = millis() + delayMs;
    s_q[slot].type = f.type;
    s_q[slot].squad = f.squad;
    s_q[slot].src = f.src;
    s_q[slot].seq = f.seq;
    s_q[slot].isRelay = isRelay;
    s_q[slot].used = true;
    return true;
}

// Un autre nœud a déjà retransmis cette trame pendant notre temps d'attente :
// notre relais n'apporterait aucune couverture et coûterait du temps d'antenne.
static bool cancelPendingRelay(uint8_t squad, uint8_t src, uint16_t seq) {
    for (size_t i = 0; i < QN; i++) {
        if (s_q[i].used && s_q[i].isRelay && s_q[i].squad == squad && s_q[i].src == src &&
            s_q[i].seq == seq) {
            s_q[i].used = false;
            return true;
        }
    }
    return false;
}

// Le nœud qui a reçu le plus FAIBLEMENT relaie le premier : c'est lui le plus
// éloigné de l'émetteur, donc celui dont le relais étend le plus la couverture.
static uint32_t relayDelayMs(int8_t snr) {
    const int s = snr < -15 ? -15 : (snr > 10 ? 10 : snr);
    return 150 + (uint32_t)((s + 15) * 30) + (uint32_t)random(0, 80);
}

static void processQueue() {
    const uint32_t now = millis();
    if ((int32_t)(now - s_nextTxMs) < 0) return;

    int best = -1;
    for (size_t i = 0; i < QN; i++) {
        if (!s_q[i].used) continue;
        if ((int32_t)(now - s_q[i].dueMs) < 0) continue;
        if (best < 0 || (int32_t)(s_q[i].dueMs - s_q[best].dueMs) < 0) best = (int)i;
    }
    if (best < 0) return;

    uint32_t airtime = 0;
    if (radio::send(s_q[best].buf, s_q[best].len, airtime)) {
        airtimeNote(airtime);
        Serial.printf("[tx] %s %u o  %lu ms d'antenne\n", typeName(s_q[best].type),
                      (unsigned)s_q[best].len, (unsigned long)airtime);
        const uint32_t duty = settings::cfg.dutyPercent ? settings::cfg.dutyPercent : 10;
        s_nextTxMs = millis() + (uint32_t)((uint64_t)airtime * (100 - duty) / duty);
    }
    s_q[best].used = false;
}

// --- Émission -----------------------------------------------------------------
void sendClear(proto::Frame &f, uint32_t delayMs) {
    f.src = settings::cfg.addr;
    f.seq = ++s_seq;
    f.epoch = settings::epoch;
    if (f.ttl == 0) f.ttl = DEFAULT_TTL;
    enqueue(f, delayMs, false);
}

void sendNetwork(proto::Frame &f, uint32_t delayMs) {
    f.squad = settings::cfg.squad;
    f.src = settings::cfg.addr;
    f.seq = ++s_seq;
    f.epoch = settings::epoch;
    if (f.ttl == 0) f.ttl = DEFAULT_TTL;

    if (settings::hasSquadKey) {
        uint8_t nonce[proto::NONCE_LEN];
        uint8_t aad[16];
        proto::nonceFor(f, nonce);
        const size_t aadLen = proto::aadFor(f, aad);

        uint8_t cipher[proto::MAX_PAYLOAD];
        if (!crypto::seal(settings::squadKey, nonce, aad, aadLen, f.payload, f.len, cipher,
                          f.tag)) {
            Serial.println("[mesh] échec du chiffrement, trame abandonnée");
            return;
        }
        memcpy(f.payload, cipher, f.len);
        f.flags |= proto::F_ENCRYPTED;
    }
    enqueue(f, delayMs, false);
}

// --- Construction -------------------------------------------------------------
void begin() {
    for (size_t i = 0; i < DEDUP_N; i++) s_seen[i] = 0xFFFFFFFFu;
    memset(s_airtimeSec, 0, sizeof(s_airtimeSec));
    memset(s_q, 0, sizeof(s_q));
    memset(s_peers, 0, sizeof(s_peers));
    s_lastSecMs = millis();
    randomSeed((uint32_t)esp_random());
}

void tick() {
    airtimeTick();
    processQueue();
}

void broadcastPosition() {
    if (!gps::hasFix() || settings::cfg.state == proto::SQUAD_ALONE) return;

    proto::Position p;
    p.lat = gps::lat();
    p.lon = gps::lon();
    p.alt = gps::alt();
    p.sats = gps::sats();
    p.hdop = gps::hdop();
    p.battPct = board::batteryPercent();
    p.status = s_status;

    proto::Frame f;
    f.type = proto::T_POSITION;
    f.len = (uint8_t)proto::encodePosition(p, f.payload);
    sendNetwork(f);
}

void broadcastNodeInfo() {
    if (settings::cfg.state == proto::SQUAD_ALONE) return;
    proto::Frame f;
    f.type = proto::T_NODEINFO;
    f.payload[0] = settings::cfg.role;
    const size_t nameLen = strnlen(settings::cfg.callsign, 16);
    memcpy(f.payload + 1, settings::cfg.callsign, nameLen);
    f.len = (uint8_t)(1 + nameLen);
    sendNetwork(f);
}

void sendConfigState() {
    proto::Frame f;
    f.type = proto::T_CFG_STATE;
    f.flags = proto::F_LOCAL;
    f.squad = settings::cfg.squad;
    f.src = settings::cfg.addr;
    f.len = (uint8_t)proto::encodeConfig(settings::cfg, f.payload);
    blelink::sendFrame(f);
}

static void sendAck(const proto::Frame &orig) {
    proto::Frame f;
    f.type = proto::T_ACK;
    f.flags = proto::F_UNICAST;
    f.dst = orig.src;
    f.len = (uint8_t)proto::encodeAck(orig.seq, orig.src, f.payload);
    sendNetwork(f, (uint32_t)random(50, 250));
}

// --- Réception ----------------------------------------------------------------
static void traceRx(const proto::Frame &f, int16_t rssi, int8_t snr, const char *note) {
    Serial.printf("[rx] %s de #%u (esc %u) seq=%u ttl=%u  %d dBm / %d dB%s%s\n", typeName(f.type),
                  f.src, f.squad, f.seq, f.ttl, rssi, snr,
                  (f.flags & proto::F_RELAYED) ? "  relayé" : "", note);
}

static void deliverPlain(const proto::Frame &enc, const uint8_t *plain, uint8_t len,
                         int16_t rssi, int8_t snr) {
    proto::Frame out = enc;
    out.flags &= (uint8_t)~proto::F_ENCRYPTED;
    out.len = len;
    memcpy(out.payload, plain, len);
    blelink::sendFrame(out);

    if (out.type == proto::T_POSITION) {
        proto::Position p;
        if (proto::decodePosition(plain, len, p)) {
            Serial.printf("       %.6f, %.6f  alt=%d m  %u sat  batt=%u%%  statut=%u\n",
                          p.lat / 1e7, p.lon / 1e7, p.alt, p.sats, p.battPct, p.status);
        }
    } else if (out.type == proto::T_TEXT) {
        Serial.printf("       « %.*s »\n", (int)len, (const char *)plain);
    } else if (out.type == proto::T_MARKER) {
        proto::Marker m;
        if (proto::decodeMarker(plain, len, m)) {
            if (m.kind == proto::MK_CLEAR) {
                Serial.printf("       point #%u de #%u retiré\n", m.id, m.owner);
            } else {
                Serial.printf("       point #%u de #%u  type=%u  %.6f, %.6f  « %s »\n", m.id,
                              m.owner, m.kind, m.lat / 1e7, m.lon / 1e7, m.label);
            }
        }
    }
    (void)rssi;
    (void)snr;
}

void onRadioFrame(const uint8_t *raw, size_t n, int16_t rssi, int8_t snr) {
    proto::Frame f;
    if (!proto::decode(raw, n, f)) return;
    if (f.flags & proto::F_LOCAL) return;

    const bool mine = f.squad == settings::cfg.squad &&
                      settings::cfg.addr != proto::ADDR_UNASSIGNED && f.src == settings::cfg.addr;
    if (mine) return;  // notre propre trame, relayée par un voisin

    s_lastRssi = rssi;
    s_lastSnr = snr;
    notePeer(f.squad, f.src);

    // Les trames d'adhésion sont volontairement répétées : elles échappent au filtre.
    const bool isJoin = f.type == proto::T_JOIN_REQUEST || f.type == proto::T_JOIN_GRANT;
    if (!isJoin) {
        if (seenBefore(f.squad, f.src, f.seq)) {
            traceRx(f, rssi, snr, "  [doublon]");
            if (cancelPendingRelay(f.squad, f.src, f.seq)) {
                Serial.println("       relais annulé — déjà retransmis par un autre nœud");
            }
            return;
        }
    }

    const bool forMe = f.unicast() && f.squad == settings::cfg.squad &&
                       f.dst == settings::cfg.addr &&
                       settings::cfg.addr != proto::ADDR_UNASSIGNED;

    // 1. Adhésion.
    if (f.type == proto::T_JOIN_REQUEST) {
        traceRx(f, rssi, snr, "");
        squad::handleJoinRequest(f);
    } else if (f.type == proto::T_JOIN_GRANT) {
        traceRx(f, rssi, snr, "");
        squad::handleJoinGrant(f);
    }
    // 2. Trafic de notre escouade : on déchiffre et on remonte au téléphone.
    else if (f.squad == settings::cfg.squad && settings::hasSquadKey && f.encrypted()) {
        uint8_t nonce[proto::NONCE_LEN];
        uint8_t aad[16];
        proto::nonceFor(f, nonce);
        const size_t aadLen = proto::aadFor(f, aad);

        uint8_t plain[proto::MAX_PAYLOAD];
        if (crypto::open(settings::squadKey, nonce, aad, aadLen, f.payload, f.len, f.tag, plain)) {
            traceRx(f, rssi, snr, "");
            deliverPlain(f, plain, f.len, rssi, snr);
            if (f.type == proto::T_TEXT && (f.flags & proto::F_WANT_ACK) && forMe) sendAck(f);
        } else {
            // Même identifiant d'escouade, clé différente : collision de noms.
            traceRx(f, rssi, snr, "  [sceau invalide]");
        }
    }
    // 3. Une autre escouade : illisible, mais on la relaie quand même.
    else {
        traceRx(f, rssi, snr, "  [autre escouade]");
    }

    // 4. Relais — indépendant de notre capacité à lire la trame.
    if (f.ttl > 1 && !forMe) {
        proto::Frame relayed = f;
        relayed.ttl = (uint8_t)(f.ttl - 1);
        relayed.flags |= proto::F_RELAYED;
        enqueue(relayed, relayDelayMs(snr), true);
    }
}

void onAppFrame(const proto::Frame &f) {
    switch (f.type) {
        case proto::T_CFG_GET:
            sendConfigState();
            // L'app pose cette question dès la connexion : c'est le bon moment
            // pour lui dire qui nous sommes.
            squad::notifyIdentity();
            break;

        case proto::T_CFG_SET: {
            proto::Config c;
            if (!proto::decodeConfig(f.payload, f.len, c)) break;
            if (settings::applyFrom(c)) radio::applyConfig();
            sendConfigState();
            broadcastNodeInfo();
            break;
        }

        case proto::T_JOIN_CMD:
            squad::handleCommand(f);
            break;

        // Une POSITION venue du téléphone ne porte pas de coordonnées : elle
        // déclare le statut du joueur.
        case proto::T_POSITION: {
            proto::Position p;
            if (!proto::decodePosition(f.payload, f.len, p)) break;
            s_status = p.status;
            broadcastPosition();
            break;
        }

        case proto::T_TEXT: {
            if (f.len == 0 || !settings::hasSquadKey) break;
            proto::Frame out;
            out.type = proto::T_TEXT;
            out.flags = f.flags & (proto::F_UNICAST | proto::F_WANT_ACK);
            out.dst = f.dst;
            out.len = f.len;
            memcpy(out.payload, f.payload, f.len);
            sendNetwork(out);
            break;
        }

        // Point tactique posé sur la carte du téléphone : le device n'en fait
        // rien d'autre que le diffuser à l'escouade, chiffré comme le reste.
        // `owner` reste celui qu'a écrit l'app — on peut retirer le point d'un
        // autre, c'est la charge utile qui porte l'identité, pas l'en-tête.
        case proto::T_MARKER: {
            proto::Marker m;
            if (!proto::decodeMarker(f.payload, f.len, m) || !settings::hasSquadKey) break;
            proto::Frame out;
            out.type = proto::T_MARKER;
            out.len = (uint8_t)proto::encodeMarker(m, out.payload);
            sendNetwork(out);
            break;
        }

        case proto::T_NODEINFO:
            broadcastNodeInfo();
            break;

        default:
            break;
    }
}

void setPlayerStatus(uint8_t status) { s_status = status; }
uint8_t playerStatus() { return s_status; }

void fillStatus(proto::Status &s) {
    s.fixValid = gps::hasFix() ? 1 : 0;
    s.lat = s.fixValid ? gps::lat() : 0;
    s.lon = s.fixValid ? gps::lon() : 0;
    s.alt = s.fixValid ? gps::alt() : 0;
    s.sats = gps::sats();
    s.hdop = gps::hdop();
    s.battPct = board::batteryPercent();
    s.battMv = board::batteryMillivolts();
    s.charging = board::isCharging() ? 1 : 0;
    s.airtimePercent = airtimePercent();
    s.lastRssi = s_lastRssi;
    s.lastSnr = s_lastSnr;
    s.peerCount = peerCount(120000);
}

}  // namespace mesh
