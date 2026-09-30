"""
discord_auth.py
---------------
Discord-Anmeldung für den Draxo Client.

Ablauf (OAuth2 Authorization Code + PKCE):

    1. Launcher erzeugt ``state`` (Zufall) und ein PKCE-Paar
       (``code_verifier`` / ``code_challenge`` = SHA256(verifier)).
    2. Ein winziger HTTP-Server bindet auf ``127.0.0.1`` einen freien Port.
    3. Der Standardbrowser öffnet die Autorisierungs-URL von Discord.
    4. Discord leitet nach dem Bestätigen auf
       ``http://127.0.0.1:<port>/callback?code=…&state=…`` zurück.
    5. Der Launcher tauscht den Code gegen ein Access-Token, holt das
       Profil über ``/users/@me`` und tritt dem Server bei
       (``PUT /users/@me/guilds/<id>``, Scope ``guilds.join``).
    6. Der Callback-Server zeigt die Bestätigungsseite und beendet sich.

Warum PKCE und kein Client-Secret?
    Der Launcher wird als ``DraxoLauncher.exe`` ausgeliefert. Ein darin
    hinterlegtes Secret wäre für jeden Nutzer auslesbar. Mit PKCE braucht
    die gepackte Anwendung nur die öffentliche Client-ID — ein Secret
    existiert gar nicht erst. Discord nennt solche Apps „Public Client“.

Konfiguration der Client-ID (Reihenfolge der Prüfung):

    1. Umgebungsvariable ``DRAXO_DISCORD_CLIENT_ID``
    2. ``discord_oauth.json`` neben der EXE  →  ``{"client_id": "…"}``
    3. ``DEFAULT_CLIENT_ID`` in dieser Datei

Token-Ablage:
    ``draxo_auth.json`` im Konfigurationsordner. Das Refresh-Token wird
    über die Windows-DPAPI (CryptProtectData, nur für diesen Nutzer)
    verschlüsselt; die Profilfelder liegen im Klartext, damit das UI sie
    ohne Entschlüsselung anzeigen kann.
"""

from __future__ import annotations

import base64
import ctypes
import hashlib
import json
import logging
import os
import secrets
import string
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
import webbrowser
from dataclasses import dataclass
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Optional

logger = logging.getLogger("DraxoClient.auth")

# ══════════════════════════════════════════════════════════════════
#  Konstanten
# ══════════════════════════════════════════════════════════════════

#: Ziel-Server. Wird beim Anmelden automatisch beigetreten.
GUILD_ID = "1534959905104986314"
GUILD_NAME = "Draxo Client | FREE"
GUILD_INVITE = "https://discord.gg/5gXXHcv6nj"
GUILD_ICON_URL = (
    "https://cdn.discordapp.com/icons/1534959905104986314/"
    "2c3e339cf6b4717cba9b714da10356b7.png?size=128"
)

#: Platzhalter — wird ersetzt, sobald die Discord-Application existiert.
DEFAULT_CLIENT_ID = ""

AUTHORIZE_URL = "https://discord.com/oauth2/authorize"
API_BASE = "https://discord.com/api/v10"

#: ``identify``  → Profil (Name, Avatar, Banner)
#: ``guilds.join`` → darf den Launcher dem Server hinzufügen
SCOPES = "identify guilds.join"

USER_AGENT = "DraxoClient/1.0 (Windows)"
CALLBACK_PATH = "/callback"
AUTH_FILE = "draxo_auth.json"
OAUTH_FILE = "discord_oauth.json"

LOGIN_TIMEOUT_SEC = 300.0
REFRESH_MARGIN_SEC = 3600.0
HTTP_TIMEOUT_SEC = 20.0

VERBOSE = bool(os.environ.get("DRAXO_VERBOSE"))


def _env(name: str, default: str) -> str:
    return os.environ.get(name) or default


def authorize_url() -> str:
    return _env("DRAXO_DISCORD_AUTHORIZE_URL", AUTHORIZE_URL)


def api_base() -> str:
    return _env("DRAXO_DISCORD_API_BASE", API_BASE).rstrip("/")


# ══════════════════════════════════════════════════════════════════
#  Datenmodelle
# ══════════════════════════════════════════════════════════════════

