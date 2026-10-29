#pragma once

// Apertura autenticada de la PharmaBox, opción A: la caja muestra un QR con un nonce, el celular
// le pide un token firmado al backend y se lo pasa por BLE. Protocolo completo en
// docs/unlock-protocol.md. Sin dependencias de Arduino: se prueba en la PC.
//
// Token (106 bytes, big-endian):
//   off  tam  campo
//     0    1  version   = 1
//     1    1  action    = 1 (abrir)
//     2    8  dev       primeros 8 bytes de SHA-256(deviceId): ata el token a UNA caja
//    10   16  nonce     el que la caja mostró en el QR
//    26    4  iat       emitido (segundos Unix)
//    30    4  exp       vence (segundos Unix)
//    34    8  actor     primeros 8 bytes de SHA-256(mspId ":" usuario): para auditar, sin datos personales
//    42   64  firma     ECDSA P-256 de SHA-256(bytes 0..41), cruda r || s, con la clave de apertura del backend

#include <stddef.h>
#include <stdint.h>

#include "pharma_reading.h"

namespace pharma {

constexpr uint8_t UNLOCK_TOKEN_VERSION = 1;
constexpr uint8_t UNLOCK_ACTION_OPEN = 1;
constexpr size_t NONCE_SIZE = 16;
constexpr size_t DEVICE_TAG_SIZE = 8;
constexpr size_t ACTOR_SIZE = 8;
constexpr size_t UNLOCK_BODY_SIZE = 42;
constexpr size_t UNLOCK_TOKEN_SIZE = UNLOCK_BODY_SIZE + SIGNATURE_SIZE;  // 106

// Vida máxima que la caja acepta de un token, aunque el backend firme uno más largo.
constexpr uint32_t UNLOCK_MAX_LIFETIME_S = 300;

// Códigos que la caja devuelve por BLE (característica `status`).
enum class UnlockResult : uint8_t {
  Ok = 0,
  NoChallenge = 1,   // no hay QR vigente (nunca se pidió, venció, o ya se usó)
  BadFormat = 2,     // largo, versión o acción inválidos, o vida del token fuera de rango
  WrongBox = 3,      // el token es de otra caja
  BadNonce = 4,      // no es el nonce que está en pantalla
  Expired = 5,       // el token ya venció según el reloj de la caja
  BadSignature = 6,  // no la firmó el backend
  LockedOut = 7,     // demasiados intentos fallidos: hay que pedir un QR nuevo
};

struct UnlockToken {
  uint8_t version;
  uint8_t action;
  uint8_t dev[DEVICE_TAG_SIZE];
  uint8_t nonce[NONCE_SIZE];
  uint32_t iat;
  uint32_t exp;
  uint8_t actor[ACTOR_SIZE];
  uint8_t signature[SIGNATURE_SIZE];
};

// Solo el layout: no verifica nada criptográfico. Falla si `len` no es UNLOCK_TOKEN_SIZE.
bool parseUnlockToken(const uint8_t* buf, size_t len, UnlockToken& out);

// base64url sin relleno. Devuelve los caracteres escritos sin el NUL, o 0 si `cap` no alcanza.
size_t base64Url(const uint8_t* data, size_t len, char* out, size_t cap);

// Texto del QR: "PB1:<deviceId>:<nonce en base64url>" (51 caracteres como máximo).
constexpr size_t QR_PAYLOAD_MAX = 4 + MAX_DEVICE_ID_LEN + 1 + 22 + 1;
size_t buildQrPayload(const char* deviceId, const uint8_t nonce[NONCE_SIZE], char* out, size_t cap);

// SHA-256 y verificación de la firma las pone quien usa la librería (mbedtls en el ESP32).
using Sha256Fn = bool (*)(const uint8_t* data, size_t len, uint8_t out[DIGEST_SIZE]);
using VerifyFn = bool (*)(void* ctx, const uint8_t digest[DIGEST_SIZE],
                          const uint8_t signature[SIGNATURE_SIZE]);

// Un desafío a la vez. begin() arma uno nuevo (y anula el anterior); el nonce es de un solo uso.
class UnlockSession {
 public:
  UnlockSession(const char* deviceId, Sha256Fn sha256, VerifyFn verify, void* verifyCtx,
                uint32_t challengeTtlMs = 120000, uint8_t maxAttempts = 3);

  // `nonce` tiene que venir de un generador aleatorio de verdad (TRNG del ATECC608B).
  bool begin(const uint8_t nonce[NONCE_SIZE], uint32_t nowMs);
  void end();
  bool active(uint32_t nowMs) const;

  // `timeValid` dice si `nowUnix` viene de un GPS/RTC sincronizado. Sin hora válida no se mira
  // `exp`: la frescura la da el nonce, que la caja generó hace instantes y nadie puede adivinar.
  UnlockResult handle(const uint8_t* token, size_t len, uint32_t nowMs, bool timeValid,
                      uint32_t nowUnix);

  const uint8_t* nonce() const { return nonce_; }
  const uint8_t* actor() const { return actor_; }  // actor del último desbloqueo exitoso

 private:
  enum class State : uint8_t { Idle, Active, Used, LockedOut };

  UnlockResult check(const uint8_t* token, size_t len, bool timeValid, uint32_t nowUnix);

  const char* deviceId_;
  Sha256Fn sha256_;
  VerifyFn verify_;
  void* verifyCtx_;
  uint32_t ttlMs_;
  uint8_t maxAttempts_;

  State state_ = State::Idle;
  uint32_t startMs_ = 0;
  uint8_t attempts_ = 0;
  uint8_t devTag_[DEVICE_TAG_SIZE] = {0};
  uint8_t nonce_[NONCE_SIZE] = {0};
  uint8_t actor_[ACTOR_SIZE] = {0};
};

}  // namespace pharma
