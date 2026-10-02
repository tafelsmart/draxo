"""
test_discord_auth.py
--------------------
Prüft den kompletten Discord-Anmeldepfad, ohne echtes Discord zu
brauchen: eine nachgebaute Autorisierungs-Seite und eine nachgebaute
Discord-API laufen lokal auf 127.0.0.1.

Geprüft wird der komplette Weg, den ein Nutzer geht:

    Browser → /oauth2/authorize  →  Redirect auf /callback
             →  Code-Tausch (inkl. echter PKCE-Prüfung)
             →  /users/@me       →  Avatar-URL
             →  Guild-Beitritt   →  Token-Ablage mit DPAPI

Außerdem die Fehlerpfade: falscher state, abgebrohene Autorisierung,
Zeitüberschreitung, defekte Client-ID, HTTP-Fehler.

Aufruf:

    python tests/test_discord_auth.py     # ohne pytest
    pytest tests/test_discord_auth.py     # wenn pytest vorhanden ist
"""

from __future__ import annotations

import base64
import hashlib
import json
import os
import sys
import threading
import time
import urllib.parse
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

_ROOT = Path(__file__).resolve().parent.parent
_LAUNCHER_DIR = _ROOT / "launcher"
sys.path.insert(0, str(_ROOT))
sys.path.insert(0, str(_LAUNCHER_DIR))

CLIENT_ID = "1234567890123456789"
ACCESS_TOKEN = "mock-access-token"
REFRESH_TOKEN = "mock-refresh-token"
USER_ID = "778899001122334455"
GUILD_ID = "1534959905104986314"
AVATAR_HASH = "abcdef0123456789"


# ══════════════════════════════════════════════════════════════════
#  Nachbau: Discord-Autorisierung + Discord-API
# ══════════════════════════════════════════════════════════════════

