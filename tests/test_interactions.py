"""Der Interactions-Endpoint, gegen echte Discord-Payloads.

Kein Mock von ``verifySignature`` und kein vorgefertigtes „PING antwortet
mit 1" — der Test bildet ab, was Discord tatsaechlich schickt, signiert es
mit einem echten Schluessel und prueft, ob die Funktion das erkennt.

Das ist der Punkt: die Signaturpruefung ist die einzige Sache, die diesen
Endpunkt ueberhaupt sicher macht, und sie ist genau die, die man kaputt
schreibt, indem man sie "kurz" macht. Ein Test mit gemockter
Signaturpruefung wuerde genau den Fehler durchlassen, vor dem die
Funktion steht.

Voraussetzung: Node im PATH. Ohne Node werden die Tests uebersprungen,
nicht stillschweigend bestanden — das waere die schlimmere Variante.

Lauf:  python -m unittest tests.test_interactions -v
"""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
import textwrap
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LAUNCHER_DIR = ROOT / "launcher"
FUNCTIONS = ROOT / "draxo-website-netlify" / "netlify" / "functions"

NODE = shutil.which("node")
HAS_NODE = NODE is not None


# ── Der Node-Testtreiber ─────────────────────────────────────────────────────
# Laeuft in einem Kindprozess: dort werden die Env-Variablen gesetzt und
# die Funktion wirklich aufgerufen. Im Python-Prozess selbst gaenge das
# auch, aber dann muesste die Signaturerzeugung in Python liegen — und
# genau dann prueft man nicht, ob Node sie akzeptiert.

