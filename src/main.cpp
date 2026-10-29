// Nodo de la PharmaBox: sensa, firma cada lectura con el ATECC608B y la sube al gateway.
//
// Flujo: muestrear -> armar el payload de 21 bytes -> SHA-256 -> firma P-256 -> cola -> POST
// /api/iot/readings. El nodo NO habla con Fabric: el gateway (backend/ de Pharmatrace) verifica
// la firma y ancla una raíz de Merkle por ventana en el ledger.
//
// ESTADO: esqueleto. La lógica del protocolo (lib/pharma_proto) está probada en la PC. Los
// drivers de sensores, el ATECC608B y el módem son TODO, y todo lo de Arduino/ESP32 de este
// archivo todavía NO se compiló ni se probó en hardware.

#include <Arduino.h>
#include <Preferences.h>
#include <Wire.h>

#include <esp_random.h>

#include <atomic>

#include "ble_unlock.h"
#include "config.h"
#include "display.h"
#include "lock.h"
#include "pharma_ingest.h"
#include "pharma_queue.h"
#include "pharma_reading.h"
#include "pharma_unlock.h"
#include "secrets.h"
#include "signer.h"
#include "unlock_key.h"
#include "uplink.h"
#include "verifier_mbedtls.h"

using namespace pharma;

static ReadingQueue<QUEUE_CAPACITY> queue;
static Preferences prefs;
static Atecc608Signer signer;
static UplinkWifi uplink;  // banco de pruebas; el transporte real es el módem (ver uplink.h)

static SerialDisplay display;  // TODO(hw): reemplazar por la e-paper
static Lock lock;
static MbedtlsVerifier unlockVerifier;
static UnlockSession unlockSession(DEVICE_ID, sha256Mbedtls, MbedtlsVerifier::verifyThunk,
                                   &unlockVerifier, UNLOCK_CHALLENGE_TTL_MS, UNLOCK_MAX_ATTEMPTS);
static bool unlockReady = false;  // clave de apertura válida cargada

// El botón solo levanta una bandera; el trabajo se hace en loop().
static volatile bool buttonPressed = false;
static void IRAM_ATTR onButtonInterrupt() { buttonPressed = true; }

// Eventos latcheados por interrupción entre dos muestras: se reportan en la siguiente lectura.
static std::atomic<uint8_t> pendingFlags{0};

static uint32_t lastTs = 0;  // última `ts` emitida: el gateway exige ts estrictamente creciente
static int32_t lastLatE5 = 0;
static int32_t lastLonE5 = 0;
static uint32_t lastSampleMs = 0;
static uint32_t lastUploadMs = 0;

static void IRAM_ATTR onLidInterrupt() { pendingFlags.fetch_or(FLAG_LID_OPENED); }
static void IRAM_ATTR onShockInterrupt() { pendingFlags.fetch_or(FLAG_SHOCK); }

// ---------------------------------------------------------------------------------------------
// TODO(hw): drivers. Cada uno devuelve false si no hay dato válido; en ese caso se saltea la
// muestra en vez de inventar un valor.
// ---------------------------------------------------------------------------------------------

// Hora Unix de un GPS con fix o de un RTC sincronizado. Sin hora válida no se emite nada.
static bool readUnixTime(uint32_t&) { return false; }

// TMP117 principal (0x48). El 2.º (0x49) es para comparar y detectar fallas o manipulación,
// pero el formato v1 tiene un solo campo de temperatura: ver "Pendiente" en el README.
static bool readTemperatureC(float&) { return false; }

// SHT45 (0x44).
static bool readHumidityPercent(float&) { return false; }

// Posición del GPS. Si no hay fix se reutiliza la última conocida (el formato no tiene un bit
// de "fix válido").
static bool readPosition(double&, double&) { return false; }

// Sello roto (NFC/NTAG 424 DNA o microswitch). Es persistente, a diferencia de tapa y golpe.
static bool sealIsBroken() { return false; }

// ---------------------------------------------------------------------------------------------

// Reserva el próximo `seq` ANTES de usarlo y lo persiste: si el equipo se reinicia justo
// después, a lo sumo queda un hueco, pero nunca se repite un `seq` con otro contenido (el
// gateway lo rechazaría como "Sequence number already used").
static uint32_t reserveSeq() {
  const uint32_t seq = prefs.getUInt("seq", 0);
  prefs.putUInt("seq", seq + 1);
  return seq;
}

