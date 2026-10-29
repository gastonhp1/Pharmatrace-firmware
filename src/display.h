#pragma once
// Pantalla donde se muestra el QR de apertura. La implementación real es una e-paper por SPI
// (TODO: elegir panel y librería de QR, ver platformio.ini). Mientras tanto, SerialDisplay
// imprime el texto del QR: alcanza para probar el flujo completo con nRF Connect y la herramienta
// tools/unlock_token.py, sin pantalla.
#include <stdint.h>

class Display {
 public:
  virtual ~Display() = default;
  virtual void begin() = 0;
  virtual void showQr(const char* text) = 0;      // `text` es el contenido del QR
  virtual void showMessage(const char* text) = 0;  // estado corto: "Abierta", "Rechazada"…
  virtual void clear() = 0;                        // deja la pantalla sin QR
};

class SerialDisplay : public Display {
 public:
  void begin() override {}
  void showQr(const char* text) override;
  void showMessage(const char* text) override;
  void clear() override;
};
