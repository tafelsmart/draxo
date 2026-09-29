"""
ui.py
-----
Minimaler Horion-style Launcher für den Draxo Client.

Das Fenster ist bewusst auf das Nötigste reduziert:
  • DRAXO-Logo (breites_logo_draxo.png)
  • Versionsauswahl mit Auto-Erkennung der laufenden Minecraft-Instanz
  • Grüner INJECT-Button
  • Versions-Nummer unten links (wie Horion "v1.1.2")

KEIN Lizenzsystem, KEIN Crash-Test, KEINE Konsole — nur Logo, Version,
Inject. (Die DLL-seitige auth_check war ohnehin nie verdrahtet —
deaktiviert Module ohne gültigen Key).

Der Build wird so weit wie möglich übersprungen: Liegt eine vorgefertigte
DLL in build/prebuilt/<version>/ (siehe tools/prebuild_dlls.py), wird
direkt injiziert — ohne CMake/MSVC/Python.
"""

from __future__ import annotations

import json
import logging
import queue
import threading
import tkinter as tk
import traceback
from pathlib import Path
from tkinter import messagebox
from typing import Optional

import customtkinter as ctk
from PIL import Image

from animations import (
    ButtonClickPulse,
    ButtonPulseGlow,
    LogoScaleIn,
    WindowFadeIn,
)
from config import ConfigManager
from process_detector import FLAVOR_LABELS, ProcessDetector
from styles import COLORS, FONTS, LAYOUT
from updater import Updater
from utils import center_window, get_workdir, load_image_safe, resource_path
from versions import version_provider

logger = logging.getLogger("DraxoClient.ui")

# WORKDIR = Projekt-Root (Frozen: Ordner der .exe; Dev: Repo-Root)
WORKDIR = get_workdir()
_PROJECT_DIR = WORKDIR

PROCESS_CHECK_INTERVAL_MS = 3000
CONSOLE_POLL_INTERVAL_MS = 40
VERSION_REFRESH_DELAY_MS = 2500

# Eintrag im Versions-Dropdown, der die Auto-Erkennung verwendet.
AUTO_VERSION_LABEL = "— automatisch —"


class StreamType:
    STDOUT = "stdout"
    STDERR = "stderr"
    INTERNAL = "internal"


class InjectStatus:
    READY = "READY"
    INJECTING = "INJECTING"
    SUCCESS = "SUCCESS"
    ERROR = "ERROR"


