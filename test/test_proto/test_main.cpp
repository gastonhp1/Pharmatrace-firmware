// Tests del protocolo en la PC: pio test -e native
#include <string.h>
#include <unity.h>

#include <math.h>
#include <stdio.h>

#include "pharma_ingest.h"
#include "pharma_queue.h"
#include "pharma_reading.h"

using namespace pharma;

void setUp() {}
void tearDown() {}

// Vector de prueba de iot/docs/READING_FORMAT.md (gastonhp1/Pharmatrace). El gateway y el
// simulador producen exactamente esto; si este test falla, el firmware ya no habla el mismo
// formato.
static const Reading kVector = {1800000000u, 7u, 450, 5500, 0x05, -3460000, -5840000};
static const char kVectorPayloadHex[] = "0x6b49d2000000000701c2157c05ffcb3460ffa6e380";

static void test_vector_payload_matches_the_contract() {
  uint8_t out[READING_SIZE];
  encodeReading(kVector, out);
  char hex[2 + 2 * READING_SIZE + 1];
  TEST_ASSERT_EQUAL_UINT32(2 + 2 * READING_SIZE, toHex(out, READING_SIZE, hex, sizeof hex));
  TEST_ASSERT_EQUAL_STRING(kVectorPayloadHex, hex);
}

static void test_decode_roundtrip_with_negative_values() {
  const Reading in = {4294967295u, 4294967295u, -1850, 65535, 0x07, -9000000, 18000000};
  uint8_t buf[READING_SIZE];
  encodeReading(in, buf);
  Reading out{};
  TEST_ASSERT_TRUE(decodeReading(buf, sizeof buf, out));
  TEST_ASSERT_EQUAL_UINT32(in.ts, out.ts);
  TEST_ASSERT_EQUAL_UINT32(in.seq, out.seq);
  TEST_ASSERT_EQUAL_INT16(in.tempCenti, out.tempCenti);
  TEST_ASSERT_EQUAL_UINT16(in.humCenti, out.humCenti);
  TEST_ASSERT_EQUAL_UINT8(in.flags, out.flags);
  TEST_ASSERT_EQUAL_INT32(in.latE5, out.latE5);
  TEST_ASSERT_EQUAL_INT32(in.lonE5, out.lonE5);
}

static void test_decode_rejects_wrong_length() {
  uint8_t buf[READING_SIZE] = {0};
  Reading out{};
  TEST_ASSERT_FALSE(decodeReading(buf, READING_SIZE - 1, out));
  TEST_ASSERT_FALSE(decodeReading(buf, READING_SIZE + 1, out));
  TEST_ASSERT_FALSE(decodeReading(nullptr, READING_SIZE, out));
}

static void test_temperature_conversion() {
  int16_t v = 0;
  TEST_ASSERT_TRUE(tempToCenti(4.5f, v));
  TEST_ASSERT_EQUAL_INT16(450, v);
  TEST_ASSERT_TRUE(tempToCenti(-18.5f, v));
  TEST_ASSERT_EQUAL_INT16(-1850, v);
  TEST_ASSERT_TRUE(tempToCenti(4.504f, v));
  TEST_ASSERT_EQUAL_INT16(450, v);
  TEST_ASSERT_TRUE(tempToCenti(4.506f, v));
  TEST_ASSERT_EQUAL_INT16(451, v);
  TEST_ASSERT_TRUE(tempToCenti(327.67f, v));
  TEST_ASSERT_EQUAL_INT16(32767, v);
}

static void test_temperature_rejects_invalid_values() {
  int16_t v = 123;
  TEST_ASSERT_FALSE(tempToCenti(NAN, v));
  TEST_ASSERT_FALSE(tempToCenti(INFINITY, v));
  TEST_ASSERT_FALSE(tempToCenti(400.0f, v));
  TEST_ASSERT_FALSE(tempToCenti(-400.0f, v));
  TEST_ASSERT_EQUAL_INT16(123, v);  // no toca la salida si falla
}

static void test_humidity_conversion_saturates() {
  uint16_t v = 0;
  TEST_ASSERT_TRUE(humidityToCenti(55.0f, v));
  TEST_ASSERT_EQUAL_UINT16(5500, v);
  TEST_ASSERT_TRUE(humidityToCenti(100.7f, v));  // el SHT45 puede pasarse un poco de 100
  TEST_ASSERT_EQUAL_UINT16(10000, v);
  TEST_ASSERT_TRUE(humidityToCenti(-0.3f, v));
  TEST_ASSERT_EQUAL_UINT16(0, v);
  TEST_ASSERT_FALSE(humidityToCenti(NAN, v));
}

static void test_coordinates() {
  int32_t v = 0;
  TEST_ASSERT_TRUE(latitudeToE5(-34.6, v));
  TEST_ASSERT_EQUAL_INT32(-3460000, v);
  TEST_ASSERT_TRUE(longitudeToE5(-58.4, v));
  TEST_ASSERT_EQUAL_INT32(-5840000, v);
  TEST_ASSERT_FALSE(latitudeToE5(91.0, v));
  TEST_ASSERT_FALSE(longitudeToE5(-181.0, v));
  TEST_ASSERT_FALSE(latitudeToE5(NAN, v));
}