class _DiscordMock:
    """Ein einziger Server für Autorisierung, Token, Profil und Guild."""

    def __init__(self) -> None:
        self.httpd = ThreadingHTTPServer(("127.0.0.1", 0), self._handler())
        self.httpd.expected_challenge = ""      # type: ignore[attr-defined]
        self.httpd.token_calls = 0             # type: ignore[attr-defined]
        self.httpd.joined = []                 # type: ignore[attr-defined]
        self.behaviour = "ok"                  # ok | deny | bad_state | not_found
        self.token_error = ""                  # error-Code für /oauth2/token
        self.thread = threading.Thread(
            target=self.httpd.serve_forever, kwargs={"poll_interval": 0.05},
            daemon=True)
        self.thread.start()

    # ── Buchstaben ────────────────────────────────────────────────
    @property
    def port(self) -> int:
        return self.httpd.server_address[1]

    @property
    def base(self) -> str:
        return f"http://127.0.0.1:{self.port}"

    def close(self) -> None:
        self.httpd.shutdown()
        self.httpd.server_close()

    # ── Request-Verarbeitung ──────────────────────────────────────
    def _handler(self):
        mock = self

        class Handler(BaseHTTPRequestHandler):
            protocol_version = "HTTP/1.0"

            def log_message(self, *_args):
                return

            def _send(self, status: int, body: bytes,
                      ctype: str = "application/json") -> None:
                self.send_response(status)
                self.send_header("Content-Type", ctype)
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)

            def _json(self, status: int, payload: dict) -> None:
                self._send(status, json.dumps(payload).encode("utf-8"))

            def do_GET(self):  # noqa: N802
                parsed = urllib.parse.urlparse(self.path)
                query = urllib.parse.parse_qs(parsed.query)

                # ── Autorisierungs-Seite ──
                if parsed.path == "/oauth2/authorize":
                    self._authorize(query)
                    return

                # ── Nutzerprofil ──
                if parsed.path.endswith("/users/@me"):
                    auth = self.headers.get("Authorization", "")
                    if auth != f"Bearer {ACCESS_TOKEN}":
                        self._json(401, {"error": "401: Unauthorized"})
                        return
                    self._json(200, mock._user_payload())
                    return

                # ── Avatar-Bild ──
                if "/avatars/" in parsed.path:
                    self._send(200, b"\x89PNG\r\n\x1a\n" + b"\x00" * 32,
                               "image/png")
                    return

                self._json(404, {"error": "not found"})

            def do_POST(self):  # noqa: N802
                length = int(self.headers.get("Content-Length", "0"))
                raw = self.rfile.read(length).decode("utf-8")
                form = urllib.parse.parse_qs(raw)
                self.server.token_calls += 1   # type: ignore[attr-defined]

                if mock.token_error:
                    self._json(400, {"error": mock.token_error,
                                     "error_description": "absichtlich fehl"})
                    return

                if (form.get("client_id") or [""])[0] != CLIENT_ID:
                    self._json(401, {"error": "invalid_client"})
                    return

                # ── Refresh: braucht keinen PKCE-Nachweis ──
                if (form.get("grant_type") or [""])[0] == "refresh_token":
                    self._json(200, {"access_token": ACCESS_TOKEN,
                                     "refresh_token": REFRESH_TOKEN,
                                     "expires_in": 604800,
                                     "token_type": "Bearer"})
                    return

                # ── PKCE wirklich prüfen (nur beim Ersttausch) ──
                verifier = (form.get("code_verifier") or [""])[0]
                expected = self.server.expected_challenge  # type: ignore[attr-defined]
                digest = hashlib.sha256(verifier.encode()).digest()
                got = base64.urlsafe_b64encode(digest).decode().rstrip("=")
                if not verifier or got != expected:
                    self._json(400, {"error": "invalid_grant",
                                     "error_description": "PKCE stimmt nicht"})
                    return

                self._json(200, {"access_token": ACCESS_TOKEN,
                                 "refresh_token": REFRESH_TOKEN,
                                 "expires_in": 604800,
                                 "scope": "identify guilds.join",
                                 "token_type": "Bearer"})

            def do_PUT(self):  # noqa: N802
                # PUT /guilds/<id>/members/@me mit {"access_token": "..."}
                parsed = urllib.parse.urlparse(self.path)
                length = int(self.headers.get("Content-Length", "0"))
                body = json.loads(self.rfile.read(length).decode("utf-8") or "{}")
                if parsed.path != f"/api/v10/guilds/{GUILD_ID}/members/@me":
                    self._json(404, {"error": "unknown route"})
                    return
                if body.get("access_token") != ACCESS_TOKEN:
                    self._json(401, {"error": "401: Unauthorized"})
                    return
                self.server.joined.append(parsed.path)   # type: ignore[attr-defined]
                self._send(204, b"", "text/plain")

            def _authorize(self, query: dict) -> None:
                redirect_uri = (query.get("redirect_uri") or [""])[0]
                state = (query.get("state") or [""])[0]
                self.server.expected_challenge = (   # type: ignore[attr-defined]
                    (query.get("code_challenge") or [""])[0])

                if mock.behaviour == "deny":
                    target = (f"{redirect_uri}?error=access_denied"
                              f"&error_description=Zugriff%20verweigert"
                              f"&state={state}")
                elif mock.behaviour == "bad_state":
                    target = f"{redirect_uri}?code=CODE&state=falscher-state"
                else:
                    target = f"{redirect_uri}?code=MOCK_CODE_123&state={state}"

                self.send_response(302)
                self.send_header("Location", target)
                self.send_header("Content-Length", "0")
                self.end_headers()

        return Handler

    def _user_payload(self) -> dict:
        return {
            "id": USER_ID,
            "username": "clemens",
            "global_name": "Clemens",
            "avatar": AVATAR_HASH,
            "discriminator": "0",
            "email": "clemens@example.com",
        }


# ══════════════════════════════════════════════════════════════════
#  Test-Harness
# ══════════════════════════════════════════════════════════════════

def _install_env(mock: _DiscordMock) -> None:
    os.environ["DRAXO_DISCORD_CLIENT_ID"] = CLIENT_ID
    os.environ["DRAXO_DISCORD_AUTHORIZE_URL"] = f"{mock.base}/oauth2/authorize"
    os.environ["DRAXO_DISCORD_API_BASE"] = f"{mock.base}/api/v10"
    os.environ["DRAXO_NO_BROWSER"] = "1"


