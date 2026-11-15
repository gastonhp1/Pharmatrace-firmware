// Comprobación en la PC del firmador por software (src/p256_soft.cpp) contra mbedtls del sistema.
//   host_crypto_check gen            -> imprime "priv pub digest sig" en hex (para verificar afuera)
//   host_crypto_check verify <pub> <digest> <sig>   -> 0 si la firma es válida (MbedtlsVerifier)
#include <stdio.h>
#include <string.h>

#include "p256_soft.h"
#include "verifier_mbedtls.h"

using namespace pharma;

static int urandom(void*, unsigned char* out, size_t len) {
  FILE* f = fopen("/dev/urandom", "rb");
  if (!f) return -1;
  const size_t n = fread(out, 1, len, f);
  fclose(f);
  return n == len ? 0 : -1;
}

static void hex(const uint8_t* d, size_t n) { for (size_t i = 0; i < n; i++) printf("%02x", d[i]); }
static bool unhex(const char* h, uint8_t* out, size_t n) {
  if (strlen(h) != 2 * n) return false;
  for (size_t i = 0; i < n; i++) { unsigned v; if (sscanf(h + 2 * i, "%2x", &v) != 1) return false; out[i] = uint8_t(v); }
  return true;
}

int main(int argc, char** argv) {
  if (argc == 2 && !strcmp(argv[1], "gen")) {
    uint8_t priv[32], pub[64], digest[32], sig[64], sig2[64];
    const uint8_t msg[] = "PharmaBox";
    if (!p256Generate(priv, urandom, nullptr) || !p256PublicKey(priv, pub, urandom, nullptr)) return 1;
    if (!sha256Mbedtls(msg, sizeof msg - 1, digest)) return 1;
    if (!p256Sign(priv, digest, sig, urandom, nullptr) || !p256Sign(priv, digest, sig2, urandom, nullptr)) return 1;
    MbedtlsVerifier v;
    if (!v.begin(pub) || !v.verify(digest, sig) || !v.verify(digest, sig2)) return 2;  // ida y vuelta
    sig2[10] ^= 1;
    if (v.verify(digest, sig2)) return 3;  // una firma alterada no pasa
    uint8_t zero[32] = {0};
    if (p256PublicKey(zero, pub + 0, urandom, nullptr)) return 4;  // clave inválida
    if (!p256PublicKey(priv, pub, urandom, nullptr)) return 1;
    hex(priv, 32); printf(" "); hex(pub, 64); printf(" "); hex(digest, 32); printf(" "); hex(sig, 64); printf("\n");
    return 0;
  }
  if (argc == 5 && !strcmp(argv[1], "verify")) {
    uint8_t pub[64], digest[32], sig[64];
    if (!unhex(argv[2], pub, 64) || !unhex(argv[3], digest, 32) || !unhex(argv[4], sig, 64)) return 2;
    MbedtlsVerifier v;
    return v.begin(pub) && v.verify(digest, sig) ? 0 : 1;
  }
  return 2;
}
