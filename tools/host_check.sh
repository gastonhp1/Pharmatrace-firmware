#!/usr/bin/env bash
# Comprobaciones en la PC de lo que no cubre `pio test -e native` porque necesita mbedtls y Python:
# el firmador por software y el verificador del token de apertura.
#
#   sudo apt install g++ libmbedtls-dev && pip install cryptography
#   tools/host_check.sh [ruta a Pharmatrace/backend]
#
# Con la ruta al backend de Pharmatrace (y node), además pasa lecturas firmadas por el código real
# del gateway (iot/ingest.js).
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
L=lib/pharma_proto/src
CXX="g++ -std=gnu++17 -Wall -Wextra -Werror -I$L -Isrc"

echo "== firmador por software (mbedtls) =="
$CXX tools/host_crypto_check.cpp src/p256_soft.cpp src/verifier_mbedtls.cpp $L/pharma_reading.cpp -lmbedcrypto -o "$OUT/crypto"
read -r PRIV PUB DIGEST SIG < <("$OUT/crypto" gen)
python3 - "$PRIV" "$PUB" "$DIGEST" "$SIG" <<'PY'
import sys
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.asymmetric.utils import Prehashed, encode_dss_signature
priv, pub, digest, sig = (bytes.fromhex(x) for x in sys.argv[1:5])
key = ec.derive_private_key(int.from_bytes(priv, "big"), ec.SECP256R1())
n = key.public_key().public_numbers()
assert n.x.to_bytes(32, "big") + n.y.to_bytes(32, "big") == pub, "la pública no coincide"
key.public_key().verify(encode_dss_signature(int.from_bytes(sig[:32], "big"), int.from_bytes(sig[32:], "big")),
                        digest, ec.ECDSA(Prehashed(hashes.SHA256())))
print("ok: Python deriva la misma pública y acepta la firma del firmware")
PY

echo "== token de apertura: Python firma, mbedtls verifica =="
$CXX tools/e2e_unlock.cpp src/verifier_mbedtls.cpp $L/pharma_reading.cpp $L/pharma_unlock.cpp -lmbedcrypto -o "$OUT/unlock"
QR="PB1:VAL-001:AAECAwQFBgcICQoLDA0ODw"
KEYPUB=$(python3 tools/unlock_token.py gen-key "$OUT/k.pem")
OTHER=$(python3 tools/unlock_token.py gen-key "$OUT/k2.pem")
"$OUT/unlock" "$KEYPUB" "$QR" "$(python3 tools/unlock_token.py sign "$OUT/k.pem" "$QR")" 0
"$OUT/unlock" "$OTHER" "$QR" "$(python3 tools/unlock_token.py sign "$OUT/k.pem" "$QR")" 6
"$OUT/unlock" "$KEYPUB" "$QR" "$(python3 tools/unlock_token.py sign "$OUT/k.pem" "PB1:VAL-002:AAECAwQFBgcICQoLDA0ODw")" 3
"$OUT/unlock" "$KEYPUB" "$QR" "$(python3 tools/unlock_token.py sign "$OUT/k.pem" "PB1:VAL-001:AAECAwQFBgcICQoLDA0ODg")" 4

if [ $# -ge 1 ]; then
  echo "== lecturas firmadas por software contra el gateway real =="
  $CXX tools/host_reading_e2e.cpp src/p256_soft.cpp src/verifier_mbedtls.cpp $L/pharma_reading.cpp $L/pharma_ingest.cpp -lmbedcrypto -o "$OUT/readings"
  "$OUT/readings" 1800000000 6 > "$OUT/readings.txt"
  node tools/gateway_check.mjs "$(cd "$1" && pwd)" "$OUT/readings.txt"
fi
echo "todo ok"
