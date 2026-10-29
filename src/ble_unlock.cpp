#include "ble_unlock.h"

#include <NimBLEDevice.h>
#include <string.h>

namespace ble_unlock {
namespace {

NimBLECharacteristic* statusChr = nullptr;

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
  NimBLEServer* server = NimBLEDevice::createServer();
  NimBLEService* service = server->createService(SERVICE_UUID);

  NimBLECharacteristic* unlock = service->createCharacteristic(UNLOCK_UUID, NIMBLE_PROPERTY::WRITE);
  unlock->setCallbacks(&unlockCallbacks);

  statusChr = service->createCharacteristic(STATUS_UUID,
                                            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  const uint8_t none = uint8_t(pharma::UnlockResult::NoChallenge);
  statusChr->setValue(&none, 1);

  service->start();
  NimBLEDevice::getAdvertising()->addServiceUUID(SERVICE_UUID);
}

void startAdvertising() { NimBLEDevice::getAdvertising()->start(); }
void stopAdvertising() { NimBLEDevice::getAdvertising()->stop(); }

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
