#pragma once
// Verifica la firma ECDSA P-256 del token de apertura con mbedtls (viene con el core del ESP32).
// Sin dependencias de Arduino: se prueba en la PC contra mbedtls del sistema.

#include <stdint.h>

#include "pharma_reading.h"

namespace pharma {

class MbedtlsVerifier {
 public:
  MbedtlsVerifier() = default;
  ~MbedtlsVerifier();
  MbedtlsVerifier(const MbedtlsVerifier&) = delete;
  MbedtlsVerifier& operator=(const MbedtlsVerifier&) = delete;

  // Clave pública del backend, X || Y (64 bytes). Falla si el punto no está en la curva.
  bool begin(const uint8_t publicKeyXY[PUBLIC_KEY_SIZE]);

  // Firma cruda r || s (64 bytes) sobre el digest SHA-256 de 32 bytes.
  bool verify(const uint8_t digest[DIGEST_SIZE], const uint8_t signature[SIGNATURE_SIZE]);

  // Adaptador para UnlockSession (ctx = MbedtlsVerifier*).
  static bool verifyThunk(void* ctx, const uint8_t digest[DIGEST_SIZE],
                          const uint8_t signature[SIGNATURE_SIZE]);

 private:
  struct Impl;
  Impl* impl_ = nullptr;
};

// SHA-256 de un buffer, con la firma que pide UnlockSession.
bool sha256Mbedtls(const uint8_t* data, size_t len, uint8_t out[DIGEST_SIZE]);

}  // namespace pharma
