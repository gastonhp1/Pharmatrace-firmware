// Precinto, reloj y SHT4x: lógica pura, se prueba en la PC (pio test -e native).
#include <unity.h>

#include "pharma_clock.h"
#include "pharma_seal.h"
#include "pharma_sht4x.h"

using namespace pharma;

void setUp() {}
void tearDown() {}

static const SealInputs CLOSED{1650, 1200, true, 0};
static const SealInputs STRAP_OPEN{3300, 1200, true, 0};
static const SealInputs STRAP_BRIDGED{20, 1200, true, 0};
static const SealInputs STRAP_LOOSE{1650, 50, true, 0};
static const SealInputs PROBE_GONE{1650, 1200, false, 0};
static const SealInputs LIGHT_INSIDE{1650, 1200, true, 180};

// Sostiene una entrada más que el antirrebote y devuelve los eventos acumulados.
static uint8_t hold(SealMonitor& m, const SealInputs& in, uint32_t& t, uint32_t ms = 1000) {
  uint8_t ev = 0;
  for (uint32_t i = 0; i <= ms; i += 100) ev |= m.update(in, t + i);
  t += ms;
  return ev;
}

void test_loop_classification() {
  SealConfig c;
  TEST_ASSERT_EQUAL_INT(int(LoopState::Closed), int(classifyLoop(1650, c)));
  TEST_ASSERT_EQUAL_INT(int(LoopState::Closed), int(classifyLoop(1300, c)));
  TEST_ASSERT_EQUAL_INT(int(LoopState::Closed), int(classifyLoop(2000, c)));
  TEST_ASSERT_EQUAL_INT(int(LoopState::Open), int(classifyLoop(2001, c)));
  TEST_ASSERT_EQUAL_INT(int(LoopState::Open), int(classifyLoop(3300, c)));
  TEST_ASSERT_EQUAL_INT(int(LoopState::Shorted), int(classifyLoop(1299, c)));
  TEST_ASSERT_EQUAL_INT(int(LoopState::Shorted), int(classifyLoop(0, c)));
}

void test_closed_box_stays_sealed() {
  SealMonitor m;
  uint32_t t = 0;
  TEST_ASSERT_EQUAL_UINT8(0, hold(m, CLOSED, t, 5000));
  TEST_ASSERT_EQUAL_INT(int(SealState::Sealed), int(m.state()));
  TEST_ASSERT_EQUAL_UINT8(0, m.stateFlags());
  TEST_ASSERT_TRUE(m.intact());
}

static void expectBreach(const SealInputs& in) {
  SealMonitor m;
  uint32_t t = 0;
  hold(m, CLOSED, t);
  hold(m, in, t);
  TEST_ASSERT_EQUAL_INT(int(SealState::Breached), int(m.state()));
  TEST_ASSERT_EQUAL_UINT8(FLAG_SEAL_BROKEN, m.stateFlags());
}

void test_opening_without_token_is_a_breach() { expectBreach(STRAP_OPEN); }
void test_bridged_loop_is_a_breach() { expectBreach(STRAP_BRIDGED); }
void test_loose_strap_is_a_breach() { expectBreach(STRAP_LOOSE); }
void test_unplugged_probe_is_a_breach() { expectBreach(PROBE_GONE); }

void test_light_inside_is_a_breach_and_a_lid_event() {
  SealMonitor m;
  uint32_t t = 0;
  hold(m, CLOSED, t);
  TEST_ASSERT_EQUAL_UINT8(FLAG_LID_OPENED, hold(m, LIGHT_INSIDE, t));  // un corte lateral también entra acá
  TEST_ASSERT_EQUAL_INT(int(SealState::Breached), int(m.state()));
  TEST_ASSERT_TRUE(m.lidOpen());
}

void test_breach_stays_after_closing_again() {
  SealMonitor m;
  uint32_t t = 0;
  hold(m, CLOSED, t);
  hold(m, STRAP_OPEN, t);
  hold(m, CLOSED, t, 60000);
  TEST_ASSERT_EQUAL_INT(int(SealState::Breached), int(m.state()));
  TEST_ASSERT_EQUAL_UINT8(FLAG_SEAL_BROKEN, m.stateFlags());
}

