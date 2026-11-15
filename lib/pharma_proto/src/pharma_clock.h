#pragma once

// Reloj de la caja. No hay GPS ni RTC externo: la hora llega de afuera (el `iat` de un token de
// apertura firmado por el backend, o el celular vinculado) y se mantiene contando milisegundos.
// Después de un reinicio no hay hora hasta la próxima sincronización, y sin hora no se emiten
// lecturas. Sin dependencias de Arduino.

#include <stdint.h>

namespace pharma {

class BoxClock {
 public:
  static constexpr uint32_t MIN_VALID_UNIX = 1767225600;  // 2026-01-01: antes de eso es un error

  // Devuelve false (y no cambia nada) si la hora es absurda o atrasa el reloj: la hora de la caja
  // nunca retrocede, porque el gateway exige `ts` creciente.
  bool set(uint32_t unixTime, uint32_t nowMs) {
    tick(nowMs);
    if (unixTime < MIN_VALID_UNIX) return false;
    if (valid_ && unixTime < unix_) return false;
    unix_ = unixTime;
    remMs_ = 0;
    lastMs_ = nowMs;
    valid_ = true;
    return true;
  }

  // Llamar seguido: acumula el tiempo transcurrido y tolera el desborde de millis() (49 días).
  void tick(uint32_t nowMs) {
    if (!valid_) {
      lastMs_ = nowMs;
      return;
    }
    const uint32_t total = remMs_ + uint32_t(nowMs - lastMs_);
    lastMs_ = nowMs;
    unix_ += total / 1000;
    remMs_ = total % 1000;
  }

  bool now(uint32_t nowMs, uint32_t& out) {
    tick(nowMs);
    if (!valid_) return false;
    out = unix_;
    return true;
  }

  bool valid() const { return valid_; }

 private:
  bool valid_ = false;
  uint32_t unix_ = 0;
  uint32_t remMs_ = 0;
  uint32_t lastMs_ = 0;
};

}  // namespace pharma
