#include "pharma_reading.h"

#include <math.h>

namespace pharma {

namespace {

void putU32(uint8_t* p, uint32_t v) {
  p[0] = uint8_t(v >> 24);
  p[1] = uint8_t(v >> 16);
  p[2] = uint8_t(v >> 8);
  p[3] = uint8_t(v);
}

void putU16(uint8_t* p, uint16_t v) {
  p[0] = uint8_t(v >> 8);
  p[1] = uint8_t(v);
}

uint32_t getU32(const uint8_t* p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

uint16_t getU16(const uint8_t* p) { return uint16_t((uint16_t(p[0]) << 8) | p[1]); }

}  // namespace

void encodeReading(const Reading& r, uint8_t out[READING_SIZE]) {
  putU32(out + 0, r.ts);
  putU32(out + 4, r.seq);
  putU16(out + 8, uint16_t(r.tempCenti));
  putU16(out + 10, r.humCenti);
  out[12] = r.flags;
  putU32(out + 13, uint32_t(r.latE5));
  putU32(out + 17, uint32_t(r.lonE5));
}

bool decodeReading(const uint8_t* buf, size_t len, Reading& out) {
  if (buf == nullptr || len != READING_SIZE) return false;
  out.ts = getU32(buf + 0);
  out.seq = getU32(buf + 4);
  out.tempCenti = int16_t(getU16(buf + 8));
  out.humCenti = getU16(buf + 10);
  out.flags = buf[12];
  out.latE5 = int32_t(getU32(buf + 13));
  out.lonE5 = int32_t(getU32(buf + 17));
  return true;
}

bool tempToCenti(float celsius, int16_t& out) {
  if (isnan(celsius) || isinf(celsius)) return false;
  const float scaled = celsius * 100.0f;
  if (scaled < -32768.5f || scaled > 32767.5f) return false;
  const long v = lroundf(scaled);
  if (v < -32768 || v > 32767) return false;
  out = int16_t(v);
  return true;
}

bool humidityToCenti(float percent, uint16_t& out) {
  if (isnan(percent) || isinf(percent)) return false;
  if (percent < 0.0f) percent = 0.0f;
  if (percent > 100.0f) percent = 100.0f;
  out = uint16_t(lroundf(percent * 100.0f));
  return true;
}

bool latitudeToE5(double degrees, int32_t& out) {
  if (!(degrees >= -90.0 && degrees <= 90.0)) return false;  // también descarta NaN
  out = int32_t(lround(degrees * 1e5));
  return true;
}

bool longitudeToE5(double degrees, int32_t& out) {
  if (!(degrees >= -180.0 && degrees <= 180.0)) return false;
  out = int32_t(lround(degrees * 1e5));
  return true;
}

bool isValidDeviceId(const char* id) {
  if (id == nullptr) return false;
  size_t n = 0;
  for (; id[n] != '\0'; n++) {
    const char c = id[n];
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                    c == '-' || c == '_';
    if (!ok || n >= MAX_DEVICE_ID_LEN) return false;
  }
  return n > 0;
}

size_t toHex(const uint8_t* data, size_t len, char* out, size_t cap) {
  if (out == nullptr || cap < 2 + 2 * len + 1) return 0;
  static const char digits[] = "0123456789abcdef";
  out[0] = '0';
  out[1] = 'x';
  for (size_t i = 0; i < len; i++) {
    out[2 + 2 * i] = digits[data[i] >> 4];
    out[3 + 2 * i] = digits[data[i] & 0x0f];
  }
  out[2 + 2 * len] = '\0';
  return 2 + 2 * len;
}

}  // namespace pharma
