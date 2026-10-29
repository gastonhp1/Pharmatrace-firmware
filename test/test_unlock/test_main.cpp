// Tests de la apertura autenticada en la PC: pio test -e native
// SHA-256 y verificación van falsos para controlar cada caso; la compatibilidad real con las
// firmas del backend se prueba aparte (tools/ + test_e2e_unlock).
#include <string.h>
#include <unity.h>

#include "pharma_unlock.h"

using namespace pharma;

void setUp() {}
void tearDown() {}

// "SHA-256" de mentira: determinístico y sensible a cada byte, suficiente para distinguir cajas.
static bool fakeSha(const uint8_t* data, size_t len, uint8_t out[DIGEST_SIZE]) {
  for (int lane = 0; lane < 4; lane++) {
    uint64_t h = 1469598103934665603ULL + uint64_t(lane) * 0x9e3779b97f4a7c15ULL;
    for (size_t i = 0; i < len; i++) {
      h ^= data[i];
      h *= 1099511628211ULL;
    }
    for (int b = 0; b < 8; b++) out[lane * 8 + b] = uint8_t(h >> (8 * b));
  }
  return true;
}

struct FakeVerifier {
  bool ok = true;
  int calls = 0;
};
static bool fakeVerify(void* ctx, const uint8_t[DIGEST_SIZE], const uint8_t[SIGNATURE_SIZE]) {
  FakeVerifier* v = static_cast<FakeVerifier*>(ctx);
  v->calls++;
  return v->ok;
}

static const uint8_t kNonce[NONCE_SIZE] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
static const uint8_t kOtherNonce[NONCE_SIZE] = {9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9};
static const uint8_t kActor[ACTOR_SIZE] = {0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8};

static void putU32(uint8_t* p, uint32_t v) {
  p[0] = uint8_t(v >> 24);
  p[1] = uint8_t(v >> 16);
  p[2] = uint8_t(v >> 8);
  p[3] = uint8_t(v);
}

static void makeToken(uint8_t out[UNLOCK_TOKEN_SIZE], const char* deviceId, const uint8_t* nonce,
                      uint32_t iat, uint32_t exp) {
  memset(out, 0, UNLOCK_TOKEN_SIZE);
  out[0] = UNLOCK_TOKEN_VERSION;
  out[1] = UNLOCK_ACTION_OPEN;
  uint8_t digest[DIGEST_SIZE];
  fakeSha(reinterpret_cast<const uint8_t*>(deviceId), strlen(deviceId), digest);
  memcpy(out + 2, digest, DEVICE_TAG_SIZE);
  memcpy(out + 10, nonce, NONCE_SIZE);
  putU32(out + 26, iat);
  putU32(out + 30, exp);
  memcpy(out + 34, kActor, ACTOR_SIZE);
  memset(out + UNLOCK_BODY_SIZE, 0x5a, SIGNATURE_SIZE);
}

static const uint32_t kNow = 1800000000u;

static void test_valid_token_opens_once() {
  FakeVerifier v;
  UnlockSession s("VAL-001", fakeSha, fakeVerify, &v);
  TEST_ASSERT_TRUE(s.begin(kNonce, 1000));
  uint8_t tok[UNLOCK_TOKEN_SIZE];
  makeToken(tok, "VAL-001", kNonce, kNow, kNow + 60);
  TEST_ASSERT_EQUAL_UINT8(UnlockResult::Ok, s.handle(tok, sizeof tok, 2000, true, kNow + 5));
  TEST_ASSERT_EQUAL_UINT8(0xa1, s.actor()[0]);
  TEST_ASSERT_EQUAL_UINT8(0xa8, s.actor()[7]);
  // Un solo uso: el mismo token ya no abre.
  TEST_ASSERT_EQUAL_UINT8(UnlockResult::NoChallenge, s.handle(tok, sizeof tok, 3000, true, kNow + 6));
}

