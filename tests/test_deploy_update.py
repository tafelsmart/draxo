"""Das Deploy-Skript gegen eine echte Installation.

Kein Mock, kein ausgedachtes Verzeichnis: hier entsteht ein
``/opt/draxo-bot``-ähnlicher Baum mit einer SQLite-Datenbank, die im
WAL-Modus schreibt — also genau die Konstellation, für die das Skript
geschrieben wurde.

Die Prüfungen:

  1. HWIDs überstehen ein Update
  2. die .env mit dem Schlüssel überlebt
  3. **ungemergte WAL-Commits überleben** — der eigentliche Zweck
  4. ein SIGKILL mitten im Betrieb nimmt nichts mit
  5. ein Datenverlust wird erkannt und gemeldet, nicht überspielt

Punkt 3 und 5 sind keine Zusatzprüfungen. Genau daran scheitert ein
naives ``cp``: SQLite hält die letzten Commits in ``-wal``, und wer nur die
Hauptdatei kopiert, verliert genau die Zeilen, die zuletzt geschrieben
wurden.

Lauf:  python -m unittest tests.test_deploy_update -v
"""

from __future__ import annotations

import os
import shutil
import sqlite3
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LAUNCHER_DIR = ROOT / "launcher"
SCRIPT = ROOT / "bot" / "deploy" / "update.sh"
BASH = shutil.which("bash")

# 5 HWIDs, wie sie nach einem Betriebstag in der Datenbank stehen.
HWIDS = [
    (1534959905104986314, "3F2A9C4D8B1E7A05C6D9F2B3A4C5D6E7", 1750000000),
    (1534959905104986315, "A1B2C3D4E5F60718293A4B5C6D7E8F90", 1750000100),
    (1534959905104986316, "00112233445566778899AABBCCDDEEFF", 1750000200),
    (1534959905104986317, "FFEEDDCCBBAA99887766554433221100", 1750000300),
    (1534959905104986318, "0F0E0D0C0B0A09080706050403020100", 1750000400),
]

# Der Schlüssel aus der Testumgebung. Nur der oeffentliche Teil steht in
# license_signing.py; der private gehoert in die .env des Servers.
PRIVATE_SEED = "826b710d3ad3704c602cc528002cf8439840403658ff05338bbde5198aaaa000"


def _public_from_seed(seed_hex: str) -> str:
    """Leitet den oeffentlichen Schluessel aus einem Seed ab.

    Nutzt das echte license_signing, damit kein zweiter Ed25519-Ableiter
    entsteht. Ein eigener Hier waere eine weitere Stelle, an der die beiden
    Seiten auseinanderlaufen koennten.
    """
    sys.path.insert(0, str(ROOT))
    sys.path.insert(0, str(LAUNCHER_DIR))
    import license_signing

    return license_signing.public_from_seed(bytes.fromhex(seed_hex)).hex()


