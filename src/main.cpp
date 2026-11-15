// PharmaBox v2: módulo sobre la tapa de la caja, sujeto por un cinto que hace de precinto.
//
//   sonda interna -> temperatura, humedad y luz        cinto -> lazo + presión (precinto)
//   cada minuto: lectura de 21 bytes -> SHA-256 -> firma P-256 por software -> cola
//   el celular vinculado por BLE pone hora y posición, y sube las lecturas al gateway
//   abrir = token firmado por el backend (QR + BLE): habilita romper el precinto sin que cuente
//           como violación
//
// La caja NO habla con Fabric ni con internet: el gateway (backend/ de Pharmatrace) verifica la
// firma de cada lectura y ancla una raíz de Merkle por ventana en el ledger.
//
// ESTADO: la lógica (lib/pharma_proto, p256_soft, verifier_mbedtls) está probada en la PC. Todo lo
// de Arduino/ESP32 de este archivo y de sensors/ble_box/display NO se compiló con las librerías
// reales ni se probó en hardware.

#include <Arduino.h>
#include <Preferences.h>
#include <esp_random.h>

#include <atomic>

#include "ble_box.h"
#include "config.h"
#include "display.h"
#include "pharma_clock.h"
#include "pharma_ingest.h"
#include "pharma_queue.h"
#include "pharma_reading.h"
#include "pharma_seal.h"
#include "pharma_unlock.h"
#include "secrets.h"
#include "sensors.h"
#include "signer.h"
#include "unlock_key.h"
#include "verifier_mbedtls.h"

#ifdef BENCH_WIFI_UPLINK
#include <time.h>

#include "uplink.h"
static UplinkWifi uplink;  // solo banco de pruebas: sube directo a un gateway de la LAN
static uint32_t lastUploadMs = 0;
#endif

using namespace pharma;

static ReadingQueue<QUEUE_CAPACITY> queue;
static Preferences prefs;
static Signer signer;
static bool signerReady = false;
static BoxClock boxClock;
static SealConfig sealConfig() {
  SealConfig c;
  if (!SEAL_USE_FSR) c.fsrMinMv = 0;  // sin sensor de presión: cualquier valor sirve
  return c;
}
static SealMonitor seal(sealConfig());
static Display& display = boxDisplay();

static MbedtlsVerifier unlockVerifier;
static UnlockSession unlockSession(DEVICE_ID, sha256Mbedtls, MbedtlsVerifier::verifyThunk,
                                   &unlockVerifier, UNLOCK_CHALLENGE_TTL_MS, UNLOCK_MAX_ATTEMPTS);
static bool unlockReady = false;  // clave de apertura válida cargada
static bool challengeShown = false;
static uint32_t pairingPin = 0;

// Las interrupciones solo levantan banderas; el trabajo se hace en loop().
static volatile bool buttonPressed = false;
static volatile bool shockSeen = false;
static std::atomic<uint8_t> pendingFlags{0};  // eventos a reportar en la próxima lectura
static void IRAM_ATTR onButtonInterrupt() { buttonPressed = true; }
static void IRAM_ATTR onShockInterrupt() {
  pendingFlags.fetch_or(FLAG_SHOCK);
  shockSeen = true;
}

static uint32_t lastTs = 0;  // última `ts` emitida: el gateway exige ts estrictamente creciente
static int32_t lastLatE5 = 0;  // última posición que mandó el celular (0,0 si nunca mandó)
static int32_t lastLonE5 = 0;
static uint32_t lastSampleMs = 0;
static uint32_t lastSealPollMs = 0;
static bool sampleNow = false;  // adelantar la muestra: algo importante acaba de pasar

static bool haveReading = false;  // última medición, para la pantalla y el celular
static int16_t lastTempCenti = 0;
static uint16_t lastHumCenti = 0;

static bool displayOn = false;
static uint32_t displayOnMs = 0;
static size_t relayInFlight = 0;  // lecturas mandadas al celular esperando su confirmación

