"""HTTP-Endpunkt für den Launcher: Grant online prüfen und widerrufen.

Der Client kann einen Grant auch offline prüfen — er trägt den öffentlichen
Schlüssel. Dieser Dienst beantwortet die eine Frage, die offline nicht geht:
*gilt der Grant noch?*  Genau daran hängt der Widerruf.

Ohne diesen Dienst läuft alles, nur ein gesperrter Grant wirkt erst nach
seinem Ablauf. Deshalb ist er optional, aber empfohlen.

Bewusst auf asyncio aufgesetzt: der Bot bringt aiohttp schon mit, das hält
den Prozess bei einer Datei und einem Port — und die Ereignisschleife des
Bots blockiert nicht, während ein Anfrage hängt.

Schutz: API_ENABLED=1 setzt API_TOKEN voraus. Ohne Token antwortet der
Dienst jedem, der die Adresse kennt; das ist bei einem Lizenzserver genau
die falsche Eigenschaft.
"""

from __future__ import annotations

import json
import logging
import time
from typing import Optional

from aiohttp import web

from .config import Config
from .service import GrantService, fingerprint
from .signing import grants  # noqa: F401 - stellt license_signing bereit
from .store import Store

log = logging.getLogger("draxo.api")

MAX_BODY = 8 * 1024


