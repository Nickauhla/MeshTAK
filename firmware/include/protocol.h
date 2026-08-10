#pragma once
// -----------------------------------------------------------------------------
// Codec du protocole MeshRadio v2 — implémentation C++ (header-only).
// Miroir exact de app/src/lib/proto/frames.ts.
// Spécification : shared/PROTOCOL.md — toute modification se fait là-bas d'abord.
//
// Ce fichier ne contient AUCUNE cryptographie (voir crypto.h) mais il calcule le
// vecteur d'initialisation et les données authentifiées, car ils se déduisent de
// l'en-tête et doivent être identiques des deux côtés.
// -----------------------------------------------------------------------------
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace proto {

static const uint8_t VERSION = 2;
static const size_t HEADER_LEN = 8;
static const size_t TAG_LEN = 8;
static const size_t MAX_PAYLOAD = 180;
static const size_t MAX_FRAME = HEADER_LEN + 1 + MAX_PAYLOAD + TAG_LEN;
static const size_t NONCE_LEN = 12;
static const size_t KEY_LEN = 16;

static const uint8_t ADDR_UNASSIGNED = 0;
static const uint8_t ADDR_MAX = 31;

// --- Types (4 bits) ----------------------------------------------------------
static const uint8_t T_POSITION = 1;
static const uint8_t T_TEXT = 2;
static const uint8_t T_NODEINFO = 3;
static const uint8_t T_ACK = 4;
static const uint8_t T_JOIN_REQUEST = 5;
static const uint8_t T_JOIN_GRANT = 6;
static const uint8_t T_CFG_GET = 8;
static const uint8_t T_CFG_STATE = 9;
static const uint8_t T_CFG_SET = 10;
static const uint8_t T_STATUS = 11;
static const uint8_t T_LOG = 12;
static const uint8_t T_JOIN_EVENT = 13;
static const uint8_t T_JOIN_CMD = 14;

// --- Drapeaux (6 bits) -------------------------------------------------------
static const uint8_t F_ENCRYPTED = 0x01;
static const uint8_t F_RELAYED = 0x02;
static const uint8_t F_UNICAST = 0x04;
static const uint8_t F_WANT_ACK = 0x08;
static const uint8_t F_LOCAL = 0x10;
static const uint8_t F_SIGNED = 0x20;  // réservé

// --- Énumérations ------------------------------------------------------------
static const uint8_t ST_OK = 0;
static const uint8_t ST_HIT = 1;
static const uint8_t ST_ELIMINATED = 2;
static const uint8_t ST_NEED_HELP = 3;

static const uint8_t ROLE_PLAYER = 0;
static const uint8_t ROLE_LEADER = 1;
static const uint8_t ROLE_RELAY = 2;
static const uint8_t ROLE_COMMAND = 3;

static const uint8_t SQUAD_ALONE = 0;
static const uint8_t SQUAD_PENDING = 1;
static const uint8_t SQUAD_MEMBER = 2;
static const uint8_t SQUAD_LEADER = 3;

static const uint8_t JOIN_EV_PENDING = 0;
static const uint8_t JOIN_EV_ACCEPTED = 1;
static const uint8_t JOIN_EV_REFUSED = 2;
static const uint8_t JOIN_EV_KEY_RECEIVED = 3;
// Identité du device lui-même, envoyée à l'app quand elle demande sa
// configuration : c'est l'empreinte que le chef lira de son côté.
static const uint8_t JOIN_EV_IDENTITY = 4;

static const uint8_t JOIN_CMD_CREATE = 0;
static const uint8_t JOIN_CMD_REQUEST = 1;
static const uint8_t JOIN_CMD_ACCEPT = 2;
static const uint8_t JOIN_CMD_REFUSE = 3;
static const uint8_t JOIN_CMD_LEAVE = 4;

// --- Accès little-endian -----------------------------------------------------
inline void put_u16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}
inline void put_u32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}
inline uint16_t get_u16(const uint8_t *p) { return (uint16_t)(p[0] | ((uint16_t)p[1] << 8)); }
inline uint32_t get_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
inline void put_i16(uint8_t *p, int16_t v) { put_u16(p, (uint16_t)v); }
inline void put_i32(uint8_t *p, int32_t v) { put_u32(p, (uint32_t)v); }
inline int16_t get_i16(const uint8_t *p) { return (int16_t)get_u16(p); }
inline int32_t get_i32(const uint8_t *p) { return (int32_t)get_u32(p); }

