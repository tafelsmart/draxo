"""Startet den Bot ohne Gateway und redet mit seiner API.

Der ganze Sinn der Netlify-Funktion ist, dass Slash-Commands ohne
Gateway-Verbindung funktionieren. Das setzt voraus, dass der Server
teilweise startet: ``LicenseAPI`` laeuft, der Bot-Client muss dafuer
*nicht* verbunden sein.

Genau das ist hier die Behauptung, die man sonst glaubt statt zu pruefen:
dass der Mint-Server ohne Discord-Login Keys ausstellt. Wenn der Bot
starr auf ``bot.start()`` wartet und die API erst danach freigibt, waere
die ganze Architektur tot — und man faende es erst heraus, wenn der erste
Nutzer /key schickt.

Lauf:  python -m unittest tests.test_mint_server -v
"""

from __future__ import annotations

import asyncio
import json
import os
import sys
import tempfile
import threading
import unittest
import urllib.error
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LAUNCHER_DIR = ROOT / "launcher"
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(LAUNCHER_DIR))
sys.path.insert(0, str(ROOT / "bot"))

TEST_HWID = "3F2A9C4D8B1E7A05C6D9F2B3A4C5D6E7"
TEST_USER = "1534959905104986314"
TOKEN = "mint-server-test-token"


