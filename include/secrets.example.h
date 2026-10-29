#pragma once

// Copiar a include/secrets.h (ignorado por git) y completar.

// Mismo deviceId con el que se registra la valija en el ledger (POST /api/iot/devices).
// Solo letras, números, guion y guion bajo.
#define DEVICE_ID "VAL-001"

// Gateway = backend/ de Pharmatrace. Ejemplo para pruebas en la LAN, sin TLS.
#define GATEWAY_URL "http://192.168.0.10:3001"

// Solo para pruebas de banco por WiFi. En la valija el transporte es el módem celular.
#define WIFI_SSID "mi-wifi"
#define WIFI_PASS "mi-clave"
