#include "signer.h"

// Esqueleto: ver el comentario de Atecc608Signer en signer.h para lo que falta.
// Falla a propósito en vez de devolver una firma falsa: sin el chip no se firma nada.

bool Atecc608Signer::begin() { return false; }

bool Atecc608Signer::publicKey(uint8_t[pharma::PUBLIC_KEY_SIZE]) { return false; }

bool Atecc608Signer::signDigest(const uint8_t[pharma::DIGEST_SIZE], uint8_t[pharma::SIGNATURE_SIZE]) {
  return false;
}