def _fake_browser(auth, mock: _DiscordMock, timeout: float = 12.0):
    """Simuliert den Browser: ruft die Autorisierungs-URL auf."""
    def open_url(url: str) -> None:
        def go():
            try:
                urllib.request.urlopen(url, timeout=5).read()
            except urllib.error.HTTPError:
                pass          # 302 ist hier das gewünschte Ergebnis
            except Exception:
                time.sleep(timeout)   # Timeout-Fall simulieren
        threading.Thread(target=go, daemon=True).start()
    auth._open_browser = open_url


def _fresh_store(tmp: Path):
    import discord_auth as da
    return da.TokenStore(tmp / "draxo_auth.json")


# ══════════════════════════════════════════════════════════════════
#  Tests
# ══════════════════════════════════════════════════════════════════

def test_vollstaendiger_login(mock: _DiscordMock, tmp: Path) -> None:
    """Der komplette Weg von der Autorisierungs-URL bis zum gespeicherten Token."""
    import discord_auth as da

    auth = da.DiscordAuth(client_id=CLIENT_ID, store=_fresh_store(tmp),
                          timeout=12.0)
    _fake_browser(auth, mock)

    result = auth.login()

    assert result.ok, f"Login schlug fehl: {result.error}"
    assert result.user is not None
    assert result.user.id == USER_ID
    assert result.user.display_name == "Clemens"
    assert result.user.handle == "@clemens"
    assert result.user.initials == "CL", result.user.initials
    assert AVATAR_HASH in result.user.avatar_url, "Avatar-URL fehlt"

    # PKCE wurde vom Server angenommen → der Challenge stimmte.
    assert mock.httpd.token_calls == 1, mock.httpd.token_calls
    assert result.guild_joined, f"Guild-Beitritt fehlgeschlagen: {result.guild_message}"
    assert any(GUILD_ID in path for path in mock.httpd.joined)

    # Token liegt verschlüsselt auf der Platte.
    store = _fresh_store(tmp)
    user, tokens = store.load()
    assert user is not None and user.id == USER_ID
    assert tokens.access_token == ACCESS_TOKEN
    assert tokens.refresh_token == REFRESH_TOKEN
    assert not tokens.expired(margin=0), tokens.expires_at
    raw = store.path.read_text(encoding="utf-8")
    assert ACCESS_TOKEN not in raw, "Token liegt im Klartext auf der Platte!"


def test_wiederherstellen(mock: _DiscordMock, tmp: Path) -> None:
    """Nach dem Neustart wird die Anmeldung ohne Browserübergabe erkannt."""
    import discord_auth as da

    store = _fresh_store(tmp)
    da.DiscordUser(id=USER_ID, username="clemens", global_name="Clemens")
    store.save(da.DiscordUser(id=USER_ID, username="clemens",
                              global_name="Clemens"),
               da.TokenBundle(access_token=ACCESS_TOKEN,
                              refresh_token=REFRESH_TOKEN,
                              expires_at=time.time() + 7200))

    session = da.DiscordSession()
    session._store = store
    session._auth = da.DiscordAuth(client_id=CLIENT_ID, store=store)

    assert session.restore() is True
    assert session.signed_in is True
    assert session.display_name == "Clemens"


def test_token_erneuern(mock: _DiscordMock, tmp: Path) -> None:
    """Abgelaufenes Access-Token wird beim Start still erneuert."""
    import discord_auth as da

    store = _fresh_store(tmp)
    store.save(da.DiscordUser(id=USER_ID, username="clemens", global_name="Clemens"),
               da.TokenBundle(access_token=ACCESS_TOKEN,
                              refresh_token=REFRESH_TOKEN,
                              expires_at=time.time() - 10))

    session = da.DiscordSession()
    session._store = store
    session._auth = da.DiscordAuth(client_id=CLIENT_ID, store=store)

    assert session.restore() is True
    assert session.signed_in is True