class MintServerTest(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self._loop: asyncio.AbstractEventLoop | None = None
        self._thread: threading.Thread | None = None
        self._ready = threading.Event()
        self._error: BaseException | None = None

    def _start(self) -> str:
        from aiohttp import web

        from draxo_bot.api import LicenseAPI
        from draxo_bot.config import Config
        from draxo_bot.service import GrantService
        from draxo_bot.store import Store

        config = Config(
            token="x" * 59,
            guild_id=None,
            welcome_channel_id=None,
            log_channel_id=None,
            member_role_id=None,
            activity_text="",
            welcome_on_join=False,
            log_commands=False,
            signing_key=bytes.fromhex(
                "826b710d3ad3704c602cc528002cf8439840403658ff05338bbde5198aaaa000"
            ),
            key_hours=24,
            key_max_per_day=1,
            bind_machine=True,
            db_path=Path(self._tmp.name) / "mint.sqlite3",
            api_enabled=True,
            api_bind="127.0.0.1",
            api_port=0,
            api_token=TOKEN,
        )

        store = Store(config.db_path)
        service = GrantService(config, store)
        api = LicenseAPI(config, store, grant_service=service)
        self.addCleanup(store.close)

        self._loop = asyncio.new_event_loop()
        self._port: int | None = None

        async def boot() -> int:
            app = web.Application()
            app.add_routes(
                [
                    web.post("/api/v1/issue", api.handle_issue),
                    web.post("/api/v1/hwid", api.handle_hwid),
                    web.post("/api/v1/verify", api.handle_verify),
                    web.get("/api/v1/health", api.handle_health),
                ]
            )
            runner = web.AppRunner(app, access_log=None)
            await runner.setup()
            site = web.TCPSite(runner, "127.0.0.1", 0)
            await site.start()
            return site._server.sockets[0].getsockname()[1]

        def run() -> None:
            asyncio.set_event_loop(self._loop)
            try:
                self._port = self._loop.run_until_complete(boot())
            except BaseException as exc:  # noqa: BLE001
                self._error = exc
            finally:
                self._ready.set()
            self._loop.run_forever()

        self._thread = threading.Thread(target=run, daemon=True, name="draxo-mint")
        self._thread.start()
        self.addCleanup(self._stop)

        if not self._ready.wait(30):
            self.fail("Server nicht hochgekommen")
        if self._error is not None:
            raise self._error
        return f"http://127.0.0.1:{self._port}"

    def _stop(self) -> None:
        loop, thread = self._loop, self._thread
        if loop is not None and thread is not None:
            loop.call_soon_threadsafe(loop.stop)
            thread.join(timeout=10)
            if not loop.is_closed():
                loop.close()

    def _post(self, base: str, path: str, body: dict, token: str = TOKEN) -> dict:
        request = urllib.request.Request(
            base + path,
            data=json.dumps(body).encode(),
            headers={
                "content-type": "application/json",
                "authorization": f"Bearer {token}",
            },
            method="POST",
        )
        with urllib.request.urlopen(request, timeout=20) as response:
            return json.loads(response.read())

    def test_ohne_gateway_erreichbar(self) -> None:
        base = self._start()

        with urllib.request.urlopen(base + "/api/v1/health", timeout=10) as response:
            health = json.loads(response.read())
        self.assertTrue(health.get("ok"), health)

    def test_ohne_token_kein_zugriff(self) -> None:
        base = self._start()
        with self.assertRaises(urllib.error.HTTPError) as caught:
            self._post(base, "/api/v1/issue", {"discord_id": TEST_USER, "hwid": TEST_HWID}, token="falsch")
        self.assertEqual(caught.exception.code, 401)

    def test_grant_ohne_discord_login(self) -> None:
        base = self._start()
        result = self._post(
            base, "/api/v1/issue", {"discord_id": TEST_USER, "hwid": TEST_HWID}
        )
        self.assertTrue(result.get("ok"), result)
        self.assertTrue(result["grant"].startswith("DRAXO3-"), result)

        from license_signing import verify_grant

        verified = verify_grant(result["grant"], hwid=TEST_HWID)
        self.assertTrue(verified.valid, verified.reason)

    def test_snowflake_als_string(self) -> None:
        """Discord-IDs sprengen 2^53 in JavaScript — sie kommen als String."""
        base = self._start()
        # 9007199254740993 ist 2^53+1 und in JS nicht darstellbar.
        tricky = "9007199254740993"
        result = self._post(base, "/api/v1/hwid", {"discord_id": tricky, "hwid": TEST_HWID})
        self.assertTrue(result.get("ok"), result)
        self.assertEqual(result["hwid"], TEST_HWID)

    def test_ohne_discord_id_abgelehnt(self) -> None:
        base = self._start()
        # Fehlende discord_id ist ein 400, kein 200 mit ok=false: die
        # Anfrage ist unbrauchbar, nicht nur inhaltlich negativ. Die
        # Funktion wertet genau diesen Unterschied aus.
        with self.assertRaises(urllib.error.HTTPError) as caught:
            self._post(base, "/api/v1/issue", {"hwid": TEST_HWID})
        self.assertEqual(caught.exception.code, 400)
        body = json.loads(caught.exception.read())
        self.assertIn("discord_id", body.get("reason", ""), body)

    def test_ungueltige_hwid_wird_abgelehnt(self) -> None:
        base = self._start()
        result = self._post(
            base, "/api/v1/issue", {"discord_id": TEST_USER, "hwid": "zu kurz"}
        )
        # Hier 200 mit ok=false: die Anfrage war gueltig, der Inhalt nicht.
        # Die Funktion zeigt genau diesen Grund dem Nutzer.
        self.assertFalse(result.get("ok"), result)
        self.assertTrue(result.get("reason"), result)

    def test_rate_limit_greift(self) -> None:
        base = self._start()
        first = self._post(base, "/api/v1/issue", {"discord_id": TEST_USER, "hwid": TEST_HWID})
        self.assertTrue(first.get("ok"), first)
        second = self._post(base, "/api/v1/issue", {"discord_id": TEST_USER, "hwid": TEST_HWID})
        self.assertFalse(second.get("ok"), second)

    def test_widerruf_macht_den_grant_ungueltig(self) -> None:
        base = self._start()
        issued = self._post(
            base, "/api/v1/issue", {"discord_id": TEST_USER, "hwid": TEST_HWID}
        )
        self.assertTrue(issued.get("ok"), issued)

        verified = self._post(
            base,
            "/api/v1/verify",
            {"grant": issued["grant"], "hwid": TEST_HWID, "discord_id": TEST_USER},
        )
        self.assertTrue(verified.get("ok"), verified)

        # Von Hand widerrufen — so macht es der Bot bei einem Missbrauchsfall.
        from draxo_bot.store import Store

        store = Store(Path(self._tmp.name) / "mint.sqlite3")
        try:
            revoked = store.revoke(issued["fingerprint"])
            self.assertEqual(revoked, 1, "Widerruf hat nichts gefunden")
        finally:
            store.close()

        after = self._post(
            base,
            "/api/v1/verify",
            {"grant": issued["grant"], "hwid": TEST_HWID, "discord_id": TEST_USER},
        )
        self.assertFalse(after.get("ok"), after)
        self.assertIn("widerrufen", after.get("reason", ""), after)


if __name__ == "__main__":
    unittest.main()