class MainWindow(ctk.CTk):
    """Minimales Horion-style Hauptfenster des Draxo Launchers."""

    def __init__(self, config_manager: Optional["ConfigManager"] = None) -> None:
        super().__init__()

        self._config_manager = config_manager or ConfigManager()
        self._updater = Updater()

        self._console_queue: "queue.Queue[tuple[str, str]]" = queue.Queue()
        self._inject_thread: Optional[threading.Thread] = None
        self._is_injecting: bool = False

        self._glow_animation: Optional[ButtonPulseGlow] = None

        self._configure_window()
        self._apply_windows_effects()
        self._build_ui()
        self._start_background_tasks()
        self._start_entry_animation()

        self.protocol("WM_DELETE_WINDOW", self._on_close)

    # ------------------------------------------------------------------
    # Fenster-Setup
    # ------------------------------------------------------------------

    def _configure_window(self) -> None:
        ctk.set_appearance_mode("dark")
        ctk.set_default_color_theme("blue")

        self.title("Draxo Client")
        self.configure(fg_color=COLORS.BACKGROUND)

        width = 460
        height = 660
        self.geometry(f"{width}x{height}")
        center_window(self, width, height)

        icon_path = resource_path("draxo.ico")
        if icon_path.exists():
            try:
                self.iconbitmap(default=str(icon_path))
            except tk.TclError:
                logger.warning("draxo.ico konnte nicht gesetzt werden.")

    def _apply_windows_effects(self) -> None:
        """Rahmenloses Fenster mit abgerundeten Ecken (Horion-Look)."""
        try:
            self.overrideredirect(True)
            self.attributes("-transparentcolor", "#010101")
            self.configure(fg_color="#010101")
            logger.info("Rahmenloses Fenster mit abgerundeten Ecken aktiviert.")
        except Exception:  # noqa: BLE001
            logger.exception("Rahmenloses Fenster nicht möglich — nutze Standardrahmen.")
            try:
                self.overrideredirect(False)
            except Exception:  # noqa: BLE001
                pass

    # ------------------------------------------------------------------
    # UI-Aufbau
    # ------------------------------------------------------------------

    def _build_ui(self) -> None:
        self._root_container = ctk.CTkFrame(self, fg_color=COLORS.BACKGROUND, corner_radius=0)
        self._root_container.pack(fill="both", expand=True)

        self._content_frame = ctk.CTkFrame(
            self._root_container, fg_color="transparent", corner_radius=0
        )
        self._content_frame.place(x=0, y=0, relwidth=1, relheight=1)

        self._content_frame.grid_rowconfigure(0, weight=0)  # Header (Logo)
        self._content_frame.grid_rowconfigure(1, weight=1)  # Abstand
        self._content_frame.grid_rowconfigure(2, weight=0)  # Auswahl
        self._content_frame.grid_rowconfigure(3, weight=0)  # Status
        self._content_frame.grid_rowconfigure(4, weight=0)  # Inject-Button
        self._content_frame.grid_rowconfigure(5, weight=1)  # Abstand
        self._content_frame.grid_rowconfigure(6, weight=0)  # Footer (Version)
        self._content_frame.grid_columnconfigure(0, weight=1)

        self._build_header(self._content_frame)
        self._build_target_row(self._content_frame)
        self._build_inject_row(self._content_frame)
        self._build_footer(self._content_frame)

        # Drag-Bereich: gesamtes Fenster verschiebbar
        self._content_frame.bind("<Button-1>", self._on_drag_start)
        self._content_frame.bind("<B1-Motion>", self._on_drag_move)

    def _build_header(self, parent: ctk.CTkFrame) -> None:
        header = ctk.CTkFrame(parent, fg_color="transparent")
        header.grid(row=0, column=0, sticky="ew", padx=20, pady=(18, 0))
        header.grid_columnconfigure(0, weight=1)
        header.grid_columnconfigure(1, weight=0)

        # Close-Button (oben rechts, wie Horion)
        close_btn = ctk.CTkButton(
            header,
            text="✕",
            command=self._on_close,
            width=34,
            height=34,
            corner_radius=9,
            fg_color="transparent",
            hover_color=COLORS.SURFACE_LIGHT,
            text_color=COLORS.TEXT_MUTED,
            font=(FONTS.FAMILY_BOLD, 14, "bold"),
        )
        close_btn.grid(row=0, column=1, sticky="ne")

        # Logo (zentriert, breites Draxo-Logo)
        self._logo_label = ctk.CTkLabel(header, text="", fg_color="transparent")
        self._logo_label.grid(row=0, column=0, padx=(34, 0))

        self._load_logo_deferred()

    def _load_logo_deferred(self) -> None:
        """Lädt breites_logo_draxo.png und startet die Scale-In-Animation."""
        base_size = (320, 214)  # kompakter fürs kleine Fenster
        image_path = resource_path("breites_logo_draxo.png")
        if not image_path.exists():
            image_path = resource_path("image.png")
        pil_image = load_image_safe(image_path, size=base_size)

        if pil_image is None:
            self._logo_label.configure(
                text="DRAXO",
                font=(FONTS.FAMILY_BOLD, 34, "bold"),
                text_color=COLORS.ACCENT,
            )
            return

        self._logo_source_image = pil_image

        def image_loader(width: int, height: int) -> ctk.CTkImage:
            resized = self._logo_source_image.resize((width, height), Image.LANCZOS)
            return ctk.CTkImage(light_image=resized, dark_image=resized, size=(width, height))

        self._logo_image_loader = image_loader
        initial_image = image_loader(*base_size)
        self._logo_label.configure(image=initial_image)
        self._logo_label.image = initial_image

    def _build_target_row(self, parent: ctk.CTkFrame) -> None:
        """Version + Instanz-Typ (Vanilla/Fabric/Forge/NeoForge) auswählbar."""
        row = ctk.CTkFrame(parent, fg_color="transparent")
        row.grid(row=2, column=0, sticky="ew", padx=34, pady=(0, 10))

        # ── Versions-Dropdown ────────────────────────────────────────
        ctk.CTkLabel(
            row,
            text="VERSION",
            font=(FONTS.FAMILY, 9, "bold"),
            text_color=COLORS.TEXT_MUTED,
            anchor="w",
        ).grid(row=0, column=0, sticky="w", padx=(0, 10))

        self._version_var = tk.StringVar(value="")
        self._version_box = ctk.CTkOptionMenu(
            row,
            variable=self._version_var,
            values=["— automatisch —"],
            command=self._on_version_selected,
            width=170,
            height=34,
            corner_radius=LAYOUT.CORNER_RADIUS_SMALL,
            fg_color=COLORS.SURFACE,
            button_color=COLORS.SURFACE_LIGHT,
            button_hover_color=COLORS.BORDER,
            text_color=COLORS.TEXT_PRIMARY,
            dropdown_fg_color=COLORS.SURFACE,
            dropdown_text_color=COLORS.TEXT_PRIMARY,
            dropdown_hover_color=COLORS.SURFACE_LIGHT,
            font=(FONTS.FAMILY, 12),
        )
        self._version_box.grid(row=0, column=1, sticky="w")

        # ── Auto-Erkennungs-Knopf ────────────────────────────────────
        self._rescan_btn = ctk.CTkButton(
            row,
            text="⟳",
            width=34,
            height=34,
            corner_radius=LAYOUT.CORNER_RADIUS_SMALL,
            fg_color=COLORS.SURFACE,
            hover_color=COLORS.SURFACE_LIGHT,
            text_color=COLORS.TEXT_SECONDARY,
            font=(FONTS.FAMILY, 14),
            command=self._on_rescan_clicked,
        )
        self._rescan_btn.grid(row=0, column=2, sticky="e", padx=(8, 0))

        # ── Flavor-Segment-Buttons ───────────────────────────────────
        flavor_row = ctk.CTkFrame(parent, fg_color="transparent")
        flavor_row.grid(row=3, column=0, sticky="ew", padx=34, pady=(0, 6))
        self._flavor_buttons: dict[str, ctk.CTkButton] = {}
        for idx, (key, label) in enumerate(FLAVOR_LABELS.items()):
            btn = ctk.CTkButton(
                flavor_row,
                text=label,
                command=lambda k=key: self._on_flavor_selected(k),
                width=88,
                height=30,
                corner_radius=LAYOUT.CORNER_RADIUS_SMALL,
                fg_color=COLORS.SURFACE,
                hover_color=COLORS.SURFACE_LIGHT,
                text_color=COLORS.TEXT_MUTED,
                font=(FONTS.FAMILY, 11, "bold"),
                border_width=1,
                border_color=COLORS.BORDER,
            )
            btn.grid(row=0, column=idx, padx=2)
            self._flavor_buttons[key] = btn

        self._flavor = "vanilla"
        self._selected_version: str = ""
        self._highlight_flavor("vanilla")

    def _highlight_flavor(self, flavor: str) -> None:
        """Aktiven Flavor optisch hervorheben."""
        for key, btn in self._flavor_buttons.items():
            active = key == flavor
            btn.configure(
                fg_color=COLORS.INJECT_GREEN if active else COLORS.SURFACE,
                text_color=COLORS.TEXT_PRIMARY if active else COLORS.TEXT_MUTED,
                border_color=COLORS.INJECT_GREEN if active else COLORS.BORDER,
            )

    def _on_flavor_selected(self, flavor: str) -> None:
        self._flavor = flavor
        self._highlight_flavor(flavor)
        logger.info("Flavor gewählt: %s", flavor)
        self._update_availability_hint()

    def _on_version_selected(self, value: str) -> None:
        if value and value != AUTO_VERSION_LABEL:
            self._selected_version = value
        else:
            self._selected_version = ""
        self._update_availability_hint()

    def _on_rescan_clicked(self) -> None:
        threading.Thread(target=self._rescan_worker, daemon=True,
                         name="Rescan").start()

    def _rescan_worker(self) -> None:
        versions = version_provider.get_versions()
        instances = ProcessDetector.list_instances(versions)
        self.after(0, lambda: self._apply_scan(versions, instances))

    def _apply_scan(self, versions: list[str], instances: list) -> None:
        """Ergebnisse der Auto-Erkennung in die UI übernehmen."""
        values = [AUTO_VERSION_LABEL] + list(versions)
        self._version_box.configure(values=values)
        current = self._version_var.get()

        if instances:
            # Mehrere Instanzen: die erste als Auswahl vorschlagen.
            first = instances[0]
            if first.version:
                self._version_var.set(first.version)
                self._selected_version = first.version
            self._on_flavor_selected(first.flavor)
            if len(instances) > 1:
                names = ", ".join(i.label for i in instances)
                self._set_status_text(
                    f"●  {len(instances)} Instanzen: {names}", COLORS.WARNING)
                logger.info("Mehrere Minecraft-Instanzen erkannt: %s", names)
        else:
            if current not in values:
                self._version_var.set(AUTO_VERSION_LABEL)
                self._selected_version = ""
        self._update_availability_hint()

    def _update_availability_hint(self) -> None:
        """Hinweis, ob für Version+Flavor eine vorgefertigte DLL existiert."""
        if not self._selected_version:
            return
        try:
            from builder_runner import prebuilt_available
            ok, hint = prebuilt_available(self._selected_version, self._flavor)
        except Exception:  # noqa: BLE001
            return
        if ok:
            self._set_status_text(
                f"●  {self._selected_version} · {FLAVOR_LABELS.get(self._flavor, self._flavor)}",
                COLORS.SUCCESS)
        else:
            self._set_status_text(hint, COLORS.WARNING)

    def _set_status_text(self, text: str, color: str) -> None:
        try:
            self._status_label.configure(text=text, text_color=color)
        except Exception:  # noqa: BLE001
            pass

    def _build_inject_row(self, parent: ctk.CTkFrame) -> None:
        # Zentrierter Container für Status + Inject-Button
        center = ctk.CTkFrame(parent, fg_color="transparent")
        center.grid(row=4, column=0, sticky="ew", padx=40)

        self._status_label = ctk.CTkLabel(
            parent,
            text="Minecraft nicht erkannt",
            font=(FONTS.FAMILY, FONTS.SMALL_SIZE),
            text_color=COLORS.TEXT_MUTED,
        )
        self._status_label.grid(row=3, column=0, pady=(0, 14))

        self._inject_button = ctk.CTkButton(
            center,
            text="INJECT",
            command=self._on_inject_clicked,
            fg_color=COLORS.INJECT_GREEN,
            hover_color=COLORS.INJECT_GREEN_HOVER,
            text_color=COLORS.TEXT_PRIMARY,
            corner_radius=LAYOUT.CORNER_RADIUS_MEDIUM,
            font=(FONTS.FAMILY_BOLD, FONTS.BUTTON_SIZE, "bold"),
            width=220,
            height=64,
            border_width=2,
            border_color=COLORS.INJECT_GREEN,
        )
        self._inject_button.pack()
        self._inject_button.bind("<Enter>", self._on_inject_hover_enter)
        self._inject_button.bind("<Leave>", self._on_inject_hover_leave)

        self._glow_animation = ButtonPulseGlow(
            self._inject_button,
            base_color=COLORS.INJECT_GREEN,
            glow_color=COLORS.INJECT_GREEN_GLOW,
        )

    def _build_footer(self, parent: ctk.CTkFrame) -> None:
        footer = ctk.CTkFrame(parent, fg_color="transparent")
        footer.grid(row=6, column=0, sticky="ew", pady=(0, 14), padx=20)

        self._version_label = ctk.CTkLabel(
            footer,
            text="v" + self._updater.current_version,
            font=(FONTS.FAMILY, FONTS.SMALL_SIZE),
            text_color=COLORS.TEXT_MUTED,
        )
        self._version_label.pack(side="left")

    # ------------------------------------------------------------------
    # Drag (rahmenloses Fenster verschieben)
    # ------------------------------------------------------------------

    def _on_drag_start(self, _event: tk.Event) -> None:
        try:
            self._drag_offset = (_event.x_root - self.winfo_x(), _event.y_root - self.winfo_y())
        except Exception:  # noqa: BLE001
            pass

    def _on_drag_move(self, _event: tk.Event) -> None:
        try:
            off = getattr(self, "_drag_offset", None)
            if not off:
                return
            x = _event.x_root - off[0]
            y = _event.y_root - off[1]
            self.geometry(f"+{x}+{y}")
        except Exception:  # noqa: BLE001
            pass

    # ------------------------------------------------------------------
    # Eintrittsanimation
    # ------------------------------------------------------------------

    def _start_entry_animation(self) -> None:
        def on_window_visible() -> None:
            if hasattr(self, "_logo_image_loader"):
                LogoScaleIn(
                    self._logo_label,
                    self._logo_image_loader,
                    base_size=(320, 214),
                ).start()
            self.after(260, self._reveal_controls)

        WindowFadeIn(self, duration_ms=550, on_complete=on_window_visible).start()

    def _reveal_controls(self) -> None:
        if self._glow_animation is not None:
            self._glow_animation.start()

    # ------------------------------------------------------------------
    # Hintergrundaufgaben
    # ------------------------------------------------------------------

    def _start_background_tasks(self) -> None:
        self._update_minecraft_status()
        self._poll_console_queue()
        # Versionsliste + laufende Instanzen einscannen
        self._rescan_worker()
        self.after(VERSION_REFRESH_DELAY_MS, self._check_update_startup)

    def _check_update_startup(self) -> None:
        """Hintergrund-Auto-Update-Check (bootstrap bleibt auto-update)."""

        def worker() -> None:
            try:
                info = self._updater.check_for_update()
                if info.update_available:
                    self.after(0, lambda: self._show_update_dialog(info))
                elif info.error:
                    logger.info("Update check skipped: %s", info.error)
            except Exception:  # noqa: BLE001
                logger.exception("Startup update check failed")

        threading.Thread(target=worker, daemon=True, name="UpdateCheck").start()

    def _show_update_dialog(self, info) -> None:
        try:
            from updater import UpdateDialog

            UpdateDialog(parent=self, update_info=info)
        except Exception:  # noqa: BLE001
            logger.exception("Update dialog could not be shown")

    def _update_minecraft_status(self) -> None:
        try:
            status = ProcessDetector.is_minecraft_running()
            if status.running:
                # Nur überschreiben, wenn der Nutzer nichts fester gewählt hat.
                if not self._selected_version:
                    self._status_label.configure(
                        text=f"●  Minecraft gefunden ({status.process_name})",
                        text_color=COLORS.SUCCESS,
                    )
            else:
                self._status_label.configure(
                    text="●  Minecraft nicht erkannt",
                    text_color=COLORS.TEXT_MUTED,
                )
        except Exception:  # noqa: BLE001
            logger.exception("Fehler bei der Minecraft-Statusaktualisierung.")
        finally:
            self.after(PROCESS_CHECK_INTERVAL_MS, self._update_minecraft_status)

    def _poll_console_queue(self) -> None:
        try:
            while True:
                line, tag = self._console_queue.get_nowait()
                self._handle_internal(line, tag)
        except queue.Empty:
            pass
        finally:
            self.after(CONSOLE_POLL_INTERVAL_MS, self._poll_console_queue)

    def _handle_internal(self, text: str, tag: str) -> None:
        if tag == "[INTERNAL_SUCCESS]":
            self._set_status(InjectStatus.SUCCESS)
            return
        if tag == "[INTERNAL_ERROR]":
            self._set_status(InjectStatus.ERROR)
            return
        if tag == "[INTERNAL_FINISH]":
            self._finish_inject()
            return
        logger.info("inject: %s", text)

    def _on_inject_hover_enter(self, _event: tk.Event) -> None:
        if self._is_injecting:
            return
        try:
            self._inject_button.configure(width=232, height=68)
        except Exception:  # noqa: BLE001
            pass

    def _on_inject_hover_leave(self, _event: tk.Event) -> None:
        if self._is_injecting:
            return
        try:
            self._inject_button.configure(width=220, height=64)
        except Exception:  # noqa: BLE001
            pass

    def _on_inject_clicked(self) -> None:
        if self._is_injecting:
            return
        try:
            ButtonClickPulse(
                self._inject_button,
                flash_color=COLORS.ACCENT_GLOW,
                base_color=COLORS.INJECT_GREEN,
            ).start()
        except Exception:  # noqa: BLE001
            pass
        self._start_inject()

    def _set_status(self, status: str) -> None:
        mapping = {
            InjectStatus.READY: ("●  READY", COLORS.SUCCESS),
            InjectStatus.INJECTING: ("●  INJECTING", COLORS.WARNING),
            InjectStatus.SUCCESS: ("●  INJECTED", COLORS.SUCCESS),
            InjectStatus.ERROR: ("●  ERROR", COLORS.ERROR),
        }
        text, color = mapping.get(status, ("●  READY", COLORS.SUCCESS))
        try:
            self._status_label.configure(text=text, text_color=color)
        except Exception:  # noqa: BLE001
            pass

    # ------------------------------------------------------------------
    # Inject-Logik (Threading)
    # ------------------------------------------------------------------

    def _start_inject(self) -> None:
        # Ausgewählte Version und Flavor haben Vorrang; nur wenn nichts
        # gewählt ist, wird automatisch erkannt.
        version = self._selected_version
        flavor = self._flavor
        detected_version: Optional[str] = None
        detected_flavor: Optional[str] = None

        try:
            detected_version, detected_flavor = \
                ProcessDetector.detect_minecraft_profile(
                    version_provider.get_versions())
            if detected_version:
                logger.info("Minecraft erkannt: %s (%s)",
                            detected_version, detected_flavor)
        except Exception:  # noqa: BLE001
            logger.exception("Versions-Auto-Erkennung fehlgeschlagen.")

        if not version:
            version = detected_version or ""
        if not version:
            self._set_status(InjectStatus.ERROR)
            self._set_status_text(
                "Kein Minecraft gefunden — starte es zuerst! oder Version wählen",
                COLORS.ERROR)
            self.after(3000, lambda: self._update_minecraft_status())
            return

        # Warnung, wenn die Wahl nicht zur laufenden Instanz passt — das ist
        # die häufigste Absturzursache (falsche Namensauflösung).
        if detected_version and detected_version != version:
            self._console_queue.put((
                f"WARNUNG: gewählt {version}, laufend {detected_version}",
                StreamType.STDERR))
        if detected_flavor and detected_flavor != flavor:
            self._console_queue.put((
                f"WARNUNG: Flavor gewählt '{flavor}', erkannt '{detected_flavor}'. "
                "Falscher Flavor führt zu einem Absturz im Spiel.",
                StreamType.STDERR))

        self._is_injecting = True
        self._inject_button.configure(state="disabled", text="INJECTING...")
        self._set_status(InjectStatus.INJECTING)
        logger.info("Starte Inject für Minecraft %s (%s) ...", version, flavor)

        self._inject_thread = threading.Thread(
            target=self._run_inject_process,
            args=(version, flavor),
            daemon=True,
        )
        self._inject_thread.start()

    def _run_inject_process(self, version: str, flavor: str = "vanilla") -> None:
        """Baut (oder nutzt Prebuilt-DLL) und injiziert IN-PROCESS."""
        try:
            from builder_runner import run_build

            if not WORKDIR.exists():
                self._console_queue.put(("[INTERNAL_ERROR]", StreamType.INTERNAL))
                return

            def on_output(line: str, stream_type: str) -> None:
                self._console_queue.put((line, stream_type))

            exit_code, error = run_build(
                version=version,
                workdir=WORKDIR,
                output=on_output,
                flavor=flavor,
            )

            if exit_code == 0:
                self._console_queue.put(("[INTERNAL_SUCCESS]", StreamType.INTERNAL))
            else:
                self._console_queue.put(
                    (f"Build/Inject fehlgeschlagen (Exit {exit_code}): {error or 'unbekannter Fehler'}",
                     StreamType.STDERR)
                )
                self._console_queue.put(("[INTERNAL_ERROR]", StreamType.INTERNAL))

        except Exception as error:  # noqa: BLE001
            logger.exception("Unerwarteter Fehler während des Inject-Vorgangs.")
            self._console_queue.put((traceback.format_exc(), StreamType.STDERR))
            self._console_queue.put(("[INTERNAL_ERROR]", StreamType.INTERNAL))
        finally:
            self._console_queue.put(("[INTERNAL_FINISH]", StreamType.INTERNAL))

    def _finish_inject(self) -> None:
        self._is_injecting = False
        try:
            self._inject_button.configure(state="normal", text="INJECT")
        except Exception:  # noqa: BLE001
            pass

    # ------------------------------------------------------------------
    # Fehlerbehandlung & Beenden
    # ------------------------------------------------------------------

    def post_bootstrap_init(self) -> None:
        """Wird nach dem Bootstrap-Fenster aufgerufen (draxo_launcher.py)."""

    def report_fatal_error(self, error: BaseException) -> None:
        logger.error("Fataler Fehler: %s", error)
        logger.error(traceback.format_exc())
        try:
            messagebox.showerror(
                "Draxo Client - Fehler",
                f"Ein unerwarteter Fehler ist aufgetreten:\n\n{error}",
            )
        except Exception:  # noqa: BLE001
            pass

    def _on_close(self) -> None:
        try:
            self._config_manager.update_window_geometry(
                self.winfo_width(), self.winfo_height(),
                self.winfo_x(), self.winfo_y(),
            )
        except Exception:  # noqa: BLE001
            pass
        self.destroy()
