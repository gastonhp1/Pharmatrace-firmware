#pragma once
// Sensores de la PharmaBox v2. Escrito contra las hojas de datos, SIN probar en hardware.
//   Sonda interna (I2C): SHT4x (temperatura y humedad) y VEML7700 (luz).
//   Módulo: LIS3DH (golpes), y por ADC el lazo del cinto, el sensor de presión y la batería.
#include <stdint.h>

namespace sensors {

void begin();

bool probePresent();                                   // el SHT4x de la sonda responde
bool readTempHumidity(float& tempC, float& humidityPct);  // ~10 ms
bool readLux(uint16_t& lux);

bool accelBegin();     // configura la interrupción de golpe en INT1
void accelClearInt();  // INT1 queda latcheada hasta leer INT1_SRC

uint16_t loopMv();
uint16_t fsrMv();
uint8_t batteryPercent();

}  // namespace sensors
