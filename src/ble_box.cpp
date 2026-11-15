#include "ble_box.h"

#include <NimBLEDevice.h>
#include <string.h>

namespace ble_box {
namespace {

NimBLEServer* server = nullptr;
NimBLECharacteristic* unlockStatusChr = nullptr;
NimBLECharacteristic* boxStatusChr = nullptr;
NimBLECharacteristic* readingsChr = nullptr;

// Buzones de una sola posición. Se escriben desde la tarea de BLE y se leen desde loop(): la
// bandera va al final para que loop() nunca vea un buffer a medio copiar.
uint8_t unlockBuf[MAX_WRITE];
volatile size_t unlockLen = 0;
volatile bool unlockReady = false;

uint8_t timePosBuf[TIMEPOS_SIZE];
volatile bool timePosReady = false;

volatile uint16_t ackCount = 0;
volatile bool ackReady = false;

volatile bool linked = false;
volatile bool linkLost = false;
volatile uint16_t linkedHandle = 0xFFFF;

uint32_t getU32(const uint8_t* p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

class ServerCallbacks : public NimBLEServerCallbacks {
  void onAuthenticationComplete(NimBLEConnInfo& info) override {
    if (info.isEncrypted() && info.isAuthenticated()) {
      linkedHandle = info.getConnHandle();
      linked = true;
    }
  }
  void onDisconnect(NimBLEServer*, NimBLEConnInfo& info, int) override {
    if (linked && info.getConnHandle() == linkedHandle) {
      linked = false;
      linkLost = true;
      linkedHandle = 0xFFFF;
    }
  }
};

class UnlockCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
    if (unlockReady) return;  // todavía no se procesó la anterior: se descarta esta
    const NimBLEAttValue v = c->getValue();
    const size_t n = v.size() < MAX_WRITE ? v.size() : MAX_WRITE;
    memcpy(unlockBuf, v.data(), n);
    unlockLen = v.size() > MAX_WRITE ? MAX_WRITE + 1 : n;  // largo excesivo = formato inválido
    unlockReady = true;
  }
};

class TimePosCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
    const NimBLEAttValue v = c->getValue();
    if (v.size() != TIMEPOS_SIZE || timePosReady) return;
    memcpy(timePosBuf, v.data(), TIMEPOS_SIZE);
    timePosReady = true;
  }
};

class AckCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
    const NimBLEAttValue v = c->getValue();
    if (v.size() != 2 || ackReady) return;
    ackCount = uint16_t((v.data()[0] << 8) | v.data()[1]);
    ackReady = true;
  }
};

ServerCallbacks serverCallbacks;
UnlockCallbacks unlockCallbacks;
TimePosCallbacks timePosCallbacks;
AckCallbacks ackCallbacks;

}  // namespace

void begin(const char* deviceName, uint32_t pairingPin) {
  NimBLEDevice::init(deviceName);
  // Vinculación con PIN: LE Secure Connections, con protección contra intermediarios. La caja
  // "muestra" el PIN (pantalla) y el celular lo tipea.
  NimBLEDevice::setSecurityAuth(true, true, true);
  NimBLEDevice::setSecurityPasskey(pairingPin);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);

  server = NimBLEDevice::createServer();
  server->setCallbacks(&serverCallbacks);

  NimBLEService* unlockSvc = server->createService(UNLOCK_SERVICE_UUID);
  NimBLECharacteristic* unlock = unlockSvc->createCharacteristic(UNLOCK_UUID, NIMBLE_PROPERTY::WRITE);
  unlock->setCallbacks(&unlockCallbacks);
  unlockStatusChr = unlockSvc->createCharacteristic(UNLOCK_STATUS_UUID,
                                                    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  const uint8_t none = uint8_t(pharma::UnlockResult::NoChallenge);
  unlockStatusChr->setValue(&none, 1);
  unlockSvc->start();

  // Escrituras del acompañante: solo con el enlace cifrado y autenticado. Si no, cualquiera en
  // alcance podría cambiarle la hora a la caja o hacerle descartar lecturas sin subir.
  const uint32_t securedWrite =
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::WRITE_AUTHEN;
  NimBLEService* companion = server->createService(COMPANION_SERVICE_UUID);
  companion->createCharacteristic(TIMEPOS_UUID, securedWrite)->setCallbacks(&timePosCallbacks);
  companion->createCharacteristic(ACK_UUID, securedWrite)->setCallbacks(&ackCallbacks);
  boxStatusChr = companion->createCharacteristic(BOX_STATUS_UUID,
                                                 NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  readingsChr = companion->createCharacteristic(READINGS_UUID, NIMBLE_PROPERTY::NOTIFY);
  companion->start();

  // El servicio de 128 bits ocupa casi todo el paquete de anuncio, así que el nombre (el deviceId,
  // hasta 24 caracteres) viaja en la respuesta al escaneo. La app busca la caja por ese nombre y,
  // si no lo ve, lo confirma leyendo el GAP Device Name.
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(UNLOCK_SERVICE_UUID);
  adv->setName(deviceName);
  adv->enableScanResponse(true);
  adv->start();
}

void keepAdvertising() {
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  if (!adv->isAdvertising()) adv->start();
}

bool takeUnlockWrite(uint8_t* out, size_t cap, size_t& len) {
  if (!unlockReady) return false;
  len = unlockLen;
  memcpy(out, unlockBuf, len < cap ? len : cap);
  unlockReady = false;
  return true;
}

void notifyUnlockResult(pharma::UnlockResult r) {
  if (unlockStatusChr == nullptr) return;
  const uint8_t code = uint8_t(r);
  unlockStatusChr->setValue(&code, 1);
  unlockStatusChr->notify();
}

bool phoneLinked() { return linked; }

bool takeLinkLost() {
  if (!linkLost) return false;
  linkLost = false;
  return true;
}

bool takeTimePos(uint32_t& unixTime, int32_t& latE5, int32_t& lonE5) {
  if (!timePosReady) return false;
  unixTime = getU32(timePosBuf);
  latE5 = int32_t(getU32(timePosBuf + 4));
  lonE5 = int32_t(getU32(timePosBuf + 8));
  timePosReady = false;
  return true;
}

bool takeAck(uint16_t& count) {
  if (!ackReady) return false;
  count = ackCount;
  ackReady = false;
  return true;
}

void setBoxStatus(const uint8_t status[BOX_STATUS_SIZE]) {
  if (boxStatusChr == nullptr) return;
  boxStatusChr->setValue(status, BOX_STATUS_SIZE);
  if (linked) boxStatusChr->notify();
}

bool sendReading(const uint8_t frame[READING_FRAME]) {
  if (readingsChr == nullptr || !linked) return false;
  readingsChr->setValue(frame, READING_FRAME);
  return readingsChr->notify();
}

}  // namespace ble_box
