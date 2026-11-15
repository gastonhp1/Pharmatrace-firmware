#include "pharma_sht4x.h"

namespace pharma {

uint8_t sht4xCrc8(const uint8_t* data, size_t len) {
  uint8_t crc = 0xFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++) crc = (crc & 0x80) ? uint8_t((crc << 1) ^ 0x31) : uint8_t(crc << 1);
  }
  return crc;
}

bool sht4xDecode(const uint8_t frame[SHT4X_FRAME], float& tempC, float& humidityPct) {
  if (sht4xCrc8(frame, 2) != frame[2] || sht4xCrc8(frame + 3, 2) != frame[5]) return false;
  const uint16_t rawT = uint16_t((frame[0] << 8) | frame[1]);
  const uint16_t rawH = uint16_t((frame[3] << 8) | frame[4]);
  tempC = -45.0f + 175.0f * float(rawT) / 65535.0f;
  float rh = -6.0f + 125.0f * float(rawH) / 65535.0f;
  if (rh < 0.0f) rh = 0.0f;
  if (rh > 100.0f) rh = 100.0f;
  humidityPct = rh;
  return true;
}

}  // namespace pharma