// --- Trame -------------------------------------------------------------------
struct Frame {
    uint8_t type;
    uint8_t flags;
    uint8_t ttl;
    uint8_t squad;
    uint8_t src;
    uint8_t dst;
    uint16_t seq;
    uint16_t epoch;
    uint8_t len;
    uint8_t payload[MAX_PAYLOAD];
    uint8_t tag[TAG_LEN];

    Frame() : type(0), flags(0), ttl(0), squad(0), src(0), dst(0), seq(0), epoch(0), len(0) {
        memset(payload, 0, sizeof(payload));
        memset(tag, 0, sizeof(tag));
    }
    bool unicast() const { return (flags & F_UNICAST) != 0; }
    bool encrypted() const { return (flags & F_ENCRYPTED) != 0; }
};

inline size_t encode(const Frame &f, uint8_t *out, size_t cap) {
    if (f.len > MAX_PAYLOAD) return 0;
    const size_t dstLen = f.unicast() ? 1 : 0;
    const size_t tagLen = f.encrypted() ? TAG_LEN : 0;
    const size_t total = HEADER_LEN + dstLen + f.len + tagLen;
    if (cap < total) return 0;

    out[0] = (uint8_t)(((VERSION & 0x0F) << 4) | (f.type & 0x0F));
    out[1] = (uint8_t)(((f.ttl & 0x03) << 6) | (f.flags & 0x3F));
    out[2] = f.squad;
    out[3] = f.src;
    put_u16(out + 4, f.seq);
    put_u16(out + 6, f.epoch);
    if (dstLen) out[8] = f.dst;
    memcpy(out + HEADER_LEN + dstLen, f.payload, f.len);
    if (tagLen) memcpy(out + HEADER_LEN + dstLen + f.len, f.tag, TAG_LEN);
    return total;
}

inline bool decode(const uint8_t *in, size_t n, Frame &f) {
    if (n < HEADER_LEN) return false;
    if ((in[0] >> 4) != VERSION) return false;

    const uint8_t flags = in[1] & 0x3F;
    const size_t dstLen = (flags & F_UNICAST) ? 1 : 0;
    const size_t tagLen = (flags & F_ENCRYPTED) ? TAG_LEN : 0;
    if (n < HEADER_LEN + dstLen + tagLen) return false;

    const size_t payloadLen = n - HEADER_LEN - dstLen - tagLen;
    if (payloadLen > MAX_PAYLOAD) return false;

    f.type = in[0] & 0x0F;
    f.flags = flags;
    f.ttl = in[1] >> 6;
    f.squad = in[2];
    f.src = in[3];
    f.seq = get_u16(in + 4);
    f.epoch = get_u16(in + 6);
    f.dst = dstLen ? in[8] : 0;
    f.len = (uint8_t)payloadLen;
    memcpy(f.payload, in + HEADER_LEN + dstLen, payloadLen);
    if (tagLen) memcpy(f.tag, in + n - TAG_LEN, TAG_LEN);
    return true;
}

// --- Éléments dérivés pour le chiffrement ------------------------------------
inline void nonceFor(const Frame &f, uint8_t out[NONCE_LEN]) {
    memset(out, 0, NONCE_LEN);
    out[0] = f.src;
    out[1] = f.squad;
    put_u16(out + 2, f.epoch);
    put_u16(out + 4, f.seq);
}