static void test_token_for_another_box_is_rejected_before_verifying() {
  FakeVerifier v;
  UnlockSession s("VAL-001", fakeSha, fakeVerify, &v);
  s.begin(kNonce, 0);
  uint8_t tok[UNLOCK_TOKEN_SIZE];
  makeToken(tok, "VAL-002", kNonce, kNow, kNow + 60);
  TEST_ASSERT_EQUAL_UINT8(UnlockResult::WrongBox, s.handle(tok, sizeof tok, 10, true, kNow));
  TEST_ASSERT_EQUAL_INT(0, v.calls);  // los chequeos baratos van primero
}

static void test_wrong_nonce_is_rejected_before_verifying() {
  FakeVerifier v;
  UnlockSession s("VAL-001", fakeSha, fakeVerify, &v);
  s.begin(kNonce, 0);
  uint8_t tok[UNLOCK_TOKEN_SIZE];
  makeToken(tok, "VAL-001", kOtherNonce, kNow, kNow + 60);
  TEST_ASSERT_EQUAL_UINT8(UnlockResult::BadNonce, s.handle(tok, sizeof tok, 10, true, kNow));
  TEST_ASSERT_EQUAL_INT(0, v.calls);
}

static void test_bad_signature() {
  FakeVerifier v;
  v.ok = false;
  UnlockSession s("VAL-001", fakeSha, fakeVerify, &v);
  s.begin(kNonce, 0);
  uint8_t tok[UNLOCK_TOKEN_SIZE];
  makeToken(tok, "VAL-001", kNonce, kNow, kNow + 60);
  TEST_ASSERT_EQUAL_UINT8(UnlockResult::BadSignature, s.handle(tok, sizeof tok, 10, true, kNow));
  TEST_ASSERT_EQUAL_INT(1, v.calls);
}

static void test_expiry_only_matters_with_a_valid_clock() {
  FakeVerifier v;
  UnlockSession s("VAL-001", fakeSha, fakeVerify, &v);
  uint8_t tok[UNLOCK_TOKEN_SIZE];
  makeToken(tok, "VAL-001", kNonce, kNow, kNow + 60);

  s.begin(kNonce, 0);
  TEST_ASSERT_EQUAL_UINT8(UnlockResult::Expired, s.handle(tok, sizeof tok, 10, true, kNow + 61));

  s.begin(kNonce, 0);  // sin hora válida no se mira exp: la frescura la da el nonce
  TEST_ASSERT_EQUAL_UINT8(UnlockResult::Ok, s.handle(tok, sizeof tok, 10, false, 0));
}

static void test_token_lifetime_is_capped_by_the_box() {
  FakeVerifier v;
  UnlockSession s("VAL-001", fakeSha, fakeVerify, &v);
  uint8_t tok[UNLOCK_TOKEN_SIZE];

  s.begin(kNonce, 0);
  makeToken(tok, "VAL-001", kNonce, kNow, kNow + UNLOCK_MAX_LIFETIME_S + 1);
  TEST_ASSERT_EQUAL_UINT8(UnlockResult::BadFormat, s.handle(tok, sizeof tok, 10, true, kNow));

  s.begin(kNonce, 0);
  makeToken(tok, "VAL-001", kNonce, kNow, kNow);  // exp == iat
  TEST_ASSERT_EQUAL_UINT8(UnlockResult::BadFormat, s.handle(tok, sizeof tok, 10, true, kNow));

  s.begin(kNonce, 0);
  makeToken(tok, "VAL-001", kNonce, kNow, kNow + UNLOCK_MAX_LIFETIME_S);  // justo en el límite
  TEST_ASSERT_EQUAL_UINT8(UnlockResult::Ok, s.handle(tok, sizeof tok, 10, true, kNow));
}

