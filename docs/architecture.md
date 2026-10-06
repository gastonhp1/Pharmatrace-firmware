# Arquitectura

```
Valija (ESP32) ──lecturas firmadas──▶ Backend / oráculo ──tx──▶ Contratos Pharmatrace
   sensores                              valida + agrega          registro del lote
```

## Decisiones

- **El ESP32 no firma transacciones on-chain.** Firma las lecturas con una clave del dispositivo; el backend las valida y es quien escribe en los contratos. Evita meter una wallet con fondos en un dispositivo que viaja por la calle.
- **Identidad del dispositivo:** una clave por valija, registrada en el contrato (o en el backend) asociada al lote durante el transporte.
- **Buffer offline:** si se pierde conectividad, las lecturas se guardan localmente y se envían en lote al reconectar.
- **Eventos vs. telemetría continua:** las excursiones de temperatura y la apertura de tapa se reportan al instante; el resto se agrega por intervalo para no inflar el costo on-chain.

## Pendiente de definir

- Sensores finales y cableado
- Conectividad (WiFi vs. LTE)
- Formato del payload y esquema de firma
- Cómo se vincula una valija a un lote en los contratos