// ---------------------------------------------------------------------------------------------
// Avisos
// ---------------------------------------------------------------------------------------------

static uint8_t beepsLeft = 0;
static bool beepOn = false;
static uint32_t beepMs = 0;

static void beep(uint8_t times) { beepsLeft = times; }

static void serviceBuzzer(uint32_t nowMs) {
  if (!beepOn && beepsLeft == 0) return;
  if (uint32_t(nowMs - beepMs) < 150) return;
  beepMs = nowMs;
  beepOn = !beepOn;
  digitalWrite(PIN_BUZZER, beepOn ? HIGH : LOW);
  if (!beepOn) beepsLeft--;
}

static StatusView statusView() {
  StatusView v;
  v.haveReading = haveReading;
  v.tempCenti = lastTempCenti;
  v.humCenti = lastHumCenti;
  v.seal = seal.state();
  v.batteryPercent = sensors::batteryPercent();
  v.queued = uint16_t(queue.size());
  v.clockValid = boxClock.valid();
  v.phoneLinked = ble_box::phoneLinked();
  v.pairingPin = pairingPin;
  return v;
}

static void show(const char* message, uint32_t nowMs) {
  display.showMessage(message, statusView());
  displayOn = true;
  displayOnMs = nowMs;
}

// Estado para el celular: tempCenti i16, humCenti u16, estado u8, batería u8, en cola u16.
// estado: bits 0-1 precinto (0 precintada, 1 abierta autorizada, 2 violada), bit 2 hora válida,
// bit 3 hay medición.
static void publishBoxStatus() {
  uint8_t s[ble_box::BOX_STATUS_SIZE];
  const uint16_t queued = uint16_t(queue.size());
  s[0] = uint8_t(uint16_t(lastTempCenti) >> 8);
  s[1] = uint8_t(lastTempCenti);
  s[2] = uint8_t(lastHumCenti >> 8);
  s[3] = uint8_t(lastHumCenti);
  s[4] = uint8_t(uint8_t(seal.state()) | (boxClock.valid() ? 0x04 : 0) | (haveReading ? 0x08 : 0));
  s[5] = sensors::batteryPercent();
  s[6] = uint8_t(queued >> 8);
  s[7] = uint8_t(queued);
  ble_box::setBoxStatus(s);
}

// ---------------------------------------------------------------------------------------------
// Lecturas
// ---------------------------------------------------------------------------------------------

// Reserva el próximo `seq` ANTES de usarlo y lo persiste: si el equipo se reinicia justo
// después, a lo sumo queda un hueco, pero nunca se repite un `seq` con otro contenido (el
// gateway lo rechazaría como "Sequence number already used").
static uint32_t reserveSeq() {
  const uint32_t seq = prefs.getUInt("seq", 0);
  prefs.putUInt("seq", seq + 1);
  return seq;
}

static void skip(const char* why) { Serial.printf("[sample] %s, se saltea\n", why); }

static void sampleOnce(uint32_t nowMs) {
  float tempC = 0, humPct = 0;
  int16_t tempCenti = 0;
  uint16_t humCenti = 0;
  // Sin sonda no hay temperatura que firmar, y el formato v1 no tiene cómo decir "sin dato": no
  // se inventa un valor. La violación no se pierde: el precinto queda en "violada" y el flag sale
  // en la primera lectura que se pueda emitir.
  if (!sensors::readTempHumidity(tempC, humPct)) {
    haveReading = false;
    return skip("la sonda no responde");
  }
  if (!tempToCenti(tempC, tempCenti) || !humidityToCenti(humPct, humCenti)) {
    return skip("medición fuera de rango");
  }
  haveReading = true;
  lastTempCenti = tempCenti;
  lastHumCenti = humCenti;

  if (!signerReady) return skip("sin clave de firma");
  uint32_t ts = 0;
  if (!boxClock.now(nowMs, ts)) return skip("sin hora: abrir con la app o vincular el celular");
  if (ts <= lastTs) return skip("el reloj no avanzó");

  const uint8_t seen = pendingFlags.load();
  Reading r{ts, 0, tempCenti, humCenti, uint8_t(seen | seal.stateFlags()), lastLatE5, lastLonE5};
  r.seq = reserveSeq();

  SignedReading sr;
  encodeReading(r, sr.payload);
  uint8_t digest[DIGEST_SIZE];
  if (!sha256Mbedtls(sr.payload, READING_SIZE, digest) || !signer.signDigest(digest, sr.signature)) {
    return skip("no se pudo firmar (se pierde la lectura)");
  }

  queue.push(sr);
  lastTs = ts;
  pendingFlags.fetch_and(uint8_t(~seen));  // solo limpia lo que esta lectura ya reportó
  Serial.printf("[sample] seq=%lu ts=%lu temp=%d hum=%u flags=0x%02x en cola=%u\n",
                (unsigned long)r.seq, (unsigned long)r.ts, r.tempCenti, r.humCenti, r.flags,
                (unsigned)queue.size());
}