static void test_hex_needs_room_for_prefix_and_nul() {
  const uint8_t data[2] = {0xab, 0x01};
  char small[6];
  char exact[7];
  TEST_ASSERT_EQUAL_UINT32(0, toHex(data, 2, small, sizeof small));
  TEST_ASSERT_EQUAL_UINT32(6, toHex(data, 2, exact, sizeof exact));
  TEST_ASSERT_EQUAL_STRING("0xab01", exact);
}

static SignedReading makeSigned(const Reading& r, uint8_t sigByte) {
  SignedReading s;
  encodeReading(r, s.payload);
  memset(s.signature, sigByte, sizeof s.signature);
  return s;
}

static void test_ingest_json_matches_the_gateway_format() {
  SignedReading items[2];
  items[0] = makeSigned(kVector, 0xaa);
  Reading second = kVector;
  second.seq = 8;
  second.ts = 1800000060u;
  items[1] = makeSigned(second, 0xbb);

  char body[1024];
  const size_t n = buildIngestJson("VAL-001", items, 2, body, sizeof body);
  TEST_ASSERT_TRUE(n > 0);
  TEST_ASSERT_EQUAL_UINT32(strlen(body), n);
  TEST_ASSERT_EQUAL_UINT32(ingestJsonSize(7, 2), n + 1);

  char sigA[2 + 128 + 1];
  char sigB[2 + 128 + 1];
  toHex(items[0].signature, SIGNATURE_SIZE, sigA, sizeof sigA);
  toHex(items[1].signature, SIGNATURE_SIZE, sigB, sizeof sigB);
  char expected[1024];
  snprintf(expected, sizeof expected,
           "{\"deviceId\":\"VAL-001\",\"readings\":["
           "{\"payload\":\"%s\",\"signature\":\"%s\"},"
           "{\"payload\":\"0x6b49d23c00000008" "01c2157c05ffcb3460ffa6e380\",\"signature\":\"%s\"}]}",
           kVectorPayloadHex, sigA, sigB);
  TEST_ASSERT_EQUAL_STRING(expected, body);
}

static void test_ingest_json_rejects_bad_input() {
  SignedReading item = makeSigned(kVector, 0x11);
  char body[1024];
  TEST_ASSERT_EQUAL_UINT32(0, buildIngestJson("", &item, 1, body, sizeof body));
  TEST_ASSERT_EQUAL_UINT32(0, buildIngestJson("VAL\"001", &item, 1, body, sizeof body));
  TEST_ASSERT_EQUAL_UINT32(0, buildIngestJson("VAL\\001", &item, 1, body, sizeof body));
  TEST_ASSERT_EQUAL_UINT32(0, buildIngestJson(nullptr, &item, 1, body, sizeof body));
  TEST_ASSERT_EQUAL_UINT32(0, buildIngestJson("VAL-001", &item, 0, body, sizeof body));
  TEST_ASSERT_EQUAL_UINT32(0, buildIngestJson("VAL-001", &item, MAX_READINGS_PER_REQUEST + 1, body, sizeof body));
}

static void test_device_id_validation() {
  TEST_ASSERT_TRUE(isValidDeviceId("VAL-001"));
  TEST_ASSERT_TRUE(isValidDeviceId("a_B-9"));
  TEST_ASSERT_TRUE(isValidDeviceId("ABCDEFGHIJKLMNOPQRSTUVWX"));    // 24: el máximo
  TEST_ASSERT_FALSE(isValidDeviceId("ABCDEFGHIJKLMNOPQRSTUVWXY"));  // 25
  TEST_ASSERT_FALSE(isValidDeviceId(""));
  TEST_ASSERT_FALSE(isValidDeviceId(nullptr));
  TEST_ASSERT_FALSE(isValidDeviceId("VAL 001"));
  TEST_ASSERT_FALSE(isValidDeviceId("VAL:001"));
}

static void test_flag_bits_do_not_overlap() {
  const uint8_t all[] = {FLAG_LID_OPENED, FLAG_SHOCK, FLAG_SEAL_BROKEN, FLAG_UNLOCK_GRANTED,
                         FLAG_UNLOCK_REJECTED};
  uint8_t seen = 0;
  for (uint8_t f : all) {
    TEST_ASSERT_EQUAL_UINT8(0, seen & f);
    seen |= f;
  }
  TEST_ASSERT_EQUAL_UINT8(0x1f, seen);
}

