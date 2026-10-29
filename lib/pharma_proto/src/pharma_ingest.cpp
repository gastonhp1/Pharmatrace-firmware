#include "pharma_ingest.h"

#include <string.h>

namespace pharma {

namespace {

constexpr char kHead[] = "{\"deviceId\":\"";
constexpr char kMid[] = "\",\"readings\":[";
constexpr char kItemA[] = "{\"payload\":\"";
constexpr char kItemB[] = "\",\"signature\":\"";
constexpr char kItemC[] = "\"}";
constexpr char kTail[] = "]}";

template <size_t N>
constexpr size_t lit(const char (&)[N]) {
  return N - 1;
}

constexpr size_t kPayloadHex = 2 + 2 * READING_SIZE;
constexpr size_t kSignatureHex = 2 + 2 * SIGNATURE_SIZE;
constexpr size_t kItemLen = sizeof(kItemA) - 1 + kPayloadHex + sizeof(kItemB) - 1 + kSignatureHex +
                            sizeof(kItemC) - 1;

// Cursor de escritura con tope: nunca pasa de `cap`.
struct Writer {
  char* out;
  size_t cap;
  size_t pos = 0;
  bool bad = false;

  void put(const char* s, size_t n) {
    if (bad || pos + n >= cap) {  // reserva un byte para el NUL
      bad = true;
      return;
    }
    memcpy(out + pos, s, n);
    pos += n;
  }
  void hex(const uint8_t* data, size_t len) {
    if (bad || pos + 2 + 2 * len >= cap) {
      bad = true;
      return;
    }
    pos += toHex(data, len, out + pos, cap - pos);
  }
};

}  // namespace

static_assert(ingestJsonSize(0, 1) == lit(kHead) + lit(kMid) + kItemLen + lit(kTail) + 1,
              "ingestJsonSize() no coincide con el formato que arma buildIngestJson()");
static_assert(ingestJsonSize(0, 2) - ingestJsonSize(0, 1) == kItemLen + 1,
              "cada lectura de más suma una lectura y una coma");

size_t buildIngestJson(const char* deviceId, const SignedReading* items, size_t count, char* out,
                       size_t cap) {
  if (items == nullptr || out == nullptr) return 0;
  if (count < 1 || count > MAX_READINGS_PER_REQUEST) return 0;
  if (!isValidDeviceId(deviceId)) return 0;
  const size_t idLen = strlen(deviceId);

  Writer w{out, cap};
  w.put(kHead, lit(kHead));
  w.put(deviceId, idLen);
  w.put(kMid, lit(kMid));
  for (size_t i = 0; i < count; i++) {
    if (i > 0) w.put(",", 1);
    w.put(kItemA, lit(kItemA));
    w.hex(items[i].payload, READING_SIZE);
    w.put(kItemB, lit(kItemB));
    w.hex(items[i].signature, SIGNATURE_SIZE);
    w.put(kItemC, lit(kItemC));
  }
  w.put(kTail, lit(kTail));
  if (w.bad) return 0;
  out[w.pos] = '\0';
  return w.pos;
}

}  // namespace pharma
