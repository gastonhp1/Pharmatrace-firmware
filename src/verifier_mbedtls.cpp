#include "verifier_mbedtls.h"

#include <mbedtls/ecdsa.h>
#include <mbedtls/ecp.h>
#include <mbedtls/sha256.h>
#include <mbedtls/version.h>

namespace pharma {

struct MbedtlsVerifier::Impl {
  mbedtls_ecp_group grp;
  mbedtls_ecp_point q;
  mbedtls_mpi r, s;
  Impl() {
    mbedtls_ecp_group_init(&grp);
    mbedtls_ecp_point_init(&q);
    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);
  }
  ~Impl() {
    mbedtls_ecp_group_free(&grp);
    mbedtls_ecp_point_free(&q);
    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&s);
  }
};

MbedtlsVerifier::~MbedtlsVerifier() { delete impl_; }

bool MbedtlsVerifier::begin(const uint8_t publicKeyXY[PUBLIC_KEY_SIZE]) {
  delete impl_;
  impl_ = new Impl();
  if (mbedtls_ecp_group_load(&impl_->grp, MBEDTLS_ECP_DP_SECP256R1) != 0) goto fail;
  {
    // Formato sin comprimir: 0x04 || X || Y
    uint8_t point[1 + PUBLIC_KEY_SIZE];
    point[0] = 0x04;
    for (size_t i = 0; i < PUBLIC_KEY_SIZE; i++) point[1 + i] = publicKeyXY[i];
    if (mbedtls_ecp_point_read_binary(&impl_->grp, &impl_->q, point, sizeof point) != 0) goto fail;
    if (mbedtls_ecp_check_pubkey(&impl_->grp, &impl_->q) != 0) goto fail;
  }
  return true;
fail:
  delete impl_;
  impl_ = nullptr;
  return false;
}

bool MbedtlsVerifier::verify(const uint8_t digest[DIGEST_SIZE],
                             const uint8_t signature[SIGNATURE_SIZE]) {
  if (impl_ == nullptr) return false;
  if (mbedtls_mpi_read_binary(&impl_->r, signature, 32) != 0) return false;
  if (mbedtls_mpi_read_binary(&impl_->s, signature + 32, 32) != 0) return false;
  return mbedtls_ecdsa_verify(&impl_->grp, digest, DIGEST_SIZE, &impl_->q, &impl_->r,
                              &impl_->s) == 0;
}

bool MbedtlsVerifier::verifyThunk(void* ctx, const uint8_t digest[DIGEST_SIZE],
                                  const uint8_t signature[SIGNATURE_SIZE]) {
  return ctx != nullptr && static_cast<MbedtlsVerifier*>(ctx)->verify(digest, signature);
}

bool sha256Mbedtls(const uint8_t* data, size_t len, uint8_t out[DIGEST_SIZE]) {
  if (data == nullptr || out == nullptr) return false;
#if MBEDTLS_VERSION_NUMBER >= 0x03000000
  return mbedtls_sha256(data, len, out, 0) == 0;      // 3.x: la variante sin _ret devuelve int
#else
  return mbedtls_sha256_ret(data, len, out, 0) == 0;  // 2.x: la sin _ret está deprecada
#endif
}

}  // namespace pharma
