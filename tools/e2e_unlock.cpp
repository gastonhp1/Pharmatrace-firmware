// Prueba de punta a punta en la PC: lee un token firmado por tools/unlock_token.py y lo valida con
// UnlockSession + el verificador mbedtls real. Uso: e2e <pubkey_hex> <qr> <token_hex> [esperado]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pharma_unlock.h"
#include "verifier_mbedtls.h"

static size_t unhex(const char* h, uint8_t* out, size_t cap) {
  size_t n = strlen(h) / 2;
  if (n > cap) return 0;
  for (size_t i = 0; i < n; i++) {
    unsigned v;
    sscanf(h + 2 * i, "%2x", &v);
    out[i] = uint8_t(v);
  }
  return n;
}

int main(int argc, char** argv) {
  if (argc < 4) return 2;
  uint8_t pub[64], tok[256];
  if (unhex(argv[1], pub, sizeof pub) != 64) return 2;
  size_t tl = unhex(argv[3], tok, sizeof tok);
  const int expected = argc > 4 ? atoi(argv[4]) : 0;

  // El QR es PB1:<id>:<nonce b64url>; se parsea igual que lo haría el celular.
  char qr[80];
  strncpy(qr, argv[2], sizeof qr - 1);
  qr[sizeof qr - 1] = 0;
  char* id = strchr(qr, ':') + 1;
  char* nc = strchr(id, ':');
  *nc++ = 0;
  // nonce = base64url inverso mínimo
  uint8_t nonce[16];
  {
    int val = 0, bits = 0;
    size_t o = 0;
    for (const char* c = nc; *c && o < 16; c++) {
      int d = (*c >= 'A' && *c <= 'Z') ? *c - 'A' : (*c >= 'a' && *c <= 'z') ? *c - 'a' + 26
            : (*c >= '0' && *c <= '9') ? *c - '0' + 52 : *c == '-' ? 62 : 63;
      val = (val << 6) | d;
      bits += 6;
      if (bits >= 8) { bits -= 8; nonce[o++] = uint8_t((val >> bits) & 0xFF); }
    }
  }

  pharma::MbedtlsVerifier v;
  if (!v.begin(pub)) { puts("begin falló"); return 3; }
  pharma::UnlockSession s(id, pharma::sha256Mbedtls, pharma::MbedtlsVerifier::verifyThunk, &v);
  s.begin(nonce, 1000);
  const auto r = s.handle(tok, tl, 2000, false, 0);
  printf("resultado=%d esperado=%d\n", int(r), expected);
  return int(r) == expected ? 0 : 1;
}