@dataclass
class DiscordUser:
    """Ein angemeldeter Discord-Nutzer."""

    id: str
    username: str = ""
    global_name: str = ""
    avatar: str = ""
    discriminator: str = "0"
    email: str = ""
    guild_joined: bool = False
    avatar_url: str = ""

    @staticmethod
    def from_api(data: dict, *, base: Optional[str] = None) -> "DiscordUser":
        base = (base or api_base()).rstrip("/")
        user_id = str(data.get("id", ""))
        avatar = str(data.get("avatar") or "")
        avatar_url = ""
        if avatar:
            ext = "gif" if avatar.startswith("a_") else "png"
            avatar_url = f"{base}/users/{user_id}/avatars/{avatar}.{ext}?size=128"
        return DiscordUser(
            id=user_id,
            username=str(data.get("username", "")),
            global_name=str(data.get("global_name") or ""),
            avatar=avatar,
            discriminator=str(data.get("discriminator") or "0"),
            email=str(data.get("email", "")),
            avatar_url=avatar_url,
        )

    @property
    def display_name(self) -> str:
        """Anzeigename: „Nickname“ bevorzugt, sonst der Konto-Name."""
        return self.global_name or self.username or self.id

    @property
    def handle(self) -> str:
        """„name“ oder bei altem Naming-Schema „name#1234“."""
        if self.discriminator and self.discriminator != "0":
            return f"{self.username}#{self.discriminator}"
        return f"@{self.username}" if self.username else ""

    @property
    def initials(self) -> str:
        """Zwei Buchstaben für den Avatar-Platzhalter."""
        name = self.display_name.strip()
        parts = [p for p in name.replace("_", " ").replace("-", " ").split() if p]
        if not parts:
            return "?"
        if len(parts) == 1:
            return parts[0][:2].upper()
        return (parts[0][0] + parts[-1][0]).upper()

    def to_dict(self) -> dict:
        return {
            "id": self.id,
            "username": self.username,
            "global_name": self.global_name,
            "avatar": self.avatar,
            "discriminator": self.discriminator,
            "email": self.email,
            "guild_joined": self.guild_joined,
            "avatar_url": self.avatar_url,
        }

    @staticmethod
    def from_dict(data: dict) -> "DiscordUser":
        return DiscordUser(
            id=str(data.get("id", "")),
            username=str(data.get("username", "")),
            global_name=str(data.get("global_name", "")),
            avatar=str(data.get("avatar", "")),
            discriminator=str(data.get("discriminator", "0")),
            email=str(data.get("email", "")),
            guild_joined=bool(data.get("guild_joined", False)),
            avatar_url=str(data.get("avatar_url", "")),
        )


@dataclass
class LoginResult:
    """Ergebnis eines Anmeldeversuchs."""

    ok: bool = False
    user: Optional[DiscordUser] = None
    error: str = ""
    guild_joined: bool = False
    guild_message: str = ""
    cancelled: bool = False

    @property
    def error_title(self) -> str:
        if self.cancelled:
            return "Anmeldung abgebrochen"
        return "Anmeldung fehlgeschlagen"


# ══════════════════════════════════════════════════════════════════
#  Client-ID ermitteln
# ══════════════════════════════════════════════════════════════════

def _workdir() -> Path:
    """Projektordner — im gepackten Build der Ordner der EXE."""
    try:
        from utils import get_workdir
        return get_workdir()
    except Exception:  # pragma: no cover - nur bei direktem Import
        return Path(__file__).resolve().parent


def oauth_config_path() -> Path:
    return _workdir() / OAUTH_FILE


def auth_file_path() -> Path:
    try:
        from utils import get_config_dir
        return get_config_dir() / AUTH_FILE
    except Exception:  # pragma: no cover
        return _workdir() / AUTH_FILE


def resolve_client_id() -> str:
    """Ermittelt die Client-ID aus Env, Datei oder Code-Default."""
    from_env = os.environ.get("DRAXO_DISCORD_CLIENT_ID", "").strip()
    if from_env:
        return from_env

    path = oauth_config_path()
    if path.exists():
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
            value = str(data.get("client_id", "")).strip()
            if value:
                return value
        except (OSError, ValueError, TypeError):
            logger.warning("discord_oauth.json ist unlesbar: %s", path)

    return DEFAULT_CLIENT_ID.strip()


