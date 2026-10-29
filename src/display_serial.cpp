#include <Arduino.h>

#include "display.h"

void SerialDisplay::showQr(const char* text) { Serial.printf("[display] QR: %s\n", text); }
void SerialDisplay::showMessage(const char* text) { Serial.printf("[display] %s\n", text); }
void SerialDisplay::clear() { Serial.println("[display] (limpia)"); }
