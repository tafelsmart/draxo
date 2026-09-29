"""
draxo_launcher.py
-----------------
Einstiegspunkt des Draxo Client.

Ablauf:
  1. Logging initialisieren
  2. Haupt-Fenster (MainWindow) erstellen und VERSTECKEN
  3. Bootstrap-Fenster öffnen:
       → Update-Check (Hintergrund)
       → Python suchen
       → pip-Requirements installieren (Erststart)
  4. Bootstrap schließt sich selbst
  5. MainWindow einblenden (Fade-In-Animation)

Alle unkritischen Fehler werden geloggt und dem Benutzer als Dialog
gezeigt; ein Absturz ohne Rückmeldung ist ausgeschlossen.
"""

from __future__ import annotations

import sys
import traceback
from pathlib import Path
from tkinter import messagebox

from utils import get_base_dir, setup_logging


class DraxoLauncherApp:
    """Kapselt den Start und die Top-Level-Fehlerbehandlung der Anwendung."""

    def __init__(self) -> None:
        self._base_dir: Path = get_base_dir()
        self._log_file: Path = self._base_dir / "draxo_client.log"
        self._logger = setup_logging(self._log_file)

    def run(self) -> int:
        self._logger.info("=" * 60)
        self._logger.info("Draxo Client startet …")

        try:
            import customtkinter as ctk
            from animations import WindowFadeIn
            from bootstrap import BootstrapWindow
            from config import ConfigManager
            from updater import UpdateDialog
            from ui import MainWindow

        except ImportError as exc:
            self._handle_error(
                f"Eine benötigte Bibliothek fehlt:\n\n{exc}\n\n"
                "Führe 'pip install -r requirements.txt' aus.",
                exc,
            )
            return 1

        try:
            # ── Konfiguration laden ────────────────────────────────────────
            cfg = ConfigManager()

            # ── Haupt-Fenster erstellen (noch unsichtbar) ──────────────────
            win = MainWindow(config_manager=cfg)
            win.withdraw()        # komplett versteckt
            win.attributes("-alpha", 0.0)

            # ── Callbacks für Bootstrap ────────────────────────────────────
            def on_ready() -> None:
                """Wird vom Bootstrap nach erfolgreichem Setup aufgerufen."""
                try:
                    win.deiconify()
                    win.lift()
                    win.focus_force()
                    WindowFadeIn(win, duration_ms=500).start()
                    win.after(500, win.post_bootstrap_init)
                except Exception as e:  # noqa: BLE001
                    self._logger.exception("Fehler beim Anzeigen des Haupt-Fensters.")
                    win.report_fatal_error(e)

            def on_update_found(update_info: object) -> None:
                """Zeigt den Update-Dialog nach dem Bootstrap."""
                try:
                    dlg = UpdateDialog(win, update_info)
                    dlg.focus()
                except Exception:  # noqa: BLE001
                    self._logger.exception("Update-Dialog konnte nicht geöffnet werden.")

            # ── Bootstrap starten ──────────────────────────────────────────
            BootstrapWindow(
                parent=win,
                config_manager=cfg,
                on_ready=on_ready,
                on_update_found=on_update_found,
            )

            win.mainloop()
            self._logger.info("Draxo Client regulär beendet.")
            return 0

        except Exception as exc:  # noqa: BLE001
            self._handle_error(f"Unerwarteter Fehler:\n\n{exc}", exc)
            return 1

    def _handle_error(self, message: str, exc: BaseException) -> None:
        self._logger.error("Kritischer Fehler: %s", exc)
        self._logger.error(traceback.format_exc())
        print(traceback.format_exc(), file=sys.stderr)
        try:
            messagebox.showerror("Draxo Client — Fehler", message)
        except Exception:  # noqa: BLE001
            print(message, file=sys.stderr)


def main() -> int:
    return DraxoLauncherApp().run()


if __name__ == "__main__":
    sys.exit(main())
