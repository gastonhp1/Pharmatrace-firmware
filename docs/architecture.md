# Arquitectura

La PharmaBox viaja con un **cargamento** (un grupo de lotes que va de una organización a la siguiente). Mide,
firma cada medición y un **gateway** las verifica y deja evidencia en Hyperledger Fabric. Este documento cubre la
parte del nodo; el resto está en [`iot/docs/`](https://github.com/gastonhp1/Pharmatrace/tree/main/iot/docs) de Pharmatrace
(arquitectura, formato de lectura y modelo de amenazas).

```
 PharmaBox (este repo)                  Gateway (backend/ de Pharmatrace)              Fabric
 ESP32-S3 + sensores + ATECC608B
   muestrea, arma el payload de 21 B
   SHA-256 + firma P-256 en el chip
   cola ──────────────────────────────▶ POST /api/iot/readings
                                          1. firma contra la clave registrada (GetDevice) ─▶ lee
                                          2. rechaza repeticiones, desorden y relojes futuros
                                          3. guarda la lectura
                                          cada tanto: POST /api/iot/cargo/:id/anchor
                                          identidad DistMSP IotGateway ─────────────────▶ AnchorWindow
                                          (cert con pharmatrace.role=iot-gateway,         ReportDeviceEvent
                                           endosa su org + el regulador)                  veredicto
```

## Qué hace cada capa

| Capa | Responsabilidad |
|---|---|
| **Nodo** | Medir, firmar, no perder lecturas y no repetir `seq`. **No decide si una temperatura está en rango.** |
| **Gateway** | Verificar firmas, ordenar, guardar las lecturas crudas y anclar una raíz de Merkle por ventana. Es el único con identidad en Fabric. |
| **Chaincode** | Guardar la clave pública de la valija, la política (rango y presupuesto de excursión), las raíces y los eventos, y dar el veredicto `COMPLIANT` / `COMPROMISED`. |

El nodo **nunca** tiene una wallet ni firma transacciones. Lo único que firma son lecturas, con una clave que vive en el ATECC608B y no sale del chip. Cualquiera puede volver a verificar una lectura con la clave pública del ledger (`GET /api/iot/cargo/:id/proof/:seq?verify=true`), sin confiar en el gateway.

## Decisiones del nodo

- **`seq` se reserva antes de usarse** y se persiste en NVS. Si el equipo se reinicia justo después, a lo sumo queda un hueco; nunca se repite un `seq` con otro contenido (el gateway lo rechazaría como "Sequence number already used").
- **`ts` estrictamente creciente.** Si el reloj no avanzó (por ejemplo, tras una corrección del GPS), se saltea la muestra en vez de emitir un `ts` repetido, que el gateway rechazaría.
- **No se inventan valores.** Si un sensor no da un dato válido, o no hay hora, se saltea la muestra: queda un hueco de `seq` visible, no una lectura falsa.
- **Sin firma no hay lectura.** Si el ATECC608B no responde, no se emite nada.
- **Subida con reintento seguro.** El gateway responde `200` con `accepted`, `duplicates` y `rejected`. Todo eso es definitivo: reenviar una lectura rechazada la rechazaría otra vez, y un duplicado exacto no cuenta como error. Con `409` (todavía no monitorea un cargamento), `403` o error de red, las lecturas se conservan en la cola.
- **El período de muestreo importa.** El gateway usa *sample-and-hold*: una lectura fuera de rango cuenta hasta la siguiente, y la última de una ventana cuenta un período (`IOT_SAMPLE_SECONDS`). Por eso `SAMPLE_INTERVAL_S` debe coincidir.
- **Eventos latcheados.** Tapa y golpe llegan por interrupción entre dos muestras y se reportan en la siguiente lectura (`flags`), aunque la tapa ya se haya cerrado. El gateway los convierte en eventos (`LID_OPENED`, `SHOCK`, `SEAL_BROKEN`); solo el sello roto es fatal para el veredicto.

## Veredicto y qué lo dispara

- Segundos fuera de rango acumulados **>** presupuesto de la política → `COMPROMISED`.
- `SEAL_BROKEN` → `COMPROMISED` en el acto.
- `LID_OPENED` y `SHOCK` se registran para la auditoría pero no son fatales: la tapa se abre en cada traspaso.

## Pendiente de definir (requiere cambiar los tres repos a la vez)

- **Segundo sensor de temperatura.** El formato v1 tiene un solo campo. Opciones: reportar siempre el peor caso de los dos, o usar un bit libre de `flags` para marcar que no coinciden.
- **Calidad de posición.** Sin fix, hoy se repite la última posición conocida. Un bit de "fix válido" evitaría confundirla con una lectura nueva.
- **Detección de huecos.** El gateway todavía no marca una ventana con un hueco de `seq` o de tiempo; el nodo ya los deja visibles.
- **Apertura autenticada de la PharmaBox** (idea en evaluación: QR en una pantalla e-paper + celular de un actor de la cadena). Hoy una apertura legítima y una forzada son indistinguibles para el chaincode. Habría que agregar eventos de desbloqueo (otorgado y denegado) y correlacionarlos con el reed switch: una tapa abierta sin un desbloqueo previo sería apertura forzada.