static void test_bad_layout_version_and_action() {
  FakeVerifier v;
  UnlockSession s("VAL-001", fakeSha, fakeVerify, &v, 120000, /*maxAttempts*/ 10);
  s.begin(kNonce, 0);
  uint8_t tok[UNLOCK_TOKEN_SIZE];
  makeToken(tok, "VAL-001", kNonce, kNow, kNow + 60);

  TEST_ASSERT_EQUAL_UINT8(UnlockResult::BadFormat, s.handle(tok, sizeof tok - 1, 10, true, kNow));
  tok[0] = 2;
  TEST_ASSERT_EQUAL_UINT8(UnlockResult::BadFormat, s.handle(tok, sizeof tok, 10, true, kNow));
  tok[0] = UNLOCK_TOKEN_VERSION;
  tok[1] = 9;
  TEST_ASSERT_EQUAL_UINT8(UnlockResult::BadFormat, s.handle(tok, sizeof tok, 10, true, kNow));
  TEST_ASSERT_EQUAL_UINT8(UnlockResult::BadFormat, s.handle(nullptr, UNLOCK_TOKEN_SIZE, 10, true, kNow));
}

static void test_lockout_after_three_failures_even_for_a_valid_token() {
  FakeVerifier v;
  UnlockSession s("VAL-001", fakeSha, fakeVerify, &v);
  s.begin(kNonce, 0);
  uint8_t bad[UNLOCK_TOKEN_SIZE];
  makeToken(bad, "VAL-001", kOtherNonce, kNow, kNow + 60);
  TEST_ASSERT_EQUAL_UINT8(UnlockResult::BadNonce, s.handle(bad, sizeof bad, 10, true, kNow));
  TEST_ASSERT_EQUAL_UINT8(UnlockResult::BadNonce, s.handle(bad, sizeof bad, 20, true, kNow));
  TEST_ASSERT_EQUAL_UINT8(UnlockResult::BadNonce, s.handle(bad, sizeof bad, 30, true, kNow));

  uint8_t good[UNLOCK_TOKEN_SIZE];
  makeToken(good, "VAL-001", kNonce, kNow, kNow + 60);
  TEST_ASSERT_EQUAL_UINT8(UnlockResult::LockedOut, s.handle(good, sizeof good, 40, true, kNow));

  // Un QR nuevo (botón) levanta el bloqueo.
  s.begin(kOtherNonce, 50);
  uint8_t fresh[UNLOCK_TOKEN_SIZE];
  makeToken(fresh, "VAL-001", kOtherNonce, kNow, kNow + 60);
  TEST_ASSERT_EQUAL_UINT8(UnlockResult::Ok, s.handle(fresh, sizeof fresh, 60, true, kNow));
}

static void test_challenge_expires_and_new_one_invalidates_the_old() {
  FakeVerifier v;
  UnlockSession s("VAL-001", fakeSha, fakeVerify, &v, /*ttl*/ 1000);
  uint8_t tok[UNLOCK_TOKEN_SIZE];
  makeToken(tok, "VAL-001", kNonce, kNow, kNow + 60);

  s.begin(kNonce, 5000);
  TEST_ASSERT_TRUE(s.active(5999));
  TEST_ASSERT_FALSE(s.active(6000));
  TEST_ASSERT_EQUAL_UINT8(UnlockResult::NoChallenge, s.handle(tok, sizeof tok, 6000, true, kNow));

  s.begin(kNonce, 7000);
  s.begin(kOtherNonce, 7100);  // un QR nuevo anula el anterior
  TEST_ASSERT_EQUAL_UINT8(UnlockResult::BadNonce, s.handle(tok, sizeof tok, 7200, true, kNow));
}

static void test_challenge_ttl_survives_millis_wraparound() {
  FakeVerifier v;
  UnlockSession s("VAL-001", fakeSha, fakeVerify, &v, 1000);
  s.begin(kNonce, 0xFFFFFF00u);
  TEST_ASSERT_TRUE(s.active(0x00000100u));   // pasaron 512 ms, cruzando el desborde
  TEST_ASSERT_TRUE(s.active(0x000002E0u));   // 992 ms: todavía vigente
  TEST_ASSERT_FALSE(s.active(0x00000300u));  // 1024 ms: venció
}

