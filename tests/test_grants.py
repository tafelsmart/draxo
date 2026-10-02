"""Grant-Kette von Ende zu Ende.

Geprueft wird die Kette, die jetzt die einzige zwischen Server und Client
besteht:

    Bot (privater Schluessel)  ->  Grant  ->  Launcher (oeffentlicher
    Schluessel)  ->  Signatur ok / HWID-Bindung / Konto-Bindung / Ablauf

Der Bot laeuft im Test ohne Discord: Store und Signer sind normale Python-
Objekte. Nur die HTTP-API braucht eine echte Event-Loop — und die laeuft
deshalb im Hintergrund, waehrend die HTTP-Aufrufe im Executor blockieren.
Ein Aufruf im Haupt-Thread wuerde die Loop anhalten und der Server wuerde
nie antworten.
"""

from __future__ import annotations

import asyncio
import json
import os
import pathlib
import sys
import tempfile
import threading
import time
import unittest
import urllib.error
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parent.parent
LAUNCHER_DIR = ROOT / "launcher"
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(LAUNCHER_DIR))
sys.path.insert(0, str(ROOT / "bot"))

import license_manager as lm  # noqa: E402
import license_signing as grants  # noqa: E402
from draxo_bot.api import LicenseAPI  # noqa: E402
from draxo_bot.config import Config  # noqa: E402
from draxo_bot.service import GrantService, fingerprint  # noqa: E402
from draxo_bot.store import Store  # noqa: E402

SEED = bytes.fromhex("826b710d3ad3704c602cc528002cf8439840403658ff05338bbde5198aaaa000")
HWID = "3F2A9C14B7E85D2061FA3C9E70B45D82"
USER = 1087401415481237545
PORT = 8899


def make_config(**overrides) -> Config:
    base = dict(
        token="x",
        guild_id=None,
        welcome_channel_id=None,
        log_channel_id=None,
        member_role_id=None,
        activity_text="",
        welcome_on_join=False,
        log_commands=False,
        signing_key=SEED,
        key_hours=24,
        key_max_per_day=1,
        bind_machine=True,
        db_path=pathlib.Path(tempfile.mkdtemp()) / "t.sqlite3",
        api_enabled=False,
        api_bind="127.0.0.1",
        api_port=PORT,
        api_token=None,
    )
    base.update(overrides)
    return Config(**base)


class FakeUser:
    """Minimaler Nutzer: der Dienst schickt DMs, die hier nur gezählt werden."""

    id = USER
    name = "tester"

    def __init__(self) -> None:
        self.dms: list = []

    def __str__(self) -> str:
        return f"@{self.name}"