def test_falscher_state(mock: _DiscordMock, tmp: Path) -> None:
    """Antwort mit falschem state wird abgewiesen (CSRF-Schutz)."""
    import discord_auth as da

    mock.behaviour = "bad_state"
    try:
        auth = da.DiscordAuth(client_id=CLIENT_ID, store=_fresh_store(tmp),
                              timeout=12.0)
        _fake_browser(auth, mock)
        result = auth.login()
        assert not result.ok
        assert "Sicherheits" in result.error, result.error
    finally:
        mock.behaviour = "ok"


def test_autorisierung_abgebrochen(mock: _DiscordMock, tmp: Path) -> None:
    """„Abbrechen“ im Browser führt zu einer verständlichen Meldung."""
    import discord_auth as da

    mock.behaviour = "deny"
    try:
        auth = da.DiscordAuth(client_id=CLIENT_ID, store=_fresh_store(tmp),
                              timeout=12.0)
        _fake_browser(auth, mock)
        result = auth.login()
        assert not result.ok
        assert result.cancelled is True
        assert result.error_title == "Anmeldung abgebrochen"
    finally:
        mock.behaviour = "ok"


def test_zeitueberschreitung(mock: _DiscordMock, tmp: Path) -> None:
    """Kein Callback → nach dem Timeout eine klare Meldung, kein Hängen."""
    import discord_auth as da

    auth = da.DiscordAuth(client_id=CLIENT_ID, store=_fresh_store(tmp),
                          timeout=1.0)
    started = time.monotonic()
    result = auth.login()          # _open_browser bleibt unangetastet
    assert not result.ok
    assert "Zeitüberschreitung" in result.error, result.error
    assert time.monotonic() - started < 8.0, "Timeout greift nicht"


def test_falsche_client_id(mock: _DiscordMock, tmp: Path) -> None:
    """Ungültige Client-ID → verständlicher Fehler statt Absturz."""
    import discord_auth as da

    auth = da.DiscordAuth(client_id="2222222222222222222",
                          store=_fresh_store(tmp), timeout=12.0)
    _fake_browser(auth, mock)
    result = auth.login()
    assert not result.ok
    assert "ungültig" in result.error.lower(), result.error


def test_nicht_konfiguriert(mock: _DiscordMock, tmp: Path) -> None:
    """Ohne Client-ID gibt es eine Anleitung statt eines Absturzes.

    Wird bewusst unabhaengig von der lokalen Konfiguration des
    Entwicklers geprueft: env-Variable, Standardwert *und* die
    ``discord_oauth.json`` neben der EXE werden ausser Kraft gesetzt.
    Sonst waere der Test bei eingetragener Client-ID rot.
    """
    import discord_auth as da

    old_env = os.environ.pop("DRAXO_DISCORD_CLIENT_ID", None)
    old_default = da.DEFAULT_CLIENT_ID
    old_path = da.oauth_config_path
    try:
        da.DEFAULT_CLIENT_ID = ""
        da.oauth_config_path = lambda: tmp / "gibt-es-nicht.json"
        assert da.resolve_client_id() == ""

        auth = da.DiscordAuth(client_id="", store=_fresh_store(tmp))
        assert auth.configured is False
        result = auth.login()
        assert not result.ok
        assert "discord_oauth.json" in result.error, result.error
    finally:
        da.oauth_config_path = old_path
        da.DEFAULT_CLIENT_ID = old_default
        if old_env is not None:
            os.environ["DRAXO_DISCORD_CLIENT_ID"] = old_env


def test_client_id_aus_datei(mock: _DiscordMock, tmp: Path) -> None:
    """Die Client-ID wird aus discord_oauth.json gelesen."""
    import discord_auth as da

    old_path = da.oauth_config_path
    old_env = os.environ.pop("DRAXO_DISCORD_CLIENT_ID", None)
    try:
        da.oauth_config_path = lambda: tmp / "discord_oauth.json"
        assert da.resolve_client_id() == ""

        (tmp / "discord_oauth.json").write_text(
            json.dumps({"client_id": CLIENT_ID}), encoding="utf-8")
        assert da.resolve_client_id() == CLIENT_ID
        assert da.is_configured() is True

        # Kaputte Datei darf nicht crashen.
        (tmp / "discord_oauth.json").write_text("{ kaputt", encoding="utf-8")
        assert da.resolve_client_id() == ""
    finally:
        da.oauth_config_path = old_path
        if old_env is not None:
            os.environ["DRAXO_DISCORD_CLIENT_ID"] = old_env