void test_short_glitch_is_ignored() {
  SealMonitor m;
  uint32_t t = 0;
  hold(m, CLOSED, t);
  m.update(STRAP_LOOSE, t + 100);  // un golpe: 300 ms, menos que el antirrebote
  m.update(STRAP_LOOSE, t + 400);
  m.update(CLOSED, t + 450);
  t += 500;
  hold(m, CLOSED, t);
  TEST_ASSERT_EQUAL_INT(int(SealState::Sealed), int(m.state()));
}

void test_authorized_opening_and_rearm() {
  SealMonitor m;
  uint32_t t = 0;
  hold(m, CLOSED, t);
  m.authorize(t);
  hold(m, STRAP_OPEN, t);
  TEST_ASSERT_EQUAL_INT(int(SealState::AuthorizedOpen), int(m.state()));
  TEST_ASSERT_EQUAL_UINT8(0, m.stateFlags());
  // Con el cinto ya abierto levantan la tapa: evento de tapa, pero sigue autorizada.
  const SealInputs lidUp{3300, 0, true, 300};
  TEST_ASSERT_EQUAL_UINT8(FLAG_LID_OPENED, hold(m, lidUp, t));
  TEST_ASSERT_EQUAL_INT(int(SealState::AuthorizedOpen), int(m.state()));
  // Puede quedar abierta más que la ventana: la ventana limita cuándo EMPIEZA la apertura.
  hold(m, lidUp, t, 700000);
  TEST_ASSERT_EQUAL_INT(int(SealState::AuthorizedOpen), int(m.state()));
  hold(m, CLOSED, t);
  TEST_ASSERT_EQUAL_INT(int(SealState::Sealed), int(m.state()));
  // El permiso se gastó: volver a abrir es una violación.
  hold(m, STRAP_OPEN, t);
  TEST_ASSERT_EQUAL_INT(int(SealState::Breached), int(m.state()));
}

void test_authorization_expires_if_unused() {
  SealMonitor m;
  uint32_t t = 0;
  hold(m, CLOSED, t);
  m.authorize(t);
  hold(m, CLOSED, t, 600000);  // pasan los 10 minutos sin abrir
  hold(m, STRAP_OPEN, t);
  TEST_ASSERT_EQUAL_INT(int(SealState::Breached), int(m.state()));
}

void test_valid_token_clears_a_breach() {
  SealMonitor m;
  uint32_t t = 0;
  hold(m, CLOSED, t);
  hold(m, STRAP_OPEN, t);
  m.authorize(t);  // todavía abierta
  TEST_ASSERT_EQUAL_INT(int(SealState::AuthorizedOpen), int(m.state()));
  hold(m, CLOSED, t);
  TEST_ASSERT_EQUAL_INT(int(SealState::Sealed), int(m.state()));

  hold(m, STRAP_OPEN, t);
  hold(m, CLOSED, t);  // violada y vuelta a cerrar
  m.authorize(t);
  TEST_ASSERT_EQUAL_INT(int(SealState::Sealed), int(m.state()));
}

void test_window_survives_millis_wraparound() {
  SealMonitor m;
  uint32_t t = 0xFFFFF000u;
  hold(m, CLOSED, t, 1000);
  m.authorize(t);
  hold(m, STRAP_OPEN, t, 6000);  // cruza el desborde
  TEST_ASSERT_EQUAL_INT(int(SealState::AuthorizedOpen), int(m.state()));
}

void test_clock_needs_a_sync() {
  BoxClock c;
  uint32_t now = 0;
  TEST_ASSERT_FALSE(c.now(5000, now));
  TEST_ASSERT_FALSE(c.set(1000, 5000));  // absurdo
  TEST_ASSERT_FALSE(c.valid());
  TEST_ASSERT_TRUE(c.set(1800000000u, 5000));
  TEST_ASSERT_TRUE(c.now(5000, now));
  TEST_ASSERT_EQUAL_UINT32(1800000000u, now);
}

