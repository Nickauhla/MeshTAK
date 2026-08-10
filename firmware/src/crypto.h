#pragma once
#include <stddef.h>
#include <stdint.h>

// -----------------------------------------------------------------------------
// Primitives cryptographiques, sur mbedtls (accéléré matériellement sur ESP32-S3).
// Doit reproduire à l'octet près l'implémentation de référence Node/OpenSSL de
// shared/crypto-ref.ts — c'est ce que vérifie selftest.cpp au démarrage.
// -----------------------------------------------------------------------------
namespace crypto {

static const size_t KEY_LEN = 16;
static const size_t NONCE_LEN = 12;
static const size_t TAG_LEN = 8;
static const size_t X25519_LEN = 32;
static const size_t FP_LEN = 4;

void randomBytes(uint8_t *out, size_t n);
void sha256(const uint8_t *data, size_t len, uint8_t out[32]);

// AES-128-GCM, sceau tronqué à 8 octets.
bool seal(const uint8_t key[KEY_LEN], const uint8_t nonce[NONCE_LEN], const uint8_t *aad,
          size_t aadLen, const uint8_t *plain, size_t len, uint8_t *cipherOut,
          uint8_t tagOut[TAG_LEN]);

// Renvoie false si le sceau est invalide — la trame doit alors être jetée.
bool open(const uint8_t key[KEY_LEN], const uint8_t nonce[NONCE_LEN], const uint8_t *aad,
          size_t aadLen, const uint8_t *cipher, size_t len, const uint8_t tag[TAG_LEN],
          uint8_t *plainOut);

// X25519. Les clés brutes sont en little-endian, format RFC 7748.
void x25519GeneratePrivate(uint8_t priv[X25519_LEN]);
bool x25519PublicFromPrivate(const uint8_t priv[X25519_LEN], uint8_t pub[X25519_LEN]);
bool x25519Shared(const uint8_t priv[X25519_LEN], const uint8_t peerPub[X25519_LEN],
                  uint8_t out[X25519_LEN]);

// Empreinte affichée au chef pour vérifier de visu qui il valide.
void fingerprint(const uint8_t pub[X25519_LEN], uint8_t out[FP_LEN]);

// Clé d'enveloppe d'un JOIN_GRANT : seul le candidat visé peut la recalculer.
void joinWrapKey(const uint8_t shared[X25519_LEN], uint8_t out[KEY_LEN]);

}  // namespace crypto