class TestGrantChain(unittest.TestCase):
    def setUp(self) -> None:
        os.environ["DRAXO_SIGNING_KEY"] = SEED.hex()
        os.environ.pop("DRAXO_PUBLIC_KEY_HEX", None)
        self.store = Store(make_config().db_path)
        self.service = GrantService(make_config(), self.store)
        self.user = FakeUser()

    def tearDown(self) -> None:
        self.store.close()

    def issue(self, **kw):
        kw.setdefault("discord_id", USER)
        kw.setdefault("hwid", HWID)
        return self.service._signer.issue(**kw)

    # ── Ausgabe ────────────────────────────────────────────────────

    def test_public_key_matches_launcher(self) -> None:
        self.assertEqual(self.service.public_key_hex, grants.SERVER_PUBLIC_KEY_HEX)

    def test_issued_grant_accepted_by_launcher(self) -> None:
        token = self.issue(hours=24).token
        grant = lm.grant_detail(token, hwid=HWID)
        self.assertTrue(grant.valid, grant.reason)
        self.assertEqual(grant.discord_id, USER)
        self.assertTrue(grant.bound_to_machine)
        self.assertTrue(grant.bound_to_account)
        self.assertGreater(grant.seconds_left(), 23 * 3600)

    def test_grant_without_binding_rejected(self) -> None:
        from draxo_bot.signing import SigningError

        with self.assertRaises(SigningError):
            self.service._signer.issue(hours=24)

    def test_token_shape(self) -> None:
        token = self.issue().token
        self.assertTrue(token.startswith("DRAXO3-"))
        # 25 Byte Nutzlast + 64 Byte Signatur -> 143 Base32-Zeichen
        self.assertEqual(len(token), len("DRAXO3-") + 143)

    def test_permanent_grant(self) -> None:
        grant = lm.grant_detail(self.issue(hours=0).token, hwid=HWID)
        self.assertTrue(grant.valid)
        self.assertEqual(grant.seconds_left(), -1)

    def test_account_only_grant_works_on_any_machine(self) -> None:
        token = self.issue(discord_id=USER, hwid="", hours=24).token
        self.assertTrue(lm.grant_detail(token, hwid=HWID).valid)
        self.assertTrue(lm.grant_detail(token, hwid="F" * 32).valid)

    # ── Ablehnung ──────────────────────────────────────────────────

    def test_foreign_hwid_rejected(self) -> None:
        reason = lm.grant_detail(self.issue().token, hwid="F" * 32).reason
        self.assertIn("Rechner", reason)

    def test_foreign_account_rejected(self) -> None:
        reason = lm.grant_detail(self.issue().token, hwid=HWID, discord_id=1).reason
        self.assertIn("Konto", reason)

    def test_expired_rejected(self) -> None:
        token = self.issue(hours=1).token
        reason = lm.grant_detail(token, hwid=HWID, now=time.time() + 3700).reason
        self.assertIn("abgelaufen", reason)

    def test_tampered_rejected(self) -> None:
        token = self.issue().token
        for mutant in (token[:-2] + "ZZ", token[:-8] + "AAAAAAAA", "DRAXO3-" + "Z" * 143):
            self.assertFalse(lm.grant_detail(mutant, hwid=HWID).valid, mutant)

    def test_legacy_key_rejected_with_clear_reason(self) -> None:
        reason = lm.grant_detail("DRAXO-F7X66-AZXVC-Z3GYE-Y776Z-JSP0W-G", hwid=HWID).reason
        self.assertIn("DRAXO3", reason)

    def test_garbage_rejected(self) -> None:
        for junk in ("", "   ", "hello", "DRAXO3-!!!!"):
            self.assertFalse(lm.grant_detail(junk, hwid=HWID).valid, repr(junk))

    def test_other_servers_key_rejected(self) -> None:
        token = self.issue().token
        os.environ["DRAXO_PUBLIC_KEY_HEX"] = "00" * 32
        try:
            self.assertIn("Signatur", lm.grant_detail(token, hwid=HWID).reason)
        finally:
            os.environ.pop("DRAXO_PUBLIC_KEY_HEX")

    def test_signature_covers_every_field(self) -> None:
        """Auch eine echt signierte Nutzlast für eine andere Maschine ist nutzlos."""
        payload = grants.encode_payload(
            discord_id=USER,
            hwid_hash=grants.hwid_fingerprint("F" * 32),
            issued_at=0,
            expires_at=int(time.time()) + 9999,
        )
        token = grants.pack(payload, grants.sign(SEED, payload))
        self.assertIn("Rechner", lm.grant_detail(token, hwid=HWID).reason)
        self.assertTrue(lm.grant_detail(token, hwid="F" * 32).valid)

    # ── Rate-Limit, Datenbank, Widerruf ────────────────────────────

    def record(self, token: str) -> str:
        payload, signature = grants.unpack(token)
        self.store.record_grant(
            user_id=USER,
            discord_id=USER,
            hwid=HWID,
            fingerprint=fingerprint(payload, signature),
            hwid_hash=grants.hwid_fingerprint(HWID),
            expiry=int(time.time()) + 3600,
        )
        return fingerprint(payload, signature)

    def test_rate_limit(self) -> None:
        self.record(self.issue().token)
        self.assertGreater(
            self.store.next_quota_reset(USER, window=1, per_day=1), int(time.time())
        )

    def test_revocation_is_stored_not_invisible(self) -> None:
        token = self.issue().token
        print_id = self.record(token)
        self.assertFalse(self.store.is_revoked(print_id))
        self.store.revoke(print_id)
        self.assertTrue(self.store.is_revoked(print_id))
        # Offline bleibt er gueltig — das ist der dokumentierte Preis.
        self.assertTrue(lm.grant_detail(token, hwid=HWID).valid)

    def test_fingerprint_is_stable_and_specific(self) -> None:
        a, b = self.issue().token, self.issue(hours=1).token
        pa, sa = grants.unpack(a)
        pb, sb = grants.unpack(b)
        self.assertEqual(fingerprint(pa, sa), fingerprint(pa, sa))
        self.assertNotEqual(fingerprint(pa, sa), fingerprint(pb, sb))

    def test_stats(self) -> None:
        self.assertEqual(self.store.stats(), {"hwids": 0, "issued": 0, "revoked": 0})

    def test_hwid_helpers(self) -> None:
        self.assertEqual(
            grants.normalise_hwid("  3f2a 9c14\nb7e8 5d20 61fa 3c9e 70b4 5d82 "),
            HWID,
        )
        self.assertEqual(grants.mask_hwid(HWID), "3F2A…5D82")
        for bad in ("", "ZU KURZ", "Z" * 32):
            with self.assertRaises(grants.GrantError):
                grants.normalise_hwid(bad)


