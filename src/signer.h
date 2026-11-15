#pragma once

#include <stdint.h>

#include "p256_soft.h"
#include "pharma_reading.h"

// Firma de lecturas con ECDSA P-256 por software (mbedtls). La clave se genera en el primer
// arranque y se guarda en la NVS del ESP32.
//
// Qué se resigna respecto de un elemento seguro (ATECC608B): quien tenga la placa en la mano puede
// leer la flash y sacar la clave, y con ella fabricar lecturas válidas. Para acotarlo hay que
// activar flash encryption y secure boot del ESP32-S3 antes de entregar una caja. Está bien para
// prototipo y demo; un producto regulado debería volver a un elemento seguro.
class Signer {
 public:
  // Carga la clave de la NVS o la genera. Requiere la radio encendida (BLE iniciado) para que el
  // generador aleatorio del ESP32 sea de verdad aleatorio.
  bool begin();

  // Clave pública X || Y (64 bytes). Es la que se registra en el ledger con
  // POST /api/iot/devices { deviceId, pubKey, calibrationExpiry }.
  bool publicKey(uint8_t out[pharma::PUBLIC_KEY_SIZE]);

  // Firma ECDSA P-256 de `digest` (SHA-256 del payload), como r || s crudos, no DER.
  bool signDigest(const uint8_t digest[pharma::DIGEST_SIZE], uint8_t signature[pharma::SIGNATURE_SIZE]);

 private:
  bool ready_ = false;
  uint8_t priv_[pharma::PRIVATE_KEY_SIZE] = {0};
};