void test_clock_counts_and_never_goes_back() {
  BoxClock c;
  uint32_t now = 0;
  c.set(1800000000u, 1000);
  c.now(1999, now);
  TEST_ASSERT_EQUAL_UINT32(1800000000u, now);
  c.now(2000, now);
  TEST_ASSERT_EQUAL_UINT32(1800000001u, now);
  c.now(62500, now);
  TEST_ASSERT_EQUAL_UINT32(1800000061u, now);
  TEST_ASSERT_FALSE(c.set(1800000000u, 62500));  // atrasaría el reloj
  TEST_ASSERT_TRUE(c.set(1800000100u, 62500));
  c.now(63500, now);
  TEST_ASSERT_EQUAL_UINT32(1800000101u, now);
}

void test_clock_survives_millis_wraparound() {
  BoxClock c;
  uint32_t now = 0;
  c.set(1800000000u, 0xFFFFFC18u);  // 1000 ms antes del desborde
  c.now(1000, now);                 // 2000 ms después
  TEST_ASSERT_EQUAL_UINT32(1800000002u, now);
}

void test_sht4x_crc_reference() {
  const uint8_t ref[] = {0xBE, 0xEF};
  TEST_ASSERT_EQUAL_UINT8(0x92, sht4xCrc8(ref, 2));  // ejemplo de la hoja de datos de Sensirion
}

static void frameFor(uint16_t rawT, uint16_t rawH, uint8_t f[6]) {
  f[0] = uint8_t(rawT >> 8); f[1] = uint8_t(rawT); f[2] = sht4xCrc8(f, 2);
  f[3] = uint8_t(rawH >> 8); f[4] = uint8_t(rawH); f[5] = sht4xCrc8(f + 3, 2);
}

void test_sht4x_decode() {
  uint8_t f[6];
  float t = 0, h = 0;
  frameFor(18724, 32507, f);  // ~5,0 °C y ~56,0 %
  TEST_ASSERT_TRUE(sht4xDecode(f, t, h));
  TEST_ASSERT_TRUE(t > 4.99f && t < 5.01f);
  TEST_ASSERT_TRUE(h > 55.99f && h < 56.01f);
  frameFor(0, 0, f);
  TEST_ASSERT_TRUE(sht4xDecode(f, t, h));
  TEST_ASSERT_TRUE(t > -45.01f && t < -44.99f);
  TEST_ASSERT_TRUE(h == 0.0f);  // -6 % se recorta a 0
  frameFor(65535, 65535, f);
  TEST_ASSERT_TRUE(sht4xDecode(f, t, h));
  TEST_ASSERT_TRUE(h == 100.0f);  // 119 % se recorta a 100
}

void test_sht4x_rejects_bad_crc() {
  uint8_t f[6];
  float t = 0, h = 0;
  frameFor(18724, 32507, f);
  f[1] ^= 0x01;
  TEST_ASSERT_FALSE(sht4xDecode(f, t, h));
  frameFor(18724, 32507, f);
  f[5] ^= 0x80;
  TEST_ASSERT_FALSE(sht4xDecode(f, t, h));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_loop_classification);
  RUN_TEST(test_closed_box_stays_sealed);
  RUN_TEST(test_opening_without_token_is_a_breach);
  RUN_TEST(test_bridged_loop_is_a_breach);
  RUN_TEST(test_loose_strap_is_a_breach);
  RUN_TEST(test_unplugged_probe_is_a_breach);
  RUN_TEST(test_light_inside_is_a_breach_and_a_lid_event);
  RUN_TEST(test_breach_stays_after_closing_again);
  RUN_TEST(test_short_glitch_is_ignored);
  RUN_TEST(test_authorized_opening_and_rearm);
  RUN_TEST(test_authorization_expires_if_unused);
  RUN_TEST(test_valid_token_clears_a_breach);
  RUN_TEST(test_window_survives_millis_wraparound);
  RUN_TEST(test_clock_needs_a_sync);
  RUN_TEST(test_clock_counts_and_never_goes_back);
  RUN_TEST(test_clock_survives_millis_wraparound);
  RUN_TEST(test_sht4x_crc_reference);
  RUN_TEST(test_sht4x_decode);
  RUN_TEST(test_sht4x_rejects_bad_crc);
  return UNITY_END();
}