def is_configured() -> bool:
    return bool(resolve_client_id())


# ══════════════════════════════════════════════════════════════════
#  DPAPI — Token-Verschlüsselung
# ══════════════════════════════════════════════════════════════════

class _DataBlob(ctypes.Structure):
    _fields_ = [("cbData", ctypes.c_uint32),
                ("pbData", ctypes.POINTER(ctypes.c_char))]


_CRYPTPROTECT_UI_FORBIDDEN = 0x1


def sys_platform_is_windows() -> bool:
    import sys
    return sys.platform == "win32"


def _protect(data: bytes) -> Optional[bytes]:
    """Verschlüsselt bytes mit der Windows-DPAPI (nur dieser Nutzer)."""
    if not sys_platform_is_windows():
        return None
    try:
        buffer = ctypes.create_string_buffer(data, len(data))
        blob_in = _DataBlob(len(data), ctypes.cast(buffer, ctypes.POINTER(ctypes.c_char)))
        blob_out = _DataBlob()
        ok = ctypes.windll.crypt32.CryptProtectData(  # type: ignore[attr-defined]
            ctypes.byref(blob_in), None, None, None, None,
            _CRYPTPROTECT_UI_FORBIDDEN, ctypes.byref(blob_out))
        if not ok:
            return None
        try:
            return ctypes.string_at(blob_out.pbData, blob_out.cbData)
        finally:
            ctypes.windll.kernel32.LocalFree(blob_out.pbData)  # type: ignore[attr-defined]
    except Exception:  # noqa: BLE001
        logger.debug("DPAPI-Verschlüsselung nicht verfügbar.", exc_info=True)
        return None


def _unprotect(data: bytes) -> Optional[bytes]:
    """Entschlüsselt bytes aus der Windows-DPAPI."""
    if not sys_platform_is_windows():
        return None
    try:
        buffer = ctypes.create_string_buffer(data, len(data))
        blob_in = _DataBlob(len(data), ctypes.cast(buffer, ctypes.POINTER(ctypes.c_char)))
        blob_out = _DataBlob()
        ok = ctypes.windll.crypt32.CryptUnprotectData(  # type: ignore[attr-defined]
            ctypes.byref(blob_in), None, None, None, None,
            _CRYPTPROTECT_UI_FORBIDDEN, ctypes.byref(blob_out))
        if not ok:
            return None
        try:
            return ctypes.string_at(blob_out.pbData, blob_out.cbData)
        finally:
            ctypes.windll.kernel32.LocalFree(blob_out.pbData)  # type: ignore[attr-defined]
    except Exception:  # noqa: BLE001
        logger.debug("DPAPI-Entschlüsselung nicht verfügbar.", exc_info=True)
        return None


def _pack(value: str) -> str:
    """Verschlüsselt (wenn möglich) und liefert einen speicherbaren String."""
    raw = value.encode("utf-8")
    blob = _protect(raw)
    if blob is not None:
        return "dpapi:" + base64.b64encode(blob).decode("ascii")
    logger.warning("DPAPI nicht verfügbar — Token wird nur Base64-kodiert gespeichert.")
    return "plain:" + base64.b64encode(raw).decode("ascii")


def _unpack(value: str) -> str:
    prefix, _, payload = value.partition(":")
    try:
        raw = base64.b64decode(payload)
    except Exception:  # noqa: BLE001
        return ""
    if prefix == "dpapi":
        out = _unprotect(raw)
        return out.decode("utf-8", "replace") if out else ""
    return raw.decode("utf-8", "replace")


# ══════════════════════════════════════════════════════════════════
#  HTTP
# ══════════════════════════════════════════════════════════════════

class DiscordAPIError(RuntimeError):
    """Fehlerantwort von Discord mit verständlichem Text."""

    def __init__(self, message: str, status: int = 0, payload: Optional[dict] = None):
        super().__init__(message)
        self.status = status
        self.payload = payload or {}


