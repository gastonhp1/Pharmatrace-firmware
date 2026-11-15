# Arquitectura

La PharmaBox viaja con un **cargamento** (un grupo de lotes que va de una organización a la siguiente). Mide,
firma cada medición y un **gateway** las verifica y deja evidencia en Hyperledger Fabric. Este documento cubre la
parte de la caja; el resto está en [`iot/docs/`](https://github.com/gastonhp1/Pharmatrace/tree/main/iot/docs) de
Pharmatrace (arquitectura, formato de lectura y modelo de amenazas).

```
 PharmaBox v2 (este repo)        Celular vinculado            Gateway (backend/ de Pharmatrace)        Fabric
 módulo sobre la tapa + cinto
   sonda: temp, humedad, luz
   precinto: lazo + presión
   payload de 21 B
   SHA-256 + firma P-256
   cola ───── BLE ─────────────▶ relevo ───────────────────▶ POST /api/iot/readings
        ◀──── hora, posición ──                                1. firma contra la clave registrada ──▶ lee
                                                               2. rechaza repeticiones y desorden
        ◀──── token de apertura ◀─ POST /api/box/unlock ────── 3. guarda la lectura
                                                               cada tanto: ancla una raíz de Merkle ─▶ AnchorWindow
                                                                                                       veredicto
```

## Qué hace cada parte

| Parte | Responsabilidad |
|---|---|
| **Caja** | Medir, vigilar el precinto, firmar, no perder lecturas y no repetir `seq`. **No decide si una temperatura está en rango.** |
| **Celular** | Transporte: sube lecturas que no puede alterar. Pone hora y posición. Pide el token de apertura. |
| **Gateway** | Verificar firmas, ordenar, guardar las lecturas crudas y anclar una raíz de Merkle por ventana. Es el único con identidad en Fabric. Decide quién puede abrir. |
| **Chaincode** | Guardar la clave pública de la caja, la política (rango y presupuesto de excursión), las raíces y los eventos, y dar el veredicto `COMPLIANT` / `COMPROMISED`. |

La caja **nunca** tiene una wallet ni firma transacciones. Lo único que firma son lecturas. Cualquiera puede volver
a verificar una lectura con la clave pública del ledger (`GET /api/iot/cargo/:id/proof/:seq?verify=true`), sin
confiar en el gateway ni en el celular.

## Lo que cambió en la v2 y qué se resigna

| Antes | Ahora | Qué se resigna |
|---|---|---|
| ATECC608B: la clave no sale del chip | Clave en la flash del ESP32, firma por software | Quien tenga la placa puede extraer la clave y fabricar lecturas. Mitigación: flash encryption + secure boot. |
| GPS propio | Posición del celular | La caja firma una posición que no puede comprobar. |
| Módem LTE | Relevo por el celular | Sin celular no hay subida (las lecturas se guardan). |
| Cerradura de solenoide | Precinto de cinto | Nada: una cerradura en una caja de telgopor no protegía. |
| 2 × TMP117 | 1 × SHT4x | Sin redundancia para detectar un sensor fallado o manipulado. |
| Reed switch en la tapa | Sensor de luz adentro | También ve un corte en la pared; no ve una apertura a oscuras. |

## Decisiones de la caja

- **`seq` se reserva antes de usarse** y se persiste en NVS. Si el equipo se reinicia justo después, a lo sumo
  queda un hueco; nunca se repite un `seq` con otro contenido.
- **`ts` estrictamente creciente** y un reloj que nunca retrocede. Sin GPS, la hora llega del `iat` de un token de
  apertura (firmado por el backend) o del celular vinculado. Después de un reinicio no hay hora hasta la próxima
  sincronización, y **sin hora no se emite nada**.
- **No se inventan valores.** Si la sonda no responde se saltea la muestra. El formato v1 no tiene «sin dato».
- **El precinto es un estado, no un evento.** Violada queda violada (`SEAL_BROKEN` en cada lectura) hasta un token
  válido, así el dato no se pierde aunque la lectura siguiente no se pueda emitir.
- **Subida con reintento seguro.** `accepted`, `duplicates` y `rejected` son definitivos; un duplicado exacto no es
  error. Con `409`, `403` o sin red, las lecturas se conservan.
- **El período de muestreo importa.** El gateway usa *sample-and-hold*: una lectura fuera de rango cuenta hasta la
  siguiente. Por eso `SAMPLE_INTERVAL_S` debe coincidir con `IOT_SAMPLE_SECONDS`.
- **Cuando pasa algo se muestrea ya.** Un cambio del precinto o una apertura adelantan la lectura para que quede
  firmado cuanto antes.

## Veredicto y qué lo dispara

- Segundos fuera de rango acumulados **>** presupuesto de la política → `COMPROMISED`.
- `SEAL_BROKEN` → `COMPROMISED` en el acto. Con el precinto de cinto, eso es cualquier apertura sin token.
- `LID_OPENED` y `SHOCK` se registran para la auditoría pero no son fatales.
- Los flags de apertura (0x08 otorgada, 0x10 rechazada) viajan firmados; el chaincode todavía no los interpreta.

## Pendiente de definir (requiere cambiar los tres repos a la vez)

- **Origen y edad de la posición.** Un bit de `flags` para «posición del celular» o «sin posición».
- **«Sin dato» de temperatura.** Para poder reportar un precinto violado aunque la sonda esté desconectada.
- **Detección de huecos.** El gateway todavía no marca una ventana con un hueco de `seq` o de tiempo.
- **Eventos de apertura en el chaincode** y detección de apertura forzada.
