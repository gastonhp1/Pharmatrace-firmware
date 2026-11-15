#pragma once
// BLE de la PharmaBox (NimBLE-Arduino 2.x). SIN PROBAR EN HARDWARE.
//
// Servicio de apertura (el que usa la app PharmaTrace-mobile; no pide vinculación, el token ya
// viene firmado por el backend):
//   unlock   escritura           el token de 106 bytes
//   status   lectura + notify    1 byte: UnlockResult
//
// Servicio de acompañante (celular vinculado con el PIN de 6 dígitos que muestra la pantalla).
// Todavía no lo implementa ninguna app; el formato está en docs/companion-protocol.md:
//   timepos  escritura cifrada   12 bytes: hora Unix u32, latE5 i32, lonE5 i32 (big-endian)
//   box      lectura + notify    8 bytes: estado de la caja (ver packBoxStatus en main.cpp)
//   readings notify              85 bytes por lectura firmada: payload (21) + firma (64)
//   ack      escritura cifrada   2 bytes u16: cuántas lecturas subió bien el celular (0 = "mandame")
//
// Los callbacks de BLE solo copian datos; el trabajo se hace en loop().
#include <stddef.h>
#include <stdint.h>

#include "pharma_unlock.h"

namespace ble_box {

constexpr const char* UNLOCK_SERVICE_UUID = "f30e1410-8085-4552-9d03-abe1d447ce2d";
constexpr const char* UNLOCK_UUID = "e7cd3dce-1611-4770-beb3-8d88cb840550";
constexpr const char* UNLOCK_STATUS_UUID = "71b4af6b-3c8f-4880-96e3-df64ba0b4459";

constexpr const char* COMPANION_SERVICE_UUID = "a95b3c98-0c98-4d45-82dc-43b5b9a9572c";
constexpr const char* TIMEPOS_UUID = "f49a4666-5431-48dd-bddb-82d30d63e1e7";
constexpr const char* BOX_STATUS_UUID = "394de1fc-6ded-4431-8ddb-6b33131144e7";
constexpr const char* READINGS_UUID = "6d76aa5c-cd56-457b-b26b-ddd8870bfab7";
constexpr const char* ACK_UUID = "eeb7ddfb-3589-471b-94f1-89f30bd1f400";

constexpr size_t MAX_WRITE = 128;  // más grande que el token: así un largo incorrecto se detecta
constexpr size_t TIMEPOS_SIZE = 12;
constexpr size_t BOX_STATUS_SIZE = 8;
constexpr size_t READING_FRAME = pharma::READING_SIZE + pharma::SIGNATURE_SIZE;  // 85

// `pairingPin`: 6 dígitos que la caja muestra en pantalla y el celular tiene que tipear.
void begin(const char* deviceName, uint32_t pairingPin);

// La caja se anuncia siempre (el acompañante tiene que poder reconectarse). NimBLE deja de anunciar
// al conectarse un cliente: llamar en cada vuelta del loop() para retomar.
void keepAdvertising();

// --- apertura ---
bool takeUnlockWrite(uint8_t* out, size_t cap, size_t& len);
void notifyUnlockResult(pharma::UnlockResult r);

// --- acompañante ---
bool phoneLinked();    // hay un celular conectado, cifrado y autenticado
bool takeLinkLost();   // true una vez por cada desconexión del celular vinculado
bool takeTimePos(uint32_t& unixTime, int32_t& latE5, int32_t& lonE5);
bool takeAck(uint16_t& count);
void setBoxStatus(const uint8_t status[BOX_STATUS_SIZE]);
bool sendReading(const uint8_t frame[READING_FRAME]);

}  // namespace ble_box
