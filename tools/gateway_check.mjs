// Pasa la salida de host_reading_e2e por el código real del gateway de Pharmatrace
// (backend/iot/ingest.js y window.js) con un ledger de mentira. Uso:
//   node tools/gateway_check.mjs <ruta absoluta a Pharmatrace/backend> <salida de host_reading_e2e>
import { createRequire } from "module";
import fs from "fs";

const require = createRequire(process.argv[2] + "/");
const { createIngest } = require("./iot/ingest.js");
const { findEvents } = require("./iot/window.js");
const [pubKey, json] = fs.readFileSync(process.argv[3], "utf8").trim().split("\n");
const body = JSON.parse(json);

const records = [];
const store = { readAll: () => records, append: (_cargo, r) => records.push(r) };
const chain = {
    activeCargoOf: async () => "CARGO-1",
    getDevice: async () => ({ pubKey, calibrationExpiry: 1900000000, active: true }),
};
const ingest = createIngest({ store, chain, now: () => 1800000400 });
const check = (ok, what) => {
    if (!ok) throw new Error("FALLÓ: " + what);
    console.log("ok: " + what);
};

const first = await ingest(body.deviceId, body.readings);
check(first.accepted === body.readings.length && first.rejected.length === 0, `el gateway acepta las ${body.readings.length} lecturas`);
const again = await ingest(body.deviceId, body.readings);
check(again.duplicates === body.readings.length, "reenviar la misma tanda da duplicados, no error");
const events = findEvents(records, 0, records.length).map((e) => e.kind);
check(events.length === 1 && events[0] === "SEAL_BROKEN", "detecta el precinto violado e ignora los flags de apertura");

records.length = 0;
const bad = structuredClone(body.readings[0]);
bad.signature = bad.signature.replace(/.$/, (c) => (c === "0" ? "1" : "0"));
const tampered = await ingest(body.deviceId, [bad]);
check(tampered.accepted === 0 && tampered.rejected[0].reason === "Invalid signature", "rechaza una firma alterada");
