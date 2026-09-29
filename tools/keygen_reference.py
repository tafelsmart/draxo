"""
keygen_reference.py
-------------------
Reference implementation of the Draxo poly-XOR license key algorithm.
Used to:
  1. Verify C++ (src/core/auth.cpp) and PHP (tools/keygen.php) agree.
  2. Generate deterministic test vectors for test_client.cpp.

Run:  python tools/keygen_reference.py
"""

import hashlib
import time

# Split secret (must mirror C++ _sa/_sb/_sc/_sd and PHP $SECRET_PARTS)
SA = [0xD4, 0x7B, 0x0E, 0x28, 0x13, 0x9C, 0xDF, 0x69]
SB = [0x1A, 0xE2, 0x6D, 0x47, 0x86, 0x71, 0x36, 0xF5]
SC = [0x8F, 0x55, 0xA3, 0xC9, 0x2D, 0xB0, 0xE8, 0xAB]
SD = [0x3C, 0x91, 0xBF, 0xFA, 0x5E, 0x44, 0x0A, 0x12]
SX = [0xA3, 0x5C, 0xF1, 0x7E]  # fixed per-part masks

B32 = "0123456789ABCDEFGHJKMNPQRSTVWXYZ"


def recon_secret():
    out = bytearray(32)
    for i in range(8):
        out[i * 4 + 0] = (SA[i] ^ ((SX[0] + i) & 0xFF)) & 0xFF
        out[i * 4 + 1] = (SB[i] ^ ((SX[1] + i) & 0xFF)) & 0xFF
        out[i * 4 + 2] = (SC[i] ^ ((SX[2] + i) & 0xFF)) & 0xFF
        out[i * 4 + 3] = (SD[i] ^ ((SX[3] + i) & 0xFF)) & 0xFF
    return bytes(out)


def poly_xor(data, key):
    """Mirror C++ _polyXor: 2 rounds, feed-forward, prev=0x7B."""
    prev = 0x7B
    out = bytearray(len(data))
    klen = len(key)
    for i in range(len(data)):
        k = key[i % klen]
        r1 = (data[i] ^ k ^ prev) & 0xFF
        r2 = (r1 ^ key[(i + 7) % klen] ^ (i & 0xFF)) & 0xFF
        out[i] = r2
        prev = r2
    return bytes(out)


def base32_encode(data):
    bits = 0
    bc = 0
    out = ""
    for b in data:
        bits = (bits << 8) | b
        bc += 8
        while bc >= 5:
            bc -= 5
            out += B32[(bits >> bc) & 0x1F]
    if bc > 0:
        out += B32[(bits << (5 - bc)) & 0x1F]
    return out


def base32_decode(text):
    """Crockford decode (mirror C++ b32dec) — inverse of base32_encode.
    Maps excluded letters to twins: I->1, L->1, O->0, U->27(V).
    Kept separate from C++ so a decode-table drift can NEVER go
    undetected again (this is exactly the class of bug that broke v3)."""
    table = [
        10, 11, 12, 13, 14, 15, 16, 17, 1, 18, 19, 1, 20, 21, 0,
        22, 23, 24, 25, 26, 27, 27, 28, 29, 30, 31,
    ]
    bits = 0
    bc = 0
    out = bytearray()
    for ch in text.upper():
        if ch == '-':
            continue
        if '0' <= ch <= '9':
            v = ord(ch) - 48
        elif 'A' <= ch <= 'Z':
            v = table[ord(ch) - 65]
        else:
            continue
        bits = (bits << 5) | v
        bc += 5
        while bc >= 8:
            bc -= 8
            out.append((bits >> bc) & 0xFF)
    return bytes(out)


def poly_xor_inv(data, key):
    """Inverse of poly_xor — mirrors C++ _polyXorInv.
    Feed-forward is NOT self-inverse; prev must track the CIPHER byte."""
    prev = 0x7B
    out = bytearray(len(data))
    klen = len(key)
    for i in range(len(data)):
        c = data[i]
        k = key[i % klen]
        p = (c ^ k ^ key[(i + 7) % klen] ^ (i & 0xFF) ^ prev) & 0xFF
        out[i] = p
        prev = c
    return bytes(out)


def decode_key(key_str):
    """Decode a DRAXO- key back to the raw 16-byte payload."""
    b = key_str.upper().replace('-', '').replace(' ', '')
    if b.startswith('DRAXO'):
        b = b[5:]
    enc = base32_decode(b)
    return poly_xor_inv(enc, recon_secret())


def generate_key(hwid_hex, expiry_hours, now=None):
    hs = hashlib.sha256(hwid_hex.encode()).digest()  # 32 bytes
    if expiry_hours > 0:
        expiry = (now if now is not None else int(time.time())) + expiry_hours * 3600
    else:
        expiry = 0
    payload = bytearray(16)
    payload[0:12] = hs[0:12]
    payload[12] = (expiry >> 24) & 0xFF
    payload[13] = (expiry >> 16) & 0xFF
    payload[14] = (expiry >> 8) & 0xFF
    payload[15] = expiry & 0xFF
    enc = poly_xor(bytes(payload), recon_secret())
    b = base32_encode(enc)
    b = (b + "0" * 26)[:26]
    return "DRAXO-" + b[0:5] + "-" + b[5:10] + "-" + b[10:15] + "-" + b[15:20] + "-" + b[20:25] + "-" + b[25:26]


if __name__ == "__main__":
    fixed_hwid = "0123456789ABCDEF0123456789ABCDEF"
    k = generate_key(fixed_hwid, 0)
    print(f"Recon secret : {recon_secret().hex()}")
    print(f"Permanent key for fixed HWID: {k}")
    print(f"Length: {len(k)}")
    FIXED_NOW = 1700000000
    k2 = generate_key(fixed_hwid, 24, now=FIXED_NOW)
    print(f"Timed(24h@fixed-now) key        : {k2}")

    # ── Decode cross-check (guards against b32 decode drift) ──────────
    payload = decode_key(k)
    hs = hashlib.sha256(fixed_hwid.encode()).digest()
    ok_hwid = payload[0:12] == hs[0:12]
    ok_exp = payload[12:16] == b'\x00\x00\x00\x00'
    print(f"Decode roundtrip HWID bytes OK  : {ok_hwid}")
    print(f"Decode roundtrip expiry 0 OK    : {ok_exp}")

    # Timed key decodes to FIXED_NOW + 24h exactly
    p2 = decode_key(k2)
    exp = int.from_bytes(p2[12:16], 'big')
    print(f"Timed key decodes FIXED_NOW+24h  : {exp == FIXED_NOW + 24 * 3600} ({exp})")
