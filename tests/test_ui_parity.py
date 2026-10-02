"""Hält die beiden Oberflächen zusammen.

`bot/draxo_bot/ui.py` und `website/netlify/functions/_embeds.js`
zeigen dasselbe Produkt in zwei Laufzeiten. Das ist eine bewusste
Doppelung: ein gemeinsames Modul müsste der Python-Bot aus Node heraus
aufrufen, bei jedem Knopfdruck.

Der Preis ist Drift — und Drift ist hier nicht kosmetisch. Ein Nutzer, der
dem Gateway-Bot `/status` schickt, bekommt grün; derselbe Nutzer, eine
Stunde später, bekommt rot, weil jemand eine Farbe in einer der beiden
Dateien geändert hat. Nichts würde das melden.

Dieser Test vergleicht deshalb die Kernwerte beider Seiten. Er prüft nicht,
ob die Texte schön sind, sondern ob sie gleich sind.

Lauf:  python -m unittest tests.test_ui_parity -v
"""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LAUNCHER_DIR = ROOT / "launcher"
FUNCTIONS = ROOT / "website" / "netlify" / "functions"
sys.path.insert(0, str(ROOT / "bot"))

NODE = shutil.which("node")

# Liest beide Seiten und gibt vergleichbare Werte zurück. Die Python-Seite
# wird in Python ausgewertet (discord.Embed existiert nur dort), die
# JS-Seite in Node — ein Vergleich in einer Sprache wäre nur für eine Seite
# möglich.
PROBE = r"""
const path = require("path");
const embeds = require(path.join(process.env.DRAXO_FUNCTIONS_DIR, "_embeds.js"));
const hwid = "3F2A9C4D8B1E7A05C6D9F2B3A4C5D6E7";
const grant = "DRAXO3-ABCDE-FGHIJ-KLMNO-PQRST-UVWXY-Z2345";
const expiry = 1750000000;

function shape(embed) {
  return {
    title: embed.title ?? null,
    color: embed.color ?? null,
    description: embed.description ?? null,
    fields: (embed.fields ?? []).map((f) => ({ name: f.name, inline: f.inline })),
    footer: embed.footer?.text ?? null,
  };
}

const out = {
  colors: embeds.COLORS,
  notReady: shape(embeds.notReady()),
  error: shape(embeds.error("Titel", "Hinweis")),
  info: shape(embeds.info("T", "D")),
  keyIssued: shape(embeds.keyIssued(grant, expiry, hwid)),
  keyIssuedPermanent: shape(embeds.keyIssued(grant, 0, hwid)),
  rateLimited: shape(embeds.rateLimited(expiry)),
  noHwid: shape(embeds.noHwid()),
  help: shape(embeds.help()),
  statusWithKey: shape(embeds.status(hwid, {
    exists: true, preview: "abcdef012345", expiresAt: expiry, expired: false,
  })),
  statusWithout: shape(embeds.status("", null)),
  masked: embeds.maskHwid(hwid),
  modalTitle: embeds.keyModal("x").title,
  modalInput: (() => {
    const input = embeds.keyModal("x").components[0].components[0];
    return { type: input.type, min: input.min_length, max: input.max_length, label: input.label };
  })(),
  viewButtons: (() => {
    const row = embeds.welcomeView("x")[0];
    return row.components.map((c) => ({
      type: c.type, style: c.style, label: c.label,
      url: c.url ?? null, hasCustomId: Boolean(c.custom_id),
    }));
  })(),
};

console.log(JSON.stringify(out));
"""