class TestLicenseAPI(unittest.TestCase):
    """Der HTTP-Endpunkt, über den der Launcher den Widerruf erfährt."""

    @classmethod
    def setUpClass(cls) -> None:
        os.environ["DRAXO_SIGNING_KEY"] = SEED.hex()
        os.environ.pop("DRAXO_PUBLIC_KEY_HEX", None)
        cls.cfg = make_config(api_enabled=True, api_token="test-token")
        cls.store = Store(cls.cfg.db_path)
        cls.service = GrantService(cls.cfg, cls.store)
        cls.api = LicenseAPI(cls.cfg, cls.store)

        # Die Loop muss durchgehend *laufen*, nicht nur bis zu einem
        # run_until_complete: run_coroutine_threadsafe stellt Aufgaben ein,
        # die jemand bedienen muss. Also ein eigener Thread.
        if sys.platform == "win32":
            # aiohttp braucht unter Windows eine Selector-Loop. Auf der
            # Proactor-Loop (Standard seit 3.8) nimmt die Verbindung an und
            # schliesst sie sofort wieder, ohne zu antworten.
            cls.loop = asyncio.SelectorEventLoop()
        else:
            cls.loop = asyncio.new_event_loop()
        cls._thread = threading.Thread(target=cls.loop.run_forever, daemon=True)
        cls._thread.start()
        asyncio.run_coroutine_threadsafe(cls.api.start(), cls.loop).result(timeout=30)

    @classmethod
    def tearDownClass(cls) -> None:
        asyncio.run_coroutine_threadsafe(cls.api.stop(), cls.loop).result(timeout=30)
        cls.loop.call_soon_threadsafe(cls.loop.stop)
        cls._thread.join(timeout=10)
        cls.loop.close()
        cls.store.close()

    def call(self, path, payload=None, token="test-token"):
        data = json.dumps(payload).encode() if payload is not None else None
        headers = {"Content-Type": "application/json"}
        if token:
            headers["Authorization"] = f"Bearer {token}"
        request = urllib.request.Request(
            f"http://127.0.0.1:{PORT}{path}",
            data=data,
            headers=headers,
            method="POST" if data else "GET",
        )

        def blocking():
            try:
                with urllib.request.urlopen(request, timeout=15) as response:
                    return response.status, json.load(response)
            except urllib.error.HTTPError as exc:
                # Fehlerseiten muessen nicht JSON sein — 500er von aiohttp
                # liefern HTML. Das hier soll der Grund sein, nicht der Parser.
                raw = exc.read().decode("utf-8", "replace")
                try:
                    return exc.code, json.loads(raw)
                except ValueError:
                    return exc.code, {"raw": raw[:400]}

        # Der Socket darf die Event-Loop nicht blockieren — der Server läuft
        # in genau dieser Loop. Deshalb läuft der Aufruf in einem Executor.
        #
        # run_coroutine_threadsafe verlangt eine Coroutine, kein Future:
        # run_in_executor liefert eins zurück, das man erst awaiten muss.
        async def runner():
            return await self.loop.run_in_executor(None, blocking)

        return asyncio.run_coroutine_threadsafe(runner(), self.loop).result(timeout=30)

    def test_server_is_reachable(self) -> None:
        """Sanity: der Dienst antwortet ueberhaupt — sonst sind die anderen
        Tests nur Rauschen auf einer kaputten Leitung."""
        status, body = self.call("/api/v1/health", token=None)
        self.assertEqual(status, 200)

    def issue(self, **kw):
        kw.setdefault("discord_id", USER)
        kw.setdefault("hwid", HWID)
        return self.service._signer.issue(**kw)

    def test_health_is_open(self) -> None:
        status, body = self.call("/api/v1/health", token=None)
        self.assertEqual(status, 200)
        self.assertTrue(body["ok"])

    def test_verify_needs_token(self) -> None:
        token = self.issue().token
        status, body = self.call(
            "/api/v1/verify", {"grant": token, "hwid": HWID}, token=None
        )
        self.assertEqual(status, 401)
        self.assertFalse(body["ok"])

    def test_verify_rejects_wrong_token(self) -> None:
        status, body = self.call(
            "/api/v1/verify", {"grant": self.issue().token, "hwid": HWID}, token="falsch"
        )
        self.assertEqual(status, 401)

    def test_verify_accepts_valid_grant(self) -> None:
        token = self.issue().token
        status, body = self.call(
            "/api/v1/verify", {"grant": token, "hwid": HWID, "discord_id": USER}
        )
        self.assertEqual(status, 200)
        self.assertTrue(body["ok"], body)
        self.assertTrue(body["machine_bound"])
        self.assertGreater(body["expires_at"], int(time.time()))

    def test_verify_rejects_wrong_hwid(self) -> None:
        token = self.issue().token
        _, body = self.call("/api/v1/verify", {"grant": token, "hwid": "F" * 32})
        self.assertFalse(body["ok"])
        self.assertIn("Rechner", body["reason"])

    def test_verify_honours_revocation(self) -> None:
        token = self.issue().token
        payload, signature = grants.unpack(token)
        self.store.record_grant(
            user_id=USER,
            discord_id=USER,
            hwid=HWID,
            fingerprint=fingerprint(payload, signature),
            hwid_hash=grants.hwid_fingerprint(HWID),
            expiry=int(time.time()) + 3600,
        )
        self.store.revoke(fingerprint(payload, signature))
        try:
            _, body = self.call("/api/v1/verify", {"grant": token, "hwid": HWID})
            self.assertFalse(body["ok"])
            self.assertIn("widerrufen", body["reason"])
        finally:
            self.store._db.execute("UPDATE grants SET revoked = 0")

    def test_launcher_against_running_api(self) -> None:
        """Der echte Launcher-Aufruf gegen den laufenden Dienst."""
        token = self.issue().token
        os.environ["DRAXO_LICENSE_API"] = f"http://127.0.0.1:{PORT}"
        try:
            ok, reason = lm.verify_online(
                token, hwid=HWID, discord_id=USER, api_token="test-token"
            )
            self.assertTrue(ok, reason)
            ok, reason = lm.verify_online(token, hwid="F" * 32, api_token="test-token")
            self.assertFalse(ok)
            self.assertIn("Rechner", reason)
        finally:
            os.environ.pop("DRAXO_LICENSE_API", None)

    def test_offline_when_no_server_configured(self) -> None:
        os.environ.pop("DRAXO_LICENSE_API", None)
        ok, reason = lm.verify_online("DRAXO3-x", hwid=HWID)
        self.assertFalse(ok)
        self.assertIn("DRAXO_LICENSE_API", reason)

    def test_publickey_endpoint(self) -> None:
        status, body = self.call("/api/v1/publickey")
        self.assertEqual(status, 200)
        self.assertEqual(body["public_key"], grants.SERVER_PUBLIC_KEY_HEX)
        self.assertEqual(body["alg"], "ed25519")