// Champs IMMUABLES uniquement : `ttl_flags` en est exclu, sinon le premier relais
// invaliderait le sceau (cf. shared/PROTOCOL.md §6).
inline size_t aadFor(const Frame &f, uint8_t *out) {
    out[0] = (uint8_t)(((VERSION & 0x0F) << 4) | (f.type & 0x0F));
    out[1] = f.squad;
    out[2] = f.src;
    put_u16(out + 3, f.seq);
    put_u16(out + 5, f.epoch);
    if (f.unicast()) {
        out[7] = f.dst;
        return 8;
    }
    return 7;
}

// --- Tassage des indicateurs de qualité --------------------------------------
inline uint8_t encodeHdop(float hdop) {
    static const float B[] = {0.8f, 1.0f, 1.3f, 1.6f, 2.0f, 2.5f, 3.0f,
                              4.0f, 5.0f, 6.0f, 8.0f, 10.f, 15.f, 20.f};
    if (!(hdop > 0)) return 15;
    for (uint8_t i = 0; i < 14; i++) {
        if (hdop < B[i]) return i;
    }
    return 14;
}

inline float decodeHdop(uint8_t code) {
    static const float V[] = {0.8f, 1.0f, 1.3f, 1.6f, 2.0f, 2.5f, 3.0f, 4.0f,
                              5.0f, 6.0f, 8.0f, 10.f, 15.f, 20.f, 30.f};
    return (code == 15) ? 0.0f : V[code & 0x0F];
}

inline uint8_t encodeBattery(uint8_t pct) {
    if (pct > 100) return 15;
    const uint8_t code = (uint8_t)((pct * 14 + 50) / 100);
    return code > 14 ? 14 : code;
}

inline uint8_t decodeBattery(uint8_t code) {
    return (code == 15) ? 255 : (uint8_t)(((code & 0x0F) * 100 + 7) / 14);
}

// --- POSITION (12 octets) ----------------------------------------------------
struct Position {
    int32_t lat;
    int32_t lon;
    int16_t alt;
    uint8_t sats;
    float hdop;
    uint8_t battPct;
    uint8_t status;
    Position() : lat(0), lon(0), alt(0), sats(0), hdop(0), battPct(255), status(ST_OK) {}
};
static const size_t POSITION_LEN = 12;

inline size_t encodePosition(const Position &p, uint8_t *out) {
    put_i32(out + 0, p.lat);
    put_i32(out + 4, p.lon);
    put_i16(out + 8, p.alt);
    out[10] = (uint8_t)(((p.sats > 15 ? 15 : p.sats) << 4) | encodeHdop(p.hdop));
    out[11] = (uint8_t)((encodeBattery(p.battPct) << 4) | (p.status & 0x0F));
    return POSITION_LEN;
}

inline bool decodePosition(const uint8_t *in, size_t n, Position &p) {
    if (n < POSITION_LEN) return false;
    p.lat = get_i32(in + 0);
    p.lon = get_i32(in + 4);
    p.alt = get_i16(in + 8);
    p.sats = in[10] >> 4;
    p.hdop = decodeHdop(in[10] & 0x0F);
    p.battPct = decodeBattery(in[11] >> 4);
    p.status = in[11] & 0x0F;
    return true;
}

// --- ACK (3 octets) ----------------------------------------------------------
static const size_t ACK_LEN = 3;
inline size_t encodeAck(uint16_t ackSeq, uint8_t ackSrc, uint8_t *out) {
    put_u16(out, ackSeq);
    out[2] = ackSrc;
    return ACK_LEN;
}

// --- CFG_STATE / CFG_SET (40 octets) -----------------------------------------
struct Config {
    uint8_t squad;
    uint8_t addr;
    uint8_t role;
    uint8_t state;
    char callsign[17];
    char squadName[13];
    uint32_t freqHz;
    uint8_t sf;
    int8_t txPower;
    uint8_t posIntervalS;
    uint8_t dutyPercent;
    Config()
        : squad(0), addr(ADDR_UNASSIGNED), role(ROLE_PLAYER), state(SQUAD_ALONE), freqHz(869525000u),
          sf(7), txPower(17), posIntervalS(10), dutyPercent(10) {
        memset(callsign, 0, sizeof(callsign));
        memset(squadName, 0, sizeof(squadName));
    }
};
static const size_t CONFIG_LEN = 40;