static bool sha256(const uint8_t* data, size_t len, uint8_t out[DIGEST_SIZE]) {
  return sha256Mbedtls(data, len, out);
}

static void skip(const char* why) { Serial.printf("[sample] %s, se saltea\n", why); }

static void sampleOnce() {
  uint32_t ts = 0;
  float tempC = 0, humPct = 0;
  if (!readUnixTime(ts)) return skip("sin hora válida");
  if (ts <= lastTs) return skip("el reloj no avanzó");

  int16_t tempCenti = 0;
  uint16_t humCenti = 0;
  if (!readTemperatureC(tempC) || !tempToCenti(tempC, tempCenti)) {
    return skip("temperatura inválida");
  }
  if (!readHumidityPercent(humPct) || !humidityToCenti(humPct, humCenti)) {
    return skip("humedad inválida");
  }

  double lat = 0, lon = 0;
  int32_t latE5 = 0, lonE5 = 0;
  if (readPosition(lat, lon) && latitudeToE5(lat, latE5) && longitudeToE5(lon, lonE5)) {
    lastLatE5 = latE5;
    lastLonE5 = lonE5;
  }

  const uint8_t seen = pendingFlags.load();
  uint8_t flags = seen;
  if (sealIsBroken()) flags |= FLAG_SEAL_BROKEN;

  Reading r{ts, 0, tempCenti, humCenti, flags, lastLatE5, lastLonE5};
  r.seq = reserveSeq();

  SignedReading sr;
  encodeReading(r, sr.payload);
  uint8_t digest[DIGEST_SIZE];
  if (!sha256(sr.payload, READING_SIZE, digest) || !signer.signDigest(digest, sr.signature)) {
    return skip("no se pudo firmar (se pierde la lectura)");
  }

  queue.push(sr);
  lastTs = ts;
  pendingFlags.fetch_and(uint8_t(~seen));  // solo limpia lo que esta lectura ya reportó
  Serial.printf("[sample] seq=%lu ts=%lu temp=%d hum=%u flags=0x%02x en cola=%u\n",
                (unsigned long)r.seq, (unsigned long)r.ts, r.tempCenti, r.humCenti, r.flags,
                (unsigned)queue.size());
}

static void note(const char* msg) { Serial.printf("[upload] %s\n", msg); }

static void uploadPending() {
  static SignedReading batch[UPLOAD_BATCH_MAX];
  static char body[ingestJsonSize(sizeof(DEVICE_ID) - 1, UPLOAD_BATCH_MAX)];
  static char response[160];

  const size_t n = queue.peek(batch, UPLOAD_BATCH_MAX);
  const size_t len = buildIngestJson(DEVICE_ID, batch, n, body, sizeof body);
  if (len == 0) return note("no se pudo armar el JSON (revisar DEVICE_ID)");

  int status = 0;
  if (!uplink.postJson(INGEST_PATH, body, len, status, response, sizeof response)) {
    return note("sin conexión, se reintenta más tarde");
  }

  if (status == 200) {
    // accepted, duplicates y rejected son definitivos: reenviar una lectura rechazada la
    // rechazaría otra vez. Un duplicado exacto no es error, así que reintentar es seguro.
    queue.pop(n);
    Serial.printf("[upload] 200, %u lecturas confirmadas, quedan %u. %s\n", (unsigned)n,
                  (unsigned)queue.size(), response);
  } else {
    // 409: la valija no está monitoreando ningún cargamento todavía. 403: no está operativa.
    // 400: error nuestro. En todos los casos las lecturas se conservan.
    Serial.printf("[upload] HTTP %d, se conservan %u lecturas. %s\n", status,
                  (unsigned)queue.size(), response);
  }
}

// ---------------------------------------------------------------------------------------------
// Apertura autenticada (docs/unlock-protocol.md)
// ---------------------------------------------------------------------------------------------

static bool challengeShown = false;

static void endChallenge() {
  unlockSession.end();
  ble_unlock::stopAdvertising();
  challengeShown = false;
}

