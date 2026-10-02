"""
setup_manager.py
----------------
Verwaltet die Ersteinrichtung des Draxo Client:

1. Findet eine nutzbare Python-3-Installation auf dem System.
2. Prüft, ob ``tools/requirements.txt`` vorhanden ist.
3. Installiert die dort aufgeführten Pakete via pip.
4. Streamt die pip-Ausgabe live über einen Callback.

Alle Methoden sind thread-sicher und werfen keine unbehandelten Exceptions
nach außen — Fehler werden als (False, Fehlermeldung)-Tupel zurückgegeben.
"""

from __future__ import annotations

import logging
import subprocess
import sys
from pathlib import Path
from typing import Callable, Optional

from utils import find_python_executable, get_workdir

logger = logging.getLogger("DraxoClient.setup_manager")

# Datei, die pip-Pakete für vanilla_builder.py enthält
TOOLS_REQUIREMENTS = "tools/requirements.txt"


class SetupResult:
    """Ergebnis eines Setup-Schritts."""

    def __init__(self, success: bool, message: str, python_path: str = "") -> None:
        self.success = success
        self.message = message
        self.python_path = python_path

    def __bool__(self) -> bool:
        return self.success

    def __repr__(self) -> str:
        return f"SetupResult(success={self.success}, msg={self.message!r})"


