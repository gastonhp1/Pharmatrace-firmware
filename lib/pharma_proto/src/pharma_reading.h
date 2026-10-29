#pragma once

// Formato de lectura v1 de Pharmatrace: 21 bytes, big-endian.
// Contrato con el gateway y el simulador: iot/docs/READING_FORMAT.md en gastonhp1/Pharmatrace.
// Si cambia ahí, cambia acá. El vector de prueba está en test/test_proto.
//
// Sin dependencias de Arduino: compila y se prueba en la PC.

#include <stddef.h>
#include <stdint.h>

namespace pharma {

constexpr size_t READING_SIZE = 21;
constexpr size_t SIGNATURE_SIZE = 64;   // r || s crudos, no DER
constexpr size_t PUBLIC_KEY_SIZE = 64;  // X || Y, sin el prefijo 0x04
constexpr size_t DIGEST_SIZE = 32;      // SHA-256

constexpr uint8_t FLAG_LID_OPENED = 0x01;
constexpr uint8_t FLAG_SHOCK = 0x02;
constexpr uint8_t FLAG_SEAL_BROKEN = 0x04;
// PROPUESTOS por la apertura autenticada (docs/unlock-protocol.md). Todavía no figuran en
// READING_FORMAT.md de Pharmatrace: el gateway los guarda firmados pero no los interpreta.
constexpr uint8_t FLAG_UNLOCK_GRANTED = 0x08;   // la caja aceptó un token y abrió la cerradura
constexpr uint8_t FLAG_UNLOCK_REJECTED = 0x10;  // la caja recibió un token inválido (posible ataque)

// El deviceId viaja en el JSON de ingesta y en el QR: letras, números, guion y guion bajo.
constexpr size_t MAX_DEVICE_ID_LEN = 24;
bool isValidDeviceId(const char* id);

struct Reading {
  uint32_t ts;        // segundos Unix (GPS o RTC sincronizado)
  uint32_t seq;       // estrictamente creciente, sobrevive a reinicios
  int16_t tempCenti;  // centésimas de °C: 4.50 °C = 450
  uint16_t humCenti;  // centésimas de % HR
  uint8_t flags;
  int32_t latE5;      // latitud * 1e5
  int32_t lonE5;      // longitud * 1e5
};

// Lo que se sube: el payload y la firma ECDSA P-256 de SHA-256(payload).
struct SignedReading {
  uint8_t payload[READING_SIZE];
  uint8_t signature[SIGNATURE_SIZE];
};

void encodeReading(const Reading& r, uint8_t out[READING_SIZE]);
bool decodeReading(const uint8_t* buf, size_t len, Reading& out);

// Conversiones de unidades físicas. Devuelven false si el valor no es válido (NaN, fuera de
// rango): en ese caso no hay que inventar una lectura, se saltea la muestra.
bool tempToCenti(float celsius, int16_t& out);
bool humidityToCenti(float percent, uint16_t& out);  // satura a 0..100 %
bool latitudeToE5(double degrees, int32_t& out);
bool longitudeToE5(double degrees, int32_t& out);

// Escribe "0x" + hex en minúsculas + NUL. Devuelve los caracteres escritos sin el NUL, o 0 si
// `cap` no alcanza.
size_t toHex(const uint8_t* data, size_t len, char* out, size_t cap);

}  // namespace pharma
