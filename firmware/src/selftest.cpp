#include "selftest.h"

#include <Arduino.h>
#include <string.h>

#include "crypto.h"
#include "protocol.h"
#include "vectors.h"

namespace selftest {

static int s_pass = 0;
static int s_fail = 0;

static void check(bool ok, const char *what) {
    if (ok) {
        s_pass++;
    } else {
        s_fail++;
        Serial.printf("[autotest] ÉCHEC : %s\n", what);
    }
}

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
    memcpy(f.payload, v.cipher, v.cipherLen);
    if (v.tagLen) memcpy(f.tag, v.tag, v.tagLen);
    return f;
}

bool run() {
    s_pass = 0;
    s_fail = 0;
    const uint32_t start = millis();

    uint8_t buf[proto::MAX_FRAME];
    uint8_t aad[16];
    uint8_t nonce[proto::NONCE_LEN];

    for (size_t i = 0; i < VECTOR_COUNT; i++) {
        const TestVector &v = VECTORS[i];
        const proto::Frame f = frameFrom(v);

        // 1. Le codec doit produire exactement les octets du TypeScript.
        const size_t n = proto::encode(f, buf, sizeof(buf));
        check(n == v.wireLen && memcmp(buf, v.wire, n) == 0, v.name);

        // 2. Et les relire sans rien perdre.
        proto::Frame back;
        check(proto::decode(v.wire, v.wireLen, back) && back.type == v.type &&
                  back.src == v.src && back.seq == v.seq && back.epoch == v.epoch &&
                  back.len == v.cipherLen,
              v.name);

        // 3. Vecteur d'initialisation et données authentifiées.
        proto::nonceFor(f, nonce);
        const size_t aadLen = proto::aadFor(f, aad);
        check(memcmp(nonce, v.nonce, proto::NONCE_LEN) == 0, v.name);
        check(aadLen == v.aadLen && memcmp(aad, v.aad, aadLen) == 0, v.name);

        // 4. mbedtls doit reproduire le chiffré et le sceau d'OpenSSL.
        if (v.tagLen == proto::TAG_LEN) {
            uint8_t cipher[proto::MAX_PAYLOAD];
            uint8_t tag[proto::TAG_LEN];
            check(crypto::seal(SQUAD_KEY, nonce, aad, aadLen, v.plain, v.plainLen, cipher, tag) &&
                      memcmp(cipher, v.cipher, v.plainLen) == 0 &&
                      memcmp(tag, v.tag, proto::TAG_LEN) == 0,
                  v.name);

            uint8_t plain[proto::MAX_PAYLOAD];
            check(crypto::open(SQUAD_KEY, nonce, aad, aadLen, v.cipher, v.cipherLen, v.tag, plain) &&
                      memcmp(plain, v.plain, v.plainLen) == 0,
                  v.name);

            // 5. Un chiffré altéré doit être refusé.
            uint8_t bad[proto::MAX_PAYLOAD];
            memcpy(bad, v.cipher, v.cipherLen);
            bad[0] ^= 0x01;
            check(!crypto::open(SQUAD_KEY, nonce, aad, aadLen, bad, v.cipherLen, v.tag, plain),
                  "altération non détectée");
        }
    }

    // 6. X25519 : les deux côtés doivent retomber sur le même secret qu'OpenSSL.
    uint8_t pub[32], shared[32], wrap[16], fp[4];
    check(crypto::x25519PublicFromPrivate(JOIN_ADMIN_PRIV, pub) &&
              memcmp(pub, JOIN_ADMIN_PUB, 32) == 0,
          "clé publique du chef");
    check(crypto::x25519PublicFromPrivate(JOIN_CAND_PRIV, pub) &&
              memcmp(pub, JOIN_CAND_PUB, 32) == 0,
          "clé publique du candidat");

    check(crypto::x25519Shared(JOIN_ADMIN_PRIV, JOIN_CAND_PUB, shared) &&
              memcmp(shared, JOIN_SHARED, 32) == 0,
          "secret partagé côté chef");
    check(crypto::x25519Shared(JOIN_CAND_PRIV, JOIN_ADMIN_PUB, shared) &&
              memcmp(shared, JOIN_SHARED, 32) == 0,
          "secret partagé côté candidat");

    crypto::joinWrapKey(shared, wrap);
    check(memcmp(wrap, JOIN_WRAP_KEY, 16) == 0, "clé d'enveloppe");

    crypto::fingerprint(JOIN_CAND_PUB, fp);
    check(memcmp(fp, JOIN_CAND_FP, 4) == 0, "empreinte du candidat");

    // 7. Le candidat doit pouvoir déchiffrer la clé d'escouade qu'on lui envoie.
    uint8_t grant[64];
    check(crypto::open(wrap, JOIN_NONCE, nullptr, 0, JOIN_CIPHER, sizeof(JOIN_CIPHER), JOIN_TAG,
                       grant) &&
              memcmp(grant, JOIN_PLAIN, sizeof(JOIN_PLAIN)) == 0,
          "déchiffrement du grant");

    Serial.printf("[autotest] %d/%d vérifications OK en %lu ms%s\n", s_pass, s_pass + s_fail,
                  (unsigned long)(millis() - start),
                  s_fail ? "  ← INCOMPATIBILITÉ AVEC L'APP" : "");
    return s_fail == 0;
}

}  // namespace selftest
