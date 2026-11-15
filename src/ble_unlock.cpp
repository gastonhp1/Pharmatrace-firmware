#include "ble_unlock.h"

#include <NimBLEDevice.h>
#include <string.h>

namespace ble_unlock {
namespace {

NimBLECharacteristic* statusChr = nullptr;
NimBLEServer* server = nullptr;

// Una sola escritura pendiente. Se escribe desde la tarea de BLE y se lee desde loop(): la bandera
// va al final para que loop() nunca vea un buffer a medio copiar.
uint8_t pending[MAX_WRITE];
volatile size_t pendingLen = 0;
volatile bool pendingReady = false;

class UnlockCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
    if (pendingReady) return;  // todavía no se procesó la anterior: se descarta esta
    const NimBLEAttValue v = c->getValue();
    const size_t n = v.size() < MAX_WRITE ? v.size() : MAX_WRITE;
    memcpy(pending, v.data(), n);
    pendingLen = v.size() > MAX_WRITE ? MAX_WRITE + 1 : n;  // largo excesivo = formato inválido
    pendingReady = true;
  }
};

UnlockCallbacks unlockCallbacks;

}  // namespace

void begin(const char* deviceName) {
  NimBLEDevice::init(deviceName);
  server = NimBLEDevice::createServer();
  NimBLEService* service = server->createService(SERVICE_UUID);

  NimBLECharacteristic* unlock = service->createCharacteristic(UNLOCK_UUID, NIMBLE_PROPERTY::WRITE);
  unlock->setCallbacks(&unlockCallbacks);

  statusChr = service->createCharacteristic(STATUS_UUID,
                                            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  const uint8_t none = uint8_t(pharma::UnlockResult::NoChallenge);
  statusChr->setValue(&none, 1);

  service->start();
  // El servicio de 128 bits ocupa casi todo el paquete de anuncio, así que el nombre (el deviceId,
  // hasta 24 caracteres) viaja en la respuesta al escaneo. La app de Pharmatrace busca la caja por
  // ese nombre y, si no lo ve, lo confirma después leyendo el GAP Device Name.
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->setName(deviceName);
  adv->enableScanResponse(true);
}

void startAdvertising() { NimBLEDevice::getAdvertising()->start(); }
void stopAdvertising() { NimBLEDevice::getAdvertising()->stop(); }

void keepAdvertising() {
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  if (server != nullptr && server->getConnectedCount() == 0 && !adv->isAdvertising()) adv->start();
}

bool takeWrite(uint8_t* out, size_t cap, size_t& len) {
  if (!pendingReady) return false;
  len = pendingLen;
  memcpy(out, pending, len < cap ? len : cap);
  pendingReady = false;
  return true;
}

void notifyResult(pharma::UnlockResult r) {
  if (statusChr == nullptr) return;
  const uint8_t code = uint8_t(r);
  statusChr->setValue(&code, 1);
  statusChr->notify();
}

}  // namespace ble_unlock
