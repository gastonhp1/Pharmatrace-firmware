// Arma en la PC lo mismo que arma la caja: lecturas de 21 bytes firmadas por software y el JSON de
// POST /api/iot/readings. Imprime "<pubKey hex>\n<json>\n". Se usa para pasarlo por el código real
// del gateway (backend/iot/ingest.js de Pharmatrace). Uso: host_reading_e2e <ts inicial> <cantidad>
#include <stdio.h>
#include <stdlib.h>

#include "p256_soft.h"
#include "pharma_ingest.h"
#include "pharma_reading.h"
#include "verifier_mbedtls.h"

using namespace pharma;

static int urandom(void*, unsigned char* out, size_t len) {
  FILE* f = fopen("/dev/urandom", "rb");
  if (!f) return -1;
  const size_t n = fread(out, 1, len, f);
  fclose(f);
  return n == len ? 0 : -1;
}

int main(int argc, char** argv) {
  if (argc != 3) return 2;
  const uint32_t ts0 = uint32_t(strtoul(argv[1], nullptr, 10));
  const size_t count = size_t(atoi(argv[2]));
  if (count < 1 || count > 50) return 2;

  uint8_t priv[PRIVATE_KEY_SIZE], pub[PUBLIC_KEY_SIZE];
  if (!p256Generate(priv, urandom, nullptr) || !p256PublicKey(priv, pub, urandom, nullptr)) return 1;

  static SignedReading items[50];
  for (size_t i = 0; i < count; i++) {
    // Sin GPS: posición 0,0 hasta que el celular mande una. La 3.ª lectura lleva precinto violado
    // y apertura rechazada; la 5.ª, apertura otorgada.
    const uint8_t flags = i == 2 ? uint8_t(FLAG_SEAL_BROKEN | FLAG_UNLOCK_REJECTED) : i == 4 ? FLAG_UNLOCK_GRANTED : 0;
    const Reading r{ts0 + uint32_t(60 * i), uint32_t(i), int16_t(450 + int(i)), 5500, flags,
                    i < 3 ? 0 : -3460000, i < 3 ? 0 : -5840000};
    encodeReading(r, items[i].payload);
    uint8_t digest[DIGEST_SIZE];
    if (!sha256Mbedtls(items[i].payload, READING_SIZE, digest)) return 1;
    if (!p256Sign(priv, digest, items[i].signature, urandom, nullptr)) return 1;
  }

  static char body[ingestJsonSize(MAX_DEVICE_ID_LEN, 50)];
  if (buildIngestJson("VAL-001", items, count, body, sizeof body) == 0) return 1;
  char hex[2 + 2 * PUBLIC_KEY_SIZE + 1];
  toHex(pub, sizeof pub, hex, sizeof hex);
  printf("%s\n%s\n", hex, body);
  return 0;
}