DRIVER = r"""
const crypto = require("crypto");
const path = require("path");

const FUNCTIONS = process.env.DRAXO_FUNCTIONS_DIR;

// Ed25519-Schluesselpaar fuer den Test. Deterministisch, damit ein
// Fehlschlag reproduzierbar ist — aber ohne Bedeutung fuer echte Daten.
const seed = Buffer.from("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60", "hex");
const publicKey = crypto.createPublicKey(
  crypto.createPrivateKey({
    key: Buffer.concat([
      Buffer.from("302e020100300506032b657004220420", "hex"),
      seed,
    ]),
    format: "der",
    type: "pkcs8",
  })
).export({ format: "der", type: "spki" }).subarray(12).toString("hex");

process.env.DISCORD_PUBLIC_KEY = publicKey;

const { verifySignature } = require(path.join(FUNCTIONS, "_discord.js"));
const embeds = require(path.join(FUNCTIONS, "_embeds.js"));
const mint = require(path.join(FUNCTIONS, "_mint.js"));
const { handler, _internals } = require(path.join(FUNCTIONS, "interactions.js"));

// Die Mint-Aufrufe werden abgefangen, statt das Netz zu berühren. Wichtig:
// `mint` ist derselbe Modul-Objekt, den interactions.js haelt — CommonJS
// liefert require() eine Einmal-Instanz pro aufgeloestem Pfad. Ein zweites
// require ueber einen anderen Pfad wuerde eine Kopie liefern, und die
// Stubs hier waeren dann wirkungslos: der Test waere gruen und der Code
// ungetestet.
const realIssue = mint.issue;
const realLookup = mint.lookup;
const realSetHwid = mint.setHwid;

function resetMint() {
  mint.issue = async () => { throw new Error("mint.issue nicht gemockt"); };
  mint.lookup = async () => { throw new Error("mint.lookup nicht gemockt"); };
  mint.setHwid = async () => { throw new Error("mint.setHwid nicht gemockt"); };
}

const cases = [];
function test(name, fn) { cases.push({ name, fn }); }

// Node hat kein assert. Diese Variante sagt, welcher Wert falsch war —
// "assert is not defined" als Testfehler zu lesen ist Zeitverschwendung.
const assert = require("assert").strict;

function sign(bodyString, timestamp) {
  const key = crypto.createPrivateKey({
    key: Buffer.concat([
      Buffer.from("302e020100300506032b657004220420", "hex"),
      seed,
    ]),
    format: "der",
    type: "pkcs8",
  });
  const signature = crypto.sign(
    null,
    Buffer.concat([Buffer.from(String(timestamp), "ascii"), Buffer.from(bodyString, "utf8")]),
    key
  );
  return signature.toString("hex");
}

async function call(payload, opts = {}) {
  const body = typeof payload === "string" ? payload : JSON.stringify(payload);
  const timestamp = opts.timestamp ?? String(Math.floor(Date.now() / 1000));
  const headers = opts.noSignature
    ? { "content-type": "application/json" }
    : {
        "content-type": "application/json",
        "x-signature-ed25519": opts.badSignature
          ? sign(body + " ", timestamp)
          : sign(body, timestamp),
        "x-signature-timestamp": opts.timestamp ?? timestamp,
      };
  return await handler({ body, headers, isBase64Encoded: false });
}

const APP = "1554874371573813389";
const USER = "1534959905104986314";
const TOKEN = "interaction-token-abc";

function command(name, options = []) {
  return {
    id: "900000000000000001",
    application_id: APP,
    type: 2,
    token: TOKEN,
    version: 1,
    guild_id: "1534959905104986314",
    channel_id: "900000000000000002",
    member: {
      user: { id: USER, username: "clemens", global_name: "Clemens" },
      roles: [],
    },
    data: { id: "900000000000000003", name, type: 1, options },
  };
}

function bodyOf(res) { return JSON.parse(res.body); }

// ═══ Signaturpruefung ═══════════════════════════════════════════════════════

test("Die Function nutzt dieselbe mint-Instanz wie der Test", async () => {
  // Ohne diese Absicherung waeren alle Mint-Stubs wirkungslos und der Rest
  // der Suite pruefte nichts. Deshalb wird es explizit festgehalten.
  resetMint();
  let hit = false;
  mint.issue = async () => { hit = true; return { ok: true, grant: "X", expires_at: 1 }; };
  mint.lookup = async () => ({ ok: true, hwid: "F".repeat(32) });
  await call(command("key"));
  assert.ok(hit, "der Stub wurde nicht erreicht");
  mint.issue = realIssue;
  mint.lookup = realLookup;
  mint.setHwid = realSetHwid;
});

test("PING wird mit PONG beantwortet", async () => {
  const res = await call({ id: "1", application_id: APP, type: 1, token: TOKEN, version: 1 });
  assert.ok(res.statusCode === 200, "status " + res.statusCode);
  assert.ok(bodyOf(res).type === 1, "callback type " + bodyOf(res).type);
});

test("Ohne Signatur wird abgelehnt", async () => {
  const res = await call({ id: "1", application_id: APP, type: 1, token: TOKEN, version: 1 }, { noSignature: true });
  assert.ok(res.statusCode === 401, "status " + res.statusCode);
});

test("Manipulierter Body wird abgelehnt", async () => {
  // Signatur gueltig fuer den Original-Body, gesendet wird ein anderer.
  const original = JSON.stringify({ id: "1", application_id: APP, type: 1, token: TOKEN, version: 1 });
  const tampered = JSON.stringify({ id: "2", application_id: APP, type: 1, token: TOKEN, version: 1 });
  const timestamp = String(Math.floor(Date.now() / 1000));
  const res = await handler({
    body: tampered,
    headers: {
      "content-type": "application/json",
      "x-signature-ed25519": sign(original, timestamp),
      "x-signature-timestamp": timestamp,
    },
    isBase64Encoded: false,
  });
  assert.ok(res.statusCode === 401, "status " + res.statusCode);
});

test("Veralteter Zeitstempel wird abgelehnt", async () => {
  const res = await call(
    { id: "1", application_id: APP, type: 1, token: TOKEN, version: 1 },
    { timestamp: "1000000000" }
  );
  // Der Zeitstempel ist Teil der Signatur, also ist die Signatur gueltig —
  // aber ein zehn Jahre alter Zeitstempel ist eine Abspielattacke. Die
  // Funktion lehnt ihn ueber die Alterspruefung ab.
  assert.equal(res.statusCode, 401, "status " + res.statusCode);
});

test("Zeitstempel knapp innerhalb der Frist wird akzeptiert", async () => {
  // 60 Sekunden alt: noch gueltig. Ohne diese Grenze koennte die Funktion
  // ihr eigenes Zeitfenster zu eng ziehen und echte Befehle ablehnen.
  const ts = String(Math.floor(Date.now() / 1000) - 60);
  const res = await call(
    { id: "1", application_id: APP, type: 1, token: TOKEN, version: 1 },
    { timestamp: ts }
  );
  assert.equal(res.statusCode, 200, "status " + res.statusCode);
});

test("Zeitstempel aus der Zukunft wird abgelehnt", async () => {
  const ts = String(Math.floor(Date.now() / 1000) + 3600);
  const res = await call(
    { id: "1", application_id: APP, type: 1, token: TOKEN, version: 1 },
    { timestamp: ts }
  );
  assert.equal(res.statusCode, 401, "status " + res.statusCode);
});

test("Abgespielte Signatur ist nach 5 Minuten wertlos", async () => {
  // Der eigentliche Replay-Test: dieselbe, korrekt signierte Anfrage wird
  // zweimal geschickt — einmal frisch, einmal mit altem Zeitstempel.
  const payload = JSON.stringify({
    id: "1", application_id: APP, type: 1, token: TOKEN, version: 1,
  });
  const fresh = String(Math.floor(Date.now() / 1000));
  const old = String(Math.floor(Date.now() / 1000) - 400);

  const first = await handler({
    body: payload,
    headers: {
      "content-type": "application/json",
      "x-signature-ed25519": sign(payload, old),
      "x-signature-timestamp": old,
    },
    isBase64Encoded: false,
  });
  assert.equal(first.statusCode, 401, "alte Signatur wurde akzeptiert");
  assert.ok(typeof fresh === "string", "frischer Zeitstempel fehlt");
});

test("Fehlender Public Key fuehrt zu 500, nicht zu 401", async () => {
  const saved = process.env.DISCORD_PUBLIC_KEY;
  delete process.env.DISCORD_PUBLIC_KEY;
  const res = await call({ id: "1", application_id: APP, type: 1, token: TOKEN, version: 1 });
  process.env.DISCORD_PUBLIC_KEY = saved;
  assert.ok(res.statusCode === 500, "status " + res.statusCode);
});

test("verifySignature lehnt Unsinn ab, ohne zu werfen", () => {
  assert(verifySignature(Buffer.from("{}"), "nicht-hex", "1234567890", publicKey) === false);
  assert(verifySignature(Buffer.from("{}"), "ab".repeat(64), "1234567890", publicKey) === false);
  assert(verifySignature(Buffer.from("{}"), null, "1234567890", publicKey) === false);
  assert(verifySignature(Buffer.from("{}"), "ab".repeat(64), "abc", publicKey) === false);
  assert(verifySignature(Buffer.from("{}"), "ab".repeat(64), "1234567890", "zz".repeat(32)) === false);
});

// ═══ Befehle ═══════════════════════════════════════════════════════════════

test("/key ohne HWID sagt, was zu tun ist", async () => {
  mint.lookup = async () => ({ ok: true, hwid: null, last: null });
  const res = await call(command("key"));
  const body = bodyOf(res);
  assert.ok(body.type === 4, "callback type " + body.type);
  const embed = body.data.embeds[0];
  assert.ok(embed.title === "Geht nicht", embed.title);
  assert.ok(/kenne deine HWID noch nicht/i.test(embed.description), embed.description);
  assert.ok((body.data.flags & 64) === 64, "nicht ephemeral");
});

test("/key mit hinterlegter HWID mintet", async () => {
  const hwid = "A".repeat(32);
  let seen = null;
  mint.lookup = async () => ({ ok: true, hwid, last: null });
  mint.issue = async (id, value) => {
    seen = { id, value };
    return { ok: true, grant: "DRAXO3-TEST", expires_at: 1700000000, discord_id: id };
  };
  const res = await call(command("key"));
  const body = bodyOf(res);
  assert.ok(seen.id === USER, "user id " + seen.id);
  assert.ok(seen.value === hwid, "hwid " + seen.value);
  const embed = body.data.embeds[0];
  assert.ok(embed.title === "Dein Key", embed.title);
  assert.ok(embed.description.includes("DRAXO3-TEST"), "kein Grant im Embed");
  // Die HWID darf im Erfolgs-Embed nicht im Klartext stehen.
  const serialised = JSON.stringify(body.data);
  assert.ok(!serialised.includes(hwid), "HWID im Klartext im Antwort-Payload");
});

test("/key meldet, wenn der Mint-Server nicht antwortet", async () => {
  mint.lookup = async () => ({ ok: true, hwid: "B".repeat(32), last: null });
  mint.issue = async () => { throw Object.assign(new Error("weg"), { code: "timeout" }); };
  const res = await call(command("key"));
  const body = bodyOf(res);
  const embed = body.data.embeds[0];
  // Der Fehlerhinweis steht im Feld, nicht in der Beschreibung — genau wie
  // in ui.error(). Ein Test, der an der falschen Stelle sucht, meldet einen
  // Fehler, den es nicht gibt.
  const hint = (embed.fields ?? []).map((f) => f.value).join(" ");
  assert.ok(/nicht an dir/i.test(hint), "kein Hinweis auf einen Serverfehler: " + hint);
  assert.ok(hint.includes("timeout"), "Fehlercode fehlt: " + hint);
});

test("/key zeigt den Grund, den der Server nennt", async () => {
  mint.lookup = async () => ({ ok: true, hwid: "C".repeat(32), last: null });
  mint.issue = async () => ({ ok: false, reason: "Du hast heute schon genug Keys geholt." });
  const res = await call(command("key"));
  const embed = bodyOf(res).data.embeds[0];
  const text = JSON.stringify(embed);
  assert.ok(text.includes("heute schon genug"), text);
});

test("/hwid weist eine zu kurze HWID ab", async () => {
  const res = await call(command("hwid", [{ type: 3, name: "hwid", value: "kurz" }]));
  const embed = bodyOf(res).data.embeds[0];
  assert.ok(/32 Zeichen/.test(embed.description), embed.description);
});

test("/hwid speichert eine gueltige HWID", async () => {
  let stored = null;
  mint.setHwid = async (id, value) => { stored = { id, value }; return { ok: true, hwid: value }; };
  const res = await call(command("hwid", [{ type: 3, name: "hwid", value: "a".repeat(32) }]));
  const embed = bodyOf(res).data.embeds[0];
  assert.ok(embed.title === "Gespeichert", embed.title);
  // Kleingeschrieben eingegeben, gross gespeichert.
  assert.ok(stored.value === "A".repeat(32), stored.value);
});

test("/hwid clear loescht", async () => {
  let cleared = false;
  mint.setHwid = async () => { cleared = true; return { ok: true, hwid: null }; };
  const res = await call(command("hwid", [{ type: 5, name: "clear", value: true }]));
  assert.ok(cleared, "clear wurde nicht durchgereicht");
  assert.ok(bodyOf(res).data.embeds[0].title === "Vergessen", "falscher Titel");
});

test("/status zeigt HWID und letzten Key", async () => {
  mint.lookup = async () => ({
    ok: true,
    hwid: "D".repeat(32),
    last: { exists: true, preview: "abcdef012345", expires_at: 1700000000, expired: false },
  });
  const res = await call(command("status"));
  const embed = bodyOf(res).data.embeds[0];
  const names = embed.fields.map((f) => f.name);
  assert.ok(names.includes("HWID"), names.join(","));
  assert.ok(names.includes("Letzter Key"), names.join(","));
});

test("/status ohne Key sagt es", async () => {
  mint.lookup = async () => ({ ok: true, hwid: null, last: null });
  const res = await call(command("status"));
  const embed = bodyOf(res).data.embeds[0];
  const last = embed.fields.find((f) => f.name === "Letzter Key");
  assert.ok(last.value === "noch keiner", last.value);
});

test("/hilfe listet die Befehle", async () => {
  const res = await call(command("hilfe"));
  const embed = bodyOf(res).data.embeds[0];
  const text = JSON.stringify(embed.fields);
  for (const name of ["/key", "/hwid", "/status"]) {
    assert.ok(text.includes(name), name + " fehlt in der Hilfe");
  }
});

test("Unbekannter Befehl antwortet statt zu crashen", async () => {
  const res = await call(command("wat"));
  const embed = bodyOf(res).data.embeds[0];
  assert.ok(/gibt es nicht/.test(embed.description), embed.description);
});

// ═══ Komponenten und Modal ══════════════════════════════════════════════════

test("Der Key-Knopf oeffnet ein Modal", async () => {
  const res = await call({
    id: "2", application_id: APP, type: 3, token: TOKEN, version: 1,
    member: { user: { id: USER, username: "clemens" }, roles: [] },
    data: { component_type: 2, custom_id: _internals.ASK_KEY },
  });
  const body = bodyOf(res);
  assert.ok(body.type === 9, "callback type " + body.type + " (9 = MODAL)");
  assert.ok(body.data.title === "Draxo — Key holen", body.data.title);
  const input = body.data.components[0].components[0];
  assert.ok(input.type === 4, "kein TextInput: " + input.type);
  assert.ok(input.min_length === 32 && input.max_length === 32, "Laengenbegrenzung");
});

test("Modal-Submit stellt den Key aus", async () => {
  const hwid = "E".repeat(32);
  let issued = null;
  mint.issue = async (id, value) => {
    issued = { id, value };
    return { ok: true, grant: "DRAXO3-MODAL", expires_at: 1700000000, discord_id: id };
  };
  const res = await call({
    id: "3", application_id: APP, type: 5, token: TOKEN, version: 1,
    member: { user: { id: USER, username: "clemens" }, roles: [] },
    data: {
      custom_id: "draxo:key_form",
      components: [{ type: 1, components: [{ type: 4, custom_id: "hwid", value: hwid }] }],
    },
  });
  assert.ok(issued.value === hwid, "hwid " + issued.value);
  const embed = bodyOf(res).data.embeds[0];
  assert.ok(embed.title === "Dein Key", embed.title);
  assert.ok(embed.description.includes("DRAXO3-MODAL"), "kein Grant");
});

test("Modal-Submit mit kaputter HWID mintet nicht", async () => {
  let called = false;
  mint.issue = async () => { called = true; return { ok: true, grant: "x" }; };
  const res = await call({
    id: "4", application_id: APP, type: 5, token: TOKEN, version: 1,
    member: { user: { id: USER, username: "clemens" }, roles: [] },
    data: {
      custom_id: "draxo:key_form",
      components: [{ type: 1, components: [{ type: 4, custom_id: "hwid", value: "keine-hwid" }] }],
    },
  });
  assert.ok(!called, "es wurde trotzdem gemintet");
  assert.ok(/32 Zeichen/.test(bodyOf(res).data.embeds[0].description), "keine Fehlermeldung");
});

test("Autocomplete antwortet mit einer leeren Liste", async () => {
  const res = await call({
    id: "5", application_id: APP, type: 4, token: TOKEN, version: 1,
    member: { user: { id: USER, username: "clemens" }, roles: [] },
    data: { name: "key", type: 1, options: [] },
  });
  const body = bodyOf(res);
  assert.ok(body.type === 8, "callback type " + body.type + " (8 = AUTOCOMPLETE)");
  assert.ok(Array.isArray(body.data.choices), "keine choices");
});

test("Unbekannter Interaction-Tip wird abgelehnt", async () => {
  const res = await call({ id: "6", application_id: APP, type: 99, token: TOKEN, version: 1 });
  assert.ok(res.statusCode === 400, "status " + res.statusCode);
});

// ═══ Der Kern: kein Schluesselmaterial in der Funktion ══════════════════════

test("Die Funktion enthaelt kein privates Schluesselmaterial", () => {
  const fs = require("fs");
  for (const name of fs.readdirSync(FUNCTIONS)) {
    if (!name.endsWith(".js")) continue;
    const source = fs.readFileSync(path.join(FUNCTIONS, name), "utf8");
    // DRAXO_SIGNING_KEY darf nirgends auftauchen. Der Hinweistext in
    // _embeds.js erwaehnt die Variable nur im Klartext, ohne Wert — das
    // wird hier zugelassen, denn ein Environment-Name ist kein Geheimnis.
    const assignment = source.match(/DRAXO_SIGNING_KEY\s*[:=]\s*["'][0-9a-fA-F]{16,}/);
    assert.ok(!assignment, name + " weist DRAXO_SIGNING_KEY einen Wert zu");
    // Eine 64-stellige Hex-Zeichenkette waere ein eingebackener Seed.
    const literal = source.match(/["'][0-9a-fA-F]{64}["']/g);
    assert.ok(!literal, name + " enthaelt eine 64-stellige Hex-Konstante: " + literal);
  }
});

test("Die Function liest den Mint-Server nur aus der Umgebung", () => {
  const fs = require("fs");
  const source = fs.readFileSync(path.join(FUNCTIONS, "interactions.js"), "utf8");
  // Kommentare sind ausgenommen: dort steht bewusst ein Beispiel, und ein
  // Beispiel ist keine Route. Entscheidend ist der ausfuehrbare Code.
  const code = source
    .split("\n")
    .filter((line) => !line.trim().startsWith("*") && !line.trim().startsWith("//"))
    .join("\n");
  const urls = code.match(/https?:\/\/[^"'\s)]+/g) ?? [];
  for (const url of urls) {
    assert.ok(
      url.startsWith("https://draxo.netlify.app"),
      "interactions.js enthaelt eine hart kodierte Adresse: " + url
    );
  }
  assert.ok(
    /DRAXO_MINT_URL/.test(source),
    "interactions.js greift nicht auf DRAXO_MINT_URL zu"
  );
});

test("maskHwid verbirgt die Mitte", () => {
  // Kurze Eingaben werden vollstaendig verdeckt — so macht es Python in
  // grants.mask_hwid() ebenfalls, sonst divergieren die Seiten.
  assert.equal(embeds.maskHwid("ABCDEFGH"), "********");
  assert.equal(embeds.maskHwid(""), "");
  const masked = embeds.maskHwid("1234567890abcdef1234567890abcdef");
  assert.equal(masked, "1234…cdef");
  assert.ok(!masked.includes("5678"), "Mitte sichtbar");
});

// ── Lauf ────────────────────────────────────────────────────────────────────

(async () => {
  let failed = 0;
  for (const c of cases) {
    try {
      await c.fn();
      console.log("  ok   " + c.name);
    } catch (error) {
      failed += 1;
      console.log("  FAIL " + c.name);
      console.log("       " + (error && error.message));
    }
  }
  console.log(failed === 0 ? "\nNODE: " + cases.length + "/" + cases.length : "\nNODE: " + failed + " FEHLER");
  process.exit(failed === 0 ? 0 : 1);
})();
"""