// ---------------------------------------------------------------------------------------------
// Precinto
// ---------------------------------------------------------------------------------------------

static void serviceSeal(uint32_t nowMs) {
  if (uint32_t(nowMs - lastSealPollMs) < SEAL_POLL_MS) return;
  lastSealPollMs = nowMs;

  SealInputs in{};
  in.loopMv = sensors::loopMv();
  in.fsrMv = sensors::fsrMv();
  in.probePresent = sensors::probePresent();
  uint16_t lux = 0;
  in.lux = (in.probePresent && sensors::readLux(lux)) ? lux : 0;

#ifdef SEAL_DEBUG
  // Para calibrar los umbrales de SealConfig: una línea por segundo con lo que ve el precinto.
  static uint32_t lastDebugMs = 0;
  if (uint32_t(nowMs - lastDebugMs) >= 1000) {
    lastDebugMs = nowMs;
    Serial.printf("[seal-debug] lazo=%u mV presion=%u mV sonda=%d lux=%u estado=%u\n", in.loopMv,
                  in.fsrMv, in.probePresent, in.lux, (unsigned)seal.state());
  }
#endif

  const SealState before = seal.state();
  const uint8_t events = seal.update(in, nowMs);
  if (events != 0) pendingFlags.fetch_or(events);
  if (seal.state() == before && events == 0) return;

  sampleNow = true;  // que el cambio quede firmado cuanto antes
  if (seal.state() == SealState::Breached && before != SealState::Breached) {
    Serial.printf("[seal] VIOLADO lazo=%u mV presion=%u mV sonda=%d lux=%u\n", in.loopMv, in.fsrMv,
                  in.probePresent, in.lux);
    beep(5);
    show("PRECINTO VIOLADO", nowMs);
  } else if (seal.state() == SealState::Sealed && before == SealState::AuthorizedOpen) {
    beep(1);
    show("Precintada", nowMs);
  }
}

// ---------------------------------------------------------------------------------------------
// Apertura autenticada (docs/unlock-protocol.md)
// ---------------------------------------------------------------------------------------------

static void endChallenge() {
  unlockSession.end();
  challengeShown = false;
}

// Botón -> nonce nuevo -> QR en pantalla.
static void startChallenge(uint32_t nowMs) {
  if (!unlockReady) return show("Apertura no disponible", nowMs);
  // esp_fill_random() es aleatorio de verdad con la radio encendida (BLE se inicia en setup()).
  uint8_t nonce[NONCE_SIZE];
  esp_fill_random(nonce, sizeof nonce);
  char qr[QR_PAYLOAD_MAX];
  if (!unlockSession.begin(nonce, nowMs) || buildQrPayload(DEVICE_ID, nonce, qr, sizeof qr) == 0) {
    return show("Error al generar el QR", nowMs);
  }
  display.showQr(qr, statusView());
  displayOn = true;
  displayOnMs = nowMs;
  challengeShown = true;
}

