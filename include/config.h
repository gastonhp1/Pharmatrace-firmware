#pragma once

#include <stddef.h>
#include <stdint.h>

// Numeración de GPIO del ESP32-S3-DevKitC-1. El mapa es una propuesta sin validar en
// hardware: verificar contra la serigrafía de tu placa (ver docs/wiring.svg).
//
// Evitar: GPIO0/3/45/46 (strapping), GPIO19/20 (USB), GPIO26-32 (flash/PSRAM) y, con módulos
// de PSRAM octal (N8R8), GPIO35-37.

// --- Bus I2C compartido ---
// TMP117 x2 (0x48, 0x49), SHT45 (0x44), VEML7700 (0x10), LIS3DH (0x18),
// ATECC608B (0x60), MAX17048 (0x36)
constexpr int PIN_I2C_SDA = 8;
constexpr int PIN_I2C_SCL = 9;

// --- GPS (UART1). RX/TX desde el punto de vista del ESP32 ---
constexpr int PIN_GPS_RX = 42;  // <- TX del GPS
constexpr int PIN_GPS_TX = 41;  // -> RX del GPS

// --- Módem celular (UART2) ---
constexpr int PIN_MODEM_RX = 40;       // <- TX del módem
constexpr int PIN_MODEM_TX = 39;       // -> RX del módem
constexpr int PIN_MODEM_PWRKEY = 47;

// GPIO39-42 son los pines de JTAG por pines. Sirven como GPIO normales mientras se depure por
// el USB nativo del S3 (lo que hace este proyecto), no con un JTAG externo.

// --- Entradas con interrupción (GPIO RTC, 0-21: pueden despertar de deep sleep) ---
constexpr int PIN_LID_REED = 1;    // reed switch de la tapa, con INPUT_PULLUP
constexpr int PIN_ACCEL_INT = 5;   // INT1 del LIS3DH (golpe)

// Con el imán cerca (tapa cerrada) el reed conecta a GND, o sea LOW. Tapa abierta = HIGH.
// Depende de cómo se monte el imán: verificar en la valija.
constexpr int LID_OPEN_LEVEL = 1;

// --- Muestreo y subida ---
// SAMPLE_INTERVAL_S debe coincidir con IOT_SAMPLE_SECONDS del gateway (por defecto 60): el
// gateway cuenta la última lectura de cada ventana como un período de muestreo.
constexpr uint32_t SAMPLE_INTERVAL_S = 60;
constexpr uint32_t UPLOAD_INTERVAL_S = 600;

// El gateway acepta hasta 200 lecturas por request (MAX_READINGS_PER_REQUEST).
constexpr size_t UPLOAD_BATCH_MAX = 50;

// Lecturas firmadas en RAM esperando subir: 85 bytes cada una. Si se llena se descarta la más
// vieja (queda un hueco de `seq`). Persistir en flash es un TODO.
constexpr size_t QUEUE_CAPACITY = 512;

// Ruta del gateway. Subir lecturas no pide clave de API: el gateway verifica la firma.
constexpr const char* INGEST_PATH = "/api/iot/readings";

// DEVICE_ID, WIFI_SSID, WIFI_PASS y GATEWAY_URL van en include/secrets.h (no se versiona).
