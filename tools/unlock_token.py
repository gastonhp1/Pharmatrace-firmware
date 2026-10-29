#!/usr/bin/env python3
"""Genera claves y firma tokens de apertura de la PharmaBox (formato en lib/pharma_proto/src/pharma_unlock.h).

Es una herramienta de banco de pruebas: hace lo que después hará el backend de Pharmatrace.
La clave privada que genera NO debe usarse en producción.

  python3 tools/unlock_token.py gen-key  clave.pem            # crea la clave y muestra la pública (X||Y, hex)
  python3 tools/unlock_token.py sign clave.pem "PB1:VAL-001:AAECAwQFBgcICQoLDA0ODw" [--ttl 120] [--actor LabMSP:ana]
      -> imprime el token en hex, listo para pegar en nRF Connect (característica `unlock`)
"""
import argparse
import base64
import hashlib
import struct
import sys
import time

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.asymmetric.utils import decode_dss_signature

VERSION, ACTION_OPEN = 1, 1


def b64url_decode(s: str) -> bytes:
    return base64.urlsafe_b64decode(s + "=" * (-len(s) % 4))


def public_xy(key) -> bytes:
    n = key.public_key().public_numbers()
    return n.x.to_bytes(32, "big") + n.y.to_bytes(32, "big")


def build_token(key, qr: str, ttl: int = 120, actor: str = "LabMSP:test", now: int | None = None) -> bytes:
    parts = qr.split(":")
    if len(parts) != 3 or parts[0] != "PB1":
        raise ValueError("el QR tiene que ser PB1:<deviceId>:<nonce>")
    device_id, nonce = parts[1], b64url_decode(parts[2])
    if len(nonce) != 16:
        raise ValueError("el nonce tiene que tener 16 bytes")
    iat = int(time.time()) if now is None else now
    body = (
        struct.pack(">BB", VERSION, ACTION_OPEN)
        + hashlib.sha256(device_id.encode()).digest()[:8]
        + nonce
        + struct.pack(">II", iat, iat + ttl)
        + hashlib.sha256(actor.encode()).digest()[:8]
    )
    assert len(body) == 42
    r, s = decode_dss_signature(key.sign(body, ec.ECDSA(hashes.SHA256())))
    return body + r.to_bytes(32, "big") + s.to_bytes(32, "big")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    g = sub.add_parser("gen-key")
    g.add_argument("pem")
    s = sub.add_parser("sign")
    s.add_argument("pem")
    s.add_argument("qr")
    s.add_argument("--ttl", type=int, default=120)
    s.add_argument("--actor", default="LabMSP:test")
    a = ap.parse_args()

    if a.cmd == "gen-key":
        key = ec.generate_private_key(ec.SECP256R1())
        with open(a.pem, "wb") as f:
            f.write(key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8,
                                      serialization.NoEncryption()))
        print(public_xy(key).hex())
        return 0

    with open(a.pem, "rb") as f:
        key = serialization.load_pem_private_key(f.read(), None)
    print(build_token(key, a.qr, a.ttl, a.actor).hex())
    return 0


if __name__ == "__main__":
    sys.exit(main())
