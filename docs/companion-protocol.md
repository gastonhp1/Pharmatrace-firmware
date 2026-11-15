# Celular vinculado: hora, posición y relevo de lecturas

Estado: **diseño + lado de la caja escrito, sin probar en hardware. Ninguna app lo implementa todavía**
(PharmaTrace-mobile hoy solo abre la caja). Este documento es el contrato para cuando se haga.

## Por qué

La PharmaBox v2 no tiene GPS ni módem. El celular de quien lleva la caja cubre las tres cosas que faltan:

| Falta en la caja | Lo pone el celular |
|---|---|
| Salida a internet | Recibe las lecturas firmadas por BLE y las sube a `POST /api/iot/readings` |
| Posición | La manda cada tanto; la caja la incluye en las lecturas siguientes |
| Hora | La manda al conectarse (además del `iat` de cada token de apertura) |

No hace falta confiar en el celular para las lecturas: van firmadas por la caja y el gateway verifica la firma.
Un celular malicioso puede no subirlas, pero no puede alterarlas. **Sí hay que confiar en él para la posición**: la
caja firma una posición que no puede comprobar (ver riesgos).

Si el celular no está, la caja sigue midiendo y firmando, y guarda las lecturas (512 en RAM, unas 8,5 horas; al
llenarse descarta la más vieja). El gateway acepta lecturas atrasadas mientras `seq` y `ts` crezcan.

## Vinculación

LE Secure Connections con PIN: la caja muestra un PIN de 6 dígitos en la pantalla (cambia en cada arranque) y el
celular lo tipea. Queda vinculado: las reconexiones no piden PIN. Las escrituras de este servicio exigen el enlace
cifrado y autenticado; sin eso cualquiera en alcance podría cambiarle la hora a la caja o hacerle descartar
lecturas sin subir.

## GATT

Servicio `a95b3c98-0c98-4d45-82dc-43b5b9a9572c`. Todo en big-endian.

| Característica | UUID | Propiedades | Contenido |
|---|---|---|---|
| `timepos` | `f49a4666-5431-48dd-bddb-82d30d63e1e7` | escritura cifrada | 12 bytes: hora Unix `u32`, `latE5` `i32`, `lonE5` `i32` |
| `box` | `394de1fc-6ded-4431-8ddb-6b33131144e7` | lectura + notificación | 8 bytes: estado de la caja |
| `readings` | `6d76aa5c-cd56-457b-b26b-ddd8870bfab7` | notificación | 85 bytes por lectura: payload (21) + firma (64) |
| `ack` | `eeb7ddfb-3589-471b-94f1-89f30bd1f400` | escritura cifrada | 2 bytes `u16`: lecturas subidas bien |

Estado de la caja (`box`):

| Offset | Bytes | Campo |
|---:|---:|---|
| 0 | 2 | `tempCenti` `i16` |
| 2 | 2 | `humCenti` `u16` |
| 4 | 1 | bits 0-1 precinto (0 precintada, 1 abierta autorizada, 2 violada) · bit 2 hora válida · bit 3 hay medición |
| 5 | 1 | batería, 0 a 100 |
| 6 | 2 | lecturas en cola `u16` |

Las notificaciones de 85 bytes necesitan MTU ≥ 88.

## Relevo de lecturas

Por pedido, de a tandas de hasta 16:

1. El celular se suscribe a `readings` y escribe `ack = 0`.
2. La caja notifica hasta 16 lecturas de la cola, **sin borrarlas**.
3. El celular arma `{ deviceId, readings: [{ payload, signature }] }` en hex y lo manda al gateway.
4. Con `200`, escribe `ack = N` (las que mandó: `accepted`, `duplicates` y `rejected` son definitivas). La caja
   borra esas N y manda la tanda siguiente. Con `409`, `403` o sin red, no escribe `ack`: la caja las conserva.
5. Si la conexión se cae, lo no confirmado se vuelve a mandar en la próxima. El gateway trata un duplicado exacto
   como duplicado, no como error, así que repetir es seguro.

`box` avisa cuántas lecturas hay en cola, para que el celular sepa cuándo pedir.

## Hora y posición

- La hora del celular se acepta si la caja no tiene hora, o para corregir hasta 5 minutos hacia adelante. La hora
  de la caja nunca retrocede (el gateway exige `ts` creciente y rechaza lecturas con más de 30 s de futuro).
  Saltos mayores solo con el `iat` de un token de apertura, que firma el backend.
- La posición se guarda si está en rango y sale en las lecturas siguientes. Antes de la primera, las lecturas
  llevan 0,0.

## Aviso de alejamiento

Si el celular vinculado se desconecta con la caja precintada, la caja suena tres veces y lo muestra en pantalla
(`ALERT_ON_LINK_LOSS`). La app debería avisar del lado del celular. Es una ayuda para no olvidarse la caja, no una
medida de seguridad.

## Riesgos conocidos

- **Posición no verificable.** El formato de lectura v1 no distingue «posición del celular» de «posición propia»
  ni dice cuán vieja es. Habría que usar un bit libre de `flags` (cambio en los tres repos) o dejar la posición
  fuera de la evidencia.
- **PIN a la vista.** Quien ve la pantalla puede vincularse. Alcanza para evitar ataques a distancia, no a alguien
  con la caja en la mano.
- **Hora adelantada.** Un celular vinculado puede adelantar el reloj de a 5 minutos por escritura. No puede
  atrasarlo. Limitar la frecuencia de correcciones queda pendiente.
- **Segundo plano.** Android necesita un servicio en primer plano para sincronizar de forma confiable; iOS limita
  mucho más el BLE en segundo plano.
