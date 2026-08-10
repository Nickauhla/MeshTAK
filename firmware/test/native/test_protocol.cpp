// -----------------------------------------------------------------------------
// Test natif du CODEC v2 : vérifie que le C++ produit exactement les octets du
// TypeScript, sur PC, sans matériel.
//
//   pio test -e native      (nécessite un compilateur C++ système)
//
// ⚠️ La CRYPTOGRAPHIE n'est pas testée ici : mbedtls n'existe que sur la cible.
// Elle est vérifiée par l'autotest embarqué (src/selftest.cpp), qui tourne au
// démarrage de la carte et confronte AES-GCM et X25519 aux mêmes vecteurs.
// -----------------------------------------------------------------------------
#include <string.h>
#include <unity.h>

#include "protocol.h"
#include "vectors.h"

static proto::Frame frameFrom(const TestVector &v) {
    proto::Frame f;
    f.type = v.type;
    f.flags = v.flags;
    f.ttl = v.ttl;
    f.squad = v.squad;
    f.src = v.src;
    f.seq = v.seq;
    f.epoch = v.epoch;
    if (v.dst >= 0) f.dst = (uint8_t)v.dst;
    f.len = (uint8_t)v.cipherLen;
    if (v.cipherLen) memcpy(f.payload, v.cipher, v.cipherLen);
    if (v.tagLen) memcpy(f.tag, v.tag, v.tagLen);
    return f;
}

void test_encode_matches_typescript(void) {
    uint8_t out[proto::MAX_FRAME];
    for (size_t i = 0; i < VECTOR_COUNT; i++) {
        const TestVector &v = VECTORS[i];
        const size_t n = proto::encode(frameFrom(v), out, sizeof(out));
        TEST_ASSERT_EQUAL_MESSAGE(v.wireLen, n, v.name);
        TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(v.wire, out, n, v.name);
    }
}

void test_decode_matches_typescript(void) {
    for (size_t i = 0; i < VECTOR_COUNT; i++) {
        const TestVector &v = VECTORS[i];
        proto::Frame f;
        TEST_ASSERT_TRUE_MESSAGE(proto::decode(v.wire, v.wireLen, f), v.name);
        TEST_ASSERT_EQUAL_MESSAGE(v.type, f.type, v.name);
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(v.flags, f.flags, v.name);
        TEST_ASSERT_EQUAL_MESSAGE(v.ttl, f.ttl, v.name);
        TEST_ASSERT_EQUAL_MESSAGE(v.squad, f.squad, v.name);
        TEST_ASSERT_EQUAL_MESSAGE(v.src, f.src, v.name);
        TEST_ASSERT_EQUAL_MESSAGE(v.seq, f.seq, v.name);
        TEST_ASSERT_EQUAL_MESSAGE(v.epoch, f.epoch, v.name);
        TEST_ASSERT_EQUAL_MESSAGE(v.cipherLen, f.len, v.name);
        if (v.cipherLen) {
            TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(v.cipher, f.payload, v.cipherLen, v.name);
        }
        if (v.dst >= 0) TEST_ASSERT_EQUAL_MESSAGE(v.dst, f.dst, v.name);
    }
}

void test_nonce_and_aad_match_typescript(void) {
    uint8_t nonce[proto::NONCE_LEN];
    uint8_t aad[16];
    for (size_t i = 0; i < VECTOR_COUNT; i++) {
        const TestVector &v = VECTORS[i];
        const proto::Frame f = frameFrom(v);
        proto::nonceFor(f, nonce);
        TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(v.nonce, nonce, proto::NONCE_LEN, v.name);
        const size_t aadLen = proto::aadFor(f, aad);
        TEST_ASSERT_EQUAL_MESSAGE(v.aadLen, aadLen, v.name);
        TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(v.aad, aad, aadLen, v.name);
    }
}

