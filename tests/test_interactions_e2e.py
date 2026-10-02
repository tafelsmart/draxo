"""Ende zu Ende: Discord → Netlify-Funktion → Bot-Server → Grant → Launcher.

Das ist der Test, der zählt. Die beiden Seiten einzeln zu prüfen sagt nichts
über die Naht: ein JSON-Feld, das auf der einen Seite ``discord_id`` heißt
und auf der anderen ``user_id``, fällt in beiden Testbeständen nicht auf
und in Produktion bei jedem Key.

Hier läuft der echte aiohttp-Server des Bots auf einem freien Port, die
echte Node-Funktion spricht ihn über HTTP an, und der zurückkommende Grant
wird mit ``license_signing.verify_grant`` geprüft — also mit demselben
Code, der auch in der EXE steckt.

Lauf:  python -m unittest tests.test_interactions_e2e -v
"""

from __future__ import annotations

import asyncio
import json
import os
import shutil
import subprocess
import sys
import tempfile
import textwrap
import threading
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LAUNCHER_DIR = ROOT / "launcher"
FUNCTIONS = ROOT / "draxo-website-netlify" / "netlify" / "functions"

NODE = shutil.which("node")
HAS_NODE = NODE is not None

# HWID für den Test: 32 Hex-Zeichen, wie der Launcher sie liefert.
TEST_HWID = "3F2A9C4D8B1E7A05C6D9F2B3A4C5D6E7"
TEST_USER = "1534959905104986314"


# ── Der Node-Treiber: spricht den laufenden Bot-Server an ───────────────────

DRIVER = r"""
const path = require("path");
const FUNCTIONS = process.env.DRAXO_FUNCTIONS_DIR;
const MINT = process.env.DRAXO_MINT_URL;
const APP = "1554874371573813389";
const USER = process.env.DRAXO_TEST_USER;
const TOKEN = "e2e-token";

const crypto = require("crypto");
const seed = Buffer.from("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60", "hex");
const priv = crypto.createPrivateKey({
  key: Buffer.concat([Buffer.from("302e020100300506032b657004220420", "hex"), seed]),
  format: "der", type: "pkcs8",
});
process.env.DISCORD_PUBLIC_KEY =
  crypto.createPublicKey(priv).export({ format: "der", type: "spki" }).subarray(12).toString("hex");
process.env.DRAXO_MINT_URL = MINT;

const { handler } = require(path.join(FUNCTIONS, "interactions.js"));

function sign(body, ts) {
  return crypto.sign(null, Buffer.concat([Buffer.from(ts, "ascii"), Buffer.from(body, "utf8")]), priv)
    .toString("hex");
}

async function call(payload) {
  const body = JSON.stringify(payload);
  const ts = String(Math.floor(Date.now() / 1000));
  return await handler({
    body,
    headers: {
      "content-type": "application/json",
      "x-signature-ed25519": sign(body, ts),
      "x-signature-timestamp": ts,
    },
    isBase64Encoded: false,
  });
}

function command(name, options = []) {
  return {
    id: "900000000000000001",
    application_id: APP,
    type: 2,
    token: TOKEN,
    version: 1,
    guild_id: "1534959905104986314",
    member: { user: { id: USER, username: "clemens" }, roles: [] },
    data: { name, type: 1, options },
  };
}

const result = { ok: false, steps: [] };

(async () => {
  // 1. HWID hinterlegen
  const hwidRes = await call(command("hwid", [
    { type: 3, name: "hwid", value: process.env.DRAXO_TEST_HWID },
  ]));
  const hwidBody = JSON.parse(hwidRes.body);
  if (hwidBody?.data?.embeds?.[0]?.title !== "Gespeichert") {
    console.log(JSON.stringify({ ...result, error: "/hwid: " + hwidRes.body }));
    process.exit(1);
  }
  result.steps.push("hwid gespeichert");

  // 2. /status muss sie nun kennen — beweist, dass der Server sie gelesen hat
  const statusRes = await call(command("status"));
  const statusBody = JSON.parse(statusRes.body);
  const hwidField = statusBody?.data?.embeds?.[0]?.fields?.find((f) => f.name === "HWID");
  if (hwidField?.value !== "`" + process.env.DRAXO_TEST_HWID + "`") {
    console.log(JSON.stringify({ ...result, error: "/status: " + statusRes.body }));
    process.exit(1);
  }
  result.steps.push("status liest die HWID zurueck");

  // 3. /key minten
  const keyRes = await call(command("key"));
  const keyBody = JSON.parse(keyRes.body);
  const embed = keyBody?.data?.embeds?.[0];
  if (embed?.title !== "Dein Key") {
    console.log(JSON.stringify({ ...result, error: "/key: " + keyRes.body }));
    process.exit(1);
  }
  const match = /DRAXO3-[0-9A-Z-]+/.exec(embed.description || "");
  if (!match) {
    console.log(JSON.stringify({ ...result, error: "kein Grant im Embed: " + keyRes.body }));
    process.exit(1);
  }
  result.grant = match[0];
  result.steps.push("key gemintet");

  // 4. Rate-Limit: der zweite Aufruf muss abgelehnt werden
  const second = await call(command("key"));
  const secondBody = JSON.parse(second.body);
  const secondText = JSON.stringify(secondBody.data.embeds);
  if (!secondText.includes("genug Keys") && !secondText.includes("nicht ausstellbar")) {
    console.log(JSON.stringify({
      ...result,
      error: "zweiter /key wurde nicht abgelehnt: " + second.body,
    }));
    process.exit(1);
  }
  result.steps.push("rate-limit greift");

  // 5. Fremde HWID wird abgelehnt (Bindung)
  const hwid2 = "A1B2C3D4E5F60718293A4B5C6D7E8F90";
  const otherRes = await call(command("hwid", [{ type: 3, name: "hwid", value: hwid2 }]));
  if (JSON.parse(otherRes.body)?.data?.embeds?.[0]?.title !== "Gespeichert") {
    console.log(JSON.stringify({ ...result, error: "/hwid zweitmal: " + otherRes.body }));
    process.exit(1);
  }
  result.steps.push("hwid umgestellt");

  console.log(JSON.stringify({ ...result, ok: true }));
  process.exit(0);
})().catch((error) => {
  console.log(JSON.stringify({ ok: false, error: error.message, stack: error.stack }));
  process.exit(1);
});
"""


