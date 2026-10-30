# Apertura autenticada de la PharmaBox (opción A: token por BLE)

Estado: **diseño + firmware de la caja + herramienta de prueba**. El backend y la app Android ya existen en
Pharmatrace (ver [Estado del lado de Pharmatrace](#estado-del-lado-de-pharmatrace)); el chaincode todavía no
interpreta los eventos de apertura.

## Idea en una línea

La caja muestra un QR con un número aleatorio (nonce). El celular, que tiene internet, se autentica con el
backend y le pide un token para *ese* nonce. El backend decide si el usuario puede abrir y, si puede, firma el
token. El celular se lo pasa a la caja por Bluetooth; la caja verifica la firma sin internet y abre.

```
 PharmaBox                       Celular (app)                    Backend Pharmatrace
    │  botón → nonce nuevo            │                                   │
    │  e-paper: QR "PB1:id:nonce"     │                                   │
    │ ◄──────── escanea el QR ────────│                                   │
    │                                 │── POST /api/box/unlock ──────────►│ ¿quién es? ¿puede abrir
    │                                 │   {deviceId, nonce}  (con sesión) │  este lote en este estado?
    │                                 │◄───────── token firmado ──────────│ audita (otorgado/denegado)
    │◄── BLE: escribe token ──────────│                                   │
    │  verifica firma, caja, nonce    │                                   │
    │  abre el pestillo               │                                   │
    │── BLE notify: resultado ───────►│                                   │
    │  próxima lectura con flag       │                                   │
    │  UNLOCK_GRANTED (0x08)          │                                   │
```

Ventajas: la caja no guarda claves de usuarios ni necesita internet para abrir (solo la clave **pública** del
backend); las reglas de quién puede abrir viven en un solo lugar. Costo: **el celular necesita internet** y hay
que desarrollar una app (ver riesgos).

## Quién puede abrir

La regla vive en el backend de Pharmatrace (`backend/box/policy.js`, documentada en `iot/docs/BOX_UNLOCK.md`);
la caja solo comprueba la firma:

| Estado | Puede abrir |
|---|---|
| Caja sin cargamento activo | un usuario de la organización que la registró |
| `OPEN` | un usuario de la organización dueña |
| `IN_TRANSIT` | un usuario de la organización destinataria (`PendingTo`); ni el remitente ni el transportista |
| `DELIVERED` | un usuario del dueño actual, que tras `ReceiveCargo` es quien recibió (descarga) |
| `DISSOLVED` | un usuario del dueño |
| cualquiera | el regulador (`RegMSP`), siempre |

Toda decisión, otorgada o denegada, se audita en el backend. (El primer borrador de este documento dejaba
`DELIVERED` y `DISSOLVED` solo al regulador; se cambió porque quien recibe no podría descargar la caja mientras el
monitoreo siga abierto.)

## QR

Texto: `PB1:<deviceId>:<nonce en base64url>`, por ejemplo `PB1:VAL-001:AAECAwQFBgcICQoLDA0ODw` (51 caracteres
como máximo). `deviceId` usa solo `[A-Za-z0-9_-]`, 1 a 24 caracteres (el mismo que se registra en el ledger).
El nonce son 16 bytes aleatorios generados al apretar el botón; vive 120 s y sirve una sola vez.

## BLE (GATT)

| | UUID | Propiedades |
|---|---|---|
| Servicio PharmaBox | `f30e1410-8085-4552-9d03-abe1d447ce2d` | |
| `unlock` | `e7cd3dce-1611-4770-beb3-8d88cb840550` | escritura: el token de 106 bytes |
| `status` | `71b4af6b-3c8f-4880-96e3-df64ba0b4459` | lectura + notificación: 1 byte, código de resultado |

La caja solo anuncia el servicio mientras hay un QR vigente. Un token de 106 bytes necesita MTU ≥ 109 o
escritura larga; NimBLE y los celulares modernos lo manejan, pero **no se probó con hardware**.

## Token (106 bytes, big-endian)

| Offset | Bytes | Campo | Contenido |
|---:|---:|---|---|
| 0 | 1 | `version` | 1 |
| 1 | 1 | `action` | 1 = abrir |
| 2 | 8 | `dev` | primeros 8 bytes de SHA-256(`deviceId`): ata el token a una caja |
| 10 | 16 | `nonce` | el del QR |
| 26 | 4 | `iat` | emitido (segundos Unix) |
| 30 | 4 | `exp` | vence (segundos Unix) |
| 34 | 8 | `actor` | primeros 8 bytes de SHA-256(`mspId:usuario`): auditoría sin datos personales |
| 42 | 64 | firma | ECDSA P-256 de SHA-256(bytes 0..41), cruda `r‖s` |

La clave de firma del backend es **una clave aparte** (no la del gateway Fabric ni la de la caja). La caja
tiene solo la pública (`include/unlock_key.h`).

## Qué valida la caja (en este orden)

1. Hay un QR vigente y no se agotaron los intentos → si no, `NoChallenge` / `LockedOut`.
2. Largo 106, `version` 1, `action` 1, `exp > iat` y vida ≤ 300 s → `BadFormat`.
3. `dev` corresponde a esta caja → `WrongBox`.
4. `nonce` es el que está en pantalla (comparación en tiempo constante) → `BadNonce`.
5. Firma válida con la clave pública del backend → `BadSignature`.
6. Si la caja tiene hora válida (GPS/RTC), `exp` no venció → `Expired`. Sin hora válida no se mira `exp`: la
   frescura la da el nonce.

Éxito: el nonce queda consumido (un token sirve una vez), se energiza el solenoide y la próxima lectura sale con
`FLAG_UNLOCK_GRANTED`. Tres fallos y la caja exige un QR nuevo (botón).

| Código | Nombre |
|---:|---|
| 0 | Ok |
| 1 | NoChallenge |
| 2 | BadFormat |
| 3 | WrongBox |
| 4 | BadNonce |
| 5 | Expired |
| 6 | BadSignature |
| 7 | LockedOut |

## Flags nuevos en la lectura firmada

| Bit | Nombre | Significado |
|---:|---|---|
| 0x08 | `FLAG_UNLOCK_GRANTED` | hubo una apertura autorizada desde la lectura anterior |
| 0x10 | `FLAG_UNLOCK_REJECTED` | hubo un intento rechazado |

Al estar dentro del payload firmado, quedan anclados en el ledger como cualquier otra lectura. **Verificado
contra el código del gateway** (`backend/iot/reading.js` y `window.js`): no valida los bits de `flags` y solo
interpreta los tres primeros, así que 0x08/0x10 se aceptan y se ignoran. Hoy no generan eventos en el
chaincode. Una tapa abierta (`LID_OPENED`) sin un `UNLOCK_GRANTED` cerca sería una **apertura forzada**.

## Estado del lado de Pharmatrace

Ya implementado y mergeado en Pharmatrace (PR #17): sesiones de usuario por organización, `POST /api/box/unlock`,
la política de arriba, auditoría, firma del token y `READING_FORMAT.md` con los bits 0x08/0x10. La app Android
está en PharmaTrace-mobile. Un token emitido por el backend real (`box/token.js`) se verificó contra el
verificador mbedtls de este repo (`tools/e2e_unlock.cpp`); falta probar con una caja real.

Sigue pendiente:

- Chaincode: eventos `UNLOCK_GRANTED` / `UNLOCK_DENIED` y detección de apertura forzada (`LID_OPENED` sin
  `UNLOCK_GRANTED`). Hoy los flags viajan anclados pero nadie los interpreta.
- Clave de apertura en KMS/HSM.
- iOS: Web Bluetooth no existe; haría falta una app nativa.

## Riesgos conocidos

- **Ataque de relevo**: alguien con permiso podría escanear el QR de una caja que no tiene delante (una foto) y
  abrirla a distancia con un cómplice cerca. Mitigaciones: nonce de 120 s, un solo uso, 3 intentos; BLE tiene
  alcance corto; para más garantía, exigir que el celular esté cerca (RSSI) o agregar un PIN en pantalla.
- La cerradura electrónica **no es seguridad física**: una caja abierta a la fuerza queda registrada, no evitada.
- El celular necesita internet en el momento de abrir.
- El nonce sale de `esp_fill_random()` con BLE encendido; falta pasarlo al TRNG del ATECC608B.
- Todo lo de BLE, e-paper, cerradura y ATECC608B es **código sin probar en hardware**.

## Probar sin app ni pantalla

```bash
python3 tools/unlock_token.py gen-key clave.pem         # imprime la pública → include/unlock_key.h
python3 tools/unlock_token.py sign clave.pem "PB1:VAL-001:<nonce del QR>"   # token en hex
```

Con la caja corriendo, el QR aparece por serie (`[display] QR: …`). Pegar el token hex en la característica
`unlock` con nRF Connect y leer el resultado en `status`.