def _js_values() -> dict:
    if NODE is None:
        raise unittest.SkipTest("node nicht im PATH")
    driver = ROOT / "_ui_parity_probe.js"
    driver.write_text(PROBE, encoding="utf-8")
    try:
        proc = subprocess.run(
            [NODE, str(driver)],
            capture_output=True,
            text=True,
            encoding="utf-8",
            timeout=60,
            # Die Umgebung wird bewusst nicht auf eine Minimal-Umgebung
            # reduziert: Node braucht fuer seinen CSPRNG Systemvariablen,
            # und ein leeres PATH laesst es mit einer ncu::CSPRNG-Assertion
            # abbrechen, statt den Test laufen zu lassen.
            env={**os.environ, "DRAXO_FUNCTIONS_DIR": str(FUNCTIONS)},
        )
    finally:
        driver.unlink(missing_ok=True)
    if proc.returncode != 0:
        raise AssertionError(f"Node-Sonde fehlgeschlagen:\n{proc.stdout}\n{proc.stderr}")
    return json.loads(proc.stdout)


def _shape(embed) -> dict:
    # discord.Colour ist ein int-Wrapper. Ohne int() vergleicht der Test
    # <Colour value=9133302> mit 9133302 und meldet einen Unterschied, den
    # es nicht gibt — bei jedem einzelnen Embed.
    color = embed.color
    return {
        "title": embed.title,
        "color": int(color) if color is not None else None,
        "description": embed.description,
        "fields": [
            {"name": f.name, "inline": f.inline} for f in getattr(embed, "fields", [])
        ],
        "footer": embed.footer.text if embed.footer else None,
    }


def _py_values() -> dict:
    from draxo_bot import ui

    hwid = "3F2A9C4D8B1E7A05C6D9F2B3A4C5D6E7"
    grant = "DRAXO3-ABCDE-FGHIJ-KLMNO-PQRST-UVWXY-Z2345"
    expiry = 1750000000

    return {
        "colors": {
            "primary": ui.COLOR_PRIMARY,
            "success": ui.COLOR_SUCCESS,
            "warn": ui.COLOR_WARN,
            "error": ui.COLOR_ERROR,
            "muted": ui.COLOR_MUTED,
        },
        "notReady": _shape(ui.not_ready()),
        "error": _shape(ui.error("Titel", "Hinweis")),
        "info": _shape(ui.info("T", "D")),
        "keyIssued": _shape(ui.key_issued(grant, expiry, hwid)),
        "keyIssuedPermanent": _shape(ui.key_issued(grant, 0, hwid)),
        "rateLimited": _shape(ui.rate_limited(expiry)),
        "noHwid": _shape(ui.no_hwid()),
        "help": _shape(_help_from_python()),
        "statusWithKey": _shape(_status_from_python(hwid, expiry, False)),
        "statusWithout": _shape(_status_from_python("", 0, False, exists=False)),
        "masked": ui.grants.mask_hwid(hwid),
        "modalTitle": "Draxo — Key holen",
        "modalInput": {"type": 4, "min": 32, "max": 32, "label": "Deine HWID"},
        "viewButtons": _welcome_buttons(),
    }


def _welcome_buttons() -> list[dict]:
    """Bucht die echte WelcomeView aus ui.py — nicht eine nachgebaute Liste."""
    from draxo_bot import ui

    # Der Dekorator erzeugt eine zufaellige custom_id. Die wird hier
    # normalisiert, weil sie per Definition nicht uebereinstimmen kann.
    row = ui.WelcomeView(timeout=1).to_components()[0]
    return [
        {
            "type": component["type"],
            "style": component["style"],
            "label": component["label"],
            "url": component.get("url"),
            "hasCustomId": "custom_id" in component,
        }
        for component in row["components"]
    ]


def _help_from_python():
    """Baut das /hilfe-Embed aus commands.py nach, wie der Bot es tut."""
    from draxo_bot import ui
    from draxo_bot.config import COLOR_PRIMARY

    embed = ui.info("Draxo Bot", "So kommst du zu deinem Key:", COLOR_PRIMARY)
    embed.add_field(name="`/key`", value="Key für deine hinterlegte HWID anfordern", inline=False)
    embed.add_field(
        name="`/hwid hwid:…`", value="HWID hinterlegen (nur für dich sichtbar)", inline=False
    )
    embed.add_field(name="`/hwid clear:true`", value="Hinterlegte HWID löschen", inline=False)
    embed.add_field(name="`/status`", value="HWID und Ablauf deines letzten Keys", inline=False)
    embed.add_field(name="`/hilfe`", value="Diese Übersicht", inline=False)
    embed.set_footer(text="Oder benutze den Button „Key holen“ im Welcome-Channel.")
    return embed