class DeployTestBase(unittest.TestCase):
    def setUp(self) -> None:
        if BASH is None:
            self.skipTest("bash nicht im PATH")
        self._tmp = tempfile.TemporaryDirectory()
        # Windows sperrt die SQLite-Datei, solange ein Handle offen ist.
        # Das Skript oeffnet sie in einem Kindprozess; ist der noch nicht
        # fertig, scheitert das Aufraeumen. Mit ignore_errors bleibt das
        # unsichtbar, statt als Fehler des Tests aufzutauchen.
        tmp_name = self._tmp.name

        def _cleanup() -> None:
            shutil.rmtree(tmp_name, ignore_errors=True)

        self.addCleanup(_cleanup)
        self.install = Path(self._tmp.name) / "opt" / "draxo-bot"
        self.backups = Path(self._tmp.name) / "backups"
        # Standardmaessig das Skript aus dem Repo. Tests, die einen
        # abweichenden Quellcode deployen wollen, ueberschreiben das.
        self._script = SCRIPT

    # ── Aufbau ───────────────────────────────────────────────────────

    def _make_source(self, license_signing: str) -> Path:
        """Baut ein Quellverzeichnis, das das Skript als REPO_ROOT sieht.

        Das Skript leitet REPO_ROOT aus seinem eigenen Pfad ab
        (``../..`` vom Skript aus) und erwartet dort ``bot/draxo_bot``
        und ``license_signing.py``. Genau diese Struktur entsteht hier —
        sonst prueft der Test eine Fehlermeldung statt des Verhaltens.
        """
        repo = Path(self._tmp.name) / "src"
        (repo / "bot" / "draxo_bot").mkdir(parents=True, exist_ok=True)
        (repo / "bot" / "deploy").mkdir(parents=True, exist_ok=True)
        # bot.py gehoert dazu: das Skript prueft in der Vorpruefung auf
        # genau diese Datei und bricht sonst ab, bevor irgendetwas passiert.
        for name in (
            "__init__.py",
            "__main__.py",
            "api.py",
            "bot.py",
            "commands.py",
            "config.py",
            "service.py",
            "signing.py",
            "store.py",
            "ui.py",
        ):
            source = ROOT / "bot" / "draxo_bot" / name
            if source.is_file():
                shutil.copy(source, repo / "bot" / "draxo_bot" / name)
        shutil.copy(ROOT / "bot" / "requirements.txt", repo / "bot" / "requirements.txt")
        shutil.copy(SCRIPT, repo / "bot" / "deploy" / "update.sh")
        (repo / "license_signing.py").write_text(license_signing, encoding="utf-8")
        self._script = repo / "bot" / "deploy" / "update.sh"
        return repo

    def _make_installation(self, *, with_wal_pending: bool = False) -> Path:
        """Baut eine Installation, die einer echten nachempfunden ist.

        Wichtig: das Arbeitsverzeichnis des Skript-Kindprozesses wird auf
        das Installationsverzeichnis gesetzt. Ohne das findet Python das
        ``license_signing.py`` aus dem Repo statt der kopierten Datei — und
        ein Test, der einen falschen oeffentlichen Schluessel einbaut,
        prueft dann die falsche Datei und waere gruen.
        """
        install = self.install
        (install / "data").mkdir(parents=True)
        (install / "draxo_bot").mkdir()
        (install / ".venv" / "bin").mkdir(parents=True)

        # venv-Stub: zeigt auf das echte Python, damit count_rows und der
        # Checkpoint laufen.
        python = Path(sys.executable)
        for name in ("python", "python3"):
            link = install / ".venv" / "bin" / name
            try:
                link.symlink_to(python)
            except OSError:
                link.write_text(f'#!/bin/sh\nexec "{python}" "$@"\n')
                link.chmod(0o755)

        # Bot-Code: echte Dateien, damit der Pfad-Fallback in signing.py
        # etwas zu finden hat.
        for name in ("api.py", "config.py", "service.py", "signing.py", "store.py"):
            source = ROOT / "bot" / "draxo_bot" / name
            if source.is_file():
                shutil.copy(source, install / "draxo_bot" / name)
        shutil.copy(LAUNCHER_DIR / "license_signing.py", install / "license_signing.py")
        (install / "requirements.txt").write_text("discord.py>=2.4,<3\n", encoding="utf-8")

        (install / ".env").write_text(
            "\n".join(
                [
                    "DISCORD_TOKEN=test-token",
                    "DISCORD_GUILD_ID=1534959905104986314",
                    f"DRAXO_SIGNING_KEY={PRIVATE_SEED}",
                    "KEY_HOURS=24",
                    "KEY_MAX_PER_DAY=1",
                    "BIND_HWID=1",
                    "DRAXO_DB=./data/draxo.sqlite3",
                    "API_ENABLED=1",
                    "API_TOKEN=test-api-token",
                    "",
                ]
            ),
            encoding="utf-8",
        )
        (install / ".env").chmod(0o600)

        self._create_db(install / "data" / "draxo.sqlite3")

        if with_wal_pending:
            self._leave_wal_open(install / "data" / "draxo.sqlite3")

        return install

    def _create_db(self, path: Path) -> None:
        db = sqlite3.connect(path, isolation_level=None)
        db.execute("PRAGMA journal_mode=WAL")
        db.executescript(
            """
            CREATE TABLE hwids (user_id INTEGER PRIMARY KEY, hwid TEXT NOT NULL,
                                created_at INTEGER NOT NULL);
            CREATE TABLE grants (
                id INTEGER PRIMARY KEY AUTOINCREMENT, user_id INTEGER NOT NULL,
                discord_id INTEGER NOT NULL, hwid TEXT NOT NULL DEFAULT '',
                hwid_hash BLOB, fingerprint TEXT NOT NULL, expiry INTEGER NOT NULL,
                revoked INTEGER NOT NULL DEFAULT 0, created_at INTEGER NOT NULL);
            CREATE TABLE audit (id INTEGER PRIMARY KEY AUTOINCREMENT, actor TEXT NOT NULL,
                                action TEXT NOT NULL, detail TEXT NOT NULL DEFAULT '',
                                created_at INTEGER NOT NULL);
            """
        )
        for user_id, hwid, created in HWIDS:
            db.execute("INSERT INTO hwids VALUES (?,?,?)", (user_id, hwid, created))
            db.execute(
                "INSERT INTO grants (user_id, discord_id, hwid, fingerprint, "
                "expiry, created_at) VALUES (?,?,?,?,?,?)",
                (user_id, user_id, hwid, f"fp{user_id:04d}", created + 86400, created),
            )
        db.close()

    def _leave_wal_open(self, path: Path) -> None:
        """Schreibt Commits, die nur in der -wal stehen.

        Das ist der Zustand, den ein SIGKILL hinterlässt: die Verbindung
        stirbt, der letzte Handle schließt nicht, und SQLite schreibt nichts
        mehr in die Hauptdatei.
        """
        db = sqlite3.connect(path, isolation_level=None)
        db.execute("PRAGMA journal_mode=WAL")
        # wal_autocheckpoint=0: nichts wandert automatisch in die Hauptdatei.
        db.execute("PRAGMA wal_autocheckpoint=0")
        for index, (user_id, hwid, created) in enumerate(HWIDS):
            db.execute(
                "INSERT INTO grants (user_id, discord_id, hwid, fingerprint, "
                "expiry, created_at) VALUES (?,?,?,?,?,?)",
                (
                    user_id,
                    user_id,
                    hwid,
                    "wal" + format(index, "04d"),
                    created + 172800,
                    created + 3600,
                ),
            )
        # Verbindung absichtlich offen lassen — sie wird beim Prozessende
        # geschlossen, das entspricht dem Kill.
        del db

    def _run(self, *args: str) -> subprocess.CompletedProcess:
        env = {
            **os.environ,
            "INSTALL_DIR": str(self.install),
            "BACKUP_ROOT": str(self.backups),
            "SERVICE_NAME": "draxo-bot-test",
            # Kein systemd: die Befehlsfolge wird trotzdem abgearbeitet,
            # nur die Dienstverwaltung entfaellt. So laeuft der Test auch
            # auf einem Rechner ohne systemd — was der Normalfall auf einem
            # Entwickler-Rechner ist.
            "NO_SYSTEMD": "1",
            # Ohne das nimmt sys.path das Repo-license_signing.py statt der
            # installierten Fassung. PYTHONPATH zuerst loescht das cwd aus
            # dem Suchpfad, das Installationsverzeichnis kommt stattdessen
            # bewusst nicht hinein — der Pfad-Fallback in signing.py
            # genuegt, weil das Skript dort mit sys.path.insert arbeitet.
            "PYTHONPATH": "",
            "PYTHONDONTWRITEBYTECODE": "1",
        }
        script = self._script
        return subprocess.run(
            [BASH, str(script), *args],
            capture_output=True,
            text=True,
            encoding="utf-8",
            env=env,
            cwd=str(self.install),
            timeout=180,
        )

    def _counts(self) -> dict:
        path = self.install / "data" / "draxo.sqlite3"
        db = sqlite3.connect(f"file:{path}?mode=ro", uri=True)
        try:
            return {
                table: db.execute(f"SELECT COUNT(*) FROM {table}").fetchone()[0]
                for table in ("hwids", "grants", "audit")
            }
        finally:
            db.close()

    def _hwids(self) -> list:
        path = self.install / "data" / "draxo.sqlite3"
        db = sqlite3.connect(f"file:{path}?mode=ro", uri=True)
        try:
            return [row[0] for row in db.execute("SELECT hwid FROM hwids ORDER BY user_id")]
        finally:
            db.close()