// La propriété sans laquelle le mesh s'effondre au premier relais.
void test_aad_survives_relay(void) {
    uint8_t aadDirect[16], aadRelayed[16];
    proto::Frame f;
    f.type = proto::T_POSITION;
    f.squad = 115;
    f.src = 5;
    f.seq = 4242;
    f.epoch = 9;

    f.ttl = 3;
    f.flags = proto::F_ENCRYPTED;
    const size_t n1 = proto::aadFor(f, aadDirect);

    f.ttl = 2;  // relayé
    f.flags = proto::F_ENCRYPTED | proto::F_RELAYED;
    const size_t n2 = proto::aadFor(f, aadRelayed);

    TEST_ASSERT_EQUAL(n1, n2);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(aadDirect, aadRelayed, n1);
}

void test_decode_rejects_malformed(void) {
    proto::Frame f;
    const TestVector &v = VECTORS[0];
    uint8_t buf[proto::MAX_FRAME];

    memcpy(buf, v.wire, v.wireLen);
    buf[0] = (9 << 4) | (buf[0] & 0x0F);  // mauvaise version
    TEST_ASSERT_FALSE(proto::decode(buf, v.wireLen, f));

    TEST_ASSERT_FALSE(proto::decode(v.wire, 7, f));  // en-tête tronqué
}

void test_quality_packing(void) {
    TEST_ASSERT_EQUAL(0, proto::encodeHdop(0.6f));
    TEST_ASSERT_EQUAL(2, proto::encodeHdop(1.2f));
    TEST_ASSERT_EQUAL(14, proto::encodeHdop(25.0f));
    TEST_ASSERT_EQUAL(15, proto::encodeHdop(0.0f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, proto::decodeHdop(15));

    TEST_ASSERT_EQUAL(15, proto::encodeBattery(255));
    for (uint8_t pct = 0; pct <= 100; pct += 5) {
        const int back = proto::decodeBattery(proto::encodeBattery(pct));
        TEST_ASSERT_INT_WITHIN(5, pct, back);
    }
}

void test_position_roundtrip(void) {
    proto::Position p;
    p.lat = -335678900;
    p.lon = -704567800;
    p.alt = -12;
    p.sats = 27;  // doit saturer à 15
    p.hdop = 5.0f;
    p.battPct = 0;
    p.status = proto::ST_ELIMINATED;

    uint8_t buf[proto::POSITION_LEN];
    proto::encodePosition(p, buf);

    proto::Position q;
    TEST_ASSERT_TRUE(proto::decodePosition(buf, sizeof(buf), q));
    TEST_ASSERT_EQUAL_INT32(p.lat, q.lat);
    TEST_ASSERT_EQUAL_INT32(p.lon, q.lon);
    TEST_ASSERT_EQUAL_INT16(p.alt, q.alt);
    TEST_ASSERT_EQUAL(15, q.sats);
    TEST_ASSERT_EQUAL(proto::ST_ELIMINATED, q.status);
}

void test_config_roundtrip(void) {
    proto::Config c;
    c.squad = 115;
    c.addr = proto::ADDR_MAX;
    c.role = proto::ROLE_LEADER;
    c.state = proto::SQUAD_LEADER;
    strncpy(c.callsign, "ALPHA-1", sizeof(c.callsign) - 1);
    strncpy(c.squadName, "Alpha", sizeof(c.squadName) - 1);
    c.txPower = -9;

    uint8_t buf[proto::CONFIG_LEN];
    TEST_ASSERT_EQUAL(proto::CONFIG_LEN, proto::encodeConfig(c, buf));

    proto::Config d;
    TEST_ASSERT_TRUE(proto::decodeConfig(buf, sizeof(buf), d));
    TEST_ASSERT_EQUAL(115, d.squad);
    TEST_ASSERT_EQUAL(proto::ADDR_MAX, d.addr);
    TEST_ASSERT_EQUAL_STRING("ALPHA-1", d.callsign);
    TEST_ASSERT_EQUAL_STRING("Alpha", d.squadName);
    TEST_ASSERT_EQUAL_INT8(-9, d.txPower);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_encode_matches_typescript);
    RUN_TEST(test_decode_matches_typescript);
    RUN_TEST(test_nonce_and_aad_match_typescript);
    RUN_TEST(test_aad_survives_relay);
    RUN_TEST(test_decode_rejects_malformed);
    RUN_TEST(test_quality_packing);
    RUN_TEST(test_position_roundtrip);
    RUN_TEST(test_config_roundtrip);
    return UNITY_END();
}