static void test_ingest_json_never_overflows_the_buffer() {
  SignedReading item = makeSigned(kVector, 0x22);
  const size_t need = ingestJsonSize(7, 1);
  char guarded[512];
  memset(guarded, 'X', sizeof guarded);
  // Un byte menos de lo necesario: no escribe nada fuera de `cap` y devuelve 0.
  TEST_ASSERT_EQUAL_UINT32(0, buildIngestJson("VAL-001", &item, 1, guarded, need - 1));
  TEST_ASSERT_EQUAL_CHAR('X', guarded[need - 1]);
  TEST_ASSERT_EQUAL_CHAR('X', guarded[need]);
  // Justo lo necesario alcanza.
  TEST_ASSERT_TRUE(buildIngestJson("VAL-001", &item, 1, guarded, need) > 0);
}

static void test_ingest_json_max_batch_size() {
  static SignedReading items[MAX_READINGS_PER_REQUEST];
  for (size_t i = 0; i < MAX_READINGS_PER_REQUEST; i++) {
    Reading r = kVector;
    r.seq = uint32_t(i);
    items[i] = makeSigned(r, 0x33);
  }
  // 200 * 203 bytes por lectura + comas + encabezado: unos 40.8 KB
  static char body[41000];
  const size_t n = buildIngestJson("VAL-001", items, MAX_READINGS_PER_REQUEST, body, sizeof body);
  TEST_ASSERT_TRUE(n > 0);
  TEST_ASSERT_EQUAL_UINT32(ingestJsonSize(7, MAX_READINGS_PER_REQUEST), n + 1);
}

static SignedReading tagged(uint8_t tag) {
  SignedReading s;
  memset(&s, tag, sizeof s);
  return s;
}

static void test_queue_keeps_order_and_pops() {
  ReadingQueue<4> q;
  TEST_ASSERT_TRUE(q.empty());
  q.push(tagged(1));
  q.push(tagged(2));
  q.push(tagged(3));
  SignedReading out[4];
  TEST_ASSERT_EQUAL_UINT32(3, q.peek(out, 4));
  TEST_ASSERT_EQUAL_UINT8(1, out[0].payload[0]);
  TEST_ASSERT_EQUAL_UINT8(3, out[2].payload[0]);
  TEST_ASSERT_EQUAL_UINT32(2, q.peek(out, 2));  // peek no saca nada
  TEST_ASSERT_EQUAL_UINT32(3, q.size());
  q.pop(2);
  TEST_ASSERT_EQUAL_UINT32(1, q.size());
  TEST_ASSERT_EQUAL_UINT32(1, q.peek(out, 4));
  TEST_ASSERT_EQUAL_UINT8(3, out[0].payload[0]);
  q.pop(99);  // pasarse no rompe
  TEST_ASSERT_TRUE(q.empty());
}

static void test_queue_drops_the_oldest_when_full() {
  ReadingQueue<3> q;
  for (uint8_t i = 1; i <= 5; i++) q.push(tagged(i));
  TEST_ASSERT_EQUAL_UINT32(3, q.size());
  TEST_ASSERT_EQUAL_UINT32(2, q.dropped());
  SignedReading out[3];
  TEST_ASSERT_EQUAL_UINT32(3, q.peek(out, 3));
  TEST_ASSERT_EQUAL_UINT8(3, out[0].payload[0]);
  TEST_ASSERT_EQUAL_UINT8(4, out[1].payload[0]);
  TEST_ASSERT_EQUAL_UINT8(5, out[2].payload[0]);
}

static void test_queue_wraps_around() {
  ReadingQueue<3> q;
  SignedReading out[3];
  q.push(tagged(1));
  q.push(tagged(2));
  q.pop(2);
  q.push(tagged(3));
  q.push(tagged(4));
  q.push(tagged(5));  // da la vuelta al arreglo interno
  TEST_ASSERT_EQUAL_UINT32(0, q.dropped());
  TEST_ASSERT_EQUAL_UINT32(3, q.peek(out, 3));
  TEST_ASSERT_EQUAL_UINT8(3, out[0].payload[0]);
  TEST_ASSERT_EQUAL_UINT8(5, out[2].payload[0]);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_vector_payload_matches_the_contract);
  RUN_TEST(test_decode_roundtrip_with_negative_values);
  RUN_TEST(test_decode_rejects_wrong_length);
  RUN_TEST(test_temperature_conversion);
  RUN_TEST(test_temperature_rejects_invalid_values);
  RUN_TEST(test_humidity_conversion_saturates);
  RUN_TEST(test_coordinates);
  RUN_TEST(test_hex_needs_room_for_prefix_and_nul);
  RUN_TEST(test_ingest_json_matches_the_gateway_format);
  RUN_TEST(test_ingest_json_rejects_bad_input);
  RUN_TEST(test_device_id_validation);
  RUN_TEST(test_flag_bits_do_not_overlap);
  RUN_TEST(test_ingest_json_never_overflows_the_buffer);
  RUN_TEST(test_ingest_json_max_batch_size);
  RUN_TEST(test_queue_keeps_order_and_pops);
  RUN_TEST(test_queue_drops_the_oldest_when_full);
  RUN_TEST(test_queue_wraps_around);
  return UNITY_END();
}