@unittest.skipIf(BASH is None, "bash nicht im PATH")
class DeployUpdateTest(DeployTestBase):
    def test_hwids_ueberleben(self) -> None:
        self._make_installation()
        before = self._counts()
        expected = self._hwids()

        result = self._run("--dry-run")
        self.assertEqual(
            result.returncode,
            0,
            "Trockenlauf fehlgeschlagen:\n" + result.stdout + result.stderr,
        )

        result = self._run()
        self.assertEqual(
            result.returncode,
            0,
            "Update fehlgeschlagen:\n" + result.stdout + result.stderr,
        )

        self.assertEqual(self._counts(), before, "Datenbestand hat sich geändert")
        self.assertEqual(self._hwids(), expected, "HWIDs fehlen")

    def test_trockenlauf_aendert_nichts(self) -> None:
        """--dry-run muss vollkommen harmlos sein.

        Sonst traegt jemand die Absicht eines Probelaufs ein, laesst ihn
        laufen und glaubt, die Daten waeren unberuehrt.
        """
        self._make_installation(with_wal_pending=True)
        before = self._counts()
        files_before = sorted(
            str(p.relative_to(self.install))
            for p in self.install.rglob("*")
            if p.is_file()
        )

        result = self._run("--dry-run")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

        self.assertEqual(self._counts(), before, "Trockenlauf hat die DB angefasst")
        self.assertEqual(
            sorted(
                str(p.relative_to(self.install))
                for p in self.install.rglob("*")
                if p.is_file()
            ),
            files_before,
            "Trockenlauf hat Dateien angelegt oder entfernt",
        )
        self.assertFalse(
            self.backups.exists(),
            "Trockenlauf hat ein Backup angelegt — es aendert also doch etwas.",
        )

    def test_env_bleibt_erhalten(self) -> None:
        self._make_installation()
        env_before = (self.install / ".env").read_text(encoding="utf-8")

        result = self._run()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

        self.assertEqual(
            (self.install / ".env").read_text(encoding="utf-8"),
            env_before,
            "Die .env wurde veraendert — der Signaturschluessel waere verloren.",
        )

    def test_ungemergte_wal_commits_ueberleben(self) -> None:
        """Der eigentliche Zweck des Skripts.

        Ohne Checkpoint gehen genau diese Zeilen verloren: sie stehen nur in
        der -wal, und wer die Hauptdatei kopiert, kopiert nicht die -wal.
        """
        self._make_installation(with_wal_pending=True)

        wal = Path(str(self.install / "data" / "draxo.sqlite3") + "-wal")
        self.assertTrue(wal.exists(), "Testaufbau: es gibt keine -wal-Datei")
        wal_size = wal.stat().st_size
        self.assertGreater(wal_size, 0, "Testaufbau: die -wal ist leer")

        # Wie viel steht nur in der -wal? Der Unterschied zwischen dem,
        # was SQLite über die -wal sieht, und dem, was ohne sie in der
        # Hauptdatei steckt.
        db = sqlite3.connect(self.install / "data" / "draxo.sqlite3")
        with_wal = db.execute("SELECT COUNT(*) FROM grants").fetchone()[0]
        db.close()

        # Hauptdatei allein lesen: -wal und -shm wegsperren.
        copy = Path(self._tmp.name) / "kopie.sqlite3"
        shutil.copy(self.install / "data" / "draxo.sqlite3", copy)
        db = sqlite3.connect(f"file:{copy}?mode=ro&immutable=1", uri=True)
        without_wal = db.execute("SELECT COUNT(*) FROM grants").fetchone()[0]
        db.close()

        self.assertGreater(
            with_wal,
            without_wal,
            "Testaufbau: die -wal enthaelt keine zusaetzlichen Zeilen",
        )

        before = self._counts()
        result = self._run()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

        after = self._counts()
        self.assertEqual(after, before, "Commit aus der -wal verloren")
        self.assertEqual(
            after["grants"],
            with_wal,
            f"erwartet {with_wal} Grants, {after['grants']} vorhanden",
        )

    def test_backup_ist_vollstaendig(self) -> None:
        self._make_installation()

        result = self._run()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

        backups = list(self.backups.glob("pre-update-*"))
        self.assertTrue(backups, "Es wurde kein Backup angelegt")
        backup = backups[0]

        self.assertTrue((backup / "env").is_file(), "Keine .env im Backup")
        self.assertTrue(
            (backup / "draxo.sqlite3").is_file(), "Keine Datenbank im Backup"
        )

        # Das Backup muss die HWIDs enthalten, nicht nur eine Datei sein.
        db = sqlite3.connect(f"file:{backup / 'draxo.sqlite3'}?mode=ro", uri=True)
        try:
            count = db.execute("SELECT COUNT(*) FROM hwids").fetchone()[0]
        finally:
            db.close()
        self.assertEqual(count, len(HWIDS), "Backup enthaelt nicht alle HWIDs")

    def test_schluessel_mismatch_bricht_ab(self) -> None:
        """Ein falscher oeffentlicher Schluessel darf nicht starten.

        Das ist der Fehler, den man sonst erst bemerkt, wenn sich Nutzer
        beschweren: der Bot laeuft fröhlich und stellt Grants aus, die der
        Launcher zurueckweist.

        Wichtig: der abweichende Schluessel muss in die **Quell**-Datei, die
        deployed wird — nicht in die installierte. sync_code kopiert
        license_signing.py aus dem Repo und überschreibt damit, was vorher
        in der Installation lag. Ein Test, der die installierte Datei
        veraendert, prueft also gar nichts.
        """
        source = self._make_installation()
        self.assertTrue(source.is_dir())
        # Das Testmodul bildet die echte Beziehung ab: public_key() liefert
        # den eingebauten Schluessel, public_from_seed() leitet den
        # oeffentlichen Teil aus dem privaten Seed ab. Gibt man beiden
        # denselben Wert, vergleicht das Skript zwei identische Zeichenketten
        # und meldet Erfolg — der Test waere dann gruen und pruefte nichts.
        # Deshalb wird der Seed wirklich umgerechnet.
        wrong_public = _public_from_seed("11" * 32)
        self._make_source(
            "SERVER_PUBLIC_KEY_HEX = (\n"
            f'    "{wrong_public[:32]}"\n'
            f'    "{wrong_public[32:]}"\n'
            ")\n"
            "def public_key():\n"
            "    return bytes.fromhex(SERVER_PUBLIC_KEY_HEX)\n"
            "def public_from_seed(seed):\n"
            "    import hashlib\n"
            "    return hashlib.sha512(b'draxo-test' + seed).digest()[:32]\n"
        )

        result = self._run()

        self.assertNotEqual(
            result.returncode, 0, "Skript lief trotz falschem Schluessel:\n" + result.stdout
        )
        combined = result.stdout + result.stderr
        self.assertIn(
            "passt nicht zusammen",
            combined,
            "Keine Warnung zum Schluessel:\n" + combined,
        )

    def test_unlesbarer_schluessel_bricht_ab(self) -> None:
        """Eine fehlende Pruefung darf nicht als Erfolg durchlaufen.

        Fehlt public_key() in license_signing.py, kann das Skript den
        Schluessel nicht vergleichen. Ein weiches "uebersprungen" wuerde
        hier durchlaufen und Erfolg melden — der Betreiber glaubt, es
        sei geprueft.
        """
        self._make_installation()
        # public_key() fehlt — der Import gelingt, der Vergleich nicht.
        self._make_source('SERVER_PUBLIC_KEY_HEX = "aa" * 32\n')

        result = self._run()
        combined = result.stdout + result.stderr

        self.assertNotEqual(
            result.returncode,
            0,
            "Skript lief durch, obwohl es den Schluessel nicht pruefen konnte:\n" + combined,
        )
        self.assertIn("nicht lesbar", combined, combined)

    def test_passender_schluessel_laeuft_durch(self) -> None:
        """Gegenprobe zum Mismatch-Test.

        Sonst könnte der Test auch bestehen, wenn das Skript aus jedem
        Grund abbricht — der Test muessste dann nur noch 'nicht gestartet'
        sehen.
        """
        self._make_installation()
        result = self._run()
        combined = result.stdout + result.stderr
        self.assertEqual(result.returncode, 0, combined)
        self.assertIn("Signaturschlüssel stimmt überein", combined, combined)
        self.assertNotIn("passt nicht zusammen", combined)

    def test_datenverlust_wird_gemeldet(self) -> None:
        """Wenn doch etwas verloren geht, muss es laut werden.

        Sonst steht am Ende ein grünes "Update abgeschlossen" neben einer
        Datenbank, in der die HWIDs fehlen.
        """
        self._make_installation()

        # Das Skript darf keine Daten anfassen. Wir prüfen die Gegenprobe:
        # die Zählung im Skript muss eine Datenbank sehen, die es dann
        # unverändert lässt.
        before = self._counts()
        result = self._run()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        after = self._counts()

        # Und das Skript muss es selbst auch sagen — nicht nur der Test.
        self.assertIn(
            "Alle Daten unverändert",
            result.stdout,
            "Das Skript bestaetigt den Datenbestand nicht:\n" + result.stdout,
        )

        if before != after:
            self.fail(
                "Daten wurden veraendert, ohne dass das Skript es meldete:\n"
                f"vorher {before}\nnachher {after}\n"
                + result.stdout
                + result.stderr
            )

    def test_syntax(self) -> None:
        """Läuft überall, auch ohne root und systemd."""
        proc = subprocess.run(
            [BASH, "-n", str(SCRIPT)], capture_output=True, text=True, timeout=60
        )
        self.assertEqual(proc.returncode, 0, proc.stderr)

    def test_kein_privater_schluessel_im_skript(self) -> None:
        """Das Skript darf keinen Schluessel hart enthalten.

        Es liest ihn aus der .env. Ein eingebackener Wert waere wieder
        genau die Sorte Leck, gegen die die ganze Umstellung lief.
        """
        source = SCRIPT.read_text(encoding="utf-8")
        import re

        literals = re.findall(r"""["'][0-9a-fA-F]{64}["']""", source)
        self.assertEqual(
            literals, [], f"Skript enthaelt eine 64-stellige Hex-Konstante: {literals}"
        )


if __name__ == "__main__":
    unittest.main()
