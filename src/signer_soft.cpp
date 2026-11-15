#include <Arduino.h>
#include <Preferences.h>
#include <esp_random.h>

#include "signer.h"

using namespace pharma;

namespace {

int espRng(void*, unsigned char* out, size_t len) {
  esp_fill_random(out, len);
  return 0;
}

}  // namespace

bool Signer::begin() {
  Preferences keys;
  if (!keys.begin("pharma-key", false)) return false;
  uint8_t pub[PUBLIC_KEY_SIZE];
  const bool stored = keys.getBytes("p256", priv_, sizeof priv_) == sizeof priv_ &&
                      p256PublicKey(priv_, pub, espRng, nullptr);
  if (!stored) {
    // Primer arranque (o NVS borrada): clave nueva. La pública cambia, hay que registrarla de nuevo.
    if (!p256Generate(priv_, espRng, nullptr) ||
        keys.putBytes("p256", priv_, sizeof priv_) != sizeof priv_) {
      keys.end();
      return false;
    }
    Serial.println("[signer] clave nueva generada: registrar la pública en el ledger");
  }
  keys.end();
  ready_ = true;
  return true;
}

bool Signer::publicKey(uint8_t out[PUBLIC_KEY_SIZE]) {
  return ready_ && p256PublicKey(priv_, out, espRng, nullptr);
}

bool Signer::signDigest(const uint8_t digest[DIGEST_SIZE], uint8_t signature[SIGNATURE_SIZE]) {
  return ready_ && p256Sign(priv_, digest, signature, espRng, nullptr);
}