def _http_json(url: str, *, data: Optional[bytes] = None,
               headers: Optional[dict] = None,
               method: str = "GET") -> dict:
    """JSON-Anfrage an Discord mit sauberer Fehlerauswertung."""
    all_headers = {"User-Agent": USER_AGENT, "Accept": "application/json"}
    if headers:
        all_headers.update(headers)

    request = urllib.request.Request(
        url, data=data, headers=all_headers, method=method)

    if VERBOSE:
        logger.debug("HTTP %s %s", method, url.split("?")[0])

    try:
        with urllib.request.urlopen(request, timeout=HTTP_TIMEOUT_SEC) as response:
            body = response.read().decode("utf-8", "replace")
    except urllib.error.HTTPError as exc:
        body = exc.read().decode("utf-8", "replace")
        payload = _safe_json(body)
        message = _translate_api_error(payload, exc.code)
        raise DiscordAPIError(message, exc.code, payload) from exc
    except urllib.error.URLError as exc:
        raise DiscordAPIError(
            "Keine Verbindung zu Discord. Internet prüfen?", 0, {}) from exc
    except TimeoutError as exc:
        raise DiscordAPIError("Zeitüberschreitung bei Discord.", 0, {}) from exc

    return _safe_json(body)


def _safe_json(body: str) -> dict:
    try:
        data = json.loads(body)
        return data if isinstance(data, dict) else {}
    except ValueError:
        return {}


#: Discord-Fehlertexte → verständliche deutsche Meldung
_ERROR_TEXT = {
    "invalid_client": "Die Client-ID ist ungültig.",
    "unauthorized_client": "Diese Client-ID ist für PKCE nicht freigeschaltet.",
    "invalid_grant": "Der Anmeldecode ist abgelaufen — bitte erneut versuchen.",
    "invalid_request": "Die Anfrage an Discord war ungültig.",
    "unsupported_grant_type": "Discord akzeptiert diese Anmeldeart nicht.",
    "invalid_scope": "Mindestens eine angeforderte Berechtigung ist ungültig.",
    "slow_down": "Zu viele Versuche — bitte kurz warten.",
}


def _translate_api_error(payload: dict, status: int) -> str:
    code = str(payload.get("error", ""))
    if code in _ERROR_TEXT:
        return _ERROR_TEXT[code]
    description = str(payload.get("error_description", "")).strip()
    if description and description.lower() != code.lower():
        return description
    if status == 401:
        return "Discord hat die Anmeldung abgelehnt (401)."
    if status == 403:
        return "Zugriff verweigert (403) — ist der Bot auf dem Server?"
    if status == 429:
        return "Zu viele Anfragen — bitte einen Moment warten."
    if status == 0:
        return "Keine Verbindung zu Discord."
    return f"Discord antwortete mit HTTP {status}."


# ══════════════════════════════════════════════════════════════════
#  PKCE
# ══════════════════════════════════════════════════════════════════

_PKCE_ALPHABET = string.ascii_letters + string.digits + "-._~"


def new_state() -> str:
    """Zufälliger, nicht vorhersagbarer Wert gegen CSRF."""
    return secrets.token_urlsafe(32)


def new_code_verifier() -> str:
    return "".join(secrets.choice(_PKCE_ALPHABET) for _ in range(128))


def code_challenge(verifier: str) -> str:
    digest = hashlib.sha256(verifier.encode("ascii")).digest()
    return base64.urlsafe_b64encode(digest).decode("ascii").rstrip("=")


def build_authorize_url(client_id: str, *, state: str, challenge: str,
                       redirect_uri: str) -> str:
    """Baut die Discord-Autorisierungs-URL."""
    query = urllib.parse.urlencode({
        "client_id": client_id,
        "response_type": "code",
        "redirect_uri": redirect_uri,
        "scope": SCOPES,
        "state": state,
        "code_challenge": challenge,
        "code_challenge_method": "S256",
        "prompt": "consent",
    })
    return f"{authorize_url()}?{query}"


# ══════════════════════════════════════════════════════════════════
#  Callback-Server
# ══════════════════════════════════════════════════════════════════