static void serviceUnlock(uint32_t nowMs) {
  if (buttonPressed) {
    buttonPressed = false;
    if (!challengeShown) startChallenge(nowMs);  // con un QR vigente, otro toque no hace nada
  }

  // El QR venció sin que nadie lo usara.
  if (challengeShown && !unlockSession.active(nowMs)) {
    endChallenge();
    show("QR vencido", nowMs);
  }

  uint8_t token[ble_box::MAX_WRITE];
  size_t len = 0;
  if (!ble_box::takeUnlockWrite(token, sizeof token, len)) return;

  uint32_t nowUnix = 0;
  const bool timeValid = boxClock.now(nowMs, nowUnix);
  const UnlockResult r = unlockSession.handle(token, len, nowMs, timeValid, nowUnix);
  ble_box::notifyUnlockResult(r);
  Serial.printf("[unlock] resultado=%u\n", (unsigned)r);

  // Cada intento deja huella firmada en la próxima lectura.
  if (r == UnlockResult::Ok) {
    pendingFlags.fetch_or(FLAG_UNLOCK_GRANTED);
    seal.authorize(nowMs);  // ahora romper el precinto no es una violación
    // El `iat` del token lo firmó el backend: es la fuente de hora más confiable que tiene la caja.
    boxClock.set(unlockSession.issuedAt(), nowMs);
    endChallenge();
    beep(2);
    show("Podes abrir", nowMs);
    sampleNow = true;
  } else {
    pendingFlags.fetch_or(FLAG_UNLOCK_REJECTED);
    if (r == UnlockResult::LockedOut || r == UnlockResult::NoChallenge) {
      endChallenge();
      show("Rechazada", nowMs);
    }
  }
}

// ---------------------------------------------------------------------------------------------
// Celular vinculado: hora, posición y relevo de lecturas (docs/companion-protocol.md)
// ---------------------------------------------------------------------------------------------

static void serviceCompanion(uint32_t nowMs) {
  ble_box::keepAdvertising();

  if (ble_box::takeLinkLost()) {
    relayInFlight = 0;  // lo que no se confirmó se vuelve a mandar
    if (ALERT_ON_LINK_LOSS && seal.state() == SealState::Sealed) {
      beep(3);
      show("Celular fuera de alcance", nowMs);
    }
  }

  uint32_t unixTime = 0;
  int32_t latE5 = 0, lonE5 = 0;
  if (ble_box::takeTimePos(unixTime, latE5, lonE5)) {
    // Un celular vinculado puede poner la hora si la caja no la tiene, o corregir hasta 5 minutos
    // hacia adelante. Saltos mayores solo con un token firmado por el backend.
    uint32_t now = 0;
    if (!boxClock.now(nowMs, now) || (unixTime >= now && unixTime - now <= 300)) {
      boxClock.set(unixTime, nowMs);
    }
    if (latE5 >= -9000000 && latE5 <= 9000000 && lonE5 >= -18000000 && lonE5 <= 18000000) {
      lastLatE5 = latE5;
      lastLonE5 = lonE5;
    }
  }

  // Relevo por pedido: el celular escribe `ack` con cuántas lecturas de la tanda anterior subió
  // bien (0 la primera vez); la caja las descarta y manda la tanda siguiente.
  uint16_t acked = 0;
  if (ble_box::takeAck(acked)) {
    queue.pop(acked < relayInFlight ? acked : relayInFlight);
    relayInFlight = 0;
    static SignedReading batch[RELAY_BATCH_MAX];
    const size_t n = queue.peek(batch, RELAY_BATCH_MAX);
    for (size_t i = 0; i < n; i++) {
      uint8_t frame[ble_box::READING_FRAME];
      memcpy(frame, batch[i].payload, READING_SIZE);
      memcpy(frame + READING_SIZE, batch[i].signature, SIGNATURE_SIZE);
      if (!ble_box::sendReading(frame)) break;
      relayInFlight++;
    }
    publishBoxStatus();
  }
}