// Botón -> nonce nuevo -> QR en pantalla -> BLE visible.
static void startChallenge(uint32_t nowMs) {
  if (!unlockReady) return display.showMessage("Apertura no disponible");
  // El nonce sale de esp_fill_random(): es aleatorio de verdad solo con BLE/WiFi encendidos
  // (BLE ya está inicializado en setup()). TODO(hw): usar el TRNG del ATECC608B.
  uint8_t nonce[NONCE_SIZE];
  esp_fill_random(nonce, sizeof nonce);
  char qr[QR_PAYLOAD_MAX];
  if (!unlockSession.begin(nonce, nowMs) || buildQrPayload(DEVICE_ID, nonce, qr, sizeof qr) == 0) {
    return display.showMessage("Error al generar el QR");
  }
  display.showQr(qr);
  ble_unlock::startAdvertising();
  challengeShown = true;
}

static void serviceUnlock(uint32_t nowMs) {
  lock.update(nowMs);

  if (buttonPressed) {
    buttonPressed = false;
    if (!challengeShown) startChallenge(nowMs);  // con un QR vigente, otro toque no hace nada
  }

  // El QR venció sin que nadie lo usara.
  if (challengeShown && !unlockSession.active(nowMs)) {
    endChallenge();
    display.clear();
  }

  uint8_t token[ble_unlock::MAX_WRITE];
  size_t len = 0;
  if (!ble_unlock::takeWrite(token, sizeof token, len)) return;

  uint32_t nowUnix = 0;
  const bool timeValid = readUnixTime(nowUnix);
  const UnlockResult r = unlockSession.handle(token, len, nowMs, timeValid, nowUnix);
  ble_unlock::notifyResult(r);
  Serial.printf("[unlock] resultado=%u\n", (unsigned)r);

  // Cada intento deja huella firmada en la próxima lectura (el ledger la ancla sin interpretarla).
  if (r == UnlockResult::Ok) {
    pendingFlags.fetch_or(FLAG_UNLOCK_GRANTED);
    lock.open(nowMs);
    display.showMessage("Abierta");
    endChallenge();
    lastSampleMs = nowMs - SAMPLE_INTERVAL_S * 1000UL;  // adelanta la muestra: que quede registrado ya
  } else {
    pendingFlags.fetch_or(FLAG_UNLOCK_REJECTED);
    if (r == UnlockResult::LockedOut || r == UnlockResult::NoChallenge) {
      endChallenge();
      display.showMessage("Rechazada");
    }
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  prefs.begin("pharma", false);

  pinMode(PIN_LID_REED, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_LID_REED), onLidInterrupt,
                  LID_OPEN_LEVEL ? RISING : FALLING);
  pinMode(PIN_ACCEL_INT, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_ACCEL_INT), onShockInterrupt, RISING);

  uplink.begin();

  lock.begin();
  display.begin();
  pinMode(PIN_UNLOCK_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_UNLOCK_BUTTON), onButtonInterrupt, FALLING);
  unlockReady = unlockVerifier.begin(UNLOCK_PUBKEY);
  if (!unlockReady) Serial.println("[boot] clave de apertura inválida (include/unlock_key.h): la caja no se puede abrir");
  ble_unlock::begin(DEVICE_ID);

  if (!signer.begin()) {
    Serial.println("[boot] ATECC608B no disponible: no se puede firmar, no se emitirá nada");
    return;
  }
  // La pública es lo que se registra en el ledger: POST /api/iot/devices { deviceId, pubKey, ... }
  uint8_t pub[PUBLIC_KEY_SIZE];
  char hex[2 + 2 * PUBLIC_KEY_SIZE + 1];
  if (signer.publicKey(pub) && toHex(pub, sizeof pub, hex, sizeof hex)) {
    Serial.printf("[boot] deviceId=%s pubKey=%s\n", DEVICE_ID, hex);
  }
}

void loop() {
  const uint32_t nowMs = millis();  // la resta sin signo tolera el desborde de millis()

  serviceUnlock(nowMs);

  if (nowMs - lastSampleMs >= SAMPLE_INTERVAL_S * 1000UL) {
    lastSampleMs = nowMs;
    sampleOnce();
  }
  if (!queue.empty() && nowMs - lastUploadMs >= UPLOAD_INTERVAL_S * 1000UL) {
    lastUploadMs = nowMs;
    uploadPending();
  }
}
