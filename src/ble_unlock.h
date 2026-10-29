#pragma once
// Servicio BLE de apertura (NimBLE-Arduino 2.x). Dos características:
//   unlock  (escritura)        el celular escribe acá el token de 106 bytes
//   status  (lectura/notify)   la caja devuelve 1 byte: UnlockResult
// El callback de BLE solo copia el token; la verificación criptográfica se hace en el loop().
#include <stddef.h>
#include <stdint.h>

#include "pharma_unlock.h"

namespace ble_unlock {

// UUIDs fijos del servicio PharmaBox.
constexpr const char* SERVICE_UUID = "f30e1410-8085-4552-9d03-abe1d447ce2d";
constexpr const char* UNLOCK_UUID = "e7cd3dce-1611-4770-beb3-8d88cb840550";
constexpr const char* STATUS_UUID = "71b4af6b-3c8f-4880-96e3-df64ba0b4459";

constexpr size_t MAX_WRITE = 128;  // más grande que el token: así un largo incorrecto se detecta

void begin(const char* deviceName);
void startAdvertising();
void stopAdvertising();

// Devuelve true si llegó una escritura desde la última llamada y la copia en `out`.
bool takeWrite(uint8_t* out, size_t cap, size_t& len);

// Publica el resultado en `status` y notifica al celular.
void notifyResult(pharma::UnlockResult r);

}  // namespace ble_unlock
