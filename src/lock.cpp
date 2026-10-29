#include "lock.h"

#include <Arduino.h>

#include "config.h"

void Lock::begin() {
  pinMode(PIN_LOCK_DRIVE, OUTPUT);
  digitalWrite(PIN_LOCK_DRIVE, LOW);  // apagado por defecto: ante un reinicio la cerradura queda cerrada
  pinMode(PIN_LOCK_SENSE, INPUT_PULLUP);
}

void Lock::open(uint32_t nowMs) {
  digitalWrite(PIN_LOCK_DRIVE, HIGH);
  energized_ = true;
  startMs_ = nowMs;
}

void Lock::update(uint32_t nowMs) {
  if (energized_ && uint32_t(nowMs - startMs_) >= LOCK_PULSE_MS) {
    digitalWrite(PIN_LOCK_DRIVE, LOW);
    energized_ = false;
  }
}

bool Lock::closed() const { return digitalRead(PIN_LOCK_SENSE) == LOW; }