def test_echte_konfiguration_wird_gefunden(mock, tmp: Path) -> None:
    """Im Projekt liegt eine Client-ID — sonst waere nichts testbar."""
    import discord_auth as da

    old_env = os.environ.pop("DRAXO_DISCORD_CLIENT_ID", None)
    try:
        pfad = da.oauth_config_path()
        if not pfad.exists():
            print("      (uebersprungen: keine discord_oauth.json im Projekt)")
            return
        cid = da.resolve_client_id()
        assert cid, "discord_oauth.json enthaelt keine client_id"
        assert cid.isdigit(), f"client_id muss eine Zahl sein: {cid!r}"
        assert da.is_configured() is True
        print(f"      (Projekt-Client-ID aus {pfad.name}: {cid})")
    finally:
        if old_env is not None:
            os.environ["DRAXO_DISCORD_CLIENT_ID"] = old_env


def test_abmelden(mock: _DiscordMock, tmp: Path) -> None:
    """logout() löscht die gespeicherte Anmeldung vollständig."""
    import discord_auth as da

    store = _fresh_store(tmp)
    store.save(da.DiscordUser(id=USER_ID, username="clemens"),
               da.TokenBundle(access_token=ACCESS_TOKEN, expires_at=time.time() + 7200))

    session = da.DiscordSession()
    session._store = store
    session._auth = da.DiscordAuth(client_id=CLIENT_ID, store=store)
    assert session.restore() is True

    session.logout()
    assert session.signed_in is False
    assert not store.path.exists()


def test_kein_leck(mock: _DiscordMock, tmp: Path) -> None:
    """Callback-Server beantwortet nur /callback — alles andere 404."""
    import discord_auth as da

    server = da.CallbackServer()
    server.start()
    try:
        root = urllib.request.urlopen(
            f"http://127.0.0.1:{server.port}/", timeout=5)
        assert root.status == 200
        assert b"Warte auf Anmeldung" in root.read()

        try:
            urllib.request.urlopen(f"http://127.0.0.1:{server.port}/fremd",
                                   timeout=5)
            raise AssertionError("Unbekannter Pfad hätte 404 sein müssen")
        except urllib.error.HTTPError as exc:
            assert exc.code == 404
    finally:
        server.close()


# ══════════════════════════════════════════════════════════════════
#  Ausführung
# ══════════════════════════════════════════════════════════════════

def main() -> int:
    import tempfile

    mock = _DiscordMock()
    _install_env(mock)
    passed, failed = 0, 0

    tests = [
        test_vollstaendiger_login,
        test_wiederherstellen,
        test_token_erneuern,
        test_falscher_state,
        test_autorisierung_abgebrochen,
        test_zeitueberschreitung,
        test_falsche_client_id,
        test_nicht_konfiguriert,
        test_client_id_aus_datei,
        test_echte_konfiguration_wird_gefunden,
        test_abmelden,
        test_kein_leck,
    ]

    print("=" * 62)
    print("  Discord-Anmeldung — End-to-End-Test")
    print("=" * 62)
    print(f"  Mock-Discord : {mock.base}")
    print(f"  Client-ID    : {CLIENT_ID}")
    print(f"  Guild        : {GUILD_ID}")
    print("-" * 62)

    for test in tests:
        with tempfile.TemporaryDirectory() as folder:
            tmp = Path(folder)
            name = test.__name__.replace("test_", "")
            try:
                test(mock, tmp)
            except Exception as exc:  # noqa: BLE001
                failed += 1
                print(f"  [FEHLER] {name:26s} {type(exc).__name__}: {exc}")
            else:
                passed += 1
                print(f"  [OK]     {name}")

    mock.close()
    print("-" * 62)
    print(f"  {passed} bestanden, {failed} fehlgeschlagen")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
