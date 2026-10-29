#pragma once

// Cuerpo de POST /api/iot/readings:
//   { "deviceId": "VAL-001", "readings": [ { "payload": "0x...", "signature": "0x..." }, ... ] }
//
// El gateway acepta hasta 200 lecturas por request. Responde 200 con
// { cargoId, accepted, duplicates, rejected: [{ index, reason }] }; un duplicado exacto no es
// error, así que reintentar un lote es seguro.

#include <stddef.h>

#include "pharma_reading.h"

namespace pharma {

constexpr size_t MAX_READINGS_PER_REQUEST = 200;

// Bytes necesarios para el cuerpo (con el NUL final). constexpr: sirve para dimensionar un
// buffer estático. 13 = `{"deviceId":"`, 14 = `","readings":[`, 2 = `]}`, y 203 bytes por
// lectura (payload 44 + firma 130 + el resto del objeto); pharma_ingest.cpp lo verifica.
constexpr size_t ingestJsonSize(size_t deviceIdLen, size_t count) {
  return 13 + deviceIdLen + 14 + count * 203 + (count > 0 ? count - 1 : 0) + 2 + 1;
}

// Escribe el JSON en `out` y devuelve su largo sin el NUL. Devuelve 0 si `count` no está entre 1
// y MAX_READINGS_PER_REQUEST, si `deviceId` está vacío o tiene algo que no sea [A-Za-z0-9_-], o
// si `cap` no alcanza.
size_t buildIngestJson(const char* deviceId, const SignedReading* items, size_t count, char* out,
                       size_t cap);

}  // namespace pharma