_PAGE_CSS = """
  * { margin:0; padding:0; box-sizing:border-box; }
  body { background:#08080a; color:#fff; height:100vh;
         display:flex; align-items:center; justify-content:center;
         font-family:'Segoe UI',system-ui,-apple-system,sans-serif; }
  .wrap { text-align:center; max-width:520px; padding:24px; }
  .mark { width:64px; height:64px; border-radius:50%; margin:0 auto 26px;
          display:flex; align-items:center; justify-content:center;
          background:#141419; border:1px solid #232329; font-size:30px; }
  .ok   { background:rgba(34,197,94,.12);  color:#4ade80; }
  .err  { background:rgba(239,68,68,.12);  color:#f87171; }
  h1 { font-size:27px; font-weight:600; letter-spacing:-.4px; line-height:1.25; }
  p  { margin-top:12px; color:#a1a1aa; font-size:15px; line-height:1.55; }
  .foot { margin-top:30px; color:#52525b; font-size:12.5px; }
"""


def _result_page(ok: bool, title: str, message: str) -> bytes:
    icon = "&#10003;" if ok else "&#10007;"
    css_class = "ok" if ok else "err"
    return (
        "<!DOCTYPE html><html lang=\"de\"><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        f"<title>{title}</title><style>{_PAGE_CSS}</style></head><body>"
        f"<div class=\"wrap\"><div class=\"mark {css_class}\">{icon}</div>"
        f"<h1>{title}</h1><p>{message}</p>"
        "<p class=\"foot\">Du kannst dieses Fenster jetzt schließen.</p>"
        "</div></body></html>"
    ).encode("utf-8")


class _CallbackHandler(BaseHTTPRequestHandler):
    """Nimmt genau einen ``/callback``-Aufruf entgegen."""

    server_version = "DraxoAuth"
    sys_version = ""
    protocol_version = "HTTP/1.0"

    def log_message(self, *_args) -> None:  # Ausgabe unterdrücken
        return

    def _respond(self, status: int, body: bytes) -> None:
        self.send_response(status)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        try:
            self.wfile.write(body)
        except OSError:
            pass

    def do_GET(self) -> None:  # noqa: N802 (Name von BaseHTTPRequestHandler)
        parsed = urllib.parse.urlparse(self.path)
        params = urllib.parse.parse_qs(parsed.query)
        server: "CallbackServer" = self.server  # type: ignore[assignment]

        if parsed.path == CALLBACK_PATH:
            server.result = {
                "code": (params.get("code") or [""])[0],
                "state": (params.get("state") or [""])[0],
                "error": (params.get("error") or [""])[0],
                "error_description": (params.get("error_description") or [""])[0],
            }
            ok = bool(server.result["code"])
            if ok:
                title, message = "Anmeldung abgeschlossen", "Du bist jetzt angemeldet."
            else:
                title = "Anmeldung abgebrochen"
                message = server.result["error_description"] or \
                    "Die Autorisierung wurde nicht freigegeben."
            self._respond(200, _result_page(ok, title, message))
            # Server beenden — handle_request läukt in einem eigenen Thread.
            threading.Thread(target=server.shutdown, daemon=True).start()
            return

        if parsed.path in ("/", "/index.html"):
            body = _result_page(
                True, "Warte auf Anmeldung …",
                "Du kannst dieses Fenster schließen und zurück zum Launcher gehen.")
            self._respond(200, body)
            return

        if parsed.path == "/favicon.ico":
            self.send_response(204)
            self.send_header("Content-Length", "0")
            self.end_headers()
            return

        self._respond(404, _result_page(False, "Nicht gefunden",
                                        "Diese Adresse ist nicht vergeben."))


