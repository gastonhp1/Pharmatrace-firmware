#pragma once

#include <stddef.h>

// Transporte hacia el gateway. Subir lecturas no lleva clave de API: el gateway acepta solo
// firmas válidas de un dispositivo registrado y con monitoreo activo.
class Uplink {
 public:
  virtual ~Uplink() = default;

  virtual bool begin() = 0;

  // POST application/json a `path`. Devuelve true si hubo respuesta HTTP (aunque sea un error) y
  // deja el código en `status` y el principio del cuerpo en `response` (siempre con NUL).
  // Devuelve false si no hubo conexión o falló la red.
  virtual bool postJson(const char* path, const char* body, size_t len, int& status, char* response,
                        size_t responseCap) = 0;
};

// Transporte por WiFi: SOLO para pruebas de banco contra un gateway en la LAN (sin TLS).
// Compilarlo no está verificado todavía.
class UplinkWifi : public Uplink {
 public:
  bool begin() override;
  bool postJson(const char* path, const char* body, size_t len, int& status, char* response,
                size_t responseCap) override;
};

// TODO: UplinkModem (A7670 LTE Cat-1 o SIM7080G LTE-M) con TLS. Es el transporte real de la
// valija. Verificar antes la cobertura de cada tecnología en las rutas reales (BOM.md).
