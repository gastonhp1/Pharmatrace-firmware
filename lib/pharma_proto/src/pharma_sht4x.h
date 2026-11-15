#pragma once

// SHT4x (SHT40/41/45): temperatura y humedad en un solo chip. Acá va solo lo que se puede probar
// en la PC: el CRC y la conversión. El I2C está en src/sensors.cpp.

#include <stddef.h>
#include <stdint.h>

namespace pharma {

constexpr uint8_t SHT4X_ADDR = 0x44;
constexpr uint8_t SHT4X_CMD_MEASURE_HIGH = 0xFD;  // alta repetibilidad, ~8,3 ms
constexpr size_t SHT4X_FRAME = 6;                 // T(2) CRC  RH(2) CRC

// CRC-8 de Sensirion: polinomio 0x31, valor inicial 0xFF. crc8({0xBE, 0xEF}) == 0x92.
uint8_t sht4xCrc8(const uint8_t* data, size_t len);

// Devuelve false si falla algún CRC. La humedad se recorta a 0..100 % como pide la hoja de datos.
bool sht4xDecode(const uint8_t frame[SHT4X_FRAME], float& tempC, float& humidityPct);

}  // namespace pharma