class CallbackServer:
    """Kurzlebiger HTTP-Server auf 127.0.0.1 für den OAuth-Callback."""

    def __init__(self, host: str = "127.0.0.1", port: int = 0) -> None:
        self._httpd = ThreadingHTTPServer((host, port), _CallbackHandler)
        self._httpd.result = {}  # type: ignore[attr-defined]
        self._thread: Optional[threading.Thread] = None

    @property
    def port(self) -> int:
        return self._httpd.server_address[1]

    @property
    def redirect_uri(self) -> str:
        return f"http://127.0.0.1:{self.port}{CALLBACK_PATH}"

    @property
    def result(self) -> dict:
        return getattr(self._httpd, "result", {}) or {}

    def start(self) -> None:
        self._thread = threading.Thread(
            target=self._httpd.serve_forever, kwargs={"poll_interval": 0.2},
            daemon=True, name="OAuthCallback")
        self._thread.start()
        logger.info("Callback-Server lauscht auf %s", self.redirect_uri)

    def wait(self, timeout: float) -> Optional[dict]:
        """Blockiert bis zum Callback oder Timeout. None = Timeout."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if self.result:
                return self.result
            if self._thread is None or not self._thread.is_alive():
                break
            time.sleep(0.1)
        return self.result or None

    def close(self) -> None:
        try:
            self._httpd.shutdown()
        except Exception:  # noqa: BLE001
            pass
        try:
            self._httpd.server_close()
        except Exception:  # noqa: BLE001
            pass


# ══════════════════════════════════════════════════════════════════
#  Token-Speicher
# ══════════════════════════════════════════════════════════════════

@dataclass
class TokenBundle:
    access_token: str = ""
    refresh_token: str = ""
    expires_at: float = 0.0

    def expired(self, margin: float = REFRESH_MARGIN_SEC) -> bool:
        return not self.access_token or time.time() + margin >= self.expires_at


class TokenStore:
    """Liest und schreibt ``draxo_auth.json``."""

    def __init__(self, path: Optional[Path] = None) -> None:
        self._path = path or auth_file_path()

    @property
    def path(self) -> Path:
        return self._path

    def load(self) -> tuple[Optional[DiscordUser], TokenBundle]:
        if not self._path.exists():
            return None, TokenBundle()
        try:
            data = json.loads(self._path.read_text(encoding="utf-8"))
        except (OSError, ValueError) as exc:
            logger.warning("Auth-Datei unlesbar (%s) — starte ohne Anmeldung.", exc)
            return None, TokenBundle()

        user = DiscordUser.from_dict(data.get("user") or {})
        tokens = TokenBundle(
            access_token=_unpack(str(data.get("access_token", ""))),
            refresh_token=_unpack(str(data.get("refresh_token", ""))),
            expires_at=float(data.get("expires_at") or 0.0),
        )
        if not user.id:
            return None, tokens
        return user, tokens

    def save(self, user: DiscordUser, tokens: TokenBundle) -> bool:
        payload = {
            "version": 1,
            "user": user.to_dict(),
            "access_token": _pack(tokens.access_token),
            "refresh_token": _pack(tokens.refresh_token),
            "expires_at": tokens.expires_at,
            "saved_at": time.time(),
            "guild_id": GUILD_ID,
        }
        try:
            self._path.parent.mkdir(parents=True, exist_ok=True)
            tmp = self._path.with_suffix(".tmp")
            tmp.write_text(json.dumps(payload, indent=2, ensure_ascii=False),
                           encoding="utf-8")
            tmp.replace(self._path)
            return True
        except OSError:
            logger.exception("Auth-Datei konnte nicht gespeichert werden.")
            return False

    def clear(self) -> None:
        try:
            if self._path.exists():
                self._path.unlink()
        except OSError:
            logger.exception("Auth-Datei konnte nicht gelöscht werden.")


# ══════════════════════════════════════════════════════════════════
#  Anmeldung
# ══════════════════════════════════════════════════════════════════

NOT_CONFIGURED_HINT = (
    "Es ist noch keine Discord-Application hinterlegt.\n\n"
    "1. Auf discord.com/developers/applications eine Application anlegen\n"
    "2. Unter „OAuth2“ eine Redirect-URL eintragen:\n"
    "   http://127.0.0.1:<beliebiger Port>/callback\n"
    "3. Die Client-ID in discord_oauth.json neben der EXE eintragen:\n"
    '   { "client_id": "DEINE_CLIENT_ID" }'
)


class DiscordAuth:
    """Ein vollständiger Anmeldedurchlauf. Blockierend — in einem Thread aufrufen."""

    def __init__(self, client_id: Optional[str] = None,
                 store: Optional[TokenStore] = None,
                 timeout: float = LOGIN_TIMEOUT_SEC) -> None:
        self._client_id = (client_id or resolve_client_id()).strip()
        self._store = store or TokenStore()
        self._timeout = timeout

    @property
    def client_id(self) -> str:
        return self._client_id

    @property
    def configured(self) -> bool:
        return bool(self._client_id)

    # ── Schritt 1+2: Browser und Callback-Server ─────────────────────

    def _open_browser(self, url: str) -> None:
        if os.environ.get("DRAXO_NO_BROWSER"):
            logger.info("Browser unterdrückt. Autorisierungs-URL: %s", url)
            return
        try:
            webbrowser.open(url, new=2)
        except Exception:  # noqa: BLE001
            logger.exception("Browser konnte nicht geöffnet werden.")

    # ── Schritt 3: Code → Token ─────────────────────────────────────

    def _exchange(self, code: str, verifier: str, redirect_uri: str) -> TokenBundle:
        payload = urllib.parse.urlencode({
            "client_id": self._client_id,
            "grant_type": "authorization_code",
            "code": code,
            "redirect_uri": redirect_uri,
            "code_verifier": verifier,
        }).encode("utf-8")

        data = _http_json(f"{api_base()}/oauth2/token", data=payload,
                          headers={"Content-Type": "application/x-www-form-urlencoded"},
                          method="POST")
        return TokenBundle(
            access_token=str(data.get("access_token", "")),
            refresh_token=str(data.get("refresh_token", "")),
            expires_at=time.time() + float(data.get("expires_in") or 0),
        )

    # ── Schritt 4: Profil ───────────────────────────────────────────

    def fetch_user(self, access_token: str) -> DiscordUser:
        data = _http_json(f"{api_base()}/users/@me",
                          headers={"Authorization": f"Bearer {access_token}"})
        if not data.get("id"):
            raise DiscordAPIError("Discord lieferte kein Nutzerprofil.")
        return DiscordUser.from_api(data)

    # ── Schritt 5: Server beitreten ─────────────────────────────────

    def join_guild(self, access_token: str,
                   guild_id: str = GUILD_ID) -> tuple[bool, str]:
        """Tritt dem Server bei. Fehler sind nicht fatal — nur ein Hinweis.

        Discord-Route: ``PUT /guilds/<guild>/members/@me`` mit dem
        Access-Token im JSON-Body (nicht als Query-Parameter).
        """
        try:
            payload = json.dumps({"access_token": access_token}).encode("utf-8")
            _http_json(f"{api_base()}/guilds/{guild_id}/members/@me",
                       data=payload, headers={"Content-Type": "application/json"},
                       method="PUT")
            return True, f"Du bist jetzt in {GUILD_NAME}."
        except DiscordAPIError as exc:
            return False, (f"Server-Beitritt nicht möglich: {exc}. "
                           f"Bitte manuell beitreten: {GUILD_INVITE}")
        except Exception as exc:  # noqa: BLE001
            return False, f"Server-Beitritt fehlgeschlagen ({exc})."

    # ── Kompletter Ablauf ──────────────────────────────────────────

    def login(self) -> LoginResult:
        if not self.configured:
            return LoginResult(ok=False, error=NOT_CONFIGURED_HINT)

        state = new_state()
        verifier = new_code_verifier()
        challenge = code_challenge(verifier)

        server = CallbackServer()
        server.start()
        try:
            url = build_authorize_url(
                self._client_id, state=state, challenge=challenge,
                redirect_uri=server.redirect_uri)
            logger.info("Öffne Discord-Autorisierung: %s", url)
            self._open_browser(url)

            result = server.wait(self._timeout)
            if result is None:
                minutes = max(1, round(self._timeout / 60))
                return LoginResult(
                    ok=False,
                    error=(f"Zeitüberschreitung — die Anmeldung wurde nicht "
                           f"innerhalb von {minutes} Minute(n) abgeschlossen."),
                    cancelled=True)
            if result.get("error"):
                description = (result.get("error_description")
                               or "Die Autorisierung wurde abgebrochen.")
                return LoginResult(ok=False, error=str(description),
                                   cancelled=True)
            if result.get("state") != state:
                logger.error("State-Prüfung fehlgeschlagen — Abbruch.")
                return LoginResult(
                    ok=False,
                    error="Sicherheitsprüfung fehlgeschlagen (state passt nicht). "
                          "Bitte erneut versuchen.")
            if not result.get("code"):
                return LoginResult(ok=False, error="Discord lieferte keinen Code.")

            tokens = self._exchange(str(result["code"]), verifier,
                                    server.redirect_uri)
            if not tokens.access_token:
                return LoginResult(ok=False,
                                   error="Discord lieferte kein Access-Token.")

            user = self.fetch_user(tokens.access_token)
            joined, message = self.join_guild(tokens.access_token)
            user.guild_joined = joined

            self._store.save(user, tokens)
            logger.info("Angemeldet als %s (%s)", user.display_name, user.id)

            return LoginResult(ok=True, user=user, guild_joined=joined,
                               guild_message=message)
        except DiscordAPIError as exc:
            logger.error("Discord-API-Fehler: %s", exc)
            return LoginResult(ok=False, error=str(exc))
        except Exception as exc:  # noqa: BLE001
            logger.exception("Unerwarteter Fehler bei der Anmeldung.")
            return LoginResult(ok=False, error=f"Unerwarteter Fehler: {exc}")
        finally:
            server.close()

    # ── Token erneuern ─────────────────────────────────────────────

    def refresh(self, tokens: TokenBundle) -> TokenBundle:
        if not tokens.refresh_token:
            raise DiscordAPIError("Kein Refresh-Token vorhanden.")
        payload = urllib.parse.urlencode({
            "client_id": self._client_id,
            "grant_type": "refresh_token",
            "refresh_token": tokens.refresh_token,
        }).encode("utf-8")
        data = _http_json(f"{api_base()}/oauth2/token", data=payload,
                          headers={"Content-Type": "application/x-www-form-urlencoded"},
                          method="POST")
        return TokenBundle(
            access_token=str(data.get("access_token", "")),
            refresh_token=str(data.get("refresh_token", "")) or tokens.refresh_token,
            expires_at=time.time() + float(data.get("expires_in") or 0),
        )


# ══════════════════════════════════════════════════════════════════
#  Sitzung (das, was das Launcher-Fenster benutzt)
# ══════════════════════════════════════════════════════════════════

class DiscordSession:
    """Hält den Anmeldestatus über die Lebensdauer des Launchers.

    Blockierend — ``login()`` und ``restore()`` gehören in einen Thread.
    """

    def __init__(self) -> None:
        self._store = TokenStore()
        self._auth = DiscordAuth(store=self._store)
        self._tokens = TokenBundle()
        self._user: Optional[DiscordUser] = None
        self._lock = threading.Lock()

    # ── Abfragen ───────────────────────────────────────────────────

    @property
    def user(self) -> Optional[DiscordUser]:
        return self._user

    @property
    def signed_in(self) -> bool:
        return self._user is not None and bool(self._user.id)

    @property
    def display_name(self) -> str:
        return self._user.display_name if self._user else "Nicht angemeldet"

    @property
    def store_path(self) -> Path:
        return self._store.path

    # ── Aktionen ───────────────────────────────────────────────────

    def login(self) -> LoginResult:
        """Vollständige Anmeldung. Blockierend."""
        with self._lock:
            result = self._auth.login()
            if result.ok and result.user is not None:
                self._user = result.user
                self._tokens = self._load_tokens()
            return result

    def restore(self) -> bool:
        """Lädt eine gespeicherte Anmeldung und erneuert sie bei Bedarf."""
        with self._lock:
            user, tokens = self._store.load()
            if user is None:
                return False
            self._user = user
            self._tokens = tokens

            if tokens.expired():
                if not tokens.refresh_token or not self._auth.configured:
                    logger.info("Gespeicherte Anmeldung abgelaufen — "
                                "erneute Anmeldung nötig.")
                    self._user = None
                    return False
                try:
                    self._tokens = self._auth.refresh(tokens)
                    self._user = self._auth.fetch_user(self._tokens.access_token)
                    self._store.save(self._user, self._tokens)
                except DiscordAPIError as exc:
                    logger.warning("Token-Erneuerung fehlgeschlagen: %s", exc)
                    self._user = None
                    return False
            return True

    def logout(self) -> None:
        """Meldet ab und löscht die lokal gespeicherten Tokens."""
        with self._lock:
            self._user = None
            self._tokens = TokenBundle()
            self._store.clear()
            logger.info("Abgemeldet, lokale Tokens entfernt.")

    def _load_tokens(self) -> TokenBundle:
        _user, tokens = self._store.load()
        return tokens


def format_session_path(path: Optional[Path] = None) -> str:
    """Für Fehlermeldungen: wo die Anmeldung gespeichert wird."""
    return str(path or auth_file_path())
