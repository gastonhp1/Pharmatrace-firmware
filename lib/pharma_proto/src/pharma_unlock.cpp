#include "pharma_unlock.h"

#include <string.h>

namespace pharma {

namespace {

uint32_t getU32(const uint8_t* p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

// Comparación en tiempo constante: no filtra por el tiempo cuántos bytes coinciden.
bool equalCt(const uint8_t* a, const uint8_t* b, size_t n) {
  uint8_t diff = 0;
  for (size_t i = 0; i < n; i++) diff |= uint8_t(a[i] ^ b[i]);
  return diff == 0;
}

}  // namespace

bool parseUnlockToken(const uint8_t* buf, size_t len, UnlockToken& out) {
  if (buf == nullptr || len != UNLOCK_TOKEN_SIZE) return false;
  out.version = buf[0];
  out.action = buf[1];
  memcpy(out.dev, buf + 2, DEVICE_TAG_SIZE);
  memcpy(out.nonce, buf + 10, NONCE_SIZE);
  out.iat = getU32(buf + 26);
  out.exp = getU32(buf + 30);
  memcpy(out.actor, buf + 34, ACTOR_SIZE);
  memcpy(out.signature, buf + UNLOCK_BODY_SIZE, SIGNATURE_SIZE);
  return true;
}

size_t base64Url(const uint8_t* data, size_t len, char* out, size_t cap) {
  static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  const size_t need = (len / 3) * 4 + (len % 3 == 0 ? 0 : len % 3 + 1);
  if (out == nullptr || cap < need + 1) return 0;
  size_t o = 0;
  size_t i = 0;
  for (; i + 3 <= len; i += 3) {
    const uint32_t v = (uint32_t(data[i]) << 16) | (uint32_t(data[i + 1]) << 8) | data[i + 2];
    out[o++] = alphabet[(v >> 18) & 63];
    out[o++] = alphabet[(v >> 12) & 63];
    out[o++] = alphabet[(v >> 6) & 63];
    out[o++] = alphabet[v & 63];
  }
  if (len - i == 1) {
    const uint32_t v = uint32_t(data[i]) << 16;
    out[o++] = alphabet[(v >> 18) & 63];
    out[o++] = alphabet[(v >> 12) & 63];
  } else if (len - i == 2) {
    const uint32_t v = (uint32_t(data[i]) << 16) | (uint32_t(data[i + 1]) << 8);
    out[o++] = alphabet[(v >> 18) & 63];
    out[o++] = alphabet[(v >> 12) & 63];
    out[o++] = alphabet[(v >> 6) & 63];
  }
  out[o] = '\0';
  return o;
}

size_t buildQrPayload(const char* deviceId, const uint8_t nonce[NONCE_SIZE], char* out, size_t cap) {
  if (!isValidDeviceId(deviceId) || nonce == nullptr || out == nullptr) return 0;
  const size_t idLen = strlen(deviceId);
  char nonceText[24];
  const size_t nonceLen = base64Url(nonce, NONCE_SIZE, nonceText, sizeof nonceText);
  if (nonceLen == 0) return 0;
  const size_t total = 4 + idLen + 1 + nonceLen;
  if (cap < total + 1) return 0;
  memcpy(out, "PB1:", 4);
  memcpy(out + 4, deviceId, idLen);
  out[4 + idLen] = ':';
  memcpy(out + 5 + idLen, nonceText, nonceLen);
  out[total] = '\0';
  return total;
}

UnlockSession::UnlockSession(const char* deviceId, Sha256Fn sha256, VerifyFn verify,
                             void* verifyCtx, uint32_t challengeTtlMs, uint8_t maxAttempts)
    : deviceId_(deviceId),
      sha256_(sha256),
      verify_(verify),
      verifyCtx_(verifyCtx),
      ttlMs_(challengeTtlMs),
      maxAttempts_(maxAttempts) {}

bool UnlockSession::begin(const uint8_t nonce[NONCE_SIZE], uint32_t nowMs) {
  end();
  if (!isValidDeviceId(deviceId_) || nonce == nullptr) return false;
  uint8_t digest[DIGEST_SIZE];
  if (!sha256_(reinterpret_cast<const uint8_t*>(deviceId_), strlen(deviceId_), digest)) return false;
  memcpy(devTag_, digest, DEVICE_TAG_SIZE);
  memcpy(nonce_, nonce, NONCE_SIZE);
  startMs_ = nowMs;
  attempts_ = 0;
  state_ = State::Active;
  return true;
}

void UnlockSession::end() {
  state_ = State::Idle;
  memset(nonce_, 0, sizeof nonce_);
  attempts_ = 0;
}

bool UnlockSession::active(uint32_t nowMs) const {
  return state_ == State::Active && uint32_t(nowMs - startMs_) < ttlMs_;  // tolera el desborde de millis()
}

UnlockResult UnlockSession::handle(const uint8_t* token, size_t len, uint32_t nowMs,
                                   bool timeValid, uint32_t nowUnix) {
  if (state_ == State::LockedOut) return UnlockResult::LockedOut;
  if (!active(nowMs)) {
    if (state_ == State::Active) end();  // venció por tiempo
    return UnlockResult::NoChallenge;
  }

  const UnlockResult r = check(token, len, timeValid, nowUnix);
  if (r == UnlockResult::Ok) {
    state_ = State::Used;  // un solo uso: el mismo token ya no sirve
    memset(nonce_, 0, sizeof nonce_);
    return r;
  }
  if (++attempts_ >= maxAttempts_) {
    state_ = State::LockedOut;
    memset(nonce_, 0, sizeof nonce_);
  }
  return r;
}

UnlockResult UnlockSession::check(const uint8_t* token, size_t len, bool timeValid,
                                  uint32_t nowUnix) {
  UnlockToken t;
  if (!parseUnlockToken(token, len, t)) return UnlockResult::BadFormat;
  if (t.version != UNLOCK_TOKEN_VERSION || t.action != UNLOCK_ACTION_OPEN) return UnlockResult::BadFormat;
  if (t.exp <= t.iat || t.exp - t.iat > UNLOCK_MAX_LIFETIME_S) return UnlockResult::BadFormat;

  if (!equalCt(t.dev, devTag_, DEVICE_TAG_SIZE)) return UnlockResult::WrongBox;
  if (!equalCt(t.nonce, nonce_, NONCE_SIZE)) return UnlockResult::BadNonce;

  uint8_t digest[DIGEST_SIZE];
  if (!sha256_(token, UNLOCK_BODY_SIZE, digest)) return UnlockResult::BadSignature;
  if (!verify_(verifyCtx_, digest, t.signature)) return UnlockResult::BadSignature;

  // Con hora válida también se mira `exp`. Se hace después de la firma: un vencimiento solo
  // importa si el token es auténtico.
  if (timeValid && t.exp < nowUnix) return UnlockResult::Expired;

  memcpy(actor_, t.actor, ACTOR_SIZE);
  return UnlockResult::Ok;
}

}  // namespace pharma
