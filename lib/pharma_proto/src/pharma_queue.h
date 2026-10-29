#pragma once

// Cola circular de lecturas firmadas pendientes de subir. Sirve para aguantar tramos sin señal.
//
// Está en RAM: se pierde si el equipo se reinicia. Si se llena descarta la más vieja y lo
// cuenta en dropped(); el gateway va a ver un hueco de `seq`. Persistir en flash (LittleFS) es
// un TODO.

#include <stddef.h>
#include <stdint.h>

#include "pharma_reading.h"

namespace pharma {

template <size_t N>
class ReadingQueue {
  static_assert(N > 0, "la cola necesita al menos un lugar");

 public:
  void push(const SignedReading& r) {
    if (count_ == N) {
      head_ = (head_ + 1) % N;
      count_--;
      dropped_++;
    }
    buf_[(head_ + count_) % N] = r;
    count_++;
  }

  size_t size() const { return count_; }
  bool empty() const { return count_ == 0; }
  uint32_t dropped() const { return dropped_; }

  // Copia hasta `max` lecturas, de la más vieja a la más nueva, sin sacarlas de la cola.
  size_t peek(SignedReading* out, size_t max) const {
    const size_t n = count_ < max ? count_ : max;
    for (size_t i = 0; i < n; i++) out[i] = buf_[(head_ + i) % N];
    return n;
  }

  // Saca las `n` más viejas (después de que el gateway las confirmó).
  void pop(size_t n) {
    if (n > count_) n = count_;
    head_ = (head_ + n) % N;
    count_ -= n;
  }

 private:
  SignedReading buf_[N];
  size_t head_ = 0;
  size_t count_ = 0;
  uint32_t dropped_ = 0;
};

}  // namespace pharma
