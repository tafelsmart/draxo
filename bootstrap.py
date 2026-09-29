"""
bootstrap.py
------------
Premium-Startbildschirm des Draxo Client.

Zeigt beim Start ein animiertes Splash-Fenster (CTkToplevel über dem
versteckten Haupt-Fenster) und führt in dieser Reihenfolge aus:

  1. Update-Check   – prüft GitHub Pages version.json (nicht blockierend)
  2. Python-Suche   – findet die System-Python-Installation
  3. Requirements   – installiert tools/requirements.txt via pip (nur Erststart)
  4. Launch         – blendet das Haupt-Fenster ein

Der gesamte Setup-Prozess läuft im Hintergrund-Thread; die GUI bleibt
während der gesamten Zeit vollständig reaktionsfähig.
"""

from __future__ import annotations

import logging
import queue
import threading
import time
from pathlib import Path
from typing import Optional

import customtkinter as ctk
from PIL import Image

from animations import WindowFadeIn
from config import ConfigManager
from setup_manager import SetupManager
from styles import COLORS, FONTS, LAYOUT
from updater import UpdateDialog, UpdateInfo, Updater
from utils import find_python_executable, load_image_safe, resource_path

logger = logging.getLogger("DraxoClient.bootstrap")

FRAME_MS = 16  # ~60 FPS

# ── Schritte im Bootstrap (Label + prozentualer Fortschritt) ──────────────────
STEPS: list[tuple[str, float]] = [
    ("Starte Draxo Client …",          0.05),
    ("Suche nach Updates …",           0.20),
    ("Suche Python-Installation …",    0.45),
    ("Installiere Abhängigkeiten …",   0.65),
    ("Starte Launcher …",              0.92),
]


class BootstrapMessage:
    """Interne Nachricht vom Hintergrund-Thread an die GUI."""

    def __init__(
        self,
        step_index: Optional[int] = None,
        log_line: Optional[tuple[str, str]] = None,
        progress: Optional[float] = None,
        update_info: Optional[UpdateInfo] = None,
        launch: bool = False,
        error: Optional[str] = None,
    ) -> None:
        self.step_index = step_index
        self.log_line = log_line
        self.progress = progress
        self.update_info = update_info
        self.launch = launch
        self.error = error


