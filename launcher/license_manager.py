"""
license_manager.py
-------------------
Launcher-side license validation.

Bis v1.2 lag hier ein symmetrisches 32-Byte-Geheimnis, das dieselbe
Pruef-Logik wie die DLL und die Web-Keygen-Seite nachbildete. Damit
konnte jeder offline Keys erzeugen — "gehaertet" war nur die DLL
selbst, nicht das Format.

Ab v2 laeuft alles ueber asymmetrische Ed25519-Grants:

  * Erzeugt werden sie ausschliesslich vom Discord-Bot auf dem Server
    (draxo_bot.signing), der den privaten Schluessel besitzt.
  * Geprueft werden sie hier mit dem oeffentlichen Schluessel aus
    license_signing.SERVER_PUBLIC_KEY_HEX. Pruefen braucht kein
    Geheimnis, also ist hier auch keines mehr.
  * Online-Abfrage beim Bot (verify_online) klärt zusaetzlich, ob der
    Grant noch gilt — offline laesst sich ein Grant nur ablaufen,
    nicht widerrufen.

Das hier ist die Python-Seite. Die DLL prueft dieselben Grants in
src/core/auth.cpp mit derselben Schluessel-Nutzlast.
"""

import hashlib
import json
import subprocess
import sys
import time
import urllib.error
import urllib.request
from dataclasses import dataclass
from pathlib import Path

import license_signing as grants

# Paths - resolve the project root the same way the rest of the launcher
# does (see utils.get_workdir): when frozen by PyInstaller the .exe sits in
# the project root, otherwise we auto-detect the folder containing tools/.
# IMPORTANT: do NOT use Path(__file__).resolve().parent here - in a onefile
# bundle that points to the _MEIPASS temp dir, which breaks config lookup.
try:
    from utils import get_workdir
except Exception:  # pragma: no cover - only when imported standalone
    def get_workdir() -> Path:  # minimal inline fallback
        if getattr(sys, "frozen", False):
            return Path(sys.executable).resolve().parent
        # Eine Ebene ueber diesem Modul: die Module liegen in launcher/,
        # der Projekt-Root ist der Parent.
        return Path(__file__).resolve().parent.parent

_PROJECT_DIR = get_workdir()
_DLL_DIR = _PROJECT_DIR / "build" / "vanilla" / "Release"
_CONFIG_FILE = _DLL_DIR / "draxo_config.ini"

@dataclass
class LicenseStatus:
    status: str          # "ACTIVE", "EXPIRED", "LOCKED", "NO_KEY"
    status_color: str    # hex color string
    expiry_str: str      # human-readable expiry date (or "Permanent"/"Unknown")
    hwid: str            # machine HWID
    key_preview: str     # first 15 chars of stored key (or "")


# Grant-Pruefung
# ==============
# Alles Kryptografische liegt in license_signing.py — eine Datei, die
# auch der Server benutzt. Hier steht bewusst kein Schluesselmaterial.


def _validate_grant(key: str, hwid: str, discord_id: int | None = None) -> tuple:
    """Prueft einen Grant gegen HWID und optional gegen das Discord-Konto.

    Rueckgabe bleibt (is_valid, expiry_unix), damit die Aufrufer von
    frueher unveraendert bleiben koennen.
    """
    grant = grants.verify_grant(key, hwid=hwid, discord_id=discord_id)
    return grant.valid, grant.expires_at


def grant_detail(
    key: str,
    hwid: str = "",
    discord_id: int | None = None,
    now: float | None = None,
) -> grants.Grant:
    """Prueft und liefert das volle Ergebnis inkl. Begruendung."""
    return grants.verify_grant(key, hwid=hwid or None, discord_id=discord_id, now=now)


def _dev_mode() -> bool:
    """True, wenn die Dev-Variante per Umgebung gesetzt ist.

    Bewusst als duenne Huelle um discord_auth: so haben Launcher- und
    Lizenzseite dieselbe Quelle und koennen nicht auseinanderlaufen.
    """
    try:
        from discord_auth import dev_mode_enabled
    except Exception:  # pragma: no cover - nur bei Teil-Import
        import os

        value = (os.environ.get("DRAXO_DEV_MODE") or "").strip().lower()
        return value not in ("", "0", "false", "no", "off")
    return dev_mode_enabled()


def _server_url() -> str:
    """Endpunkt des Bots. Leer lassen heisst: nur offline pruefen."""
    import os

    return (os.environ.get("DRAXO_LICENSE_API", "") or "").rstrip("/")


def verify_online(
    key: str,
    *,
    hwid: str = "",
    discord_id: int | None = None,
    api_token: str = "",
    timeout: float = 5.0,
) -> tuple[bool, str]:
    """Fragt den Bot, ob der Grant noch gilt.

    Das ist der einzige Weg zu einem sofortigen Widerruf — offline kann
    ein Grant nur auslaufen. Ohne konfigurierte URL wird das nicht
    versucht und der Aufrufer entscheidet selbst.

    Rueckgabe: (verstanden, hinweis). Bei jedem Netzfehler wird False
    geliefert, mit einem Klartext, warum — ein stilles Scheitern waere
    hier gefaehrlich, weil es wie ein gueltiger Grant aussieht.
    """
    url = _server_url()
    if not url:
        return False, "Kein Lizenzserver konfiguriert (DRAXO_LICENSE_API)."

    body = json.dumps(
        {"grant": key, "hwid": hwid, "discord_id": discord_id}
    ).encode()
    headers = {"Content-Type": "application/json", "User-Agent": "DraxoLauncher"}
    if api_token:
        headers["Authorization"] = f"Bearer {api_token}"

    request = urllib.request.Request(
        url + "/api/v1/verify", data=body, headers=headers, method="POST"
    )
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            payload = json.load(response)
    except urllib.error.HTTPError as exc:
        try:
            payload = json.load(exc)
        except Exception:
            payload = {}
        return False, payload.get("reason") or f"Server antwortet mit HTTP {exc.code}."
    except urllib.error.URLError as exc:
        return False, f"Server nicht erreichbar ({exc.reason})."
    except Exception as exc:  # noqa: BLE001
        return False, f"Server-Antwort unlesbar ({exc})."

    return bool(payload.get("ok")), payload.get("reason") or "keine Angabe"