#ifdef BENCH_WIFI_UPLINK
static void uploadPending() {
  static SignedReading batch[UPLOAD_BATCH_MAX];
  static char body[ingestJsonSize(sizeof(DEVICE_ID) - 1, UPLOAD_BATCH_MAX)];
  static char response[160];

  const size_t n = queue.peek(batch, UPLOAD_BATCH_MAX);
  const size_t len = buildIngestJson(DEVICE_ID, batch, n, body, sizeof body);
  if (len == 0) return;
  int status = 0;
  if (!uplink.postJson(INGEST_PATH, body, len, status, response, sizeof response)) return;
  // 200: accepted, duplicates y rejected son definitivos. 409 (no monitorea ningún cargamento),
  // 403 (no operativa) y 400: las lecturas se conservan.
  if (status == 200) queue.pop(n);
  Serial.printf("[upload] HTTP %d, quedan %u. %s\n", status, (unsigned)queue.size(), response);
}
#endif

// ---------------------------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  prefs.begin("pharma", false);

  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_BUTTON), onButtonInterrupt, FALLING);

  sensors::begin();
  if (sensors::accelBegin()) {
    pinMode(PIN_ACCEL_INT, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_ACCEL_INT), onShockInterrupt, RISING);
  } else {
    Serial.println("[boot] LIS3DH no responde: no se detectan golpes");
  }
  display.begin();

  // PIN de vinculación nuevo en cada arranque (los celulares ya vinculados no lo necesitan).
  pairingPin = esp_random() % 1000000;
  ble_box::begin(DEVICE_ID, pairingPin);

  unlockReady = unlockVerifier.begin(UNLOCK_PUBKEY);
  if (!unlockReady) Serial.println("[boot] clave de apertura inválida (include/unlock_key.h)");

  signerReady = signer.begin();  // después de BLE: el generador aleatorio necesita la radio
  uint8_t pub[PUBLIC_KEY_SIZE];
  char hex[2 + 2 * PUBLIC_KEY_SIZE + 1];
  if (signerReady && signer.publicKey(pub) && toHex(pub, sizeof pub, hex, sizeof hex)) {
    // La pública es lo que se registra en el ledger: POST /api/iot/devices { deviceId, pubKey, ... }
    Serial.printf("[boot] deviceId=%s pubKey=%s\n", DEVICE_ID, hex);
  } else {
    Serial.println("[boot] no hay clave de firma: no se emitirá nada");
  }

#ifdef BENCH_WIFI_UPLINK
  uplink.begin();
  configTime(0, 0, "pool.ntp.org");  // en banco la hora sale de NTP: no hace falta token ni celular
#endif
  show("PharmaBox", millis());
}

void loop() {
  const uint32_t nowMs = millis();  // las restas sin signo toleran el desborde de millis()
  boxClock.tick(nowMs);

  if (shockSeen) {
    shockSeen = false;
    sensors::accelClearInt();  // INT1 queda latcheada hasta leerla
  }

  serviceSeal(nowMs);
  serviceUnlock(nowMs);
  serviceCompanion(nowMs);
  serviceBuzzer(nowMs);

  if (sampleNow || uint32_t(nowMs - lastSampleMs) >= SAMPLE_INTERVAL_S * 1000UL) {
    sampleNow = false;
    lastSampleMs = nowMs;
    sampleOnce(nowMs);
    publishBoxStatus();
  }

#ifdef BENCH_WIFI_UPLINK
  if (!boxClock.valid()) {
    const time_t t = time(nullptr);  // queda en ~0 hasta que NTP responde
    if (t > time_t(BoxClock::MIN_VALID_UNIX)) boxClock.set(uint32_t(t), nowMs);
  }
  if (!queue.empty() && uint32_t(nowMs - lastUploadMs) >= UPLOAD_INTERVAL_S * 1000UL) {
    lastUploadMs = nowMs;
    uploadPending();
  }
#endif

  if (displayOn && !challengeShown && uint32_t(nowMs - displayOnMs) >= DISPLAY_TIMEOUT_MS) {
    display.off();
    displayOn = false;
  }
}
