# Guía del prototipo (etapa 1)

Objetivo: armar la PharmaBox v2 en protoboard con lo que ya hay y contestar tres preguntas antes de diseñar una
placa propia. No es un producto: es para aprender qué falla.

1. ¿El precinto da falsas alarmas?
2. ¿Cuánto dura la batería?
3. ¿El celular puede relevar las lecturas en segundo plano? (hoy **no se puede medir**: falta la app, ver abajo)

Todo el código de ESP32 de este repo está **sin compilar con las librerías reales y sin probar en hardware**. El
primer resultado de esta guía es justamente ese: que compile y arranque. Anotá lo que falle.

## Lista de compras

| Pieza | Qué buscar | Cant. |
|---|---|---|
| ESP32-S3-DevKitC-1 | (ya lo tenés) | 1 |
| Pantalla ILI9488 SPI 3,5" sin táctil | (ya la tenés) | 1 |
| Sensor de temperatura y humedad | Módulo SHT40 o SHT41 o SHT45 (I2C, 0x44) | 1 |
| Sensor de luz | Módulo VEML7700 (I2C, 0x10) | 1 |
| Acelerómetro | Módulo LIS3DH (I2C) | 1 |
| Resistencias | 10 kΩ × 3, 100 kΩ × 2 | |
| Hebilla eléctrica | Plug y jack de audio de 3,5 mm (mono alcanza) | 1 par |
| Cinto | Cinta de nylon de 25 mm con hebilla plástica, largo para rodear la caja | 1 |
| Cable del lazo | Cable fino multifilar, del largo del cinto | |
| Cable de la sonda | Cable plano de 4 hilos, 40–50 cm | |
| Botón y buzzer activo | Pulsador y buzzer de 3,3 V | 1 y 1 |
| Protoboard y cables | El DevKitC-1 es ancho: conviene usar dos protoboards unidas o cables hembra | |
| Batería (para la medición 2) | LiPo de una celda con módulo cargador con protección y salida de 5 V | 1 |
| Caja de telgopor | La que se use para medicamentos, con refrigerantes | 1 |

El sensor de presión (FSR402) queda para después: en el firmware viene desactivado (`SEAL_USE_FSR = false`).

## Armado

El cableado está en [`wiring.es.svg`](wiring.es.svg) y los pines en [`include/config.h`](../include/config.h).
En este orden, probando cada paso antes de seguir:

1. **Solo el DevKit por USB.** Compilar y cargar (ver «Firmware»). Tiene que aparecer el arranque por serie.
2. **Pantalla.** VCC a 3V3, GND, CS 42, RESET 40, DC 41, SDI/MOSI 39, SCK 38. SDO/MISO sin conectar.
   **Ojo con el pin LED:** la retroiluminación consume 40–80 mA y un GPIO no debería entregar más de ~20 mA. Si tu
   módulo no trae un transistor para ese pin, para la primera prueba conectá LED directo a 3V3 (queda siempre
   encendida) y después poné un transistor o MOSFET manejado por el GPIO21.
3. **LIS3DH** en el bus I2C (SDA 8, SCL 9), INT1 al GPIO5. Dirección 0x18 (SA0 a GND).
4. **Sonda.** SHT4x y VEML7700 al final del cable plano: 3V3, GND, SDA, SCL. Con 40–50 cm el I2C funciona; si
   aparecen errores de lectura, acortar el cable o bajar la velocidad del bus.
5. **Lazo** (dibujo en [`seal-loop.es.svg`](seal-loop.es.svg)). 3V3 → 10 kΩ → GPIO4. Del GPIO4 sale el cable que recorre el cinto de punta a punta y termina en la
   punta del plug de 3,5 mm, con una resistencia de 10 kΩ soldada **adentro del plug**, entre punta y masa. El
   jack va en el módulo, con su contacto de masa a GND: al enchufar, el cinto cierra el anillo alrededor de la
   caja y el circuito a la vez. Cortar el cinto corta el cable. Enchufado: ~1,65 V. Desenchufado: ~3,3 V.
6. **Botón** entre GPIO2 y GND. **Buzzer** entre GPIO47 y GND (con transistor si consume más de ~20 mA).
7. **Batería** recién para la medición 2: salida de 5 V del cargador al pin 5V; divisor 100 kΩ + 100 kΩ desde
   VBAT al GPIO7.

## Firmware

```bash
cp include/secrets.example.h include/secrets.h        # DEVICE_ID, GATEWAY_URL, WIFI_SSID, WIFI_PASS
cp include/unlock_key.example.h include/unlock_key.h  # clave pública de apertura (ver abajo)
```

En `platformio.ini`, descomentar para esta etapa:

```ini
    -DBENCH_WIFI_UPLINK    ; sube directo por WiFi y toma la hora de NTP: no hace falta celular
    -DSEAL_DEBUG           ; una línea por segundo con lo que mide el precinto
```

```bash
pio run -t upload && pio device monitor
```

Qué mirar en el arranque:

