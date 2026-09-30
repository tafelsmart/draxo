"""Ed25519-Grants für den Draxo Client — gemeinsame Definition.

Warum es diese Datei gibt
------------------------
Bis v1.2 war die Lizenzprüfung symmetrisch: ein 32-Byte-Geheimnis lag im
DLL-Quelltext (``src/core/auth.cpp``), im Launcher und im Klartext auf der
Website. Damit konnte jeder offline beliebige Keys erzeugen — die „Härtung"
in auth.cpp (3-Slot-Konsens, Split-Secret, CRC-Canary) schützt nur vor
Byte-Patches in der DLL, nicht vor jemandem, der den Key selbst baut.

Ab v2 ist es asymmetrisch:
  * Der **private** Schlüssel liegt ausschliesslich auf dem Server (Bot).
  * Der **öffentliche** Schlüssel steckt im Launcher und in der DLL.
  * Ein Grant kann damit nur vom Server erzeugt, aber von jedem geprüft
    werden — und die Prüfung braucht kein Geheimnis.

Der Token-Aufbau ist bewusst simpel und in C++ nachbaubar:

    DRAXO3-<base32(nutzlast || signatur)>

    Nutzlast (25 Bytes, little-endian)
      [0]      Version = 3
      [1..9)   Discord-User-ID als uint64   (0 = nicht an ein Konto gebunden)
      [9..17)  sha256(HWID)[0:8]            (0 = nicht an einen Rechner gebunden)
      [17..21) ausgestellt (Unix, uint32)
      [21..25) läuft ab    (Unix, uint32, 0 = unbefristet)

    Signatur (64 Bytes) = Ed25519(nutzlast)

Keine Zeitstempel- oder Wiederholungsschicht im Token: der Server hält den
Zustand, der Client prüft nur die Signatur. Widerruf ist damit eine
Serverfrage, keine Tokenfrage.
"""

from __future__ import annotations

import base64
import hashlib
import os
from dataclasses import dataclass
from typing import Optional

# ── Öffentlicher Schlüssel des Servers ───────────────────────────────────────
# Erzeugt mit:  python -m draxo_bot.signing keygen
# Rotation: neuen Schlüssel auf dem Server erzeugen und DIESE eine Zahl
# ersetzen. Der alte Schlüssel bleibt dann für bestehende Grants gültig,
# bis deren Ablaufzeit überschritten ist — deshalb nicht zu früh rotieren.
#
# Testschlüssel. Für den Produktivbetrieb eigenen erzeugen — die .env des
# Bots enthält den privaten Teil, der niemals hier landen darf.
SERVER_PUBLIC_KEY_HEX = (
    "81e1069c534f727c6054a48b9c32980c"
    "7eba097b8777e26765a41a914fa5a187"
)

TOKEN_PREFIX = "DRAXO3"
_VERSION = 3
_PAYLOAD_LEN = 25
_SIGNATURE_LEN = 64
B32_ALPHABET = "0123456789ABCDEFGHJKMNPQRSTVWXYZ"

# Crockford: kein I/L/O/U, damit ein falsch gelesenes O kein 0 wird.
_B32_DECODE = {c: i for i, c in enumerate(B32_ALPHABET)}


class GrantError(ValueError):
    """Grant ist strukturell kaputt — kein Grund, das genauer zu prüfen."""


# ═══════════════════════════════════════════════════════════════════════════
#  Ed25519 (RFC 8032) — reine Verifikation, keine Abhängigkeiten
# ═══════════════════════════════════════════════════════════════════════════
# Absichtlich ohne `cryptography`: der Launcher wird per PyInstaller zu einer
# EXE gebündelt, und ein reines Python-Modul verhält sich darin immer gleich.
# Der Server (Bot) benutzt zur *Erzeugung* sehr wohl `cryptography` — dort
# ist die Abhängigkeit unkritisch.