# Config readers

def _read_config_key(k: str) -> str:
    """Read a top-level key from draxo_config.ini (dot-notation)."""
    if not _CONFIG_FILE.exists():
        return ""
    try:
        text = _CONFIG_FILE.read_text(encoding="utf-8", errors="ignore")
        for line in text.splitlines():
            line = line.strip()
            if line.startswith(k + "="):
                return line[len(k) + 1:]
    except Exception:
        pass
    return ""


def _get_hwid() -> str:
    """Read HWID from DLL config, or compute it via WMI fallback."""
    hwid = _read_config_key("License.hwid")
    if hwid:
        return hwid
    return _compute_hwid_fallback()


def _compute_hwid_fallback() -> str:
    """Compute HWID from WMI (same approach as C++ auth::getHWID).
    Best-effort convenience for the first-launch screen; the DLL's
    getHWID() is the authoritative source. Always copy the HWID AFTER
    the first injection when requesting a license key."""
    try:
        import pythoncom
        import wmi  # type: ignore
        import warnings
        warnings.filterwarnings("ignore")
        pythoncom.CoInitialize()
        try:
            c = wmi.WMI()
            cpu = ""
            for p in c.Win32_Processor():
                cpu = p.ProcessorId or ""
                break
            mb = ""
            for b in c.Win32_BaseBoard():
                mb = b.SerialNumber or ""
                break
            if not cpu:
                cpu = subprocess.check_output(
                    'reg query "HKLM\\SOFTWARE\\Microsoft\\Cryptography" /v MachineGuid',
                    shell=True, text=True
                ).split("REG_SZ")[-1].strip()
            if not mb:
                mb = "DEFAULT_BOARD"
            if not cpu:
                cpu = "DEFAULT_CPU"
        finally:
            pythoncom.CoUninitialize()
    except ImportError:
        try:
            cpu = subprocess.check_output(
                'reg query "HKLM\\SOFTWARE\\Microsoft\\Cryptography" /v MachineGuid',
                shell=True, text=True
            ).split("REG_SZ")[-1].strip() or "DEFAULT_CPU"
        except Exception:
            cpu = "DEFAULT_CPU"
        mb = "DEFAULT_BOARD"

    combined = f"{cpu}|{mb}"
    hs = hashlib.sha256(combined.encode()).digest()
    return hs[:16].hex().upper()


# Public API

def get_license_status() -> LicenseStatus:
    """Read the current license state from the DLL config and validate it."""
    hwid = _get_hwid()
    key = _read_config_key("License.key")

    if _dev_mode():
        # Dev-Variante: als ACTIVE melden, damit jede Oberflaeche, die diesen
        # Status benutzt, nicht gesperrt bleibt. Die Grant-Pruefung laeuft hier
        # bewusst nicht — sie wuerde ohne Server-Grant scheitern und genau das
        # verhindern, was die Dev-Variante erlauben soll. Das ist keine
        # Signaturpruefung, die umgangen wird: eine erfundene Signatur faellt
        # weiter durch, wenn sie jemand an die DLL uebergibt.
        return LicenseStatus(
            status="ACTIVE",
            status_color="#2ed573",
            expiry_str="Dev-Variante",
            hwid=hwid,
            key_preview="(kein Grant noetig)" if not key else key[:15] + "...",
        )

    if not key:
        return LicenseStatus(
            status="LOCKED",
            status_color="#ff4757",
            expiry_str="-",
            hwid=hwid,
            key_preview="",
        )

    grant = grant_detail(key, hwid=hwid)
    is_valid = grant.valid
    expiry_unix = grant.expires_at

    if not is_valid:
        return LicenseStatus(
            status="LOCKED",
            status_color="#ff4757",
            # Nicht nur "—": der Grund entscheidet, ob der Nutzer sein
            # Discord-Konto, den Rechner oder den Key pruefen muss.
            expiry_str=grant.reason,
            hwid=hwid,
            key_preview=key[:15] + "..." if len(key) > 15 else key,
        )

    if expiry_unix > 0:
        if expiry_unix < time.time():
            return LicenseStatus(
                status="EXPIRED",
                status_color="#ffa502",
                expiry_str=_format_expiry(expiry_unix),
                hwid=hwid,
                key_preview=key[:15] + "...",
            )
        else:
            return LicenseStatus(
                status="ACTIVE",
                status_color="#2ed573",
                expiry_str=_format_expiry(expiry_unix),
                hwid=hwid,
                key_preview=key[:15] + "...",
            )
    else:
        return LicenseStatus(
            status="ACTIVE",
            status_color="#2ed573",
            expiry_str="Permanent",
            hwid=hwid,
            key_preview=key[:15] + "..." if len(key) > 15 else key,
        )


def _format_expiry(unix: int) -> str:
    """Format Unix timestamp to readable date."""
    import datetime
    dt = datetime.datetime.fromtimestamp(unix)
    return dt.strftime("%Y-%m-%d %H:%M")
