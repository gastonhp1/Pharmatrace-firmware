#include <Arduino.h>

#include "display.h"

const char* sealStateText(pharma::SealState s) {
  switch (s) {
    case pharma::SealState::Sealed: return "Precintada";
    case pharma::SealState::AuthorizedOpen: return "Abierta (autorizada)";
    case pharma::SealState::Breached: return "PRECINTO VIOLADO";
  }
  return "?";
}

static void printStatus(const StatusView& s) {
  if (s.haveReading) {
    Serial.printf("[display] %.2f C  %.1f %%HR  ", s.tempCenti / 100.0, s.humCenti / 100.0);
  } else {
    Serial.print("[display] sin lectura  ");
  }
  Serial.printf("%s  bat %u%%  en cola %u  hora %s  celular %s  PIN %06lu\n", sealStateText(s.seal),
                s.batteryPercent, s.queued, s.clockValid ? "ok" : "SIN SINCRONIZAR",
                s.phoneLinked ? "vinculado" : "no", (unsigned long)s.pairingPin);
}

void SerialDisplay::showQr(const char* qrText, const StatusView& status) {
  Serial.printf("[display] QR: %s\n", qrText);
  printStatus(status);
}

void SerialDisplay::showMessage(const char* text, const StatusView& status) {
  Serial.printf("[display] %s\n", text);
  printStatus(status);
}

void SerialDisplay::off() { Serial.println("[display] (apagada)"); }

#if !__has_include(<LovyanGFX.hpp>)
Display& boxDisplay() {
  static SerialDisplay d;
  return d;
}
#endif