class InteractionsTest(unittest.TestCase):
    """Führt den Node-Treiber aus und übersetzt sein Ergebnis."""

    def test_function_behaviour(self) -> None:
        if not HAS_NODE:
            self.skipTest("node nicht im PATH — Endpunkt-Tests übersprungen")

        driver = ROOT / "_interactions_driver.js"
        driver.write_text(DRIVER, encoding="utf-8")
        try:
            proc = subprocess.run(
                [
                    NODE,
                    str(driver),
                ],
                capture_output=True,
                text=True,
                encoding="utf-8",
                env={
                    **os.environ,
                    "DRAXO_FUNCTIONS_DIR": str(FUNCTIONS),
                    "PYTHONIOENCODING": "utf-8",
                },
                timeout=120,
            )
        finally:
            driver.unlink(missing_ok=True)

        sys.stdout.write(textwrap.indent(proc.stdout, "    "))
        if proc.stderr.strip():
            sys.stdout.write("    stderr:\n")
            sys.stdout.write(textwrap.indent(proc.stderr, "      "))
        self.assertEqual(
            proc.returncode,
            0,
            f"Node-Treiber meldet Fehler:\n{proc.stdout}\n{proc.stderr}",
        )

    def test_required_files_exist(self) -> None:
        for name in ("interactions.js", "_discord.js", "_embeds.js", "_mint.js"):
            with self.subTest(datei=name):
                self.assertTrue((FUNCTIONS / name).is_file(), f"{name} fehlt")

    def test_no_node_modules_dependency(self) -> None:
        """Die Funktion darf keine npm-Pakete brauchen.

        Netlify Functions werden ohne ``node_modules`` gebaut. Ein
        ``require("discord.js")`` fällt dort nicht beim Testen auf, sondern
        erst beim Deploy — als leere Fehlermeldung im Log.
        """
        import re

        builtin = re.compile(
            r"^(node:)?(assert|buffer|crypto|events|fs|http|https|os|path|"
            r"querystring|stream|string_decoder|timers|tls|url|util|zlib)$"
        )
        for path in FUNCTIONS.glob("*.js"):
            source = path.read_text(encoding="utf-8")
            for match in re.finditer(r"""require\(\s*["']([^"']+)["']""", source):
                module = match.group(1)
                with self.subTest(datei=path.name, modul=module):
                    self.assertTrue(
                        module.startswith(".") or builtin.match(module),
                        f"{path.name} requiret {module!r} — das ist kein Node-"
                        f"Builtin, und Netlify installiert nichts nach",
                    )


if __name__ == "__main__":
    unittest.main()