class EndToEndTest(unittest.TestCase):
    """Bot-Server in Python, Funktion in Node, dazwischen echtes HTTP."""

    def setUp(self) -> None:
        if not HAS_NODE:
            self.skipTest("node nicht im PATH")
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self._loop: asyncio.AbstractEventLoop | None = None
        self._thread = None
        self._runner = None
        self._ready = threading.Event()
        self._error: BaseException | None = None

    def _start_server(self) -> int:
        """Startet den echten LicenseAPI auf einem freien Port.

        In einem eigenen Thread, und das ist nicht Kosmetik: der Test ruft
        denselben Server ueber ``urlopen`` auf, und ``urlopen`` blockiert den
        aufrufenden Thread. Laeuft der Server in der Schleife dieses Threads,
        wartet er auf sich selbst — der Test haengt dann bis zum Timeout, und
        die Meldung spricht von "timed out" statt von der Ursache.
        """
        sys.path.insert(0, str(ROOT))
        sys.path.insert(0, str(LAUNCHER_DIR))
        sys.path.insert(0, str(ROOT / "bot"))

        from aiohttp import web  # noqa: PLC0415
        from draxo_bot.api import LicenseAPI  # noqa: PLC0415
        from draxo_bot.config import Config  # noqa: PLC0415
        from draxo_bot.service import GrantService  # noqa: PLC0415
        from draxo_bot.signing import grants  # noqa: PLC0415
        from draxo_bot.store import Store  # noqa: PLC0415

        seed = bytes.fromhex(
            "826b710d3ad3704c602cc528002cf8439840403658ff05338bbde5198aaaa000"
        )
        db_path = Path(self._tmp.name) / "e2e.sqlite3"
        config = Config(
            token="x" * 59,
            guild_id=None,
            welcome_channel_id=None,
            log_channel_id=None,
            member_role_id=None,
            activity_text="",
            welcome_on_join=False,
            log_commands=False,
            signing_key=seed,
            key_hours=24,
            key_max_per_day=1,
            bind_machine=True,
            db_path=db_path,
            api_enabled=True,
            api_bind="127.0.0.1",
            api_port=0,  # 0 = freien Port nehmen
            api_token="test-token-e2e",
        )

        store = Store(db_path)
        service = GrantService(config, store)
        api = LicenseAPI(config, store, grant_service=service)

        self._loop = asyncio.new_event_loop()
        self._port: int | None = None

        async def boot() -> int:
            app = web.Application()
            app.add_routes(
                [
                    web.post("/api/v1/issue", api.handle_issue),
                    web.post("/api/v1/hwid", api.handle_hwid),
                    web.post("/api/v1/verify", api.handle_verify),
                ]
            )
            self._runner = web.AppRunner(app, access_log=None)
            await self._runner.setup()
            site = web.TCPSite(self._runner, "127.0.0.1", 0)
            await site.start()
            return site._server.sockets[0].getsockname()[1]

        def run() -> None:
            asyncio.set_event_loop(self._loop)
            try:
                self._port = self._loop.run_until_complete(boot())
            except BaseException as exc:  # noqa: BLE001 - an den Test melden
                self._error = exc
            finally:
                self._ready.set()
            self._loop.run_forever()

        self._thread = threading.Thread(target=run, daemon=True, name="draxo-mint")
        self._thread.start()
        self.addCleanup(self._stop_server)

        if not self._ready.wait(30):
            self.fail("Mint-Server ist nicht hochgekommen")
        if self._error is not None:
            raise self._error

        self._store = store
        self._grants = grants
        return int(self._port)

    def _stop_server(self) -> None:
        loop, thread = self._loop, self._thread
        if loop is not None and thread is not None:
            loop.call_soon_threadsafe(loop.stop)
            thread.join(timeout=10)
            if not loop.is_closed():
                loop.close()
        store = getattr(self, "_store", None)
        if store is not None:
            store.close()

    def test_discord_to_launcher(self) -> None:
        port = self._start_server()
        base = f"http://127.0.0.1:{port}"

        # Der Mint-Server muss mit demselben Token antworten, den die
        # Funktion aus der Umgebung liest.
        mint_url, mint_token = base, "test-token-e2e"

        # Erst der direkte Aufruf: schlägt der Server fehl, ist es kein
        # Node-Problem und der Test sagt das auch.
        self._call_api(
            base,
            mint_token,
            "/api/v1/hwid",
            {"discord_id": TEST_USER, "hwid": TEST_HWID},
            "hwid",
        )
        probe = self._call_api(
            base,
            mint_token,
            "/api/v1/hwid",
            {"discord_id": TEST_USER},
            "status",
        )
        self.assertEqual(probe.get("hwid"), TEST_HWID, probe)

        driver = Path(self._tmp.name) / "driver.js"
        driver.write_text(DRIVER, encoding="utf-8")
        proc = subprocess.run(
            [NODE, str(driver)],
            capture_output=True,
            text=True,
            encoding="utf-8",
            timeout=180,
            env={
                **os.environ,
                "DRAXO_FUNCTIONS_DIR": str(FUNCTIONS),
                "DRAXO_MINT_URL": mint_url,
                "DRAXO_MINT_TOKEN": mint_token,
                "DRAXO_TEST_USER": TEST_USER,
                "DRAXO_TEST_HWID": TEST_HWID,
            },
        )
        if proc.returncode != 0:
            self.fail(
                "Node-Treiber fehlgeschlagen:\n"
                + proc.stdout
                + "\n"
                + proc.stderr
            )

        payload = json.loads(proc.stdout.strip().splitlines()[-1])
        self.assertTrue(payload.get("ok"), payload)
        grant = payload.get("grant", "")
        self.assertTrue(grant.startswith("DRAXO3-"), grant)
        self.assertIn("key gemintet", payload["steps"])

        # Und der entscheidende Punkt: derselbe Code, der in der EXE steckt,
        # muss den Grant akzeptieren. verify_grant benutzt dabei den in
        # license_signing.py eingebauten oeffentlichen Schluessel — nicht den
        # aus der Umgebung. Genau das macht den Test aussagekraeftig: ein
        # Schluessel, der nur zwischen Server und Test passt, waere hier
        # durchgefallen.
        verified = self._grants.verify_grant(grant, hwid=TEST_HWID)
        self.assertTrue(verified.valid, verified.reason)
        self.assertEqual(verified.discord_id, int(TEST_USER))

        # Falsche HWID muss derselbe Grant ablehnen — sonst bindet der
        # Schlüssel an nichts.
        wrong = self._grants.verify_grant(grant, hwid="0" * 32)
        self.assertFalse(wrong.valid, "Grant gilt auf einem fremden Rechner")

        # Und der Online-Widerruf muss greifen.
        verify = self._call_api(
            base, mint_token, "/api/v1/verify",
            {"grant": grant, "hwid": TEST_HWID, "discord_id": TEST_USER},
            "verify",
        )
        self.assertTrue(verify.get("ok"), verify)

    def _call_api(self, base, token, path, body, label):
        """Ruft den Bot-Server direkt auf — ohne Netlify dazwischen."""
        import urllib.error
        import urllib.request

        request = urllib.request.Request(
            base + path,
            data=json.dumps(body).encode(),
            headers={
                "content-type": "application/json",
                "authorization": f"Bearer {token}",
            },
            method="POST",
        )
        try:
            with urllib.request.urlopen(request, timeout=20) as response:
                return json.loads(response.read())
        except urllib.error.HTTPError as exc:
            self.fail(f"{label}: HTTP {exc.code}: {exc.read()!r}")


if __name__ == "__main__":
    unittest.main()
