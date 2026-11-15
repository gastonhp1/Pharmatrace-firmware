#pragma once

#include <stddef.h>
#include <stdint.h>

// PharmaBox v2: un módulo que va sobre la tapa de la caja de telgopor, sujeto por un cinto que hace
// de precinto, y una sonda con los sensores que entra a la caja. Sin GPS, sin módem, sin elemento
// seguro y sin cerradura: el celular vinculado por BLE pone la posición y sube las lecturas.
//
// Numeración de GPIO del ESP32-S3-DevKitC-1. El mapa es una propuesta sin validar en hardware:
// verificar contra la serigrafía de tu placa (ver docs/wiring.es.svg).
//
// Evitar: GPIO0/3/45/46 (strapping), GPIO19/20 (USB), GPIO26-32 (flash/PSRAM) y, con módulos de
// PSRAM octal (N8R8), GPIO35-37. Las entradas analógicas van en ADC1 (GPIO1-10): ADC2 no sirve
// con la radio encendida.

// --- Bus I2C ---
// En el módulo: LIS3DH (0x18). En la sonda interna: SHT4x (0x44) y VEML7700 (0x10).
constexpr int PIN_I2C_SDA = 8;
constexpr int PIN_I2C_SCL = 9;

// --- Pantalla TFT ILI9488 (SPI, 320x480, sin táctil) ---
// Todos sobre la cabecera J3, del mismo lado que la pantalla. El S3 rutea SPI a cualquier pin.
// GPIO39-42 son los de JTAG por pines: sirven como GPIO normales depurando por el USB nativo.
constexpr int PIN_TFT_CS = 42;
constexpr int PIN_TFT_DC = 41;
constexpr int PIN_TFT_RST = 40;
constexpr int PIN_TFT_MOSI = 39;
constexpr int PIN_TFT_SCK = 38;
constexpr int PIN_TFT_BL = 21;  // retroiluminación: se apaga sola para cuidar la batería

// --- Entradas y avisos ---
constexpr int PIN_BUTTON = 2;     // a GND, INPUT_PULLUP: enciende la pantalla y genera un QR
constexpr int PIN_ACCEL_INT = 5;  // INT1 del LIS3DH (golpe / movimiento)
constexpr int PIN_BUZZER = 47;    // buzzer activo (con transistor si consume más de ~20 mA)

// --- Precinto y batería (ADC1) ---
// Lazo:    3V3 - 10k - [GPIO4] - cable del cinto - 10k en la hebilla - GND   (cerrado ~1650 mV)
// Presión: 3V3 - FSR - [GPIO6] - 10k - GND                                  (apretado: sube)
// Batería: VBAT - 100k - [GPIO7] - 100k - GND                               (mitad de VBAT)
constexpr int PIN_SEAL_LOOP = 4;
constexpr int PIN_SEAL_FSR = 6;
constexpr int PIN_BATTERY = 7;

constexpr uint32_t SEAL_POLL_MS = 100;         // cada cuánto se mira el precinto

// El sensor de presión es la parte del precinto más propensa a falsas alarmas, y una falsa alarma
// deja el cargamento comprometido. Empezar sin él (solo lazo, sonda y luz) y sumarlo después de
// calibrarlo con la caja real. Con false, el pin puede quedar sin conectar.
constexpr bool SEAL_USE_FSR = false;
constexpr uint32_t DISPLAY_TIMEOUT_MS = 30000;  // pantalla encendida después del último uso
constexpr bool ALERT_ON_LINK_LOSS = true;       // sonar si el celular vinculado se aleja

// --- Apertura autenticada (docs/unlock-protocol.md) ---
constexpr uint32_t UNLOCK_CHALLENGE_TTL_MS = 120000;  // vida del QR en pantalla
constexpr uint8_t UNLOCK_MAX_ATTEMPTS = 3;            // intentos fallidos por QR

// --- Muestreo y subida ---
// SAMPLE_INTERVAL_S debe coincidir con IOT_SAMPLE_SECONDS del gateway (por defecto 60): el
// gateway cuenta la última lectura de cada ventana como un período de muestreo.
constexpr uint32_t SAMPLE_INTERVAL_S = 60;
constexpr uint32_t UPLOAD_INTERVAL_S = 600;  // solo para la subida de banco por WiFi

// El gateway acepta hasta 200 lecturas por request (MAX_READINGS_PER_REQUEST).
constexpr size_t UPLOAD_BATCH_MAX = 50;
constexpr size_t RELAY_BATCH_MAX = 16;  // lecturas por tanda hacia el celular

// Lecturas firmadas en RAM esperando subir: 85 bytes cada una (512 = 8,5 h a una por minuto).
// Si se llena se descarta la más vieja (queda un hueco de `seq`). Persistir en flash es un TODO.
constexpr size_t QUEUE_CAPACITY = 512;

// Ruta del gateway. Subir lecturas no pide clave de API: el gateway verifica la firma.
constexpr const char* INGEST_PATH = "/api/iot/readings";

// DEVICE_ID, WIFI_SSID, WIFI_PASS y GATEWAY_URL van en include/secrets.h (no se versiona).
