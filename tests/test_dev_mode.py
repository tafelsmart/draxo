"""
test_dev_mode.py
------------------
Prüft die Dev-Variante (``DRAXO_DEV_MODE``), die
``bypassstartinjector.bat`` einschaltet.

Der Modus betrifft zwei getrennte Stellen, die beide auf dieselbe
Umgebungsvariable schauen:

  * ``discord_auth.DiscordSession`` — Anmeldeschritt im Bootstrap und
    das Injektions-Gate im UI
  * ``license_manager.get_license_status`` — die Lizenzanzeige

Beide werden hier einzeln geprüft. Entscheidend ist vor allem der
Negativfall: ohne die Variable darf sich am Verhalten nichts ändern,
sonst ist der Schalter still undogmatisch und man merkt eine
Regression erst am Kundenrechner.

Aufruf:

    python -m unittest discover -s tests -t .
"""

from __future__ import annotations

import os
import sys
import unittest
from pathlib import Path
from unittest import mock

_ROOT = Path(__file__).resolve().parent.parent
_LAUNCHER_DIR = _ROOT / "launcher"
sys.path.insert(0, str(_ROOT))
sys.path.insert(0, str(_LAUNCHER_DIR))

import discord_auth as da  # noqa: E402
import license_manager as lm  # noqa: E402

ENV = "DRAXO_DEV_MODE"


class _EnvGuard:
    """Setzt DRAXO_DEV_MODE fuer die Dauer eines Tests und raeumt auf.

    Wichtig: kein ``os.environ``-Backup im Modulbereich — sonst lebt der
    Zustand in anderen Tests weiter und die Tests werden von ihrer
    Ausfuehrungsreihenfolge abhaengig.
    """

    def __init__(self, value):
        self.value = value
        self._missing = object()

    def __enter__(self):
        self._previous = os.environ.get(ENV, self._missing)
        if self.value is None:
            os.environ.pop(ENV, None)
        else:
            os.environ[ENV] = self.value
        return self

    def __exit__(self, *_exc):
        if self._previous is self._missing:
            os.environ.pop(ENV, None)
        else:
            os.environ[ENV] = self._previous
        return False


class TestDevFlag(unittest.TestCase):
    """Die Abfrage selbst: welche Werte zaehlen als 'an'?"""

    def test_aus_ist_aus(self):
        with _EnvGuard(None):
            self.assertFalse(da.dev_mode_enabled())

    def test_leerer_wert_ist_aus(self):
        for wert in ("", "   "):
            with self.subTest(wert=wert), _EnvGuard(wert):
                self.assertFalse(da.dev_mode_enabled())

    def test_falsch_ist_aus(self):
        for wert in ("0", "false", "no", "off", "FALSE", "Off"):
            with self.subTest(wert=wert), _EnvGuard(wert):
                self.assertFalse(da.dev_mode_enabled())

    def test_wahr_ist_an(self):
        for wert in ("1", "true", "yes", "on", "TRUE"):
            with self.subTest(wert=wert), _EnvGuard(wert):
                self.assertTrue(da.dev_mode_enabled())

    def test_wird_bei_jedem_abruf_gelesen(self):
        """Ein Test darf die Variable auch mitten im Lauf setzen.

        Deshalb liest die Abfrage bei jedem Aufruf und nicht einmal
        beim Modulimport.
        """
        with _EnvGuard(None):
            self.assertFalse(da.dev_mode_enabled())
        with _EnvGuard("1"):
            self.assertTrue(da.dev_mode_enabled())


class TestDevSession(unittest.TestCase):
    """discord_auth.DiscordSession in der Dev-Variante."""

    def _session(self):
        # TokenStore/DiscordAuth legen nur Pfade an und greifen nicht auf
        # das Netz zu; fuer diese Aussage reicht die echte Klasse.
        return da.DiscordSession()

    def test_anmeldung_gilt_ohne_datenbank(self):
        with _EnvGuard("1"):
            session = self._session()
            self.assertTrue(session.signed_in)

    def test_restore_liefert_true_ohne_tokens(self):
        """restore() ist der Punkt, an dem der Bootstrap die Anmeldung prueft.

        Liefert es hier True, faellt das Anmeldefenster nie auf.
        """
        with _EnvGuard("1"):
            session = self._session()
            self.assertTrue(session.restore())
            self.assertTrue(session.signed_in)

    def test_abmelden_beendet_den_modus_nicht(self):
        """Sonst sperrt logout() das Gate wieder ohne sichtbaren Grund."""
        with _EnvGuard("1"):
            session = self._session()
            session.logout()
            self.assertTrue(session.signed_in)

    def test_anzeigename_ist_als_dev_konto_erkennbar(self):
        """In Screenshots und Logs soll erkennbar sein, dass Discord fehlt."""
        with _EnvGuard("1"):
            session = self._session()
            self.assertNotEqual(session.display_name, "Nicht angemeldet")
            self.assertIn("Dev", session.display_name)

    def test_ohne_variable_ist_alles_unveraendert(self):
        """Der Negativfall: ohne Schalter darf sich nichts verschieben."""
        with _EnvGuard(None):
            session = self._session()
            self.assertFalse(session.signed_in)
            self.assertIsNone(session.user)
            self.assertEqual(session.display_name, "Nicht angemeldet")


