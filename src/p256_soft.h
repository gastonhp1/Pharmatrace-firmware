#pragma once
// ECDSA P-256 por software con mbedtls (viene con el core del ESP32). La PharmaBox ya no lleva un
// elemento seguro: la clave privada vive en la flash del micro. Sin dependencias de Arduino: se
// prueba en la PC.
#include <stddef.h>
#include <stdint.h>

#include "pharma_reading.h"

namespace pharma {

constexpr size_t PRIVATE_KEY_SIZE = 32;

// Generador aleatorio con la firma que pide mbedtls (devuelve 0 si salió bien).
using RngFn = int (*)(void* ctx, unsigned char* out, size_t len);

bool p256Generate(uint8_t priv[PRIVATE_KEY_SIZE], RngFn rng, void* rngCtx);

// Clave pública X || Y (64 bytes): la que se registra en el ledger.
bool p256PublicKey(const uint8_t priv[PRIVATE_KEY_SIZE], uint8_t pub[PUBLIC_KEY_SIZE], RngFn rng,
                   void* rngCtx);

// Firma cruda r || s (64 bytes) del digest SHA-256, como la espera el gateway.
bool p256Sign(const uint8_t priv[PRIVATE_KEY_SIZE], const uint8_t digest[DIGEST_SIZE],
              uint8_t signature[SIGNATURE_SIZE], RngFn rng, void* rngCtx);

}  // namespace pharma
