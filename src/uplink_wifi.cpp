#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "secrets.h"
#include "uplink.h"

bool UplinkWifi::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);  // no bloquea: postJson() reintenta mientras no haya conexión
  return true;
}

bool UplinkWifi::postJson(const char* path, const char* body, size_t len, int& status,
                          char* response, size_t responseCap) {
  if (response != nullptr && responseCap > 0) response[0] = '\0';
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  const String url = String(GATEWAY_URL) + path;
  if (!http.begin(url)) return false;
  http.addHeader("Content-Type", "application/json");
  const int code = http.POST(reinterpret_cast<const uint8_t*>(body), len);
  if (code <= 0) {  // error de red, no respuesta HTTP
    http.end();
    return false;
  }
  status = code;
  if (response != nullptr && responseCap > 0) {
    const String text = http.getString();
    strlcpy(response, text.c_str(), responseCap);
  }
  http.end();
  return true;
}