class TestDevLicense(unittest.TestCase):
    """license_manager.get_license_status in der Dev-Variante."""

    def test_status_ist_active(self):
        with _EnvGuard("1"):
            status = lm.get_license_status()
            self.assertEqual(status.status, "ACTIVE")

    def test_gueltige_farbe(self):
        with _EnvGuard("1"):
            self.assertEqual(lm.get_license_status().status_color, "#2ed573")

    def test_hwid_wird_trotzdem_berechnet(self):
        """Der HWID bleibt auch offline sichtbar — er wird fuer Keys gebraucht."""
        with _EnvGuard("1"):
            status = lm.get_license_status()
            self.assertTrue(status.hwid)

    def test_ohne_variable_bleibt_es_gesperrt(self):
        """Ohne Schalter und ohne Key darf sich der Status nicht entsperren."""
        with _EnvGuard(None):
            with mock.patch.object(
                lm, "_read_config_key", return_value=""
            ):
                self.assertEqual(lm.get_license_status().status, "LOCKED")


class TestDevModeKeepsNetz(unittest.TestCase):
    """Die Dev-Variante ist KEIN Offline-Modus.

    Das ist die Verwechslung, die beim Namen entstanden ist: die
    Dev-Variante ueberspringt die Discord-Anmeldung, nicht das Netz.
    Update-Check und Versionsliste muessen in diesem Modus also
    weiterhin wirklich Netzzugriff versuchen. Ein Test, der nur prueft
    "ohne Discord geht es", wuerde einen stillen Offline-Schalter
    durchwinken.
    """

    def test_update_check_versucht_es_weiterhin(self):
        """Der Update-Check darf nicht durch den Dev-Modus stillgelegt werden."""
        from unittest import mock

        from updater import Updater

        with _EnvGuard("1"):
            with mock.patch(
                "updater.urllib.request.urlopen"
            ) as urlopen:
                urlopen.return_value.__enter__.return_value.read.return_value = (
                    b'{"version": "1.2.0"}'
                )
                Updater().check_for_update()
                self.assertTrue(
                    urlopen.called,
                    "Der Update-Check hat keinen Netzzugriff versucht — "
                    "die Dev-Variante hat Netzverhalten abgeschaltet.",
                )

    def test_lizenzserver_wird_nicht_konsultiert(self):
        """verify_online bleibt unberuehrt: kein Key, keine Serverfrage."""
        from unittest import mock

        import license_manager as lm_mod

        with _EnvGuard("1"):
            with mock.patch.object(lm_mod, "_server_url", return_value=""):
                ok, hinweis = lm_mod.verify_online("DRAXO3-test")
                self.assertFalse(ok)
                self.assertIn("DRAXO_LICENSE_API", hinweis)


class TestBypassBat(unittest.TestCase):
    """Die .bat muss genau die Variable setzen, die der Code liest."""

    BAT = Path(__file__).resolve().parent.parent / "bypassstartinjector.bat"

    def test_datei_existiert(self):
        self.assertTrue(self.BAT.exists(), "bypassstartinjector.bat fehlt")

    def test_setzt_die_richtige_variable(self):
        text = self.BAT.read_text(encoding="utf-8", errors="ignore")
        self.assertIn(f'set "{ENV}=1"', text)

    def test_nennt_sich_nicht_offline(self):
        """Der Dateiname sagt 'bypass', die Datei darf sich nicht 'offline' nennen."""
        text = self.BAT.read_text(encoding="utf-8", errors="ignore").lower()
        self.assertNotIn("offline-modus", text.replace("kein offline-modus", ""))

    def test_startet_den_launcher(self):
        text = self.BAT.read_text(encoding="utf-8", errors="ignore")
        self.assertIn("draxo_launcher.py", text)

    def test_zeilenenden_sind_crlf(self):
        """cmd.exe bricht Klammerbloecke ab, wenn die Datei LF hat."""
        raw = self.BAT.read_bytes()
        self.assertNotIn(b"\n", raw.replace(b"\r\n", b""))

    def test_keine_verschachtelten_klammerbloecke(self):
        """Darin werden %VARIABLE% zur Parse-Zeit ausgewertet.

        Nur ausfuehrbare Zeilen zaehlen: Kommentare duerfen Klammern
        enthalten, sonst prueft der Test sich selbst statt der Datei.
        Gesucht ist die Blockform ``if (...) (``, nicht ein einzelnes
        ``%~dp0`` oder ``2>&1``.
        """
        import re

        text = self.BAT.read_text(encoding="utf-8", errors="ignore")
        for nummer, zeile in enumerate(text.splitlines(), 1):
            if zeile.lstrip().lower().startswith("rem"):
                continue
            self.assertIsNone(
                re.search(r"\(\s*\(", zeile),
                f"Zeile {nummer} hat einen Klammerblock: {zeile}",
            )


if __name__ == "__main__":
    unittest.main()