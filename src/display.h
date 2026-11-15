#pragma once
// Pantalla del módulo: muestra el QR de apertura y el estado de la caja.
//   Ili9488Display  TFT ILI9488 por SPI con LovyanGFX (src/display_ili9488.cpp). Se compila solo
//                   si la librería está instalada. Sin probar en hardware.
//   SerialDisplay   imprime lo mismo por serie: alcanza para probar el flujo sin pantalla.
#include <stdint.h>

#include "pharma_seal.h"

struct StatusView {
  bool haveReading = false;
  int16_t tempCenti = 0;
  uint16_t humCenti = 0;
  pharma::SealState seal = pharma::SealState::Sealed;
  uint8_t batteryPercent = 0;
  uint16_t queued = 0;      // lecturas esperando subir
  bool clockValid = false;  // sin hora no se emiten lecturas
  bool phoneLinked = false;
  uint32_t pairingPin = 0;  // PIN de 6 dígitos para vincular el celular
};

class Display {
 public:
  virtual ~Display() = default;
  virtual void begin() = 0;
  virtual void showQr(const char* qrText, const StatusView& status) = 0;
  virtual void showMessage(const char* text, const StatusView& status) = 0;
  virtual void off() = 0;  // apaga la retroiluminación
};

class SerialDisplay : public Display {
 public:
  void begin() override {}
  void showQr(const char* qrText, const StatusView& status) override;
  void showMessage(const char* text, const StatusView& status) override;
  void off() override;
};

const char* sealStateText(pharma::SealState s);

// La pantalla que haya: la TFT si LovyanGFX está instalada, si no la de serie.
Display& boxDisplay();