inline size_t encodeConfig(const Config &c, uint8_t *out) {
    memset(out, 0, CONFIG_LEN);
    out[0] = c.squad;
    out[1] = c.addr;
    out[2] = c.role;
    out[3] = c.state;
    memcpy(out + 4, c.callsign, strnlen(c.callsign, 16));
    memcpy(out + 20, c.squadName, strnlen(c.squadName, 12));
    put_u32(out + 32, c.freqHz);
    out[36] = c.sf;
    out[37] = (uint8_t)c.txPower;
    out[38] = c.posIntervalS;
    out[39] = c.dutyPercent;
    return CONFIG_LEN;
}

inline bool decodeConfig(const uint8_t *in, size_t n, Config &c) {
    if (n < CONFIG_LEN) return false;
    c.squad = in[0];
    c.addr = in[1];
    c.role = in[2];
    c.state = in[3];
    memset(c.callsign, 0, sizeof(c.callsign));
    memcpy(c.callsign, in + 4, 16);
    memset(c.squadName, 0, sizeof(c.squadName));
    memcpy(c.squadName, in + 20, 12);
    c.freqHz = get_u32(in + 32);
    c.sf = in[36];
    c.txPower = (int8_t)in[37];
    c.posIntervalS = in[38];
    c.dutyPercent = in[39];
    return true;
}

// --- STATUS (20 octets) ------------------------------------------------------
struct Status {
    uint8_t fixValid;
    int32_t lat;
    int32_t lon;
    int16_t alt;
    uint8_t sats;
    float hdop;
    uint8_t battPct;
    uint16_t battMv;
    uint8_t charging;
    uint8_t airtimePercent;
    int16_t lastRssi;
    int8_t lastSnr;
    uint8_t peerCount;
    Status()
        : fixValid(0), lat(0), lon(0), alt(0), sats(0), hdop(0), battPct(255), battMv(0), charging(0),
          airtimePercent(0), lastRssi(0), lastSnr(0), peerCount(0) {}
};
static const size_t STATUS_LEN = 20;

inline size_t encodeStatus(const Status &s, uint8_t *out) {
    out[0] = s.fixValid;
    put_i32(out + 1, s.lat);
    put_i32(out + 5, s.lon);
    put_i16(out + 9, s.alt);
    out[11] = (uint8_t)(((s.sats > 15 ? 15 : s.sats) << 4) | encodeHdop(s.hdop));
    out[12] = s.battPct;
    put_u16(out + 13, s.battMv);
    out[15] = s.charging;
    out[16] = s.airtimePercent;
    const int32_t rssi = s.lastRssi < 0 ? -s.lastRssi : s.lastRssi;
    out[17] = (uint8_t)(rssi > 255 ? 255 : rssi);
    out[18] = (uint8_t)s.lastSnr;
    out[19] = s.peerCount;
    return STATUS_LEN;
}

// --- Adhésion ----------------------------------------------------------------
inline size_t encodeJoinEvent(uint8_t event, const uint8_t fingerprint[4], const char *callsign,
                              uint8_t *out) {
    const size_t nameLen = strnlen(callsign, 16);
    out[0] = event;
    memcpy(out + 1, fingerprint, 4);
    out[5] = (uint8_t)nameLen;
    memcpy(out + 6, callsign, nameLen);
    return 6 + nameLen;
}

inline bool decodeJoinCmd(const uint8_t *in, size_t n, uint8_t &cmd, uint8_t fingerprint[4],
                          char *squadName, size_t nameCap) {
    if (n < 5) return false;
    cmd = in[0];
    memcpy(fingerprint, in + 1, 4);
    const size_t nameLen = n - 5;
    const size_t take = nameLen < nameCap - 1 ? nameLen : nameCap - 1;
    memset(squadName, 0, nameCap);
    memcpy(squadName, in + 5, take);
    return true;
}

}  // namespace proto
