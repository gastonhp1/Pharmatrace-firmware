#include "p256_soft.h"

#include <mbedtls/bignum.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/ecp.h>

namespace pharma {

namespace {

struct Ctx {
  mbedtls_ecp_group grp;
  mbedtls_ecp_point q;
  mbedtls_mpi d, r, s;
  Ctx() {
    mbedtls_ecp_group_init(&grp);
    mbedtls_ecp_point_init(&q);
    mbedtls_mpi_init(&d);
    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);
  }
  ~Ctx() {
    mbedtls_ecp_group_free(&grp);
    mbedtls_ecp_point_free(&q);
    mbedtls_mpi_free(&d);  // mbedtls borra el contenido antes de liberar
    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&s);
  }
  bool load() { return mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1) == 0; }
  bool loadKey(const uint8_t priv[PRIVATE_KEY_SIZE]) {
    return load() && mbedtls_mpi_read_binary(&d, priv, PRIVATE_KEY_SIZE) == 0 &&
           mbedtls_ecp_check_privkey(&grp, &d) == 0;
  }
};

}  // namespace

bool p256Generate(uint8_t priv[PRIVATE_KEY_SIZE], RngFn rng, void* rngCtx) {
  Ctx c;
  if (rng == nullptr || !c.load()) return false;
  if (mbedtls_ecp_gen_privkey(&c.grp, &c.d, rng, rngCtx) != 0) return false;
  return mbedtls_mpi_write_binary(&c.d, priv, PRIVATE_KEY_SIZE) == 0;
}

bool p256PublicKey(const uint8_t priv[PRIVATE_KEY_SIZE], uint8_t pub[PUBLIC_KEY_SIZE], RngFn rng,
                   void* rngCtx) {
  Ctx c;
  if (rng == nullptr || !c.loadKey(priv)) return false;
  if (mbedtls_ecp_mul(&c.grp, &c.q, &c.d, &c.grp.G, rng, rngCtx) != 0) return false;
  uint8_t point[1 + PUBLIC_KEY_SIZE];  // 0x04 || X || Y
  size_t len = 0;
  if (mbedtls_ecp_point_write_binary(&c.grp, &c.q, MBEDTLS_ECP_PF_UNCOMPRESSED, &len, point,
                                     sizeof point) != 0 ||
      len != sizeof point) {
    return false;
  }
  for (size_t i = 0; i < PUBLIC_KEY_SIZE; i++) pub[i] = point[1 + i];
  return true;
}

bool p256Sign(const uint8_t priv[PRIVATE_KEY_SIZE], const uint8_t digest[DIGEST_SIZE],
              uint8_t signature[SIGNATURE_SIZE], RngFn rng, void* rngCtx) {
  Ctx c;
  if (rng == nullptr || !c.loadKey(priv)) return false;
  if (mbedtls_ecdsa_sign(&c.grp, &c.r, &c.s, &c.d, digest, DIGEST_SIZE, rng, rngCtx) != 0) return false;
  return mbedtls_mpi_write_binary(&c.r, signature, 32) == 0 &&
         mbedtls_mpi_write_binary(&c.s, signature + 32, 32) == 0;
}

}  // namespace pharma
