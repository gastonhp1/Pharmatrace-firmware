#pragma once

#include <stdint.h>

#include "pharma_reading.h"

// Firma de lecturas. La clave privada P-256 vive en el ATECC608B y nunca sale del chip: el
// firmware calcula SHA-256(payload) y le pide al chip que firme ese digest.
class Signer {
 public:
  virtual ~Signer() = default;

  virtual bool begin() = 0;

  // Clave pública X || Y (64 bytes). Es la que se registra en el ledger con
  // POST /api/iot/devices { deviceId, pubKey, calibrationExpiry }.
  virtual bool publicKey(uint8_t out[pharma::PUBLIC_KEY_SIZE]) = 0;

  // Firma ECDSA P-256 de `digest` (SHA-256 del payload), como r || s crudos, no DER.
  virtual bool signDigest(const uint8_t digest[pharma::DIGEST_SIZE],
                          uint8_t signature[pharma::SIGNATURE_SIZE]) = 0;
};

// ATECC608B por I2C (0x60). TODAVÍA NO IMPLEMENTADO: begin() devuelve false.
//
// Lo que falta, en orden (ver "Aprovisionamiento seguro" en iot/hardware/BOM.md):
//   1. Elegir el driver (ArduinoECCX08 o cryptoauthlib) y descomentarlo en platformio.ini.
//   2. Aprovisionamiento una sola vez: configurar el chip, generar la clave P-256 en el slot
//      de firma, leer solo la pública y bloquear la configuración y los datos.
//   3. begin(): inicializar I2C 0x60 y comprobar que el chip está bloqueado.
//   4. publicKey() y signDigest() con las llamadas del driver (firma cruda r || s).
//   5. Secure boot y flash encryption del ESP32-S3 antes de entregar la valija.
class Atecc608Signer : public Signer {
 public:
  bool begin() override;
  bool publicKey(uint8_t out[pharma::PUBLIC_KEY_SIZE]) override;
  bool signDigest(const uint8_t digest[pharma::DIGEST_SIZE],
                  uint8_t signature[pharma::SIGNATURE_SIZE]) override;
};