- `[boot] deviceId=... pubKey=0x...` → la clave pública de la caja. Se genera una sola vez y queda guardada.
- `[seal-debug] lazo=1650 mV presion=... sonda=1 lux=0 estado=0` → el precinto midiendo.
- `[sample] seq=0 ts=... temp=... hum=...` → la primera lectura firmada (necesita WiFi para tener hora).

Si la pantalla no muestra nada, el firmware igual imprime por serie lo que mostraría (`[display] ...`).

## Conectarlo con Pharmatrace

Con el backend de Pharmatrace corriendo en la misma red (pasos completos en su `iot/README.md`):

1. Registrar la caja con la `pubKey` del arranque: `POST /api/iot/devices`.
2. Crear un cargamento y empezar a monitorearlo con esa caja: `POST /api/iot/monitoring/start`.
3. Esperar: el firmware sube cada 10 minutos (`UPLOAD_INTERVAL_S`). Tiene que aparecer `[upload] HTTP 200`.
   Un `409` significa que la caja todavía no está monitoreando un cargamento.
4. Ver el estado: `GET /api/iot/cargo/<id>`.

**Precintar antes de empezar a monitorear.** Una caja con el cinto suelto está en «violado», y un `SEAL_BROKEN`
deja el cargamento `COMPROMISED` en el acto.

Para la clave de apertura: `node scripts/unlock-key.js generate data/unlock-key.pem` en el backend imprime el
contenido de `include/unlock_key.h`. Para probar una apertura sin la app, ver
[`unlock-protocol.md`](unlock-protocol.md#probar-sin-app-ni-pantalla).

## Medición 1: falsas alarmas del precinto

1. **Calibrar.** Con `SEAL_DEBUG`, anotar el valor del lazo en cuatro situaciones: enchufado, desenchufado,
   puenteado con un cable, y moviendo el cinto y el plug con la mano. Los umbrales por defecto son 1300–2000 mV
   para «cerrado». Si los valores reales quedan cerca de los bordes, ajustar `SealConfig` en
   `lib/pharma_proto/src/pharma_seal.h`.
2. **Luz.** Con la caja cerrada, `lux` tiene que dar 0 o muy poco. Abrir la tapa en una habitación iluminada y en
   penumbra: anotar los valores. El umbral por defecto es 10 lux.
3. **Uso real.** Caja cerrada y precintada, una semana de uso normal: en un auto, apoyada, apilada, cargada y
   descargada. Guardar el log por serie o mirar el estado en el backend.
4. **Contar.** Cada línea `[seal] VIOLADO lazo=... sonda=... lux=...` sin que nadie haya abierto es una falsa
   alarma. La línea dice qué señal la disparó.

Criterio: **cero** falsas alarmas en la semana. Con una sola ya hay que cambiar algo: subir el antirrebote
(`debounceMs`, hoy 500 ms), mejorar el contacto del plug o correr un umbral.

Probar también lo contrario: abrir la tapa sin soltar el cinto, soltar el cinto, desenchufar la sonda, hacer un
agujero chico en una caja de descarte. Todas tienen que dar `VIOLADO`.

## Medición 2: batería

El firmware de hoy **no duerme nunca** y mantiene el Bluetooth anunciando, así que el consumo va a ser alto. El
objetivo de esta medición es tener el número de partida, no un buen número.

1. Poner un multímetro en serie con la salida de la batería (escala de mA), o un medidor USB en la entrada.
2. Anotar el consumo en tres estados: pantalla apagada en reposo, pantalla encendida mostrando el QR, y durante
   una subida por WiFi.
3. Autonomía estimada en horas = capacidad de la batería en mAh ÷ consumo en reposo en mA × 0,8.

Con ese número se decide cuánto hay que ganar con *deep sleep* (pendiente en el firmware) y si el ESP32-S3
alcanza o conviene un micro solo BLE.

## Medición 3: relevo por el celular (bloqueada)

Es el supuesto más frágil del diseño y **hoy no se puede probar**: la app PharmaTrace-mobile solo abre la caja,
no implementa el servicio del celular vinculado ([`companion-protocol.md`](companion-protocol.md)). Lo que hay
que medir cuando exista:

- Con la app en segundo plano y la pantalla apagada, ¿siguen llegando lecturas al backend durante 8 horas?
- ¿Qué pasa cuando Android la mata por ahorro de batería? ¿Se recupera sola al volver al alcance?
- ¿Cuánta batería del celular consume?

Mientras tanto se puede comprobar con nRF Connect que la caja se anuncia, que la vinculación pide el PIN de la
pantalla y que la característica `box` devuelve el estado.

## Qué anotar

| | Resultado |
|---|---|
| ¿Compiló? Errores y en qué archivo | |
| Valores del lazo: enchufado / desenchufado / puenteado | |
| Lux con la tapa cerrada / abierta | |
| Falsas alarmas en una semana, y qué señal | |
| Consumo: reposo / pantalla / subida | |
| ¿El backend aceptó las lecturas? | |

Con esa tabla completa se puede decidir la etapa 2 con datos.
