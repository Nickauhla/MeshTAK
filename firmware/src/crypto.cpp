#include "crypto.h"

#include <esp_random.h>
#include <mbedtls/ecdh.h>
#include <mbedtls/ecp.h>
#include <mbedtls/gcm.h>
#include <mbedtls/sha256.h>
#include <string.h>

namespace crypto {

static const char JOIN_CONTEXT[] = "MESHRADIO-JOIN-v2";

void randomBytes(uint8_t *out, size_t n) { esp_fill_random(out, n); }

// mbedtls réclame une source d'aléa pour l'aveuglement des multiplications ECP.
static int rngAdapter(void *, unsigned char *out, size_t len) {
    esp_fill_random(out, len);
    return 0;
}

void sha256(const uint8_t *data, size_t len, uint8_t out[32]) {
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts_ret(&ctx, 0);
    mbedtls_sha256_update_ret(&ctx, data, len);
    mbedtls_sha256_finish_ret(&ctx, out);
    mbedtls_sha256_free(&ctx);
}

// --- AES-128-GCM -------------------------------------------------------------
bool seal(const uint8_t key[KEY_LEN], const uint8_t nonce[NONCE_LEN], const uint8_t *aad,
          size_t aadLen, const uint8_t *plain, size_t len, uint8_t *cipherOut,
          uint8_t tagOut[TAG_LEN]) {
    mbedtls_gcm_context ctx;
    mbedtls_gcm_init(&ctx);
    bool ok = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, KEY_LEN * 8) == 0 &&
              mbedtls_gcm_crypt_and_tag(&ctx, MBEDTLS_GCM_ENCRYPT, len, nonce, NONCE_LEN, aad,
                                        aadLen, plain, cipherOut, TAG_LEN, tagOut) == 0;
    mbedtls_gcm_free(&ctx);
    return ok;
}

bool open(const uint8_t key[KEY_LEN], const uint8_t nonce[NONCE_LEN], const uint8_t *aad,
          size_t aadLen, const uint8_t *cipher, size_t len, const uint8_t tag[TAG_LEN],
          uint8_t *plainOut) {
    mbedtls_gcm_context ctx;
    mbedtls_gcm_init(&ctx);
    bool ok = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, KEY_LEN * 8) == 0 &&
              mbedtls_gcm_auth_decrypt(&ctx, len, nonce, NONCE_LEN, aad, aadLen, tag, TAG_LEN,
                                       cipher, plainOut) == 0;
    mbedtls_gcm_free(&ctx);
    return ok;
}

// --- X25519 ------------------------------------------------------------------
// Le « clamping » fait partie de X25519 (RFC 7748 §5) : sans lui, mbedtls rejette
// le scalaire, et surtout on ne retomberait pas sur la même clé publique
// qu'OpenSSL pour un même scalaire brut — donc les vecteurs de test échoueraient.
static void clampScalar(uint8_t k[X25519_LEN]) {
    k[0] &= 248;
    k[31] &= 127;
    k[31] |= 64;
}

void x25519GeneratePrivate(uint8_t priv[X25519_LEN]) {
    esp_fill_random(priv, X25519_LEN);
    clampScalar(priv);
}

bool x25519PublicFromPrivate(const uint8_t priv[X25519_LEN], uint8_t pub[X25519_LEN]) {
    uint8_t k[X25519_LEN];
    memcpy(k, priv, X25519_LEN);
    clampScalar(k);

    mbedtls_ecp_group grp;
    mbedtls_mpi d;
    mbedtls_ecp_point Q;
    mbedtls_ecp_group_init(&grp);
    mbedtls_mpi_init(&d);
    mbedtls_ecp_point_init(&Q);

    bool ok = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_CURVE25519) == 0 &&
              mbedtls_mpi_read_binary_le(&d, k, X25519_LEN) == 0 &&
              mbedtls_ecp_mul(&grp, &Q, &d, &grp.G, rngAdapter, nullptr) == 0 &&
              mbedtls_mpi_write_binary_le(&Q.X, pub, X25519_LEN) == 0;

    mbedtls_ecp_point_free(&Q);
    mbedtls_mpi_free(&d);
    mbedtls_ecp_group_free(&grp);
    return ok;
}

bool x25519Shared(const uint8_t priv[X25519_LEN], const uint8_t peerPub[X25519_LEN],
                  uint8_t out[X25519_LEN]) {
    uint8_t k[X25519_LEN];
    memcpy(k, priv, X25519_LEN);
    clampScalar(k);

    mbedtls_ecp_group grp;
    mbedtls_mpi d, z;
    mbedtls_ecp_point Qp;
    mbedtls_ecp_group_init(&grp);
    mbedtls_mpi_init(&d);
    mbedtls_mpi_init(&z);
    mbedtls_ecp_point_init(&Qp);

    bool ok = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_CURVE25519) == 0 &&
              mbedtls_mpi_read_binary_le(&d, k, X25519_LEN) == 0 &&
              mbedtls_mpi_read_binary_le(&Qp.X, peerPub, X25519_LEN) == 0 &&
              mbedtls_mpi_lset(&Qp.Z, 1) == 0 &&
              mbedtls_ecdh_compute_shared(&grp, &z, &Qp, &d, rngAdapter, nullptr) == 0 &&
              mbedtls_mpi_write_binary_le(&z, out, X25519_LEN) == 0;

    mbedtls_ecp_point_free(&Qp);
    mbedtls_mpi_free(&z);
    mbedtls_mpi_free(&d);
    mbedtls_ecp_group_free(&grp);
    return ok;
}

void fingerprint(const uint8_t pub[X25519_LEN], uint8_t out[FP_LEN]) {
    uint8_t h[32];
    sha256(pub, X25519_LEN, h);
    memcpy(out, h, FP_LEN);
}

void joinWrapKey(const uint8_t shared[X25519_LEN], uint8_t out[KEY_LEN]) {
    uint8_t buf[X25519_LEN + sizeof(JOIN_CONTEXT) - 1];
    memcpy(buf, shared, X25519_LEN);
    memcpy(buf + X25519_LEN, JOIN_CONTEXT, sizeof(JOIN_CONTEXT) - 1);
    uint8_t h[32];
    sha256(buf, sizeof(buf), h);
    memcpy(out, h, KEY_LEN);
}

}  // namespace crypto