def _status_from_python(hwid: str, expiry: int, expired: bool, exists: bool = True):
    """Baut das /status-Embed aus commands.py nach."""
    import time

    from draxo_bot import ui
    from draxo_bot.config import COLOR_PRIMARY
    from draxo_bot.service import relative

    embed = ui.info("Dein Status", "", COLOR_PRIMARY)
    embed.add_field(
        name="HWID", value=f"`{hwid}`" if hwid else "nicht hinterlegt", inline=True
    )
    if exists:
        embed.add_field(name="Letzter Key", value="`abcdef012345…`", inline=True)
        embed.add_field(
            name="Gültig bis",
            value=relative(expiry) + (" **(abgelaufen)**" if expired else ""),
            inline=True,
        )
    else:
        embed.add_field(name="Letzter Key", value="noch keiner", inline=True)
    return embed


class UiParityTest(unittest.TestCase):
    """Python- und JavaScript-Oberfläche müssen dasselbe sagen."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.py = _py_values()
        cls.js = _js_values()

    def test_farben_sind_gleich(self) -> None:
        self.assertEqual(
            self.py["colors"],
            self.js["colors"],
            "Farben divergieren zwischen ui.py und _embeds.js",
        )

    def test_not_ready(self) -> None:
        self.assertEqual(self.py["notReady"], self.js["notReady"])

    def test_error_mit_hinweis(self) -> None:
        self.assertEqual(self.py["error"], self.js["error"])

    def test_info(self) -> None:
        self.assertEqual(self.py["info"], self.js["info"])

    def test_key_ausgestellt(self) -> None:
        self.assertEqual(self.py["keyIssued"], self.js["keyIssued"])

    def test_key_unbefristet(self) -> None:
        self.assertEqual(self.py["keyIssuedPermanent"], self.js["keyIssuedPermanent"])

    def test_rate_limited(self) -> None:
        self.assertEqual(self.py["rateLimited"], self.js["rateLimited"])

    def test_keine_hwid(self) -> None:
        self.assertEqual(self.py["noHwid"], self.js["noHwid"])

    def test_hilfe(self) -> None:
        self.assertEqual(self.py["help"], self.js["help"])

    def test_status_mit_key(self) -> None:
        self.assertEqual(self.py["statusWithKey"], self.js["statusWithKey"])

    def test_status_ohne_key(self) -> None:
        self.assertEqual(self.py["statusWithout"], self.js["statusWithout"])

    def test_hwid_maskierung(self) -> None:
        self.assertEqual(
            self.py["masked"],
            self.js["masked"],
            "maskHwid() weicht zwischen Python und JavaScript ab",
        )

    def test_modal(self) -> None:
        self.assertEqual(self.py["modalTitle"], self.js["modalTitle"])
        self.assertEqual(self.py["modalInput"], self.js["modalInput"])

    def test_welcome_view(self) -> None:
        self.assertEqual(
            self.py["viewButtons"],
            self.js["viewButtons"],
            "Die Knopfleiste im Welcome-Channel weicht ab. type muss 2 sein "
            "(Button); style 5 steht fuer einen Link-Knopf.",
        )

    def test_kein_komponententyp_5(self) -> None:
        """Komponente 5 existiert nicht — 5 ist ein Button-Style."""
        for name, buttons in (("python", self.py["viewButtons"]), ("js", self.js["viewButtons"])):
            for button in buttons:
                with self.subTest(seite=name, label=button["label"]):
                    self.assertEqual(
                        button["type"],
                        2,
                        f"{name}: Komponententyp {button['type']} ist ungueltig",
                    )


if __name__ == "__main__":
    unittest.main()
