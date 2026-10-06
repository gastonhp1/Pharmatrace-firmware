#pragma once

// --- Intervalos ---
constexpr uint32_t SAMPLE_INTERVAL_MS = 10 * 1000;   // sensado
constexpr uint32_t REPORT_INTERVAL_MS = 60 * 1000;   // envío de lote de lecturas

// --- Umbrales de cadena de frío (ejemplo 2-8 °C, ajustar por producto) ---
constexpr float TEMP_MIN_C = 2.0f;
constexpr float TEMP_MAX_C = 8.0f;

// --- Pines (placeholders, ajustar al cableado real) ---
constexpr int PIN_LID_REED = 27;
constexpr int PIN_I2C_SDA  = 21;
constexpr int PIN_I2C_SCL  = 22;
constexpr int PIN_GPS_RX   = 16;
constexpr int PIN_GPS_TX   = 17;

// Credenciales y endpoints van en include/secrets.h (no se versiona)
