// TFT ILI9488 (320x480, SPI, sin táctil) con LovyanGFX. Se compila solo si la librería está en
// lib_deps (platformio.ini). SIN PROBAR EN HARDWARE: revisar pines, rotación e inversión de color
// con tu módulo. El ILI9488 por SPI usa color de 18 bits, por eso refresca más lento que un ILI9341.
#if __has_include(<LovyanGFX.hpp>)

#include <Arduino.h>
#include <LovyanGFX.hpp>

#include "config.h"
#include "display.h"

namespace {

class Panel : public lgfx::LGFX_Device {
 public:
  Panel() {
    auto bus = bus_.config();
    bus.spi_host = SPI2_HOST;
    bus.spi_mode = 0;
    bus.freq_write = 27000000;
    bus.pin_sclk = PIN_TFT_SCK;
    bus.pin_mosi = PIN_TFT_MOSI;
    bus.pin_miso = -1;  // no se lee de la pantalla
    bus.pin_dc = PIN_TFT_DC;
    bus_.config(bus);
    panel_.setBus(&bus_);

    auto panel = panel_.config();
    panel.pin_cs = PIN_TFT_CS;
    panel.pin_rst = PIN_TFT_RST;
    panel.pin_busy = -1;
    panel.panel_width = 320;
    panel.panel_height = 480;
    panel.readable = false;
    panel.bus_shared = false;
    panel_.config(panel);

    auto light = light_.config();
    light.pin_bl = PIN_TFT_BL;
    light_.config(light);
    panel_.setLight(&light_);

    setPanel(&panel_);
  }

 private:
  lgfx::Panel_ILI9488 panel_;
  lgfx::Bus_SPI bus_;
  lgfx::Light_PWM light_;
};

class Ili9488Display : public Display {
 public:
  void begin() override {
    lcd_.init();
    lcd_.setRotation(0);  // vertical: 320 de ancho, 480 de alto
    off();
  }

  void showQr(const char* qrText, const StatusView& status) override {
    wake();
    lcd_.fillScreen(TFT_WHITE);
    lcd_.setTextColor(TFT_BLACK, TFT_WHITE);
    lcd_.setTextDatum(textdatum_t::top_center);
    lcd_.setFont(&fonts::FreeSansBold12pt7b);
    lcd_.drawString("Escanea para abrir", 160, 10);
    lcd_.qrcode(qrText, 30, 44, 260);  // el texto del QR tiene 51 caracteres como máximo
    footer(status, 320);
  }

  void showMessage(const char* text, const StatusView& status) override {
    wake();
    lcd_.fillScreen(TFT_WHITE);
    lcd_.setTextColor(TFT_BLACK, TFT_WHITE);
    lcd_.setTextDatum(textdatum_t::middle_center);
    lcd_.setFont(&fonts::FreeSansBold18pt7b);
    lcd_.drawString(text, 160, 150);
    footer(status, 320);
  }

  void off() override { lcd_.setBrightness(0); }

 private:
  void wake() { lcd_.setBrightness(180); }

  void footer(const StatusView& s, int y) {
    lcd_.setTextDatum(textdatum_t::top_left);
    lcd_.setFont(&fonts::FreeSans12pt7b);
    char line[48];
    if (s.haveReading) {
      snprintf(line, sizeof line, "%.1f C   %.0f %% HR", s.tempCenti / 100.0, s.humCenti / 100.0);
    } else {
      snprintf(line, sizeof line, "Sin lectura");
    }
    lcd_.drawString(line, 12, y);
    lcd_.setTextColor(s.seal == pharma::SealState::Breached ? TFT_RED : TFT_BLACK, TFT_WHITE);
    lcd_.drawString(sealStateText(s.seal), 12, y + 30);
    lcd_.setTextColor(TFT_BLACK, TFT_WHITE);
    snprintf(line, sizeof line, "Bateria %u%%   En cola %u", s.batteryPercent, s.queued);
    lcd_.drawString(line, 12, y + 60);
    lcd_.drawString(s.clockValid ? "Hora sincronizada" : "SIN HORA: abrir con la app", 12, y + 90);
    snprintf(line, sizeof line, "Celular: %s   PIN %06lu", s.phoneLinked ? "si" : "no",
             (unsigned long)s.pairingPin);
    lcd_.drawString(line, 12, y + 120);
  }

  Panel lcd_;
};

}  // namespace

Display& boxDisplay() {
  static Ili9488Display d;
  return d;
}

#endif