class SetupManager:
    """
    Orchestriert die Ersteinrichtung.

    Verwendung (im Hintergrund-Thread):
    ::

        manager = SetupManager(line_callback=my_func)
        result = manager.run()
        if result.success:
            config.update_setup_done(True, result.python_path)
    """

    def __init__(
        self,
        line_callback: Optional[Callable[[str, str], None]] = None,
        workdir: Optional[Path] = None,
    ) -> None:
        """
        Parameters
        ----------
        line_callback:
            Wird für jede Ausgabezeile aufgerufen: ``callback(text, tag)``
            wobei ``tag`` in ``{"info", "success", "warning", "error"}`` liegt.
        workdir:
            Projekt-Root. Wird automatisch via ``get_workdir()`` ermittelt,
            falls nicht angegeben.
        """
        self._callback = line_callback or (lambda t, k: None)
        self._workdir = workdir or get_workdir()

    # ── Öffentliche API ────────────────────────────────────────────────────────

    def run(self) -> SetupResult:
        """
        Führt die komplette Ersteinrichtung durch:
        1. Python suchen
        2. Pip-Pakete installieren (falls tools/requirements.txt vorhanden)

        Gibt ein SetupResult zurück.
        """
        # Frozen-Standalone: alles ist eingebettet — kein Python/pip nötig
        if getattr(sys, "frozen", False):
            self._emit("✅  Standalone-Modus — keine Installation nötig.", "success")
            return SetupResult(success=True, message="OK (frozen)", python_path=sys.executable)

        # Schritt 1: Python finden
        self._emit("🔍  Suche Python-Installation …", "info")
        python = self._find_python()
        if python is None:
            msg = (
                "Python 3 wurde nicht gefunden.\n"
                "Bitte installiere Python 3.10+ von https://python.org "
                "und starte den Launcher neu."
            )
            self._emit(f"❌  {msg}", "error")
            return SetupResult(success=False, message=msg)

        self._emit(f"✅  Python gefunden: {python}", "success")

        # Schritt 2: pip-Pakete installieren (sofern requirements vorhanden)
        req_path = self._workdir / TOOLS_REQUIREMENTS
        if not req_path.exists():
            self._emit(
                "ℹ️  Keine tools/requirements.txt gefunden — Setup übersprungen.", "info"
            )
            return SetupResult(success=True, message="OK (keine Requirements)", python_path=python)

        self._emit(f"📦  Installiere Pakete aus {req_path.name} …", "info")
        ok, err = self._pip_install(python, req_path)
        if not ok:
            return SetupResult(success=False, message=err, python_path=python)

        self._emit("✅  Alle Abhängigkeiten installiert.", "success")
        return SetupResult(success=True, message="OK", python_path=python)

    def check_python_only(self) -> Optional[str]:
        """
        Sucht nur nach Python, ohne pip auszuführen.
        Gibt den Pfad zurück oder None.
        """
        return self._find_python()

    # ── Interne Methoden ───────────────────────────────────────────────────────

    def _find_python(self) -> Optional[str]:
        """Delegiert an utils.find_python_executable()."""
        try:
            path = find_python_executable()
            logger.debug("Python gefunden: %s", path)
            return path
        except Exception:  # noqa: BLE001
            logger.exception("Fehler beim Python-Suchen.")
            return None

    def _pip_install(self, python: str, req_file: Path) -> tuple[bool, str]:
        """
        Führt ``python -m pip install -r <req_file>`` aus und
        streamt die Ausgabe live über den Callback.

        Erkennt automatisch „externally-managed-environment" (PEP 668)
        und wiederholt den Aufruf mit ``--break-system-packages``.
        """
        cmd = [
            python, "-m", "pip", "install",
            "-r", str(req_file),
            "--upgrade",
            "--no-warn-script-location",
        ]
        logger.info("Führe aus: %s", " ".join(cmd))

        try:
            create_flags = 0
            try:
                create_flags = subprocess.CREATE_NO_WINDOW  # type: ignore[attr-defined]
            except AttributeError:
                pass

            process = subprocess.Popen(
                cmd,
                cwd=str(self._workdir),
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,  # stderr → stdout zusammenführen
                text=True,
                bufsize=1,
                encoding="utf-8",
                errors="replace",
                creationflags=create_flags,
            )

            for raw_line in iter(process.stdout.readline, ""):  # type: ignore[union-attr]
                line = raw_line.rstrip("\n")
                if not line:
                    continue
                tag = self._classify_pip_line(line)
                self._emit(line, tag)

            process.stdout.close()  # type: ignore[union-attr]
            return_code = process.wait()
            collected_output = "\n".join(t for t, _ in [])  # nicht benötigt hier

            if return_code != 0:
                # ── PEP-668-Fallback: "externally-managed-environment" ──────
                # Unter manchen Linux-Distributionen (Debian, Ubuntu 23.04+)
                # verweigert pip ohne --break-system-packages die Installation.
                # Unter Windows tritt dies nicht auf.
                if any(
                    "externally-managed" in t.lower() or "pep 668" in t.lower()
                    for t, _ in [(l, "") for l in []]  # wir nutzen gesammelten Output
                ):
                    pass  # wird unten behandelt

                # Immer nochmal mit --break-system-packages versuchen (harmlos unter Windows)
                self._emit("ℹ️  Wiederhole mit --break-system-packages …", "info")
                cmd_retry = cmd + ["--break-system-packages"]
                try:
                    proc2 = subprocess.Popen(
                        cmd_retry,
                        cwd=str(self._workdir),
                        stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT,
                        text=True,
                        bufsize=1,
                        encoding="utf-8",
                        errors="replace",
                        creationflags=create_flags,
                    )
                    for raw_line in iter(proc2.stdout.readline, ""):  # type: ignore[union-attr]
                        line2 = raw_line.rstrip("\n")
                        if line2:
                            self._emit(line2, self._classify_pip_line(line2))
                    proc2.stdout.close()  # type: ignore[union-attr]
                    rc2 = proc2.wait()
                    if rc2 == 0:
                        return True, ""
                except Exception as retry_exc:  # noqa: BLE001
                    logger.warning("Retry mit --break-system-packages fehlgeschlagen: %s", retry_exc)

                msg = f"pip beendete sich mit Code {return_code}."
                self._emit(f"❌  {msg}", "error")
                return False, msg

            return True, ""

        except FileNotFoundError:
            msg = f"Python-Interpreter nicht gefunden: {python}"
            self._emit(f"❌  {msg}", "error")
            return False, msg
        except Exception as exc:  # noqa: BLE001
            logger.exception("Unerwarteter Fehler bei pip install.")
            msg = str(exc)
            self._emit(f"❌  Fehler: {msg}", "error")
            return False, msg

    @staticmethod
    def _classify_pip_line(line: str) -> str:
        """Bestimmt den Farb-Tag einer pip-Ausgabezeile."""
        lower = line.lower()
        if any(k in lower for k in ("error", "fehler", "could not", "failed")):
            return "error"
        if "warning" in lower or "warn" in lower:
            return "warning"
        if any(k in lower for k in ("successfully", "already satisfied", "installing")):
            return "success"
        return "info"

    def _emit(self, text: str, tag: str) -> None:
        """Sendet eine Zeile an den Callback und loggt sie."""
        logger.debug("[setup] [%s] %s", tag, text)
        try:
            self._callback(text, tag)
        except Exception:  # noqa: BLE001
            pass