static void test_no_session_without_begin() {
  FakeVerifier v;
  UnlockSession s("VAL-001", fakeSha, fakeVerify, &v);
  uint8_t tok[UNLOCK_TOKEN_SIZE];
  makeToken(tok, "VAL-001", kNonce, kNow, kNow + 60);
  TEST_ASSERT_EQUAL_UINT8(UnlockResult::NoChallenge, s.handle(tok, sizeof tok, 0, true, kNow));
}

static void test_begin_rejects_invalid_device_id() {
  FakeVerifier v;
  UnlockSession s("VAL 001", fakeSha, fakeVerify, &v);
  TEST_ASSERT_FALSE(s.begin(kNonce, 0));
}

static void test_base64url_matches_the_reference() {
  char out[32];
  // base64.urlsafe_b64encode(bytes(range(16))).rstrip(b"=") en Python
  TEST_ASSERT_EQUAL_UINT32(22, base64Url(kNonce, NONCE_SIZE, out, sizeof out));
  TEST_ASSERT_EQUAL_STRING("AAECAwQFBgcICQoLDA0ODw", out);
  const uint8_t one[1] = {0xff};
  const uint8_t two[2] = {0xfb, 0xff};
  TEST_ASSERT_EQUAL_UINT32(2, base64Url(one, 1, out, sizeof out));
  TEST_ASSERT_EQUAL_STRING("_w", out);
  TEST_ASSERT_EQUAL_UINT32(3, base64Url(two, 2, out, sizeof out));
  TEST_ASSERT_EQUAL_STRING("-_8", out);
  TEST_ASSERT_EQUAL_UINT32(0, base64Url(kNonce, NONCE_SIZE, out, 22));  // sin lugar para el NUL
}

static void test_qr_payload() {
  char out[QR_PAYLOAD_MAX];
  TEST_ASSERT_EQUAL_UINT32(34, buildQrPayload("VAL-001", kNonce, out, sizeof out));
  TEST_ASSERT_EQUAL_STRING("PB1:VAL-001:AAECAwQFBgcICQoLDA0ODw", out);
  // El peor caso entra en el buffer declarado
  TEST_ASSERT_EQUAL_UINT32(QR_PAYLOAD_MAX - 1,
                           buildQrPayload("ABCDEFGHIJKLMNOPQRSTUVWX", kNonce, out, sizeof out));
  TEST_ASSERT_EQUAL_UINT32(0, buildQrPayload("ABCDEFGHIJKLMNOPQRSTUVWXY", kNonce, out, sizeof out));  // 25
  TEST_ASSERT_EQUAL_UINT32(0, buildQrPayload("VAL:001", kNonce, out, sizeof out));
  TEST_ASSERT_EQUAL_UINT32(0, buildQrPayload("VAL-001", kNonce, out, 10));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_valid_token_opens_once);
  RUN_TEST(test_token_for_another_box_is_rejected_before_verifying);
  RUN_TEST(test_wrong_nonce_is_rejected_before_verifying);
  RUN_TEST(test_bad_signature);
  RUN_TEST(test_expiry_only_matters_with_a_valid_clock);
  RUN_TEST(test_token_lifetime_is_capped_by_the_box);
  RUN_TEST(test_bad_layout_version_and_action);
  RUN_TEST(test_lockout_after_three_failures_even_for_a_valid_token);
  RUN_TEST(test_challenge_expires_and_new_one_invalidates_the_old);
  RUN_TEST(test_challenge_ttl_survives_millis_wraparound);
  RUN_TEST(test_no_session_without_begin);
  RUN_TEST(test_begin_rejects_invalid_device_id);
  RUN_TEST(test_base64url_matches_the_reference);
  RUN_TEST(test_qr_payload);
  return UNITY_END();
}
