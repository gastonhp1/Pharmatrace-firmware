# pharmatrace-firmware

Firmware de la valija IoT de cadena de frío para [Pharmatrace](https://github.com/gastonhp1/Pharmatrace).

La valija sensa el estado del lote durante el transporte (temperatura, humedad, apertura, golpes, ubicación) y reporta lecturas firmadas que terminan registradas en los contratos inteligentes de Pharmatrace.

## Estado

Scaffolding inicial. Nada de esto está validado en hardware todavía.

## Supuestos (a confirmar)

- MCU: **ESP32** (DevKit v1 para prototipar)
- Toolchain: **PlatformIO** + framework Arduino
- Sensores previstos: temperatura/humedad (SHT31 o DS18B20), reed switch de tapa, acelerómetro (golpes), GPS
- Transporte: WiFi/LTE → MQTT o HTTPS hacia un backend/oráculo. El ESP32 **no** habla directo con la blockchain.

## Estructura

```
include/config.h      Pines, intervalos y umbrales
src/main.cpp          Ciclo principal: sensar → evaluar → reportar
docs/architecture.md  Flujo de datos y decisiones de diseño
platformio.ini        Entorno de build
```

## Build

```bash
pip install platformio
pio run                 # compilar
pio run -t upload       # flashear
pio device monitor      # serial a 115200
```

## Relación con el repo principal

Pharmatrace es un monorepo (contratos, backend, frontend). Este repo es aparte porque el ciclo de build, flasheo y versionado del firmware es independiente.