class BootstrapWindow(ctk.CTkToplevel):
    """
    Premium-Splash/Setup-Fenster.

    Erstellt als CTkToplevel über dem versteckten Haupt-Fenster.
    Wenn alles fertig ist, zerstört es sich selbst und ruft
    ``on_ready(python_path)`` auf.
    """

    WIDTH = 580
    HEIGHT = 440

    def __init__(
        self,
        parent: ctk.CTk,
        config_manager: ConfigManager,
        on_ready: object,
        on_update_found: object,
    ) -> None:
        super().__init__(parent)
        self._parent = parent
        self._cfg = config_manager
        self._on_ready = on_ready
        self._on_update_found = on_update_found
        self._msg_queue: "queue.Queue[BootstrapMessage]" = queue.Queue()
        self._progress_target: float = 0.0
        self._progress_current: float = 0.0
        self._launch_pending: bool = False
        self._update_info: Optional[UpdateInfo] = None

        self._configure_window()
        self._build_ui()
        self._animate_progress_bar()
        self._poll_queue()

        # Hintergrund-Thread starten
        threading.Thread(
            target=self._bootstrap_thread,
            daemon=True,
            name="Bootstrap",
        ).start()

    # ── Fenster-Konfiguration ─────────────────────────────────────────────────

    def _configure_window(self) -> None:
        self.title("Draxo Client")
        self.configure(fg_color=COLORS.BACKGROUND)
        self.resizable(False, False)
        self.overrideredirect(True)  # Kein Standard-Fensterrahmen → Premium-Look

        # Zentriert auf dem Bildschirm
        try:
            sw = self.winfo_screenwidth()
            sh = self.winfo_screenheight()
            x = (sw - self.WIDTH) // 2
            y = (sh - self.HEIGHT) // 2
            self.geometry(f"{self.WIDTH}x{self.HEIGHT}+{x}+{y}")
        except Exception:  # noqa: BLE001
            self.geometry(f"{self.WIDTH}x{self.HEIGHT}")

        # Sanft einblenden
        try:
            self.attributes("-alpha", 0.0)
        except Exception:  # noqa: BLE001
            pass
        WindowFadeIn(self, duration_ms=400).start()
        self.lift()
        self.focus_force()

    # ── UI-Aufbau ─────────────────────────────────────────────────────────────

    def _build_ui(self) -> None:
        # ── Äußerer Rahmen mit Glow-Border ────────────────────────────────
        outer = ctk.CTkFrame(
            self,
            fg_color=COLORS.SECONDARY,
            corner_radius=LAYOUT.CORNER_RADIUS_LARGE,
            border_width=1,
            border_color=COLORS.ACCENT,
        )
        outer.pack(fill="both", expand=True, padx=2, pady=2)

        # ── Logo ──────────────────────────────────────────────────────────
        logo_frame = ctk.CTkFrame(outer, fg_color="transparent")
        logo_frame.pack(pady=(32, 0))

        image_path = resource_path("image.png")
        pil_img = load_image_safe(image_path, size=(80, 80))
        if pil_img:
            ctk_img = ctk.CTkImage(light_image=pil_img, dark_image=pil_img, size=(80, 80))
            ctk.CTkLabel(logo_frame, image=ctk_img, text="").pack()
            self._logo_img_ref = ctk_img  # GC-Schutz
        else:
            ctk.CTkLabel(
                logo_frame,
                text="⬡",
                font=("Segoe UI", 48),
                text_color=COLORS.ACCENT,
            ).pack()

        # ── Titel ─────────────────────────────────────────────────────────
        ctk.CTkLabel(
            outer,
            text="DRAXO CLIENT",
            font=(FONTS.FAMILY_BOLD, 22, "bold"),
            text_color=COLORS.TEXT_PRIMARY,
        ).pack(pady=(10, 2))

        ctk.CTkLabel(
            outer,
            text=f"v{self._read_version()}  ·  Premium Minecraft Launcher",
            font=(FONTS.FAMILY, FONTS.SMALL_SIZE),
            text_color=COLORS.TEXT_MUTED,
        ).pack()

        # ── Trennlinie ────────────────────────────────────────────────────
        ctk.CTkFrame(
            outer, height=1, fg_color=COLORS.BORDER
        ).pack(fill="x", padx=40, pady=(18, 14))

        # ── Status-Label ──────────────────────────────────────────────────
        self._status_label = ctk.CTkLabel(
            outer,
            text="Initialisiere …",
            font=(FONTS.FAMILY, FONTS.SMALL_SIZE),
            text_color=COLORS.TEXT_SECONDARY,
            anchor="w",
            width=460,
        )
        self._status_label.pack(padx=60, anchor="w")

        # ── Fortschrittsbalken ────────────────────────────────────────────
        self._progress_bar = ctk.CTkProgressBar(
            outer,
            width=460,
            height=6,
            corner_radius=3,
            progress_color=COLORS.ACCENT,
            fg_color=COLORS.SURFACE,
            mode="determinate",
        )
        self._progress_bar.set(0.0)
        self._progress_bar.pack(padx=60, pady=(8, 14))

        # ── Mini-Konsole (pip-Output) ─────────────────────────────────────
        self._log_box = ctk.CTkTextbox(
            outer,
            height=110,
            fg_color="#050507",
            text_color=COLORS.TEXT_MUTED,
            corner_radius=LAYOUT.CORNER_RADIUS_SMALL,
            font=(FONTS.MONO, 10),
            wrap="word",
            state="disabled",
        )
        self._log_box.pack(padx=40, pady=(0, 14), fill="x")
        self._log_box.tag_config("error",   foreground=COLORS.ERROR)
        self._log_box.tag_config("warning", foreground=COLORS.WARNING)
        self._log_box.tag_config("success", foreground=COLORS.SUCCESS)
        self._log_box.tag_config("info",    foreground=COLORS.TEXT_MUTED)

        # ── Fußzeile ──────────────────────────────────────────────────────
        ctk.CTkLabel(
            outer,
            text="© Draxo Client  ·  batotomato",
            font=(FONTS.FAMILY, 9),
            text_color=COLORS.TEXT_MUTED,
        ).pack(side="bottom", pady=(0, 10))

    # ── Hintergrund-Thread ────────────────────────────────────────────────────

    def _bootstrap_thread(self) -> None:
        """
        Läuft komplett im Hintergrund-Thread.
        Kommuniziert mit der GUI ausschließlich über _msg_queue.
        """
        cfg = self._cfg.config

        # ── Schritt 0: kurze Pause, damit Fade-In sichtbar ist ────────────
        self._post(step_index=0, progress=STEPS[0][1])
        time.sleep(0.4)

        # ── Schritt 1: Update-Check ────────────────────────────────────────
        self._post(step_index=1, progress=STEPS[1][1])
        update_info: Optional[UpdateInfo] = None
        try:
            update_info = Updater().check_for_update()
        except Exception:  # noqa: BLE001
            logger.debug("Update-Check fehlgeschlagen (kein Internet?).")

        # ── Schritt 2: Python suchen ───────────────────────────────────────
        self._post(step_index=2, progress=STEPS[2][1])

        python_path: str = cfg.python_path or ""

        # Gespeicherten Pfad zuerst prüfen
        if python_path:
            import shutil
            if not shutil.which(python_path) and not Path(python_path).exists():
                python_path = ""

        if not python_path:
            found = find_python_executable()
            python_path = found or ""

        if python_path:
            self._post_log(f"Python: {python_path}", "success")
        else:
            self._post_log("⚠️  Python nicht gefunden – Inject möglicherweise nicht möglich.", "warning")

        # ── Schritt 3: Requirements installieren (nur wenn nötig) ──────────
        needs_setup = (not cfg.setup_done) or (not python_path and bool(cfg.python_path))

        if needs_setup and python_path:
            self._post(step_index=3, progress=STEPS[3][1])

            manager = SetupManager(
                line_callback=lambda text, tag: self._post_log(text, tag),
            )
            result = manager.run()

            if result.success:
                self._cfg.update_setup_done(True, python_path=python_path)
                python_path = result.python_path or python_path
            else:
                # Setup fehlgeschlagen → trotzdem weitermachen, Fehler wird gezeigt
                self._post_log(f"Setup-Fehler: {result.message}", "error")
        else:
            # Setup bereits gemacht oder Python nicht verfügbar: Pfad nur speichern
            if python_path and not cfg.setup_done:
                # Keine requirements.txt → direkt als done markieren
                self._cfg.update_setup_done(True, python_path=python_path)
            elif python_path:
                self._cfg.update_python_path(python_path)

        # ── Schritt 4: Update-Dialog anfordern (falls Update verfügbar) ────
        if update_info and update_info.update_available:
            self._post(update_info=update_info, progress=STEPS[4][1])
        else:
            self._post(step_index=4, progress=STEPS[4][1])

        time.sleep(0.3)

        # ── Fertig: Haupt-Fenster starten ──────────────────────────────────
        self._post(launch=True, progress=1.0)

    # ── Queue-Kommunikation ───────────────────────────────────────────────────

    def _post(
        self,
        step_index: Optional[int] = None,
        progress: Optional[float] = None,
        log_line: Optional[tuple[str, str]] = None,
        update_info: Optional[UpdateInfo] = None,
        launch: bool = False,
        error: Optional[str] = None,
    ) -> None:
        self._msg_queue.put(
            BootstrapMessage(
                step_index=step_index,
                progress=progress,
                log_line=log_line,
                update_info=update_info,
                launch=launch,
                error=error,
            )
        )

    def _post_log(self, text: str, tag: str) -> None:
        self._msg_queue.put(BootstrapMessage(log_line=(text, tag)))

    def _poll_queue(self) -> None:
        """Liest die Queue im GUI-Thread und verarbeitet Nachrichten."""
        try:
            while True:
                msg: BootstrapMessage = self._msg_queue.get_nowait()
                self._handle_message(msg)
        except queue.Empty:
            pass
        except Exception:  # noqa: BLE001
            logger.exception("Fehler beim Verarbeiten der Bootstrap-Queue.")
        finally:
            if not self._launch_pending:
                self.after(FRAME_MS, self._poll_queue)

    def _handle_message(self, msg: BootstrapMessage) -> None:
        if msg.progress is not None:
            self._progress_target = msg.progress

        if msg.step_index is not None and 0 <= msg.step_index < len(STEPS):
            self._status_label.configure(text=STEPS[msg.step_index][0])

        if msg.log_line is not None:
            text, tag = msg.log_line
            self._append_log(text, tag)

        if msg.update_info is not None:
            self._update_info = msg.update_info
            # Update-Benachrichtigung kurz einblenden
            self._status_label.configure(
                text=f"🚀 Update verfügbar: v{msg.update_info.latest_version}!",
                text_color=COLORS.ACCENT,
            )

        if msg.error is not None:
            self._status_label.configure(text=f"❌ {msg.error}", text_color=COLORS.ERROR)

        if msg.launch:
            self._launch_pending = True
            # Noch letzte Queue-Nachrichten leeren, dann launchen
            self.after(350, self._do_launch)

    def _do_launch(self) -> None:
        """Schließt das Bootstrap-Fenster und öffnet das Haupt-Fenster."""
        try:
            self.destroy()
        except Exception:  # noqa: BLE001
            pass

        try:
            if self._update_info and self._update_info.update_available:
                self._on_update_found(self._update_info)
            self._on_ready()
        except Exception:  # noqa: BLE001
            logger.exception("Fehler beim Starten des Haupt-Fensters.")

    # ── Fortschrittsbalken-Animation ─────────────────────────────────────────

    def _animate_progress_bar(self) -> None:
        """Bewegt den Balken sanft auf den nächsten Zielwert zu (Easing)."""
        if self._launch_pending:
            return
        diff = self._progress_target - self._progress_current
        if abs(diff) > 0.001:
            self._progress_current += diff * 0.12
            try:
                self._progress_bar.set(self._progress_current)
            except Exception:  # noqa: BLE001
                return
        self.after(FRAME_MS, self._animate_progress_bar)

    # ── Log-Box ───────────────────────────────────────────────────────────────

    def _append_log(self, text: str, tag: str = "info") -> None:
        try:
            self._log_box.configure(state="normal")
            self._log_box.insert("end", text.rstrip() + "\n", tag)
            self._log_box.see("end")
            self._log_box.configure(state="disabled")
        except Exception:  # noqa: BLE001
            pass

    # ── Hilfsmethoden ─────────────────────────────────────────────────────────

    @staticmethod
    def _read_version() -> str:
        vfile = resource_path("VERSION")
        if vfile.exists():
            return vfile.read_text(encoding="utf-8").strip()
        return "1.0.0"
