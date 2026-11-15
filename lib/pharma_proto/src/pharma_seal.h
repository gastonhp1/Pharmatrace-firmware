#pragma once

// Precinto electrónico de la PharmaBox: un cinto que rodea la caja por encima de la tapa, con el
// módulo apoyado sobre ella. Sin dependencias de Arduino: se prueba en la PC.
//
// Tres señales dicen si la caja sigue cerrada:
//   lazo     cable a lo largo del cinto que cierra en la hebilla a través de una resistencia
//            conocida. Se mide tensión, no solo continuidad: abrir o cortar da un valor, puentear
//            la hebilla con un cable da otro.
//   presión  sensor de fuerza (FSR) entre el módulo y la tapa: cae si aflojan el cinto.
//   sonda    el cable con los sensores que entra a la caja; adentro hay además un sensor de luz
//            que ve la tapa abierta o un corte en el telgopor.
//
// Es evidencia, no protección física: el telgopor se corta con un cúter. Lo que importa es que
// cualquier apertura quede en una lectura firmada, y que una apertura sin autorización previa
// (token por BLE, ver pharma_unlock.h) se distinga de una autorizada.

#include <stdint.h>

#include "pharma_reading.h"

namespace pharma {

enum class LoopState : uint8_t { Closed, Open, Shorted };

struct SealConfig {
  // Divisor 3V3 - 10k - [ADC] - lazo con 10k en la hebilla - GND: cerrado da ~1650 mV.
  uint16_t loopClosedMinMv = 1300;  // por debajo: lazo puenteado (falta la resistencia)
  uint16_t loopClosedMaxMv = 2000;  // por encima: lazo abierto o cortado
  uint16_t fsrMinMv = 300;          // por debajo: el módulo ya no está apretado contra la tapa
  uint16_t luxOpen = 10;            // luz adentro de una caja cerrada: ~0 lux
  uint32_t debounceMs = 500;        // cuánto tiene que durar un cambio para creerlo
  uint32_t authWindowMs = 600000;   // tiempo para abrir después de un desbloqueo (10 min)
};

struct SealInputs {
  uint16_t loopMv;
  uint16_t fsrMv;
  bool probePresent;  // la sonda interna responde por I2C
  uint16_t lux;       // solo vale si probePresent
};

LoopState classifyLoop(uint16_t mv, const SealConfig& cfg);

enum class SealState : uint8_t {
  Sealed,          // cerrada y vigilada
  AuthorizedOpen,  // abierta después de un desbloqueo válido; se rearma sola al cerrarla
  Breached,        // se abrió sin autorización. Queda así hasta un desbloqueo válido
};

class SealMonitor {
 public:
  explicit SealMonitor(const SealConfig& cfg = SealConfig()) : cfg_(cfg) {}

  // Llamar seguido (varias veces por segundo). Devuelve los flags de EVENTO que hay que sumar a
  // la próxima lectura (FLAG_LID_OPENED al detectar luz adentro).
  uint8_t update(const SealInputs& in, uint32_t nowMs);

  // Un token de apertura válido: habilita abrir durante authWindowMs.
  void authorize(uint32_t nowMs);

  SealState state() const { return state_; }
  bool intact() const { return intact_; }    // lazo, presión y sonda en orden ahora mismo
  bool lidOpen() const { return lidOpen_; }  // hay luz adentro ahora mismo

  // Flags de ESTADO para cada lectura: FLAG_SEAL_BROKEN mientras esté violada.
  uint8_t stateFlags() const { return state_ == SealState::Breached ? FLAG_SEAL_BROKEN : 0; }

 private:
  bool windowOpen(uint32_t nowMs) const;

  SealConfig cfg_;
  SealState state_ = SealState::Sealed;
  bool intact_ = true;
  bool lidOpen_ = false;

  bool pendingValid_ = false;  // hay un cambio esperando el antirrebote
  bool pendingOpen_ = false;
  uint32_t pendingSinceMs_ = 0;
  bool open_ = false;  // estado ya filtrado: algo está abierto

  bool windowArmed_ = false;
  uint32_t windowStartMs_ = 0;
};

}  // namespace pharma