_P = 2**255 - 19
_L = 2**252 + 27742317777372353535851937790883648493
_D = -121665 * pow(121666, _P - 2, _P) % _P
_I = pow(2, (_P - 1) // 4, _P)

# Basispunkt B in erweiterten Edwards-Koordinaten (x, y, z, t)
_B = (
    15112221349535400772501151409588531511454012693041857206046113283949847762202,
    46316835694926478169428394003475163141307993866256225615783033603165251855960,
    1,
    15112221349535400772501151409588531511454012693041857206046113283949847762202
    * 46316835694926478169428394003475163141307993866256225615783033603165251855960
    % _P,
)


def _add(p: tuple, q: tuple) -> tuple:
    """add-2008-hwcd-3 (a = -1), erweiterte Edwards-Koordinaten (x,y,z,t).

    D ist 2·Z1·Z2 — beide Z, nicht X. Auch hier gilt: ein Tippfehler bleibt
    auf der Kurve und fällt erst beim Vergleich mit RFC-Vektoren auf.
    """
    px, py, pz, pt = p
    qx, qy, qz, qt = q
    a = (py - px) * (qy - qx) % _P
    b = (py + px) * (qy + qx) % _P
    c = 2 * pt * qt * _D % _P
    dd = 2 * pz * qz % _P
    e, f, g, h = b - a, dd - c, dd + c, b + a
    return (e * f % _P, g * h % _P, f * g % _P, e * h % _P)


def _double(p: tuple) -> tuple:
    """dbl-2008-hwcd für a = -1 (Ed25519), erweiterte Koordinaten (x,y,z,t).

    Achtung G und H: mit D = a·A = -A gilt G = D + B = B - A und
    H = D - B = -A - B. Vertauscht man die beiden Vorzeichen, bleibt das
    Ergebnis auf der Kurve formal gültig, aber nicht mehr auf *dieser*
    Kurve — die Signaturprüfung fällt dann stillschweigend immer positiv aus.
    """
    px, py, pz, _ = p
    a = px * px % _P
    b = py * py % _P
    c = 2 * pz * pz % _P
    e = ((px + py) * (px + py) - a - b) % _P
    g = (b - a) % _P
    f = (g - c) % _P
    h = (-a - b) % _P
    return (e * f % _P, g * h % _P, f * g % _P, e * h % _P)


def _mul(scalar: int, point: tuple) -> tuple:
    result = (0, 1, 1, 0)  # neutrales Element
    while scalar > 0:
        if scalar & 1:
            result = _add(result, point)
        point = _double(point)
        scalar >>= 1
    return result


def _compress(p: tuple) -> bytes:
    x, y, z, _ = p
    inv_z = pow(z, _P - 2, _P)
    x, y = x * inv_z % _P, y * inv_z % _P
    return int.to_bytes(y | ((x & 1) << 255), 32, "little")


def _decompress(data: bytes) -> Optional[tuple]:
    """Punkt aus 32 Bytes. None, wenn der encodierte Punkt ungültig ist."""
    if len(data) != 32:
        return None
    value = int.from_bytes(data, "little")
    sign = value >> 255
    y = value & ((1 << 255) - 1)
    if y >= _P:
        return None
    x2 = (y * y - 1) * pow(_D * y * y + 1, _P - 2, _P) % _P
    if x2 == 0:
        return None if sign else (0, y, 1, 0)
    x = pow(x2, (_P + 3) // 8, _P)
    if (x * x - x2) % _P != 0:
        x = x * _I % _P
    if (x * x - x2) % _P != 0:
        return None
    if x & 1 != sign:
        x = _P - x
    return (x, y, 1, x * y % _P)


def _verify(public_key: bytes, message: bytes, signature: bytes) -> bool:
    """RFC-8032 §5.1.7: [s]B == R + [h]A."""
    if len(signature) != _SIGNATURE_LEN or len(public_key) != 32:
        return False
    point_a = _decompress(public_key)
    if point_a is None:
        return False
    enc_r = signature[:32]
    point_r = _decompress(enc_r)
    if point_r is None:
        return False
    s = int.from_bytes(signature[32:], "little")
    if s >= _L:
        return False
    h = int.from_bytes(
        hashlib.sha512(enc_r + public_key + message).digest(), "little"
    ) % _L
    # Signiergleichung ist  [s]B == R + [h]A.
    # Umformuliert:        [s]B - [h]A == R
    # Nicht: R - [h]A == [s]B — das ist eine andere Gleichung, faellt aber
    # nicht durch Exceptions auf, sondern lehnt gueltige Signaturen ab.
    neg_a = (-point_a[0] % _P, point_a[1], point_a[2], -point_a[3] % _P)
    left = _add(_mul(s, _B), _mul(h, neg_a))
    return _compress(left) == _compress(point_r)


def _expand(seed: bytes) -> tuple[int, bytes]:
    """RFC-8032 §5.1.5: Geheimnis auf 32 Byte Scalar + 32 Byte Prefix spreizen."""
    if len(seed) != 32:
        raise GrantError("Ed25519-Seed muss 32 Bytes lang sein.")
    h = hashlib.sha512(seed).digest()
    scalar = int.from_bytes(h[:32], "little")
    scalar &= (1 << 254) - 8      # unterste drei Bits clear
    scalar |= 1 << 254            # high bit setzen
    return scalar, h[32:]


def _sign_pure(seed: bytes, message: bytes) -> bytes:
    """Erzeugt die Signatur ohne Fremdbibliothek (RFC-8032 §5.1.6)."""
    scalar, prefix = _expand(seed)
    enc_a = _compress(_mul(scalar, _B))
    r = int.from_bytes(hashlib.sha512(prefix + message).digest(), "little") % _L
    enc_r = _compress(_mul(r, _B))
    k = int.from_bytes(
        hashlib.sha512(enc_r + enc_a + message).digest(), "little"
    ) % _L
    s = (r + k * scalar) % _L
    return enc_r + s.to_bytes(32, "little")


def _public_from_seed(seed: bytes) -> bytes:
    scalar, _ = _expand(seed)
    return _compress(_mul(scalar, _B))


# `cryptography` ist auf dem Server verfügbar und deutlich schneller. Fehlt
# es, gilt der Reinstieg — beide Wege müssen dasselbe liefern, sonst hängt
# die Gültigkeit eines Grants an der Server-Bibliothek.
try:  # pragma: no cover - abhängig von der Umgebung
    from cryptography.hazmat.primitives.asymmetric.ed25519 import (
        Ed25519PrivateKey as _CryptoKey,
    )

    _HAVE_CRYPTOGRAPHY = True
except Exception:  # noqa: BLE001 - Fehlimpport ist der Normalfall am Client
    _HAVE_CRYPTOGRAPHY = False


def public_from_seed(seed: bytes) -> bytes:
    if _HAVE_CRYPTOGRAPHY:
        from cryptography.hazmat.primitives import serialization

        key = _CryptoKey.from_private_bytes(seed)
        return key.public_key().public_bytes(
            encoding=serialization.Encoding.Raw,
            format=serialization.PublicFormat.Raw,
        )
    return _public_from_seed(seed)


def sign(seed: bytes, message: bytes) -> bytes:
    """Signiert ``message``. Nur der Server darf das aufrufen."""
    if _HAVE_CRYPTOGRAPHY:
        return _CryptoKey.from_private_bytes(seed).sign(message)
    return _sign_pure(seed, message)


# ═══════════════════════════════════════════════════════════════════════════
#  Base32 (Crockford)
# ═══════════════════════════════════════════════════════════════════════════


def b32encode(data: bytes) -> str:
    acc = bits = 0
    out: list[str] = []
    for byte in data:
        acc = (acc << 8) | byte
        bits += 8
        while bits >= 5:
            bits -= 5
            out.append(B32_ALPHABET[(acc >> bits) & 0x1F])
    if bits:
        out.append(B32_ALPHABET[(acc << (5 - bits)) & 0x1F])
    return "".join(out)


def b32decode(text: str) -> bytes:
    acc = bits = 0
    out = bytearray()
    for ch in text.upper():
        if ch in "-_ \t":
            continue
        value = _B32_DECODE.get(ch)
        if value is None:
            raise GrantError(f"Ungültiges Zeichen {ch!r} im Grant.")
        acc = (acc << 5) | value
        bits += 5
        while bits >= 8:
            bits -= 8
            out.append((acc >> bits) & 0xFF)
    return bytes(out)


# ═══════════════════════════════════════════════════════════════════════════
#  Grant: erzeugen, prüfen, lesen
# ═══════════════════════════════════════════════════════════════════════════


def encode_payload(
    *,
    discord_id: int = 0,
    hwid_hash: bytes = b"\x00" * 8,
    issued_at: int = 0,
    expires_at: int = 0,
) -> bytes:
    if len(hwid_hash) != 8:
        raise GrantError("hwid_hash muss 8 Bytes lang sein.")
    if not 0 <= discord_id < 2**64:
        raise GrantError("discord_id passt nicht in uint64.")
    if not 0 <= issued_at < 2**32 or not 0 <= expires_at < 2**32:
        raise GrantError("Zeitstempel passen nicht in uint32 (Jahr 2106).")
    return (
        bytes([_VERSION])
        + discord_id.to_bytes(8, "little")
        + hwid_hash
        + issued_at.to_bytes(4, "little")
        + expires_at.to_bytes(4, "little")
    )


def decode_payload(payload: bytes) -> dict:
    if len(payload) != _PAYLOAD_LEN:
        raise GrantError(
            f"Nutzlast muss {_PAYLOAD_LEN} Bytes sein, hat aber {len(payload)}."
        )
    if payload[0] != _VERSION:
        raise GrantError(
            f"Grant-Version {payload[0]} wird nicht unterstützt (erwartet {_VERSION})."
        )
    return {
        "discord_id": int.from_bytes(payload[1:9], "little"),
        "hwid_hash": payload[9:17],
        "issued_at": int.from_bytes(payload[17:21], "little"),
        "expires_at": int.from_bytes(payload[21:25], "little"),
    }


def normalise_hwid(hwid: str) -> str:
    """HWID auf das Format bringen, das der Launcher verwendet.

    ``license_manager._get_hwid`` liefert 32 Hex-Zeichen in Grossbuchstaben.
    Alles Kleingeschriebene wird hier angehoben, damit ein Tippfehler beim
    Einfügen nicht den ganzen Grant verbrennt.
    """
    cleaned = "".join(str(hwid).split()).upper()
    if not cleaned:
        raise GrantError("HWID ist leer.")
    if len(cleaned) != 32:
        raise GrantError(
            f"HWID muss 32 Zeichen haben, hat aber {len(cleaned)}. Sie steht "
            "im Launcher unter CONFIG → LICENSE neben dem Knopf „Copy“."
        )
    if any(c not in "0123456789ABCDEF" for c in cleaned):
        raise GrantError("HWID enthält Zeichen ausser 0-9 und A-F.")
    return cleaned


def mask_hwid(hwid: str) -> str:
    """HWID für Logs und Embeds: nur Anfang und Ende sichtbar."""
    if len(hwid) <= 8:
        return "*" * len(hwid)
    return f"{hwid[:4]}…{hwid[-4:]}"


def hwid_fingerprint(hwid: str) -> bytes:
    """Die 8 Bytes, die im Grant stehen: sha256(HWID)[:8]."""
    return hashlib.sha256(hwid.strip().upper().encode()).digest()[:8]


def pack(payload: bytes, signature: bytes) -> str:
    """Signatur an die Nutzlast hängen und als Token schreiben."""
    if len(signature) != _SIGNATURE_LEN:
        raise GrantError("Signatur muss 64 Bytes lang sein.")
    return f"{TOKEN_PREFIX}-{b32encode(payload + signature)}"


def unpack(token: str) -> tuple[bytes, bytes]:
    """Token in Nutzlast und Signatur zerlegen — ohne jede Prüfung."""
    cleaned = token.strip().replace(" ", "")
    if not cleaned.upper().startswith(TOKEN_PREFIX):
        raise GrantError(
            f"Kein Draxo-Grant. Erwartet wird {TOKEN_PREFIX}-…, "
            "alte DRAXO-Keys aus der Zeit vor der Umstellung werden nicht mehr "
            "akzeptiert."
        )
    raw = b32decode(cleaned[len(TOKEN_PREFIX):])
    if len(raw) < _PAYLOAD_LEN + _SIGNATURE_LEN:
        raise GrantError("Grant ist zu kurz.")
    return raw[:_PAYLOAD_LEN], raw[_PAYLOAD_LEN:]


def public_key() -> bytes:
    """Öffentlicher Schlüssel — Umgebung gewinnt, damit testbar bleibt.

    DRAXO_PUBLIC_KEY_HEX überschreibt den eingebauten Wert. Das ist der
    einzige Weg, den Schlüssel ohne Codeänderung zu rotieren; im gebauten
    EXE ist die Variable normalerweise nicht gesetzt.
    """
    raw = os.environ.get("DRAXO_PUBLIC_KEY_HEX", "").strip() or SERVER_PUBLIC_KEY_HEX
    try:
        key = bytes.fromhex(raw)
    except ValueError as exc:
        raise GrantError("DRAXO_PUBLIC_KEY_HEX ist kein Hex-String.") from exc
    if len(key) != 32:
        raise GrantError(f"Öffentlicher Schlüssel muss 32 Bytes sein, hat {len(key)}.")
    return key


@dataclass(frozen=True)
class Grant:
    """Ergebnis einer Prüfung."""

    valid: bool
    reason: str
    discord_id: int = 0
    hwid_hash: bytes = b"\x00" * 8
    issued_at: int = 0
    expires_at: int = 0

    @property
    def expired(self) -> bool:
        return self.expires_at != 0

    @property
    def bound_to_account(self) -> bool:
        return self.discord_id != 0

    @property
    def bound_to_machine(self) -> bool:
        return self.hwid_hash != b"\x00" * 8

    @property
    def hwid_prefix(self) -> str:
        return base64.b16encode(self.hwid_hash).decode().upper()

    def seconds_left(self, now: Optional[float] = None) -> int:
        if not self.expires_at:
            return -1
        import time

        return int(self.expires_at - (now if now is not None else time.time()))


def verify_grant(
    token: str,
    *,
    hwid: Optional[str] = None,
    discord_id: Optional[int] = None,
    now: Optional[float] = None,
) -> Grant:
    """Signatur, HWID-Bindung, Konto-Bindung und Ablauf prüfen.

    Die Reihenfolge ist Absicht: erst die Signatur (der teure, aber einzige
    Schritt, der Echtheit belegt), danach die günstigen Bindungsprüfungen.
    Eine manipulierte Nutzlast wird also nie als „falscher Rechner"
    gemeldet, sondern als ungültige Signatur.
    """
    import time

    moment = time.time() if now is None else now

    try:
        payload, signature = unpack(token)
    except GrantError as exc:
        return Grant(False, str(exc))

    if not _verify(public_key(), payload, signature):
        return Grant(
            False,
            "Signatur ungültig. Der Grant stammt nicht von unserem Server.",
        )

    try:
        fields = decode_payload(payload)
    except GrantError as exc:
        return Grant(False, str(exc))

    grant = Grant(True, "OK", **fields)

    if hwid is not None and grant.bound_to_machine:
        if grant.hwid_hash != hwid_fingerprint(hwid):
            return Grant(
                False,
                "Grant gehört zu einem anderen Rechner.",
                grant.discord_id,
                grant.hwid_hash,
                grant.issued_at,
                grant.expires_at,
            )

    if discord_id is not None and grant.bound_to_account:
        if grant.discord_id != discord_id:
            return Grant(
                False,
                "Grant gehört zu einem anderen Discord-Konto.",
                grant.discord_id,
                grant.hwid_hash,
                grant.issued_at,
                grant.expires_at,
            )

    if grant.expires_at and grant.expires_at <= moment:
        return Grant(
            False,
            "Grant ist abgelaufen.",
            grant.discord_id,
            grant.hwid_hash,
            grant.issued_at,
            grant.expires_at,
        )

    return grant