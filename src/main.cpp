#include <Arduino.h>
#include "config.h"

struct Reading {
  uint32_t ts_ms;
  float temp_c;
  float humidity_pct;
  bool lid_open;
  bool out_of_range;
};

static uint32_t lastSample = 0;
static uint32_t lastReport = 0;

static Reading sample() {
  Reading r{};
  r.ts_ms = millis();
  // TODO: leer sensores reales (SHT31/DS18B20, acelerómetro, GPS)
  r.temp_c = NAN;
  r.humidity_pct = NAN;
  r.lid_open = digitalRead(PIN_LID_REED) == HIGH;
  r.out_of_range = !isnan(r.temp_c) && (r.temp_c < TEMP_MIN_C || r.temp_c > TEMP_MAX_C);
  return r;
}

static void report() {
  // TODO: firmar lecturas y enviarlas al backend/oráculo (MQTT o HTTPS)
  Serial.println("[report] pendiente de implementar");
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LID_REED, INPUT_PULLUP);
  Serial.println("pharmatrace-firmware boot");
}

void loop() {
  const uint32_t now = millis();

  if (now - lastSample >= SAMPLE_INTERVAL_MS) {
    lastSample = now;
    Reading r = sample();
    Serial.printf("[sample] t=%lu temp=%.2f hum=%.1f lid=%d oor=%d\n",
                  (unsigned long)r.ts_ms, r.temp_c, r.humidity_pct, r.lid_open, r.out_of_range);
  }

  if (now - lastReport >= REPORT_INTERVAL_MS) {
    lastReport = now;
    report();
  }
}