if __name__ == "__main__":
    unittest.main(verbosity=2)

class TestModuleSurface(unittest.TestCase):
    """Jedes Modul muss einzeln ladbar sein.

    Grund: beim Umstieg von KeyService auf GrantService blieb ein Import auf
    ein geloeschtes Modul stehen, und kein Test fiel auf, weil die Tests nur
    signing.py und service.py direkt importierten. Der Absturz kam erst beim
    echten Bot-Start. Genau das verhindert dieser Test.
    """

    def test_every_bot_module_imports(self) -> None:
        import importlib

        import draxo_bot

        bot_dir = pathlib.Path(draxo_bot.__file__).parent
        for path in sorted(bot_dir.glob("*.py")):
            if path.stem in ("__init__", "__main__"):
                continue
            with self.subTest(modul=path.name):
                importlib.import_module(f"draxo_bot.{path.stem}")

    def test_launcher_modules_import(self) -> None:
        import importlib

        for name in ("license_signing", "license_manager", "discord_auth"):
            with self.subTest(modul=name):
                importlib.import_module(name)

    def test_no_removed_symbols_left(self) -> None:
        """Nach dem Umstieg darf kein alter Name mehr auftauchen."""
        bot_dir = pathlib.Path(sys.modules["draxo_bot"].__file__).parent
        for path in sorted(bot_dir.glob("*.py")):
            text = path.read_text(encoding="utf-8")
            for alt in ("keyforge", "KeyforgeError", "KeyService", "key_service"):
                with self.subTest(datei=path.name, symbol=alt):
                    self.assertNotIn(alt, text, f"{path.name} erwaehnt noch {alt}")