class LicenseAPI:
    def __init__(
        self,
        config: Config,
        store: Store,
        grant_service: Optional[GrantService] = None,
    ) -> None:
        self._cfg = config
        self._store = store
        self._grants = grant_service
        self._runner: Optional[web.AppRunner] = None

    # ── Lebenszyklus ───────────────────────────────────────────────

    async def start(self) -> None:
        if not self._cfg.api_enabled:
            return
        if not self._cfg.api_token:
            # Bewusst Abbruch statt laufen: ein offener Lizenzserver ohne
            # Token ist ein offener Lizenzserver.
            raise RuntimeError(
                "API_ENABLED=1 verlangt ein API_TOKEN. Ohne Token antwortet der "
                "Dienst jedem, der die Adresse kennt."
            )

        app = web.Application(client_max_size=MAX_BODY)
        app.add_routes(
            [
                web.post("/api/v1/verify", self.handle_verify),
                web.post("/api/v1/issue", self.handle_issue),
                web.post("/api/v1/hwid", self.handle_hwid),
                web.get("/api/v1/publickey", self.handle_publickey),
                web.get("/api/v1/health", self.handle_health),
            ]
        )
        self._runner = web.AppRunner(app, access_log=None)
        await self._runner.setup()
        site = web.TCPSite(self._runner, self._cfg.api_bind, self._cfg.api_port)
        await site.start()
        log.info("Lizenz-API auf http://%s:%s bereit", self._cfg.api_bind, self._cfg.api_port)

    async def stop(self) -> None:
        if self._runner is not None:
            await self._runner.cleanup()
            self._runner = None

    # ── Hilfen ─────────────────────────────────────────────────────

    def _authorised(self, request: web.Request) -> bool:
        import hmac

        header = request.headers.get("Authorization", "")
        expected = f"Bearer {self._cfg.api_token}"
        return hmac.compare_digest(header, expected)

    def _reject(self, message: str, status: int = 401) -> web.Response:
        return web.json_response({"ok": False, "reason": message}, status=status)

    # ── Routen ─────────────────────────────────────────────────────

    async def handle_health(self, request: web.Request) -> web.Response:
        """Offen gedacht: sagt nur, ob der Dienst läuft."""
        return web.json_response({"ok": True, "service": "draxo-license"})

    async def handle_publickey(self, request: web.Request) -> web.Response:
        """Der oeffentliche Schluessel — unkritisch, aber nur mit Token.

        So kann ein frisch gebauter Client den Schluessel holen, ohne ihn
        erst in den Quelltext schreiben zu muessen.
        """
        if not self._authorised(request):
            return self._reject("Token fehlt oder passt nicht.")
        return web.json_response(
            {"ok": True, "public_key": grants.SERVER_PUBLIC_KEY_HEX, "alg": "ed25519"}
        )

    async def handle_issue(self, request: web.Request) -> web.Response:
        """Mintet einen Grant — die einzige Route, die den Schlüssel benutzt.

        Aufgerufen von der Netlify-Funktion, die den Interactions-Endpoint
        bedient. Diese Trennung ist Absicht: der private Schlüssel liegt in
        der ``.env`` auf diesem Server und wird auf Netlify nie deployed.
        Wäre die Funktion der Minter, w\u00e4re genau das Leck wieder da, das
        die Ed25519-Umstellung beseitigt hat — nur an einem anderen Ort.
        """
        if not self._authorised(request):
            return self._reject("Token fehlt oder passt nicht.")

        if self._grants is None or not self._grants.enabled:
            return web.json_response(
                {"ok": False, "reason": "Auf diesem Server ist kein Signaturschlüssel hinterlegt."},
                status=503,
            )

        try:
            body = await request.json()
        except (json.JSONDecodeError, ValueError):
            return web.json_response({"ok": False, "reason": "Ungültiges JSON."}, status=400)

        # Snowflakes sind Strings in JSON — sie sprengen 2^53 in JavaScript.
        # str() statt int() ist hier deshalb Absicht, nicht Bequemlichkeit.
        raw_id = str(body.get("discord_id", "")).strip()
        hwid = str(body.get("hwid", "") or "").strip()

        if not raw_id.isdigit():
            return web.json_response(
                {"ok": False, "reason": "discord_id fehlt oder ist keine Zahl."}, status=400
            )
        discord_id = int(raw_id)

        # Kein DM-Sender: der Aufrufer ist die Funktion, die dem Nutzer selbst
        # antwortet. Ein zweiter Zustellweg w\u00ecrde hier nur doppelt zustellen.
        token, expires_at, failure = await self._grants.issue(discord_id, hwid)
        if token is None:
            reason = failure.reason if failure and hasattr(failure, "reason") else None
            detail = getattr(failure, "description", None) or reason or "Kein Grant."
            return web.json_response({"ok": False, "reason": detail}, status=200)

        payload, signature = grants.unpack(token)
        return web.json_response(
            {
                "ok": True,
                "grant": token,
                "expires_at": expires_at,
                "discord_id": discord_id,
                "fingerprint": fingerprint(payload, signature),
                "machine_bound": bool(
                    grants.decode_payload(payload)["hwid_hash"] != b"\x00" * 8
                ),
            }
        )

    async def handle_hwid(self, request: web.Request) -> web.Response:
        """Hinterlegte HWID lesen, setzen oder loeschen.

        Die Netlify-Funktion braucht das fuer ``/key`` ohne Argument und fuer
        ``/status``. Im Gateway-Modus liest der Bot direkt aus SQLite; hier
        ginge das nicht, weil die Funktion keinen Datenbank-Zugriff hat.
        """
        if not self._authorised(request):
            return self._reject("Token fehlt oder passt nicht.")

        try:
            body = await request.json()
        except (json.JSONDecodeError, ValueError):
            return web.json_response({"ok": False, "reason": "Ungültiges JSON."}, status=400)

        raw_id = str(body.get("discord_id", "")).strip()
        if not raw_id.isdigit():
            return web.json_response(
                {"ok": False, "reason": "discord_id fehlt oder ist keine Zahl."}, status=400
            )
        user_id = int(raw_id)

        if body.get("clear") is True:
            self._store.clear_hwid(user_id)
            self._store.audit(str(user_id), "hwid.cleared")
            return web.json_response({"ok": True, "hwid": None})

        raw_hwid = str(body.get("hwid", "") or "").strip()
        if raw_hwid:
            try:
                normalised = grants.normalise_hwid(raw_hwid)
            except grants.GrantError as exc:
                return web.json_response({"ok": False, "reason": str(exc)}, status=400)
            self._store.set_hwid(user_id, normalised)
            self._store.audit(str(user_id), "hwid.set")
            return web.json_response({"ok": True, "hwid": normalised})

        stored = self._store.get_hwid(user_id)
        row = self._store.last_issue(user_id)
        last: Optional[dict] = None
        if row is not None:
            token_preview = f"{row['fingerprint'][:12]}"
            last = {
                "exists": True,
                # Nur der Fingerabdruck, nie der Grant: der Server kennt den
                # Token nicht (und soll ihn nicht), also gibt es hier auch
                # nichts zu vergeben.
                "preview": token_preview,
                "expires_at": int(row["expiry"]),
                "expired": bool(row["expiry"]) and int(row["expiry"]) < time.time(),
            }

        return web.json_response({"ok": True, "hwid": stored, "last": last})

    async def handle_verify(self, request: web.Request) -> web.Response:
        """Prueft einen Grant: Signatur, Bindung, Ablauf, Widerruf."""
        if not self._authorised(request):
            return self._reject("Token fehlt oder passt nicht.")

        try:
            body = await request.json()
        except (json.JSONDecodeError, ValueError):
            return web.json_response({"ok": False, "reason": "Ungültiges JSON."}, status=400)

        token = str(body.get("grant", "")).strip()
        if not token:
            return web.json_response({"ok": False, "reason": "Kein Grant mitgeschickt."}, status=400)

        hwid = str(body.get("hwid", "") or "").strip()
        discord_id = body.get("discord_id")
        try:
            discord_id = int(discord_id) if discord_id not in (None, "") else None
        except (TypeError, ValueError):
            discord_id = None

        grant = grants.verify_grant(token, hwid=hwid or None, discord_id=discord_id)
        if not grant.valid:
            return web.json_response({"ok": False, "reason": grant.reason}, status=200)

        # Signatur passt — jetzt der Serverteil: ist der Grant gesperrt?
        try:
            payload, signature = grants.unpack(token)
        except grants.GrantError as exc:
            return web.json_response({"ok": False, "reason": str(exc)}, status=200)

        print_id = fingerprint(payload, signature)
        if self._store.is_revoked(print_id):
            return web.json_response(
                {"ok": False, "reason": "Grant wurde vom Betreiber widerrufen."},
                status=200,
            )

        self._store.audit("api", "verify.ok", f"discord={grant.discord_id}")
        return web.json_response(
            {
                "ok": True,
                "reason": "OK",
                "expires_at": grant.expires_at,
                "discord_id": grant.discord_id,
                "machine_bound": grant.bound_to_machine,
            }
        )