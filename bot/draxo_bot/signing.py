"""Serverseitige Grant-Ausgabe — die einzige Stelle mit dem privaten Schlüssel.

Der private Seed kommt aus ``DRAXO_SIGNING_KEY`` (64 Hex-Zeichen) in der
``.env`` und verlässt diesen Server nie. Wird er versehentlich rotiert,
betrifft das nur neue Grants; bestehende bleiben bis zu ihrem Ablauf gültig.

Schlüssel erzeugen::

    python -m draxo_bot.signing keygen

Das druckt den privaten Seed (einmalig!) und den öffentlichen Schlüssel.
Den privaten in die ``.env`` auf dem Server, den öffentlichen in
``SERVER_PUBLIC_KEY_HEX`` in ``license_signing.py`` — dort ist er fest
eingebaut, damit die EXE ihn nicht erst vom Server holen muss.
"""

from __future__ import annotations

import os
import secrets
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

# license_signing.py liegt neben dem Paket — im Repo eine Ebene höher,
# auf dem Server (/opt/draxo-bot) direkt daneben. Beides wird hier abgedeckt,
# damit derselbe Code ohne Pfad-Akrobatik überall läuft.
_HERE = Path(__file__).resolve().parent
for _candidate in (_HERE.parent.parent, _HERE.parent, Path.cwd()):
    if (_candidate / "license_signing.py").is_file():
        if str(_candidate) not in sys.path:
            sys.path.insert(0, str(_candidate))
        break

import license_signing as grants  # noqa: E402


class SigningError(RuntimeError):
    """Kein oder unbrauchbarer privater Schlüssel."""


def _seed_from_env() -> bytes:
    raw = os.environ.get("DRAXO_SIGNING_KEY", "").strip()
    if not raw:
        raise SigningError(
            "DRAXO_SIGNING_KEY fehlt. Erzeugen mit:\n"
            "    python -m draxo_bot.signing keygen"
        )
    try:
        seed = bytes.fromhex(raw)
    except ValueError as exc:
        raise SigningError("DRAXO_SIGNING_KEY muss Hex sein (64 Zeichen).") from exc
    if len(seed) != 32:
        raise SigningError(
            f"DRAXO_SIGNING_KEY muss 32 Bytes sein, hat aber {len(seed)}."
        )
    return seed


@dataclass(frozen=True)
class IssuedGrant:
    token: str
    expires_at: int
    discord_id: int
    hwid_hash: bytes


class Signer:
    """Signiert Grants mit dem serverseitigen Schlüssel."""

    def __init__(self, seed: Optional[bytes] = None) -> None:
        self._seed = seed if seed is not None else _seed_from_env()
        self._public = grants.public_from_seed(self._seed)

    @property
    def public_key(self) -> bytes:
        return self._public

    @property
    def public_key_hex(self) -> str:
        return self._public.hex()

    def issue(
        self,
        *,
        discord_id: int = 0,
        hwid: str = "",
        hours: int = 24,
        now: Optional[float] = None,
    ) -> IssuedGrant:
        """Erzeugt einen Grant.

        ``discord_id`` und ``hwid`` sind beide optional, aber mindestens eines
        sollte gesetzt sein: ein Grant ohne jede Bindung wäre ein Schlüssel,
        den jeder kopieren kann.
        """
        moment = int(now if now is not None else time.time())
        if discord_id == 0 and not hwid:
            raise SigningError(
                "Ein Grant muss an ein Konto oder einen Rechner gebunden sein."
            )
        if hours < 0:
            raise SigningError("hours darf nicht negativ sein.")

        expires_at = moment + hours * 3600 if hours else 0
        if expires_at > 0xFFFFFFFF:
            expires_at = 0xFFFFFFFF

        hwid_hash = grants.hwid_fingerprint(hwid) if hwid else b"\x00" * 8
        payload = grants.encode_payload(
            discord_id=discord_id,
            hwid_hash=hwid_hash,
            issued_at=moment,
            expires_at=expires_at,
        )
        token = grants.pack(payload, grants.sign(self._seed, payload))
        return IssuedGrant(token, expires_at, discord_id, hwid_hash)

    def peek(self, token: str) -> dict:
        """Liest die Felder eines Grants, ohne die Signatur zu prüfen.

        Nur für Anzeigezwecke (Ablaufdatum im Log). Für jede Entscheidung
        ``grants.verify_grant`` benutzen.
        """
        payload, _ = grants.unpack(token)
        return grants.decode_payload(payload)


def _cmd_keygen() -> int:
    seed = secrets.token_bytes(32)
    public = grants.public_from_seed(seed)
    print("╔══════════════════════════════════════════════════════════════╗")
    print("║  Neuer Ed25519-Schlüssel für Draxo                             ║")
    print("╚══════════════════════════════════════════════════════════════╝\n")
    print("PRIVAT  (nur auf dem Server, in .env als DRAXO_SIGNING_KEY):")
    print(f"  {seed.hex()}\n")
    print("ÖFFENTLICH  (in license_signing.py als SERVER_PUBLIC_KEY_HEX):")
    print(f"  {public.hex()}\n")
    print("Gegenprobe (muss dreimal OK sein):")
    g = grants.encode_payload(discord_id=1, hwid_hash=b"\x01" * 8,
                              issued_at=0, expires_at=1)
    token = grants.pack(g, grants.sign(seed, g))
    ok = grants._verify(public, g, grants.sign(seed, g))
    print(f"  Signieren/Prüfen mit Public ....... {'OK' if ok else 'FEHLER'}")
    print(f"  Beispiel-Grant ................... {token[:44]}…")
    print(f"  Länge eines Grants ................ {len(token)} Zeichen")
    print("\nWichtig: Der private Schlüssel erscheint genau einmal hier.")
    return 0


def main(argv: Optional[list[str]] = None) -> int:
    argv = list(sys.argv[1:] if argv is None else argv)
    if argv and argv[0] == "keygen":
        return _cmd_keygen()
    print(__doc__)
    print("Aufruf:  python -m draxo_bot.signing keygen")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())