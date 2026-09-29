"""
license_manager.py
-------------------
Launcher-side license validation. Reads the Draxo DLL config
(build/vanilla/Release/draxo_config.ini) and validates the stored
license key using the EXACT same poly-XOR / Crockford Base32 algorithm
as the C++ DLL (src/core/auth.cpp).

This means the launcher can show:
  - License status (ACTIVE / EXPIRED / LOCKED / NO_KEY)
  - Expiry date
  - HWID (for copying into the Linkvertise flow)

Without needing the DLL to be injected first.

NOTE: This is the root-level copy used by the packaged DraxoLauncher.exe.
The algorithm MUST stay byte-identical with the C++ DLL - any change here
that diverges from src/core/auth.cpp breaks key validation.
"""

import hashlib
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path

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
        return Path(__file__).resolve().parent

_PROJECT_DIR = get_workdir()
_DLL_DIR = _PROJECT_DIR / "build" / "vanilla" / "Release"
_CONFIG_FILE = _DLL_DIR / "draxo_config.ini"

# Split secret (mirrors C++ _sa/_sb/_sc/_sd + _sx)
_SA = [0xD4, 0x7B, 0x0E, 0x28, 0x13, 0x9C, 0xDF, 0x69]
_SB = [0x1A, 0xE2, 0x6D, 0x47, 0x86, 0x71, 0x36, 0xF5]
_SC = [0x8F, 0x55, 0xA3, 0xC9, 0x2D, 0xB0, 0xE8, 0xAB]
_SD = [0x3C, 0x91, 0xBF, 0xFA, 0x5E, 0x44, 0x0A, 0x12]
_SX = [0xA3, 0x5C, 0xF1, 0x7E]

# Crockford Base32 alphabet (mirrors C++)
_B32 = "0123456789ABCDEFGHJKMNPQRSTVWXYZ"
# Decode table A..Z (mirrors C++ T[26])
_T = [10, 11, 12, 13, 14, 15, 16, 17, 1, 18, 19, 1, 20, 21, 0,
      22, 23, 24, 25, 26, 27, 27, 28, 29, 30, 31]


@dataclass
class LicenseStatus:
    status: str          # "ACTIVE", "EXPIRED", "LOCKED", "NO_KEY"
    status_color: str    # hex color string
    expiry_str: str      # human-readable expiry date (or "Permanent"/"Unknown")
    hwid: str            # machine HWID
    key_preview: str     # first 15 chars of stored key (or "")


# Crypto primitives (mirror C++ exactly)

def _recon_secret() -> bytes:
    """Reconstruct 32-byte secret deterministically (mirrors C++ _recon)."""
    out = bytearray(32)
    for i in range(8):
        out[i * 4 + 0] = (_SA[i] ^ ((_SX[0] + i) & 0xFF)) & 0xFF
        out[i * 4 + 1] = (_SB[i] ^ ((_SX[1] + i) & 0xFF)) & 0xFF
        out[i * 4 + 2] = (_SC[i] ^ ((_SX[2] + i) & 0xFF)) & 0xFF
        out[i * 4 + 3] = (_SD[i] ^ ((_SX[3] + i) & 0xFF)) & 0xFF
    return bytes(out)


def _poly_xor_inv(data: bytes, key: bytes) -> bytes:
    """Inverse of poly-XOR (mirrors C++ _polyXorInv). NOT self-inverse."""
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


def _b32_decode(text: str) -> bytes:
    """Crockford Base32 decode (mirrors C++ b32dec)."""
    bits = 0
    bc = 0
    out = bytearray()
    for ch in text.upper():
        if ch == "-":
            continue
        if "0" <= ch <= "9":
            v = ord(ch) - 48
        elif "A" <= ch <= "Z":
            v = _T[ord(ch) - 65]
        else:
            continue
        bits = (bits << 5) | v
        bc += 5
        while bc >= 8:
            bc -= 8
            out.append((bits >> bc) & 0xFF)
    return bytes(out)


def _validate_key(key: str, hwid: str) -> tuple:
    """Validate a Draxo license key against an HWID.
    Returns (is_valid, expiry_unix). expiry_unix=0 means permanent."""
    if not key or not hwid:
        return False, 0
    b = key.upper().replace("-", "").replace(" ", "")
    if b.startswith("DRAXO"):
        b = b[5:]
    is_v2 = len(b) >= 26
    plen = 16 if is_v2 else 12
    if len(b) < (26 if is_v2 else 20):
        return False, 0
    b = b[:26] if is_v2 else b[:20]

    enc = _b32_decode(b)
    if len(enc) != plen:
        return False, 0

    sec = _recon_secret()
    dec = _poly_xor_inv(enc, sec)

    hwid_hash = hashlib.sha256(hwid.encode()).digest()
    if dec[:12] != hwid_hash[:12]:
        return False, 0

    if is_v2:
        expiry = (dec[12] << 24) | (dec[13] << 16) | (dec[14] << 8) | dec[15]
    else:
        expiry = 0

    return True, expiry


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

    if not key:
        return LicenseStatus(
            status="LOCKED",
            status_color="#ff4757",
            expiry_str="-",
            hwid=hwid,
            key_preview="",
        )

    is_valid, expiry_unix = _validate_key(key, hwid)

    if not is_valid:
        return LicenseStatus(
            status="LOCKED",
            status_color="#ff4757",
            expiry_str="-",
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
