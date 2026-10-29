#pragma once
// Cerradura de solenoide con pestillo. No bloquea: open() energiza y update() apaga a tiempo.
// Una cerradura electrónica NO es seguridad física: la evidencia está en el ledger, no en el pestillo.
#include <stdint.h>

class Lock {
 public:
  void begin();
  void open(uint32_t nowMs);
  void update(uint32_t nowMs);  // llamar en cada vuelta del loop
  bool closed() const;          // el microswitch dice que el pestillo está cerrado
  bool energized() const { return energized_; }

 private:
  bool energized_ = false;
  uint32_t startMs_ = 0;
};
