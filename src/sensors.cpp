#include "sensors.h"

#include <Arduino.h>
#include <Wire.h>

#include "config.h"
#include "pharma_sht4x.h"

namespace sensors {
namespace {

constexpr uint8_t VEML7700_ADDR = 0x10;
constexpr uint8_t LIS3DH_ADDR = 0x18;

bool writeReg8(uint8_t addr, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool readReg8(uint8_t addr, uint8_t reg, uint8_t& value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(int(addr), 1) != 1) return false;
  value = uint8_t(Wire.read());
  return true;
}

bool veml7700Ready = false;

// ALS_CONF_0 (0x00) = 0x0000: ganancia x1, integración 100 ms, encendido. Con eso cada cuenta
// vale 0,0576 lux.
bool veml7700Begin() {
  Wire.beginTransmission(VEML7700_ADDR);
  Wire.write(0x00);
  Wire.write(0x00);  // byte bajo primero
  Wire.write(0x00);
  veml7700Ready = Wire.endTransmission() == 0;
  return veml7700Ready;
}

}  // namespace

void begin() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  analogReadResolution(12);
  pinMode(PIN_SEAL_LOOP, INPUT);
  pinMode(PIN_SEAL_FSR, INPUT);
  pinMode(PIN_BATTERY, INPUT);
  veml7700Begin();
}

bool probePresent() {
  Wire.beginTransmission(pharma::SHT4X_ADDR);
  return Wire.endTransmission() == 0;
}

bool readTempHumidity(float& tempC, float& humidityPct) {
  Wire.beginTransmission(pharma::SHT4X_ADDR);
  Wire.write(pharma::SHT4X_CMD_MEASURE_HIGH);
  if (Wire.endTransmission() != 0) return false;
  delay(10);  // la medición de alta repetibilidad tarda hasta 8,3 ms
  uint8_t frame[pharma::SHT4X_FRAME];
  if (Wire.requestFrom(int(pharma::SHT4X_ADDR), int(sizeof frame)) != int(sizeof frame)) return false;
  for (uint8_t& b : frame) b = uint8_t(Wire.read());
  return pharma::sht4xDecode(frame, tempC, humidityPct);
}

bool readLux(uint16_t& lux) {
  // Si la sonda se desconectó y volvió, el VEML7700 arranca apagado: hay que configurarlo otra vez.
  if (!veml7700Ready && !veml7700Begin()) return false;
  Wire.beginTransmission(VEML7700_ADDR);
  Wire.write(0x04);  // ALS
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom(int(VEML7700_ADDR), 2) != 2) {
    veml7700Ready = false;
    return false;
  }
  const uint16_t lo = uint8_t(Wire.read());
  const uint16_t hi = uint8_t(Wire.read());
  const uint32_t raw = uint32_t(hi << 8 | lo);
  const uint32_t value = raw * 576 / 10000;  // 0,0576 lux por cuenta
  lux = value > 65535 ? 65535 : uint16_t(value);
  return true;
}

bool accelBegin() {
  uint8_t who = 0;
  if (!readReg8(LIS3DH_ADDR, 0x0F, who) || who != 0x33) return false;  // WHO_AM_I
  return writeReg8(LIS3DH_ADDR, 0x20, 0x57) &&  // CTRL_REG1: 100 Hz, X/Y/Z
         writeReg8(LIS3DH_ADDR, 0x21, 0x01) &&  // CTRL_REG2: pasa-altos para la interrupción 1
         writeReg8(LIS3DH_ADDR, 0x22, 0x40) &&  // CTRL_REG3: interrupción 1 en INT1
         writeReg8(LIS3DH_ADDR, 0x23, 0x10) &&  // CTRL_REG4: ±4 g
         writeReg8(LIS3DH_ADDR, 0x24, 0x08) &&  // CTRL_REG5: latch hasta leer INT1_SRC
         writeReg8(LIS3DH_ADDR, 0x32, 0x30) &&  // INT1_THS: 48 x 32 mg ≈ 1,5 g
         writeReg8(LIS3DH_ADDR, 0x33, 0x00) &&  // INT1_DURATION
         writeReg8(LIS3DH_ADDR, 0x30, 0x2A);    // INT1_CFG: X, Y o Z por encima del umbral
}

void accelClearInt() {
  uint8_t src = 0;
  readReg8(LIS3DH_ADDR, 0x31, src);
}

uint16_t loopMv() { return uint16_t(analogReadMilliVolts(PIN_SEAL_LOOP)); }
uint16_t fsrMv() { return uint16_t(analogReadMilliVolts(PIN_SEAL_FSR)); }

uint8_t batteryPercent() {
  const uint32_t mv = analogReadMilliVolts(PIN_BATTERY) * 2;  // divisor a la mitad
  if (mv <= 3300) return 0;                                   // aproximación lineal de una LiPo
  if (mv >= 4200) return 100;
  return uint8_t((mv - 3300) * 100 / 900);
}

}  // namespace sensors
