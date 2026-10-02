"""
ui.py
-----
Hauptfenster des Draxo Launchers.

Aufbau ist ein klassisches Desktop-App-Layout:

    ┌──────────────────────────────────────────────────────────┐
    │  Titelleiste  Logo · Name                    – ▢ ✕      │
    ├────────────┬─────────────────────────────────────────────┤
    │  Sidebar   │  Inhaltsbereich (Seiten)                    │
    │            │                                             │
    │  Home      │   Karten, Auswahl, Aktionen                 │
    │  Injizieren│                                             │
    │  Updates   │                                             │
    │  About     │                                             │
    │            │                                             │
    │  [Konto]   │                                             │
    └────────────┴─────────────────────────────────────────────┘

Die Seiten werden gestapelt und bei Bedarf ein-/ausgeblendet — das
verbraucht weniger Speicher als echte Tab-Container und macht den
Zustand trivial nachvollziehbar.

Sämtliche Logik (Versionserkennung, Flavor-Auswahl, Injekt-Thread)
bleibt unverändert erhalten; neu ist ausschließlich die Darstellung.
"""

from __future__ import annotations

import ctypes
import logging
import queue
import threading
import tkinter as tk
import traceback
from datetime import datetime
from tkinter import messagebox
from typing import Optional

import customtkinter as ctk

from animations import ButtonClickPulse, WindowFadeIn
from config import ConfigManager
from discord_auth import DiscordSession
from process_detector import FLAVOR_LABELS, ProcessDetector
from styles import COLORS, FONTS, LAYOUT
from updater import Updater
from utils import center_window, get_workdir, resource_path
from widgets import (
    DISCORD_INVITE,
    ICON_CHECK,
    ICON_CLOSE,
    ICON_DISCORD,
    ICON_GAME,
    ICON_GITHUB,
    ICON_GLOBE,
    ICON_HOME,
    ICON_INFO,
    ICON_INJECT,
    ICON_MAXIMIZE,
    ICON_MINIMIZE,
    ICON_MORE,
    ICON_REFRESH,
    ICON_RESTORE,
    ICON_UPDATE,
    ICON_USER,
    ICON_WARN,
    GITHUB_URL,
    WEBSITE_URL,
    DiscordMark,
    Tooltip,
    card as make_card,
    icon_button,
    open_url,
)
from versions import version_provider


def _short_url(url: str) -> str:
    """``https://discord.gg/abc`` -> ``discord.gg/abc``.

    Wird fuer die Sidebar gebraucht, damit der angezeigte Text immer zum
    echten Link passt und nicht auseinanderlaufen kann.
    """
    return url.split("://", 1)[-1].rstrip("/")

logger = logging.getLogger("DraxoClient.ui")

WORKDIR = get_workdir()

PROCESS_CHECK_INTERVAL_MS = 3000
CONSOLE_POLL_INTERVAL_MS = 40
VERSION_REFRESH_DELAY_MS = 2500

AUTO_VERSION_LABEL = "Automatisch"

# Kurzer Changelog fuer die Uebersicht (neueste Version zuerst):
# (Version, Bereich, Datum, Beschreibung)
CHANGELOG: tuple[tuple[str, str, str, str], ...] = (
    ("1.2.0", "Launcher", "28. Sep 2026",
     "Fabric-Erkennung, Flavor-Auswahl und Auto-Updater repariert."),
    ("1.1.0", "Injektion", "21. Sep 2026",
     "Eigene DLLs fuer Vanilla, Fabric, Forge und NeoForge."),
    ("1.0.2", "Oberflaeche", "14. Sep 2026",
     "Neues Layout mit Sidebar, Karten und dunklem Farbschema."),
    ("1.0.0", "Start", "07. Sep 2026", "Erster oeffentlicher Release."),
)

# Fenstermanipulation für die Titelleiste
SW_MINIMIZE = 6
SW_RESTORE = 9



class StreamType:
    STDOUT = "stdout"
    STDERR = "stderr"
    INTERNAL = "internal"


class InjectStatus:
    READY = "READY"
    INJECTING = "INJECTING"
    SUCCESS = "SUCCESS"
    ERROR = "ERROR"


# ══════════════════════════════════════════════════════════════════
#  Seiten
# ══════════════════════════════════════════════════════════════════

PAGES: tuple[tuple[str, str, str], ...] = (
    ("home", ICON_HOME, "Übersicht"),
    ("inject", ICON_INJECT, "Injizieren"),
    ("updates", ICON_UPDATE, "Updates"),
    ("about", ICON_INFO, "Über"),
)

# Zuordnung der Icon-Leiste in der Titelleiste: None = nur Aktion
# (Link öffnen), sonst die Seite, die beim Klick aufleuchtet.
TOOL_PAGES: tuple[Optional[str], ...] = ("home", "inject", "updates",
                                         None, None, None, "about")


class MainWindow(ctk.CTk):
    """Hauptfenster des Draxo Launchers."""

    def __init__(self, config_manager: Optional["ConfigManager"] = None,
                 session: Optional[object] = None) -> None:
        super().__init__()

        self._config_manager = config_manager or ConfigManager()
        self._updater = Updater()

        # Discord-Anmeldung. Wird vom Launcher durchgereicht; fehlt sie,
        # gilt der Client als nicht angemeldet (Injektion gesperrt).
        self._session = session if session is not None else DiscordSession()

        self._console_queue: "queue.Queue[tuple[str, str]]" = queue.Queue()
        self._inject_thread: Optional[threading.Thread] = None
        self._is_injecting: bool = False

        # Auswahl-Zustand
        self._flavor: str = "vanilla"
        self._selected_version: str = ""
        self._instances: list = []
        self._active_page: str = "home"

        # Widget-Referenzen
        self._page_frames: dict[str, ctk.CTkFrame] = {}
        self._nav_buttons: dict[str, ctk.CTkButton] = {}
        self._flavor_buttons: dict[str, ctk.CTkButton] = {}

        self._configure_window()
        self._apply_windows_effects()
        self._build_ui()
        self._apply_session()
        self._start_background_tasks()
        WindowFadeIn(self, duration_ms=380).start()

        self.protocol("WM_DELETE_WINDOW", self._on_close)

    # ------------------------------------------------------------------
    # Fenster
    # ------------------------------------------------------------------

    def _configure_window(self) -> None:
        ctk.set_appearance_mode("dark")
        ctk.set_default_color_theme("blue")

        self.title("Draxo Client")
        self.configure(fg_color="#010101")

        w, h = LAYOUT.WINDOW_WIDTH, LAYOUT.WINDOW_HEIGHT
        self.geometry(f"{w}x{h}")
        center_window(self, w, h)
        self.minsize(w, h)

        icon_path = resource_path("assets/branding/draxo.ico")
        if icon_path.exists():
            try:
                self.iconbitmap(default=str(icon_path))
            except tk.TclError:
                logger.debug("draxo.ico konnte nicht gesetzt werden.")

    def _apply_windows_effects(self) -> None:
        """Rahmenloses Fenster mit abgerundeten Ecken."""
        try:
            self.overrideredirect(True)
            self.attributes("-transparentcolor", "#010101")
        except Exception:  # noqa: BLE001
            logger.exception("Rahmenloses Fenster nicht möglich.")
            try:
                self.overrideredirect(False)
            except Exception:  # noqa: BLE001
                pass

    # ------------------------------------------------------------------
    # Grundgerüst
    # ------------------------------------------------------------------

    def _build_ui(self) -> None:
        # „Shell" bekommt den Radius — die Ecken bleiben dadurch
        # transparent und das Fenster wirkt abgerundet.
        shell = ctk.CTkFrame(
            self,
            fg_color=COLORS.BACKGROUND,
            corner_radius=LAYOUT.CORNER_RADIUS_WINDOW,
        )
        shell.pack(fill="both", expand=True)

        self._build_titlebar(shell)

        body = ctk.CTkFrame(shell, fg_color="transparent", corner_radius=0)
        body.pack(fill="both", expand=True, padx=0, pady=0)
        body.grid_columnconfigure(1, weight=1)
        body.grid_rowconfigure(0, weight=1)

        self._build_sidebar(body)
        self._build_content(body)
        self._select_page("home")

    # ── Titelleiste ────────────────────────────────────────────────

    def _build_titlebar(self, parent: ctk.CTkFrame) -> None:
        bar = ctk.CTkFrame(
            parent,
            fg_color=COLORS.BACKGROUND,
            height=LAYOUT.TITLEBAR_HEIGHT,
            corner_radius=LAYOUT.CORNER_RADIUS_WINDOW,
        )
        bar.pack(fill="x", padx=1, pady=(1, 0))
        bar.pack_propagate(False)
        # Zwei gleich schwere Spuren links und rechts der Icon-Leiste —
        # dadurch steht die Icon-Leiste exakt in der Fenstermitte.
        bar.grid_columnconfigure(2, weight=1)
        bar.grid_columnconfigure(4, weight=1)

        # Logo-Marke
        mark = ctk.CTkFrame(
            bar, width=26, height=26, corner_radius=7,
            fg_color=COLORS.ACCENT, border_width=0,
        )
        mark.grid(row=0, column=0, padx=(16, 10), pady=11)
        mark.pack_propagate(False)
        ctk.CTkLabel(
            mark, text="D", font=(FONTS.FAMILY_BOLD, 13, "bold"),
            text_color="#FFFFFF",
        ).pack(expand=True)

        ctk.CTkLabel(
            bar, text="Draxo", font=(FONTS.FAMILY_BOLD, FONTS.LOGO_SIZE, "bold"),
            text_color=COLORS.TEXT_PRIMARY, anchor="w",
        ).grid(row=0, column=1, sticky="w")

        # Zentrale Icon-Leiste: Schnellzugriff auf Start, Protokoll,
        # Updates und die externen Ziele. Bewusst nur Symbole — die
        # Navigation selbst steht ausgeschrieben in der Sidebar.
        tools = ctk.CTkFrame(bar, fg_color="transparent", corner_radius=0)
        tools.grid(row=0, column=3, pady=0)
        self._tool_buttons: list[ctk.CTkButton] = []
        self._tool_marks: set[int] = set()
        for col, (glyph, tip, cmd) in enumerate((
            (ICON_HOME, "Übersicht", lambda: self._select_page("home")),
            (ICON_INJECT, "Injizieren", lambda: self._select_page("inject")),
            (ICON_UPDATE, "Updates", lambda: self._select_page("updates")),
            ("", "Discord", lambda: self._open_url(DISCORD_INVITE)),
            (ICON_GITHUB, "GitHub", lambda: self._open_url(GITHUB_URL)),
            (ICON_GLOBE, "Website", lambda: self._open_url(WEBSITE_URL)),
            (ICON_USER, "Konto", lambda: self._select_page("about")),
        )):
            btn = self._icon_button(tools, glyph, tip, cmd)
            btn.grid(row=0, column=col, padx=1)
            self._tool_buttons.append(btn)
            if not glyph:
                DiscordMark(btn, size=17, fg=COLORS.TEXT_MUTED,
                            bg=COLORS.BACKGROUND).place(relx=0.5, rely=0.5,
                                                         anchor="center")
                self._tool_marks.add(id(btn))
        self._highlight_tools("home")

        # Fensterknöpfe
        self._max_button: Optional[ctk.CTkButton] = None
        for col, (glyph, cmd, color) in enumerate((
            (ICON_MINIMIZE, self._on_minimize, COLORS.TEXT_MUTED),
            (ICON_MAXIMIZE, self._on_maximize, COLORS.TEXT_MUTED),
            (ICON_CLOSE, self._on_close, COLORS.TEXT_MUTED),
        ), start=5):
            btn = ctk.CTkButton(
                bar, text=glyph, command=cmd, width=44, height=LAYOUT.TITLEBAR_HEIGHT,
                corner_radius=0, fg_color="transparent",
                hover_color=COLORS.HOVER_OVERLAY if col < 7 else COLORS.INJECT_RED,
                text_color=color, font=(FONTS.ICON, 10),
            )
            btn.grid(row=0, column=col, sticky="nse", padx=0, pady=0)
            if col == 6:
                self._max_button = btn

        # Ziehbereich
        for widget in (bar, mark, tools):
            widget.bind("<Button-1>", self._on_drag_start)
            widget.bind("<B1-Motion>", self._on_drag_move)

    # ── Kleine Helfer ─────────────────────────────────────────────

    def _icon_button(self, parent: ctk.CTkFrame, glyph: str, tip: str,
                     command) -> ctk.CTkButton:
        """Quadratischer Icon-Knopf — Kurzform auf den Baustein."""
        return icon_button(parent, glyph, tip, command)

    def _highlight_tools(self, key: str) -> None:
        """Hinterlegt den aktiven Icon-Knopf in der Titelleiste."""
        for btn, page in zip(self._tool_buttons, TOOL_PAGES):
            active = page == key
            if id(btn) in getattr(self, "_tool_marks", ()):
                # Beim Discord-Knopf sitzt eine Grafik statt eines
                # Zeichens — nur die Fläche kann umgefärbt werden.
                btn.configure(fg_color=COLORS.ACCENT_SOFT if active else "transparent")
            else:
                btn.configure(
                    fg_color=COLORS.ACCENT_SOFT if active else "transparent",
                    text_color=COLORS.TEXT_PRIMARY if active else COLORS.TEXT_MUTED,
                )

    def _open_url(self, url: str) -> None:
        open_url(url)

    def _on_minimize(self) -> None:
        try:
            ctypes.windll.user32.ShowWindow(
                ctypes.windll.user32.GetParent(self.winfo_id()) or self.winfo_id(),
                SW_MINIMIZE)
        except Exception:  # noqa: BLE001
            try:
                self.withdraw()
            except Exception:  # noqa: BLE001
                pass

    def _on_maximize(self) -> None:
        self._is_maximized = not getattr(self, "_is_maximized", False)
        if self._is_maximized:
            self.geometry(f"{self.winfo_screenwidth()}x{self.winfo_screenheight()}+0+0")
            if self._max_button is not None:
                self._max_button.configure(text=ICON_RESTORE)
        else:
            w, h = LAYOUT.WINDOW_WIDTH, LAYOUT.WINDOW_HEIGHT
            self.geometry(f"{w}x{h}")
            center_window(self, w, h)
            if self._max_button is not None:
                self._max_button.configure(text=ICON_MAXIMIZE)

    # ── Sidebar ────────────────────────────────────────────────────

    def _build_sidebar(self, parent: ctk.CTkFrame) -> None:
        side = ctk.CTkFrame(
            parent, fg_color=COLORS.SIDEBAR, width=LAYOUT.SIDEBAR_WIDTH,
            corner_radius=LAYOUT.CORNER_RADIUS_WINDOW,
        )
        side.grid(row=0, column=0, sticky="nsw")
        side.grid_propagate(False)
        side.grid_rowconfigure(1, weight=1)   # Navigation dehnt sich
        side.grid_columnconfigure(0, weight=1)

        # Seitenüberschrift
        self._sidebar_title = ctk.CTkLabel(
            side, text="Übersicht", font=(FONTS.FAMILY_BOLD, FONTS.DISPLAY_SIZE, "bold"),
            text_color=COLORS.TEXT_PRIMARY, anchor="w", justify="left",
        )
        self._sidebar_title.grid(row=0, column=0, sticky="w",
                                 padx=20, pady=(26, 20))

        nav = ctk.CTkFrame(side, fg_color="transparent", corner_radius=0)
        nav.grid(row=1, column=0, sticky="new", padx=10)
        nav.grid_columnconfigure(0, weight=1)

        for row, (key, icon, label) in enumerate(PAGES):
            btn = ctk.CTkButton(
                nav,
                text=f"  {icon}   {label}",
                command=lambda k=key: self._select_page(k),
                anchor="w",
                height=LAYOUT.NAV_HEIGHT,
                corner_radius=LAYOUT.RADIUS_SMALL,
                fg_color="transparent",
                hover_color=COLORS.SURFACE_HOVER,
                text_color=COLORS.TEXT_SECONDARY,
                font=(FONTS.FAMILY, FONTS.BODY_SIZE),
            )
            btn.grid(row=row, column=0, sticky="ew", pady=2)
            self._nav_buttons[key] = btn

        # Konto-Block am Fuß
        self._build_account_block(side)

    def _link_row(self, parent: ctk.CTkFrame, row: int, icon: str, title: str,
                  subtitle: str, url: str) -> None:
        """Kompakter Link-Eintrag (Icon + Titel + Adresse) wie in den Referenzen."""
        holder = ctk.CTkFrame(parent, fg_color="transparent", corner_radius=0)
        holder.grid(row=row, column=0, sticky="ew", pady=1)
        holder.grid_columnconfigure(1, weight=1)

        badge = ctk.CTkFrame(holder, width=30, height=30, corner_radius=8,
                             fg_color=COLORS.SURFACE, border_width=1,
                             border_color=COLORS.BORDER)
        badge.grid(row=0, column=0, padx=(4, 10), pady=4)
        badge.pack_propagate(False)
        if icon == ICON_DISCORD:
            DiscordMark(badge, size=17, fg=COLORS.TEXT_SECONDARY,
                        bg=COLORS.SURFACE).pack(expand=True)
        else:
            ctk.CTkLabel(badge, text=icon, font=(FONTS.ICON, 13),
                         text_color=COLORS.TEXT_SECONDARY, height=30).pack(expand=True)

        texts = ctk.CTkFrame(holder, fg_color="transparent", corner_radius=0)
        texts.grid(row=0, column=1, sticky="w")
        # Feste, kleine Höhen: CTkLabel ist sonst 28 px hoch und
        # reißt Title und Untertitel auseinander.
        ctk.CTkLabel(
            texts, text=title, height=16,
            font=(FONTS.FAMILY_BOLD, FONTS.SMALL_SIZE, "bold"),
            text_color=COLORS.TEXT_SECONDARY, anchor="w",
        ).pack(anchor="w")
        ctk.CTkLabel(
            texts, text=subtitle, height=14,
            font=(FONTS.FAMILY, FONTS.TINY_SIZE),
            text_color=COLORS.TEXT_DIM, anchor="w",
        ).pack(anchor="w")

        Tooltip(holder, url)
        for widget in (holder, badge, texts):
            widget.bind("<Button-1>", lambda _e, u=url: self._open_url(u))
            widget.configure(cursor="hand2")

    def _build_account_block(self, parent: ctk.CTkFrame) -> None:
        wrapper = ctk.CTkFrame(parent, fg_color="transparent", corner_radius=0)
        wrapper.grid(row=2, column=0, sticky="sew", padx=10, pady=12)
        wrapper.grid_columnconfigure(0, weight=1)

        self._link_row(wrapper, 0, ICON_DISCORD, "Discord",
                       _short_url(DISCORD_INVITE), DISCORD_INVITE)
        self._link_row(wrapper, 1, ICON_GLOBE, "Website",
                       _short_url(WEBSITE_URL), WEBSITE_URL)

        ctk.CTkFrame(wrapper, height=1, fg_color=COLORS.BORDER).grid(
            row=2, column=0, sticky="ew", pady=(10, 10))

        row = ctk.CTkFrame(wrapper, fg_color="transparent", corner_radius=0)
        row.grid(row=3, column=0, sticky="ew")
        row.grid_columnconfigure(1, weight=1)

        avatar = ctk.CTkFrame(row, width=34, height=34, corner_radius=9,
                              fg_color=COLORS.ACCENT_SOFT,
                              border_width=1, border_color=COLORS.BORDER)
        avatar.grid(row=0, column=0, padx=(4, 10), pady=2)
        avatar.pack_propagate(False)
        self._avatar_icon = ctk.CTkLabel(
            avatar, text=ICON_USER, font=(FONTS.ICON, 14),
            text_color=COLORS.TEXT_MUTED,
        )
        self._avatar_icon.pack(expand=True)

        texts = ctk.CTkFrame(row, fg_color="transparent", corner_radius=0)
        texts.grid(row=0, column=1, sticky="w")
        self._account_name = ctk.CTkLabel(
            texts, text="Nicht angemeldet", height=16,
            font=(FONTS.FAMILY_BOLD, 12, "bold"),
            text_color=COLORS.TEXT_SECONDARY, anchor="w",
        )
        self._account_name.pack(anchor="w")
        self._account_sub = ctk.CTkLabel(
            texts, text="Kein Konto verknüpft", height=14,
            font=(FONTS.FAMILY, FONTS.TINY_SIZE),
            text_color=COLORS.TEXT_DIM, anchor="w",
        )
        self._account_sub.pack(anchor="w")

        # Abmelden erscheint nur, wenn wirklich jemand angemeldet ist.
        self._logout_button = ctk.CTkButton(
            row, text=ICON_MORE, command=self._on_logout_clicked,
            width=26, height=26, corner_radius=LAYOUT.RADIUS_SMALL,
            fg_color="transparent", hover_color=COLORS.SURFACE_HOVER,
            text_color=COLORS.TEXT_DIM, font=(FONTS.ICON, 12),
        )
        self._logout_button.grid(row=0, column=2, sticky="e", padx=(6, 2))
        Tooltip(self._logout_button, "Abmelden")

        # Der ganze Block ist anklickbar: angemeldet → Konto-Seite,
        # nicht angemeldet → Anmeldefenster. Sonst waere der gesperrte
        # Inject-Knopf ohne Ausweg.
        for widget in (row, avatar, texts):
            widget.bind("<Button-1>", self._on_account_clicked)
            widget.configure(cursor="hand2")
        self._account_tooltip = Tooltip(texts, "Anmelden")

    # ══════════════════════════════════════════════════════════════
    #  Konto
    # ══════════════════════════════════════════════════════════════

    @property
    def _signed_in(self) -> bool:
        return bool(getattr(self._session, "signed_in", False))

    def _apply_session(self) -> None:
        """Sidebar-Konto und Knopf-Zustände an die Sitzung angleichen."""
        user = getattr(self._session, "user", None)
        try:
            if user is not None:
                self._account_name.configure(text=user.display_name)
                self._account_sub.configure(
                    text=user.handle or "Angemeldet",
                    text_color=COLORS.SUCCESS)
                self._avatar_icon.configure(text=user.initials,
                                            text_color=COLORS.TEXT_PRIMARY)
                self._load_avatar(user)
                self._logout_button.grid()
            else:
                self._account_name.configure(text="Nicht angemeldet")
                self._account_sub.configure(text="Injektion gesperrt",
                                            text_color=COLORS.WARNING)
                self._avatar_icon.configure(text=ICON_USER,
                                            text_color=COLORS.TEXT_MUTED)
                self._logout_button.grid_remove()
        except Exception:  # noqa: BLE001
            logger.debug("Kontoanzeige konnte nicht aktualisiert werden.",
                         exc_info=True)
        self._update_auth_gate()

    def _load_avatar(self, user) -> None:
        """Lädt das Discord-Avatarbild in den Kreis neben dem Namen."""
        try:
            from auth_ui import load_avatar
        except Exception:  # noqa: BLE001
            return
        if not user.avatar_url:
            return

        def worker() -> None:
            image = load_avatar(user.avatar_url, size=64)
            if image is None:
                return
            try:
                photo = ctk.CTkImage(light_image=image, dark_image=image,
                                     size=(34, 34))
                self.after(0, lambda: self._show_avatar(photo))
            except Exception:  # noqa: BLE001
                logger.debug("Avatar konnte nicht gesetzt werden.", exc_info=True)

        threading.Thread(target=worker, daemon=True, name="Avatar").start()

    def _show_avatar(self, photo) -> None:
        try:
            self._avatar_icon.configure(image=photo, text="")
        except Exception:  # noqa: BLE001
            pass

    def _update_auth_gate(self) -> None:
        """Sperrt die Injektion, solange niemand angemeldet ist."""
        allowed = self._signed_in
        for button in (self._inject_button, self._home_inject):
            try:
                if self._is_injecting:
                    continue
                button.configure(state="normal" if allowed else "disabled")
            except Exception:  # noqa: BLE001
                pass
        try:
            if not allowed:
                self._status_label.configure(
                    text="Anmeldung erforderlich — Injektion ist gesperrt.",
                    text_color=COLORS.WARNING)
        except Exception:  # noqa: BLE001
            pass

    def _on_account_clicked(self, _event: Optional[tk.Event] = None) -> None:
        if self._signed_in:
            self._select_page("about")
        else:
            self._request_login()

    def _on_logout_clicked(self) -> None:
        try:
            self._session.logout()
        except Exception:  # noqa: BLE001
            logger.exception("Abmeldung fehlgeschlagen.")
            return
        self._apply_session()
        self._append_log("Abgemeldet.", "warning")
        self._select_page("home")

    def _request_login(self) -> None:
        """Öffnet das Anmeldefenster erneut (Knopf im gesperrten Zustand)."""
        from auth_ui import WelcomeWindow

        def done(_result) -> None:
            self._apply_session()
            if self._signed_in:
                self._append_log(f"Angemeldet als {self._session.display_name}.",
                                 "success")

        try:
            WelcomeWindow(parent=self, session=self._session,
                          on_success=done)
        except Exception:  # noqa: BLE001
            logger.exception("Anmeldefenster konnte nicht geöffnet werden.")
            messagebox.showerror(
                "Draxo Client",
                "Das Anmeldefenster konnte nicht geöffnet werden.")

    # ── Inhaltsbereich ─────────────────────────────────────────────

    def _build_content(self, parent: ctk.CTkFrame) -> None:
        content = ctk.CTkFrame(parent, fg_color="transparent", corner_radius=0)
        content.grid(row=0, column=1, sticky="nsew", padx=(0, 1))
        content.grid_rowconfigure(0, weight=1)
        content.grid_columnconfigure(0, weight=1)

        for key, _icon, label in PAGES:
            page = ctk.CTkFrame(content, fg_color="transparent", corner_radius=0)
            page.grid(row=0, column=0, sticky="nsew")
            self._page_frames[key] = page

        self._build_page_home(self._page_frames["home"])
        self._build_page_inject(self._page_frames["inject"])
        self._build_page_updates(self._page_frames["updates"])
        self._build_page_about(self._page_frames["about"])

    def _select_page(self, key: str) -> None:
        """Seite anzeigen und Navigation aktualisieren."""
        self._active_page = key
        for name, frame in self._page_frames.items():
            if name == key:
                # grid() ohne Argumente stellt die zuletzt gespeicherten
                # Grid-Optionen wieder her. grid_remove() allein reicht
                # nicht — ein ausgeblendetes Fenster bleibt sonst leer.
                frame.grid()
                frame.tkraise()
            else:
                frame.grid_remove()

        for name, btn in self._nav_buttons.items():
            active = name == key
            btn.configure(
                fg_color=COLORS.ACCENT_SOFT if active else "transparent",
                text_color=COLORS.TEXT_PRIMARY if active else COLORS.TEXT_SECONDARY,
            )

        title = next((lbl for k, _i, lbl in PAGES if k == key), "")
        self._sidebar_title.configure(text=title)
        self._highlight_tools(key)

        # Die Update-Seite lädt ihre Daten beim ersten Besuch selbst —
        # sonst steht dort dauerhaft ein leerer Zustand.
        if key == "updates" and not getattr(self, "_updates_loaded", True):
            self._updates_loaded = True
            self.after(150, self._on_manual_update_check)

    # ══════════════════════════════════════════════════════════════
    #  Bausteine
    # ══════════════════════════════════════════════════════════════

    def _card(self, parent: ctk.CTkFrame, **kwargs) -> ctk.CTkFrame:
        return make_card(parent, **kwargs)

    @staticmethod
    def _field(parent: ctk.CTkFrame, label: str, value: str,
               value_color: str = COLORS.TEXT_PRIMARY,
               row: int = 0, column: int = 0,
               label_font: Optional[tuple] = None) -> ctk.CTkLabel:
        """Beschriftetes Wert-Feld wie in den Referenz-Apps."""
        ctk.CTkLabel(
            parent, text=label,
            font=label_font or (FONTS.FAMILY, FONTS.SMALL_SIZE),
            text_color=COLORS.TEXT_MUTED, anchor="w",
        ).grid(row=row, column=column, sticky="w", pady=(0, 3))
        lbl = ctk.CTkLabel(
            parent, text=value,
            font=(FONTS.FAMILY_BOLD, FONTS.HEADING_SIZE, "bold"),
            text_color=value_color, anchor="w",
        )
        lbl.grid(row=row + 1, column=column, sticky="w")
        return lbl

    # ══════════════════════════════════════════════════════════════
    #  Seite: Übersicht
    # ══════════════════════════════════════════════════════════════

    def _build_page_home(self, page: ctk.CTkFrame) -> None:
        pad = LAYOUT.CARD_PADDING + 4

        header = ctk.CTkFrame(page, fg_color="transparent", corner_radius=0)
        header.pack(fill="x", padx=pad, pady=(30, 0))
        hour = datetime.now().hour
        greeting = ("Guten Morgen" if hour < 11 else
                    "Guten Tag" if hour < 18 else "Guten Abend")
        ctk.CTkLabel(
            header, text=f"{greeting} 👋",
            font=(FONTS.FAMILY_BOLD, 22, "bold"),
            text_color=COLORS.TEXT_PRIMARY, anchor="w",
        ).pack(anchor="w")
        ctk.CTkLabel(
            header, text="Hier ist der Status deines Clients.",
            font=(FONTS.FAMILY, FONTS.BODY_SIZE),
            text_color=COLORS.TEXT_MUTED, anchor="w",
        ).pack(anchor="w", pady=(2, 0))

        grid = ctk.CTkFrame(page, fg_color="transparent", corner_radius=0)
        grid.pack(fill="both", expand=True, padx=pad, pady=(20, pad))
        grid.grid_columnconfigure((0, 1), weight=1, uniform="cards")
        grid.grid_rowconfigure(1, weight=1)

        # ── Karte: Minecraft ──
        mc = self._card(grid)
        mc.grid(row=0, column=0, sticky="new", padx=(0, 7), pady=(0, 7))
        mc.grid_columnconfigure(0, weight=1)
        top = ctk.CTkFrame(mc, fg_color="transparent", corner_radius=0)
        top.grid(row=0, column=0, sticky="ew", padx=20, pady=(18, 0))
        top.grid_columnconfigure(1, weight=1)
        ctk.CTkLabel(top, text="Minecraft", font=(FONTS.FAMILY, FONTS.SMALL_SIZE),
                     text_color=COLORS.TEXT_MUTED, anchor="w").grid(row=0, column=0, sticky="w")
        ctk.CTkLabel(top, text=ICON_GAME, font=(FONTS.ICON, 15),
                     text_color=COLORS.TEXT_DIM).grid(row=0, column=1, sticky="e")

        body = ctk.CTkFrame(mc, fg_color="transparent", corner_radius=0)
        body.grid(row=1, column=0, sticky="ew", padx=20, pady=(10, 20))
        body.grid_columnconfigure((0, 1), weight=1, uniform="f")
        self._home_status = self._field(body, "Status", "Nicht erkannt", COLORS.TEXT_SECONDARY, 0, 0)
        self._home_pid = self._field(body, "Prozess", "—", COLORS.TEXT_SECONDARY, 0, 1)

        # ── Karte: Ziel ──
        tgt = self._card(grid)
        tgt.grid(row=0, column=1, sticky="new", padx=(7, 0), pady=(0, 7))
        tgt.grid_columnconfigure(0, weight=1)
        top2 = ctk.CTkFrame(tgt, fg_color="transparent", corner_radius=0)
        top2.grid(row=0, column=0, sticky="ew", padx=20, pady=(18, 0))
        top2.grid_columnconfigure(1, weight=1)
        ctk.CTkLabel(top2, text="Injektionsziel", font=(FONTS.FAMILY, FONTS.SMALL_SIZE),
                     text_color=COLORS.TEXT_MUTED, anchor="w").grid(row=0, column=0, sticky="w")
        self._home_flavor_icon = ctk.CTkLabel(
            top2, text=ICON_INJECT, font=(FONTS.ICON, 15), text_color=COLORS.ACCENT)
        self._home_flavor_icon.grid(row=0, column=1, sticky="e")

        body2 = ctk.CTkFrame(tgt, fg_color="transparent", corner_radius=0)
        body2.grid(row=1, column=0, sticky="ew", padx=20, pady=(10, 20))
        body2.grid_columnconfigure((0, 1), weight=1, uniform="f")
        self._home_version = self._field(body2, "Version", "—", COLORS.TEXT_SECONDARY, 0, 0)
        self._home_flavor = self._field(body2, "Loader", "Vanilla", COLORS.TEXT_SECONDARY, 0, 1)

        # ── Karte: Was ist neu ──
        news = self._card(grid)
        news.grid(row=1, column=0, columnspan=2, sticky="nsew")
        news.grid_columnconfigure(0, weight=1)

        news_head = ctk.CTkFrame(news, fg_color="transparent", corner_radius=0)
        news_head.grid(row=0, column=0, sticky="ew", padx=20, pady=(16, 6))
        news_head.grid_columnconfigure(0, weight=1)
        ctk.CTkLabel(
            news_head, text="Was ist neu",
            font=(FONTS.FAMILY_BOLD, FONTS.HEADING_SIZE, "bold"),
            text_color=COLORS.TEXT_PRIMARY, anchor="w",
        ).grid(row=0, column=0, sticky="w")
        ctk.CTkButton(
            news_head, text="Alle Updates  ›",
            command=lambda: self._select_page("updates"),
            width=120, height=26, corner_radius=LAYOUT.RADIUS_SMALL,
            fg_color="transparent", hover_color=COLORS.SURFACE_HOVER,
            text_color=COLORS.TEXT_MUTED, font=(FONTS.FAMILY, FONTS.TINY_SIZE),
        ).grid(row=0, column=1, sticky="e")

        for i, (ver, area, date, text) in enumerate(CHANGELOG):
            entry = ctk.CTkFrame(news, fg_color="transparent", corner_radius=0)
            entry.grid(row=i + 1, column=0, sticky="ew", padx=20, pady=(0, 4 if i else 2))
            entry.grid_columnconfigure(2, weight=1)

            chip = ctk.CTkLabel(
                entry, text=f"v{ver}", height=18,
                font=(FONTS.FAMILY_BOLD, FONTS.TINY_SIZE, "bold"),
                text_color=COLORS.ACCENT, anchor="w",
            )
            chip.grid(row=0, column=0, sticky="w", padx=(0, 12))
            Tooltip(chip, area)

            ctk.CTkLabel(
                entry, text=text, height=18,
                font=(FONTS.FAMILY, FONTS.SMALL_SIZE),
                text_color=COLORS.TEXT_SECONDARY, anchor="w",
            ).grid(row=0, column=2, sticky="w")

            ctk.CTkLabel(
                entry, text=date, height=18,
                font=(FONTS.FAMILY, FONTS.TINY_SIZE),
                text_color=COLORS.TEXT_DIM, anchor="e",
            ).grid(row=0, column=3, sticky="e", padx=(12, 0))

        ctk.CTkFrame(news, height=16, fg_color="transparent").grid(
            row=len(CHANGELOG) + 1, column=0)

        # ── Aktionsleiste ──
        action = self._card(page)
        action.pack(fill="x", padx=pad, pady=(0, pad))
        action.grid_columnconfigure(0, weight=1)

        line = ctk.CTkFrame(action, fg_color="transparent", corner_radius=0)
        line.grid(row=0, column=0, sticky="ew", padx=20, pady=(18, 0))
        line.grid_columnconfigure(0, weight=1)
        ctk.CTkLabel(line, text="Bereit?", font=(FONTS.FAMILY_BOLD, FONTS.HEADING_SIZE, "bold"),
                     text_color=COLORS.TEXT_PRIMARY, anchor="w").grid(row=0, column=0, sticky="w")
        self._home_status_detail = ctk.CTkLabel(
            line, text="Starte Minecraft und wähle oben ein Ziel.",
            font=(FONTS.FAMILY, FONTS.SMALL_SIZE),
            text_color=COLORS.TEXT_MUTED, anchor="w",
        )
        self._home_status_detail.grid(row=1, column=0, sticky="w", pady=(3, 0))

        self._home_inject = ctk.CTkButton(
            line, text=f"  {ICON_INJECT}   Jetzt injizieren",
            command=self._on_inject_clicked,
            width=210, height=LAYOUT.BUTTON_HEIGHT_LG,
            corner_radius=LAYOUT.RADIUS_MEDIUM,
            fg_color=COLORS.INJECT_GREEN, hover_color=COLORS.INJECT_GREEN_HOVER,
            text_color="#FFFFFF", font=(FONTS.FAMILY_BOLD, FONTS.BUTTON_SIZE, "bold"),
        )
        self._home_inject.grid(row=0, column=1, rowspan=2, sticky="e")

        hint = ctk.CTkFrame(action, fg_color="transparent", corner_radius=0)
        hint.grid(row=1, column=0, sticky="ew", padx=20, pady=(14, 20))
        hint.grid_columnconfigure(0, weight=1)
        self._home_hint = ctk.CTkLabel(
            hint, text="Tipp: Wird eine laufende Instanz automatisch erkannt? "
                       "Dann genügt ein Klick.",
            font=(FONTS.FAMILY, FONTS.TINY_SIZE), text_color=COLORS.TEXT_DIM,
            anchor="w", wraplength=520, justify="left",
        )
        self._home_hint.grid(row=0, column=0, sticky="w")

    # ══════════════════════════════════════════════════════════════
    #  Seite: Injizieren
    # ══════════════════════════════════════════════════════════════

    def _build_page_inject(self, page: ctk.CTkFrame) -> None:
        pad = LAYOUT.CARD_PADDING + 4
        page.grid_columnconfigure(0, weight=1)
        page.grid_rowconfigure(2, weight=1)

        ctk.CTkLabel(
            page, text="Injektion", font=(FONTS.FAMILY_BOLD, 20, "bold"),
            text_color=COLORS.TEXT_PRIMARY, anchor="w",
        ).grid(row=0, column=0, sticky="w", padx=pad, pady=(30, 0))
        ctk.CTkLabel(
            page, text="Version und Loader wählen, dann injizieren.",
            font=(FONTS.FAMILY, FONTS.BODY_SIZE),
            text_color=COLORS.TEXT_MUTED, anchor="w",
        ).grid(row=0, column=0, sticky="w", padx=pad, pady=(34, 0))

        # ── Auswahl-Karte ──
        sel = self._card(page)
        sel.grid(row=1, column=0, sticky="ew", padx=pad, pady=(18, 12))
        sel.grid_columnconfigure(0, weight=1)

        grid = ctk.CTkFrame(sel, fg_color="transparent", corner_radius=0)
        grid.grid(row=0, column=0, sticky="ew", padx=20, pady=20)
        grid.grid_columnconfigure(1, weight=1)

        ctk.CTkLabel(
            grid, text="Minecraft-Version", font=(FONTS.FAMILY, FONTS.SMALL_SIZE),
            text_color=COLORS.TEXT_MUTED, anchor="w",
        ).grid(row=0, column=0, sticky="w", padx=(0, 16))

        self._version_var = tk.StringVar(value=AUTO_VERSION_LABEL)
        self._version_box = ctk.CTkOptionMenu(
            grid, variable=self._version_var, values=[AUTO_VERSION_LABEL],
            command=self._on_version_selected,
            width=200, height=LAYOUT.BUTTON_HEIGHT,
            corner_radius=LAYOUT.RADIUS_SMALL,
            fg_color=COLORS.SURFACE_LIGHT, button_color=COLORS.SURFACE_HOVER,
            button_hover_color=COLORS.BORDER, text_color=COLORS.TEXT_PRIMARY,
            dropdown_fg_color=COLORS.SURFACE_LIGHT,
            dropdown_text_color=COLORS.TEXT_PRIMARY,
            dropdown_hover_color=COLORS.SURFACE_HOVER,
            font=(FONTS.FAMILY, FONTS.BODY_SIZE),
            dropdown_font=(FONTS.FAMILY, FONTS.BODY_SIZE),
        )
        self._version_box.grid(row=0, column=1, sticky="w")

        ctk.CTkButton(
            grid, text=ICON_REFRESH, command=self._on_rescan_clicked,
            width=LAYOUT.BUTTON_HEIGHT, height=LAYOUT.BUTTON_HEIGHT,
            corner_radius=LAYOUT.RADIUS_SMALL,
            fg_color=COLORS.SURFACE_LIGHT, hover_color=COLORS.SURFACE_HOVER,
            text_color=COLORS.TEXT_SECONDARY, font=(FONTS.ICON, 13),
        ).grid(row=0, column=2, padx=(8, 0))

        # Loader-Auswahl
        ctk.CTkLabel(
            grid, text="Loader", font=(FONTS.FAMILY, FONTS.SMALL_SIZE),
            text_color=COLORS.TEXT_MUTED, anchor="w",
        ).grid(row=1, column=0, sticky="w", padx=(0, 16), pady=(16, 0))

        flavors = ctk.CTkFrame(grid, fg_color="transparent", corner_radius=0)
        flavors.grid(row=1, column=1, columnspan=2, sticky="w", pady=(16, 0))
        for i, (key, label) in enumerate(FLAVOR_LABELS.items()):
            btn = ctk.CTkButton(
                flavors, text=label,
                command=lambda k=key: self._on_flavor_selected(k),
                width=104, height=34, corner_radius=LAYOUT.RADIUS_SMALL,
                fg_color=COLORS.SURFACE_LIGHT, hover_color=COLORS.SURFACE_HOVER,
                text_color=COLORS.TEXT_MUTED,
                font=(FONTS.FAMILY, FONTS.SMALL_SIZE, "bold"),
                border_width=1, border_color=COLORS.BORDER,
            )
            btn.grid(row=0, column=i, padx=(0, 6))
            self._flavor_buttons[key] = btn
        self._highlight_flavor("vanilla")

        # Status + Button
        act = ctk.CTkFrame(sel, fg_color="transparent", corner_radius=0)
        act.grid(row=1, column=0, sticky="ew", padx=20, pady=(0, 20))
        act.grid_columnconfigure(0, weight=1)

        self._status_label = ctk.CTkLabel(
            act, text="●  Bereit",
            font=(FONTS.FAMILY, FONTS.BODY_SIZE), text_color=COLORS.TEXT_MUTED,
            anchor="w", wraplength=520, justify="left",
        )
        self._status_label.grid(row=0, column=0, sticky="w", padx=(0, 16))

        self._inject_button = ctk.CTkButton(
            act, text=f"  {ICON_INJECT}   INJECT",
            command=self._on_inject_clicked,
            width=200, height=LAYOUT.BUTTON_HEIGHT_LG,
            corner_radius=LAYOUT.RADIUS_MEDIUM,
            fg_color=COLORS.INJECT_GREEN, hover_color=COLORS.INJECT_GREEN_HOVER,
            text_color="#FFFFFF",
            font=(FONTS.FAMILY_BOLD, FONTS.BUTTON_SIZE + 1, "bold"),
        )
        self._inject_button.grid(row=0, column=1, sticky="e")

        # ── Protokoll ──
        log = self._card(page)
        log.grid(row=2, column=0, sticky="nsew", padx=pad, pady=(0, pad))
        log.grid_columnconfigure(0, weight=1)
        log.grid_rowconfigure(1, weight=1)

        head = ctk.CTkFrame(log, fg_color="transparent", corner_radius=0)
        head.grid(row=0, column=0, sticky="ew", padx=20, pady=(16, 10))
        head.grid_columnconfigure(1, weight=1)
        ctk.CTkLabel(head, text="Protokoll", font=(FONTS.FAMILY_BOLD, FONTS.HEADING_SIZE, "bold"),
                     text_color=COLORS.TEXT_PRIMARY, anchor="w").grid(row=0, column=0, sticky="w")
        ctk.CTkButton(
            head, text="Leeren", command=self._clear_log,
            width=64, height=26, corner_radius=LAYOUT.RADIUS_SMALL,
            fg_color="transparent", hover_color=COLORS.SURFACE_HOVER,
            text_color=COLORS.TEXT_MUTED, font=(FONTS.FAMILY, FONTS.TINY_SIZE),
        ).grid(row=0, column=2, sticky="e")

        self._log_box = ctk.CTkTextbox(
            log, height=200, fg_color=COLORS.BACKGROUND,
            text_color=COLORS.TEXT_SECONDARY,
            corner_radius=LAYOUT.RADIUS_SMALL,
            font=(FONTS.MONO, FONTS.TINY_SIZE + 1), wrap="word",
            state="disabled", border_width=1, border_color=COLORS.BORDER_SOFT,
        )
        self._log_box.grid(row=1, column=0, sticky="nsew", padx=20, pady=(0, 20))
        for tag, color in (("error", COLORS.ERROR), ("warning", COLORS.WARNING),
                           ("success", COLORS.SUCCESS), ("info", COLORS.TEXT_SECONDARY),
                           ("internal", COLORS.ACCENT)):
            self._log_box.tag_config(tag, foreground=color)
        self._append_log("Bereit. Wähle ein Ziel und klicke auf INJECT.", "info")

    # ══════════════════════════════════════════════════════════════
    #  Seite: Updates
    # ══════════════════════════════════════════════════════════════

    def _build_page_updates(self, page: ctk.CTkFrame) -> None:
        pad = LAYOUT.CARD_PADDING + 4
        page.grid_columnconfigure(0, weight=1)
        page.grid_rowconfigure(2, weight=1)

        head = ctk.CTkFrame(page, fg_color="transparent", corner_radius=0)
        head.grid(row=0, column=0, sticky="ew", padx=pad, pady=(30, 0))
        head.grid_columnconfigure(0, weight=1)
        ctk.CTkLabel(
            head, text="Updates", font=(FONTS.FAMILY_BOLD, 20, "bold"),
            text_color=COLORS.TEXT_PRIMARY, anchor="w",
        ).grid(row=0, column=0, sticky="w")
        ctk.CTkLabel(
            head, text="Versionen und Änderungen.", font=(FONTS.FAMILY, FONTS.BODY_SIZE),
            text_color=COLORS.TEXT_MUTED, anchor="w",
        ).grid(row=1, column=0, sticky="w", pady=(2, 0))

        # Der Knopf spannt beide Kopfzeilen — dadurch steht er exakt
        # auf der Mitte und nicht bündig an der Oberkante.
        self._update_check_button = ctk.CTkButton(
            head, text=f"{ICON_REFRESH}   Auf Updates prüfen",
            command=self._on_manual_update_check,
            width=180, height=34, corner_radius=LAYOUT.RADIUS_SMALL,
            fg_color=COLORS.SURFACE, hover_color=COLORS.SURFACE_HOVER,
            text_color=COLORS.TEXT_SECONDARY,
            font=(FONTS.FAMILY, FONTS.SMALL_SIZE),
        )
        self._update_check_button.grid(row=0, column=1, rowspan=2, sticky="e")

        self._updates_box = ctk.CTkScrollableFrame(
            page, fg_color="transparent", corner_radius=0,
            scrollbar_button_color=COLORS.SURFACE_HOVER,
            scrollbar_button_hover_color=COLORS.BORDER,
        )
        self._updates_box.grid(row=2, column=0, sticky="nsew", padx=pad, pady=(16, pad))
        self._updates_box.grid_columnconfigure(0, weight=1)

        self._update_status = ctk.CTkLabel(
            page, text="", font=(FONTS.FAMILY, FONTS.SMALL_SIZE),
            text_color=COLORS.TEXT_MUTED, anchor="w",
        )
        self._update_status.grid(row=1, column=0, sticky="ew", padx=pad, pady=(6, 0))
        self._updates_placeholder = ctk.CTkLabel(
            self._updates_box, text="Noch nicht geladen …",
            font=(FONTS.FAMILY, FONTS.BODY_SIZE), text_color=COLORS.TEXT_DIM,
        )
        self._updates_placeholder.grid(row=0, column=0, pady=40)
        self._updates_loaded = False

    def _on_manual_update_check(self) -> None:
        self._set_update_status("Prüfe GitHub …", COLORS.TEXT_MUTED)
        try:
            self._update_check_button.configure(state="disabled", text="Prüfe …")
        except Exception:  # noqa: BLE001
            pass

        def worker() -> None:
            try:
                info = self._updater.check_for_update()
                self.after(0, lambda: self._render_update_info(info))
            except Exception:  # noqa: BLE001
                logger.exception("Manueller Update-Check fehlgeschlagen.")
                self.after(0, lambda: self._set_update_status(
                    "Fehler bei der Prüfung.", COLORS.ERROR))
                self.after(0, self._reset_update_button)

        threading.Thread(target=worker, daemon=True, name="UpdateCheck").start()

    def _render_update_info(self, info) -> None:
        self._reset_update_button()
        for child in self._updates_box.winfo_children():
            if child is not self._updates_placeholder:
                child.destroy()

        if info.error and not info.latest_version:
            self._set_update_status(f"GitHub nicht erreichbar: {info.error}", COLORS.WARNING)
            if self._updates_placeholder is not None:
                self._updates_placeholder.configure(
                    text="Keine Verbindung zum Update-Server.",
                    text_color=COLORS.TEXT_DIM)
                self._updates_placeholder.grid(row=0, column=0, pady=40)
            return

        if self._updates_placeholder is not None:
            self._updates_placeholder.destroy()
            self._updates_placeholder = None
        self._set_update_status("", COLORS.TEXT_MUTED)

        # Aktuelle Version
        cur = self._card(self._updates_box)
        cur.grid(row=0, column=0, sticky="ew", pady=(0, 12))
        cur.grid_columnconfigure(0, weight=1)
        row = ctk.CTkFrame(cur, fg_color="transparent", corner_radius=0)
        row.grid(row=0, column=0, sticky="ew", padx=20, pady=18)
        row.grid_columnconfigure(1, weight=1)
        ctk.CTkLabel(row, text="Installiert", font=(FONTS.FAMILY, FONTS.SMALL_SIZE),
                     text_color=COLORS.TEXT_MUTED, anchor="w").grid(row=0, column=0, sticky="w")
        ctk.CTkLabel(row, text=f"v{info.current_version}",
                     font=(FONTS.FAMILY_BOLD, FONTS.HEADING_SIZE, "bold"),
                     text_color=COLORS.TEXT_PRIMARY, anchor="w").grid(row=1, column=0, sticky="w")

        if info.update_available:
            ctk.CTkLabel(row, text="Neueste Version",
                         font=(FONTS.FAMILY, FONTS.SMALL_SIZE),
                         text_color=COLORS.TEXT_MUTED, anchor="e").grid(
                             row=0, column=2, sticky="e")
            ctk.CTkLabel(row, text="v" + str(info.latest_version),
                         font=(FONTS.FAMILY_BOLD, FONTS.DISPLAY_SIZE, "bold"),
                         text_color=COLORS.ACCENT, anchor="e").grid(
                             row=1, column=2, sticky="e")
        else:
            ctk.CTkLabel(row, text=f"{ICON_CHECK}   Auf dem neuesten Stand",
                         font=(FONTS.FAMILY_BOLD, FONTS.BODY_SIZE, "bold"),
                         text_color=COLORS.SUCCESS, anchor="e").grid(
                             row=0, column=2, rowspan=2, sticky="e")

        # Release-Notizen
        notes = (info.release_notes or "").strip()
        if notes:
            card = self._card(self._updates_box)
            card.grid(row=1, column=0, sticky="ew")
            card.grid_columnconfigure(0, weight=1)
            ctk.CTkLabel(card, text="Was ist neu",
                         font=(FONTS.FAMILY_BOLD, FONTS.HEADING_SIZE, "bold"),
                         text_color=COLORS.TEXT_PRIMARY, anchor="w"
                         ).grid(row=0, column=0, sticky="w", padx=20, pady=(18, 8))
            body = ctk.CTkTextbox(
                card, height=260, fg_color=COLORS.BACKGROUND,
                text_color=COLORS.TEXT_SECONDARY, corner_radius=LAYOUT.RADIUS_SMALL,
                font=(FONTS.FAMILY, FONTS.SMALL_SIZE), wrap="word",
                border_width=1, border_color=COLORS.BORDER_SOFT,
                scrollbar_button_color=COLORS.SURFACE_HOVER,
            )
            body.grid(row=1, column=0, sticky="ew", padx=20, pady=(0, 18))
            body.insert("1.0", notes)
            body.configure(state="disabled")

        if info.update_available:
            bar = self._card(self._updates_box)
            bar.grid(row=2, column=0, sticky="ew", pady=(12, 0))
            bar.grid_columnconfigure(0, weight=1)
            inner = ctk.CTkFrame(bar, fg_color="transparent", corner_radius=0)
            inner.grid(row=0, column=0, sticky="ew", padx=20, pady=18)
            inner.grid_columnconfigure(0, weight=1)
            ctk.CTkLabel(inner, text=f"Version {info.latest_version} ist verfügbar",
                         font=(FONTS.FAMILY, FONTS.BODY_SIZE),
                         text_color=COLORS.TEXT_SECONDARY, anchor="w").grid(row=0, column=0, sticky="w")
            from updater import UpdateDialog
            ctk.CTkButton(
                inner, text="Jetzt aktualisieren",
                command=lambda: UpdateDialog(parent=self, update_info=info),
                width=170, height=LAYOUT.BUTTON_HEIGHT,
                corner_radius=LAYOUT.RADIUS_MEDIUM,
                fg_color=COLORS.ACCENT, hover_color=COLORS.ACCENT_HOVER,
                text_color="#FFFFFF", font=(FONTS.FAMILY_BOLD, FONTS.BODY_SIZE, "bold"),
            ).grid(row=0, column=1, sticky="e")

    def _set_update_status(self, text: str, color: str) -> None:
        try:
            self._update_status.configure(text=text, text_color=color)
        except Exception:  # noqa: BLE001
            pass

    def _reset_update_button(self) -> None:
        try:
            self._update_check_button.configure(
                state="normal", text=f"{ICON_REFRESH}   Auf Updates prüfen")
        except Exception:  # noqa: BLE001
            pass

    # ══════════════════════════════════════════════════════════════
    #  Seite: Über
    # ══════════════════════════════════════════════════════════════

    def _build_page_about(self, page: ctk.CTkFrame) -> None:
        pad = LAYOUT.CARD_PADDING + 4
        page.grid_columnconfigure(0, weight=1)
        page.grid_rowconfigure(1, weight=1)

        ctk.CTkLabel(
            page, text="Über Draxo", font=(FONTS.FAMILY_BOLD, 20, "bold"),
            text_color=COLORS.TEXT_PRIMARY, anchor="w",
        ).grid(row=0, column=0, sticky="w", padx=pad, pady=(30, 18))

        card = self._card(page)
        card.grid(row=1, column=0, sticky="nsew", padx=pad, pady=(0, pad))
        card.grid_columnconfigure(0, weight=1)
        inner = ctk.CTkFrame(card, fg_color="transparent", corner_radius=0)
        inner.grid(row=0, column=0, sticky="nsew", padx=24, pady=24)
        inner.grid_columnconfigure(0, weight=1)

        ctk.CTkLabel(inner, text="Draxo Client",
                     font=(FONTS.FAMILY_BOLD, FONTS.DISPLAY_SIZE, "bold"),
                     text_color=COLORS.TEXT_PRIMARY, anchor="w").grid(row=0, column=0, sticky="w")
        ctk.CTkLabel(
            inner, text="Premium Minecraft Client für Windows",
            font=(FONTS.FAMILY, FONTS.BODY_SIZE), text_color=COLORS.TEXT_MUTED, anchor="w",
        ).grid(row=1, column=0, sticky="w", pady=(2, 18))

        info = ctk.CTkFrame(inner, fg_color="transparent", corner_radius=0)
        info.grid(row=2, column=0, sticky="ew")
        info.grid_columnconfigure((0, 1), weight=1, uniform="ab")
        self._field(info, "Version", "v" + self._updater.current_version, COLORS.TEXT_PRIMARY, 0, 0)
        self._field(info, "Python", f"{__import__('sys').version_info.major}."
                    f"{__import__('sys').version_info.minor}", COLORS.TEXT_PRIMARY, 0, 1)
        self._field(info, "Minecraft", "1.17 – 26.x", COLORS.TEXT_PRIMARY, 2, 0)
        self._field(info, "Loader", "Vanilla · Fabric · Forge · NeoForge",
                    COLORS.TEXT_PRIMARY, 2, 1)

        ctk.CTkFrame(inner, height=1, fg_color=COLORS.BORDER).grid(
            row=3, column=0, sticky="ew", pady=22)

        links = ctk.CTkFrame(inner, fg_color="transparent", corner_radius=0)
        links.grid(row=4, column=0, sticky="ew")
        for col, (glyph, tip, url) in enumerate((
            ("", "Discord beitreten", DISCORD_INVITE),
            (ICON_GITHUB, "Quellcode auf GitHub", GITHUB_URL),
            (ICON_GLOBE, "Website öffnen", WEBSITE_URL),
        )):
            btn = self._icon_button(links, glyph, tip, lambda u=url: self._open_url(u))
            btn.configure(width=40, height=38, fg_color=COLORS.SURFACE_LIGHT,
                          hover_color=COLORS.SURFACE_HOVER, text_color=COLORS.TEXT_SECONDARY)
            if not glyph:
                DiscordMark(btn, size=18, fg=COLORS.TEXT_SECONDARY,
                            bg=COLORS.SURFACE_LIGHT).place(relx=0.5, rely=0.5,
                                                           anchor="center")
            btn.grid(row=0, column=col, padx=(0, 8))

        ctk.CTkLabel(
            inner,
            text="Minecraft ist ein eingetragenes Markenzeichen von Mojang Studios. "
                 "Dieses Projekt steht in keiner Verbindung zu Mojang oder Microsoft.",
            font=(FONTS.FAMILY, FONTS.SMALL_SIZE), text_color=COLORS.TEXT_DIM,
            anchor="w", wraplength=620, justify="left",
        ).grid(row=5, column=0, sticky="w", pady=(22, 0))

    # ══════════════════════════════════════════════════════════════
    #  Auswahl
    # ══════════════════════════════════════════════════════════════

    def _highlight_flavor(self, flavor: str) -> None:
        for key, btn in self._flavor_buttons.items():
            active = key == flavor
            btn.configure(
                fg_color=COLORS.ACCENT_SOFT if active else COLORS.SURFACE_LIGHT,
                text_color=COLORS.TEXT_PRIMARY if active else COLORS.TEXT_MUTED,
                border_color=COLORS.ACCENT if active else COLORS.BORDER,
            )

    def _on_flavor_selected(self, flavor: str) -> None:
        self._flavor = flavor
        self._highlight_flavor(flavor)
        self._refresh_home_target()
        self._update_availability_hint()
        logger.info("Loader gewählt: %s", flavor)

    def _on_version_selected(self, value: str) -> None:
        self._selected_version = "" if value == AUTO_VERSION_LABEL else value
        self._refresh_home_target()
        self._update_availability_hint()

    def _on_rescan_clicked(self) -> None:
        self._append_log("Suche laufende Minecraft-Instanzen …", "info")
        threading.Thread(target=self._rescan_worker, daemon=True, name="Rescan").start()

    def _rescan_worker(self) -> None:
        versions = version_provider.get_versions()
        instances = ProcessDetector.list_instances(versions)
        self.after(0, lambda: self._apply_scan(versions, instances))

    def _apply_scan(self, versions: list[str], instances: list) -> None:
        self._instances = instances
        self._version_box.configure(values=[AUTO_VERSION_LABEL] + list(versions))
        current = self._version_var.get()

        if instances:
            first = instances[0]
            if first.version:
                self._version_var.set(first.version)
                self._selected_version = first.version
            self._on_flavor_selected(first.flavor)
            names = ", ".join(i.label for i in instances)
            self._append_log(f"Erkannt: {names}", "success")
            if len(instances) > 1:
                self._append_log(
                    f"{len(instances)} Instanzen aktiv — wähle die gewünschte manuell.",
                    "warning")
        else:
            if current not in ([AUTO_VERSION_LABEL] + list(versions)):
                self._version_var.set(AUTO_VERSION_LABEL)
                self._selected_version = ""
            self._append_log("Keine laufende Minecraft-Instanz gefunden.", "info")

        self._refresh_home_target()
        self._update_availability_hint()

    def _refresh_home_target(self) -> None:
        version = self._selected_version or "Automatisch"
        label = FLAVOR_LABELS.get(self._flavor, self._flavor)
        try:
            self._home_version.configure(text=version)
            self._home_flavor.configure(text=label)
        except Exception:  # noqa: BLE001
            pass

    def _update_availability_hint(self) -> None:
        """Warnt, wenn für Version+Loader keine vorgefertigte DLL existiert."""
        version = self._selected_version
        if not version:
            self._set_status_text("●  Bereit — Version wird beim Start erkannt.",
                                  COLORS.TEXT_MUTED)
            return
        try:
            from builder_runner import prebuilt_available
            ok, hint = prebuilt_available(version, self._flavor)
        except Exception:  # noqa: BLE001
            return
        if ok:
            self._set_status_text(f"{ICON_CHECK}  {version} · "
                                  f"{FLAVOR_LABELS.get(self._flavor, self._flavor)} — bereit",
                                  COLORS.SUCCESS)
        else:
            self._set_status_text(f"{ICON_WARN}  {version} · "
                                  f"{FLAVOR_LABELS.get(self._flavor, self._flavor)}: "
                                  "wird beim Injizieren gebaut", COLORS.WARNING)

    # ══════════════════════════════════════════════════════════════
    #  Injektion
    # ══════════════════════════════════════════════════════════════

    def _on_inject_clicked(self) -> None:
        if self._is_injecting:
            return
        if not self._require_login():
            return
        try:
            ButtonClickPulse(
                self._inject_button,
                flash_color=COLORS.ACCENT_GLOW,
                base_color=COLORS.INJECT_GREEN,
            ).start()
        except Exception:  # noqa: BLE001
            pass
        self._select_page("inject")
        self._start_inject()

    def _require_login(self) -> bool:
        """Striktes Gate: ohne Discord-Anmeldung keine Injektion.

        Liegt keine Anmeldung vor, wird das Anmeldefenster geöffnet und
        ``False`` zurückgegeben — der Aufrufer bricht ab.
        """
        if self._signed_in:
            return True
        self._set_status_text(
            "●  Anmeldung erforderlich — Injektion ist gesperrt.",
            COLORS.WARNING)
        self._append_log(
            "Injektion gesperrt: bitte mit Discord anmelden.", "warning")
        self._request_login()
        return False

    def _start_inject(self) -> None:
        # Zweite Sperre direkt vor dem Start — auch wenn jemand den
        # Knopf z. B. per Tastaturbindung ausloest.
        if not self._signed_in:
            self._require_login()
            return

        version = self._selected_version
        flavor = self._flavor

        detected_version: Optional[str] = None
        detected_flavor: Optional[str] = None
        try:
            detected_version, detected_flavor = \
                ProcessDetector.detect_minecraft_profile(version_provider.get_versions())
            if detected_version:
                logger.info("Minecraft erkannt: %s (%s)", detected_version, detected_flavor)
                self._append_log(f"Erkannt: {detected_version} "
                                 f"({FLAVOR_LABELS.get(detected_flavor, detected_flavor)})", "info")
        except Exception:  # noqa: BLE001
            logger.exception("Versions-Auto-Erkennung fehlgeschlagen.")

        if not version:
            version = detected_version or ""

        if not version:
            self._set_status_text("●  Kein Minecraft gefunden — starte es zuerst "
                                  "oder wähle oben eine Version.", COLORS.ERROR)
            self._append_log("Kein Minecraft-Prozess gefunden.", "error")
            return

        if detected_version and detected_version != version:
            self._append_log(f"Gewählt {version}, laufend {detected_version} "
                             "— es wird die gewählte Version verwendet.", "warning")
        if detected_flavor and detected_flavor != flavor:
            self._append_log(
                f"Loader '{flavor}' gewählt, erkannt '{detected_flavor}'. "
                "Ein falscher Loader führt zu einem Absturz im Spiel.", "warning")

        self._is_injecting = True
        self._set_buttons_injecting(True)
        self._set_status(InjectStatus.INJECTING)
        self._append_log(f"Starte Injektion für {version} "
                         f"({FLAVOR_LABELS.get(flavor, flavor)}) …", "internal")

        self._inject_thread = threading.Thread(
            target=self._run_inject_process, args=(version, flavor), daemon=True)
        self._inject_thread.start()

    def _set_buttons_injecting(self, active: bool) -> None:
        text = "  Lädt …" if active else f"  {ICON_INJECT}   INJECT"
        allowed = self._signed_in
        for btn in (self._inject_button, self._home_inject):
            try:
                btn.configure(
                    state="normal" if (allowed and not active) else "disabled",
                    text=text if btn is self._inject_button else (
                        "  Lädt …" if active else f"  {ICON_INJECT}   Jetzt injizieren"),
                    fg_color=COLORS.SURFACE_LIGHT if active else COLORS.INJECT_GREEN,
                )
            except Exception:  # noqa: BLE001
                pass

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
                version=version, workdir=WORKDIR, output=on_output, flavor=flavor)

            if exit_code == 0:
                self._console_queue.put(("[INTERNAL_SUCCESS]", StreamType.INTERNAL))
            else:
                self._console_queue.put((
                    f"Fehlgeschlagen (Exit {exit_code}): {error or 'unbekannter Fehler'}",
                    StreamType.STDERR))
                self._console_queue.put(("[INTERNAL_ERROR]", StreamType.INTERNAL))

        except Exception:  # noqa: BLE001
            logger.exception("Unerwarteter Fehler während der Injektion.")
            self._console_queue.put((traceback.format_exc(), StreamType.STDERR))
            self._console_queue.put(("[INTERNAL_ERROR]", StreamType.INTERNAL))
        finally:
            self._console_queue.put(("[INTERNAL_FINISH]", StreamType.INTERNAL))

    def _finish_inject(self) -> None:
        self._is_injecting = False
        self._set_buttons_injecting(False)

    def _set_status(self, status: str) -> None:
        mapping = {
            InjectStatus.READY: ("●  Bereit", COLORS.TEXT_MUTED),
            InjectStatus.INJECTING: ("●  Injiziere …", COLORS.WARNING),
            InjectStatus.SUCCESS: ("●  Erfolgreich injiziert", COLORS.SUCCESS),
            InjectStatus.ERROR: ("●  Fehlgeschlagen", COLORS.ERROR),
        }
        text, color = mapping.get(status, mapping[InjectStatus.READY])
        self._set_status_text(text, color)

    def _set_status_text(self, text: str, color: str) -> None:
        try:
            self._status_label.configure(text=text, text_color=color)
        except Exception:  # noqa: BLE001
            pass

    # ══════════════════════════════════════════════════════════════
    #  Protokoll
    # ══════════════════════════════════════════════════════════════

    def _append_log(self, text: str, tag: str = "info") -> None:
        stamp = datetime.now().strftime("%H:%M:%S")
        try:
            self._log_box.configure(state="normal")
            self._log_box.insert("end", f"{stamp}  {text.rstrip()}\n", tag)
            self._log_box.see("end")
            self._log_box.configure(state="disabled")
        except Exception:  # noqa: BLE001
            pass

    def _clear_log(self) -> None:
        try:
            self._log_box.configure(state="normal")
            self._log_box.delete("1.0", "end")
            self._log_box.configure(state="disabled")
        except Exception:  # noqa: BLE001
            pass

    # ══════════════════════════════════════════════════════════════
    #  Hintergrund
    # ══════════════════════════════════════════════════════════════

    def _start_background_tasks(self) -> None:
        self._update_minecraft_status()
        self._poll_console_queue()
        self._rescan_worker()
        self.after(VERSION_REFRESH_DELAY_MS, self._check_update_startup)

    def _check_update_startup(self) -> None:
        def worker() -> None:
            try:
                info = self._updater.check_for_update()
                if info.update_available:
                    self.after(0, lambda: self._show_update_dialog(info))
            except Exception:  # noqa: BLE001
                logger.exception("Update-Check beim Start fehlgeschlagen.")

        threading.Thread(target=worker, daemon=True, name="UpdateCheck").start()

    def _show_update_dialog(self, info) -> None:
        try:
            from updater import UpdateDialog
            UpdateDialog(parent=self, update_info=info)
        except Exception:  # noqa: BLE001
            logger.exception("Update-Dialog konnte nicht geöffnet werden.")

    def _update_minecraft_status(self) -> None:
        try:
            status = ProcessDetector.is_minecraft_running()
            if status.running:
                self._home_status.configure(text="Läuft", text_color=COLORS.SUCCESS)
                self._home_pid.configure(text=status.process_name or "—",
                                         text_color=COLORS.TEXT_SECONDARY)
                self._home_status_detail.configure(
                    text="Minecraft erkannt — Injektion ist möglich.",
                    text_color=COLORS.TEXT_SECONDARY)
                if not self._selected_version:
                    self._set_status_text("●  Minecraft läuft — bereit zum Injizieren.",
                                          COLORS.SUCCESS)
            else:
                self._home_status.configure(text="Nicht erkannt", text_color=COLORS.TEXT_MUTED)
                self._home_pid.configure(text="—", text_color=COLORS.TEXT_DIM)
                self._home_status_detail.configure(
                    text="Starte Minecraft, um fortzufahren.",
                    text_color=COLORS.TEXT_MUTED)
                if not self._selected_version:
                    self._set_status_text("●  Kein Minecraft gefunden.",
                                          COLORS.TEXT_MUTED)
        except Exception:  # noqa: BLE001
            logger.exception("Fehler beim Aktualisieren des Minecraft-Status.")
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
            self._append_log("Injektion erfolgreich abgeschlossen.", "success")
            return
        if tag == "[INTERNAL_ERROR]":
            self._set_status(InjectStatus.ERROR)
            self._append_log("Die Injektion ist fehlgeschlagen.", "error")
            return
        if tag == "[INTERNAL_FINISH]":
            self._finish_inject()
            return
        kind = "error" if tag == StreamType.STDERR else "info"
        self._append_log(text, kind)

    # ══════════════════════════════════════════════════════════════
    #  Fenster
    # ══════════════════════════════════════════════════════════════

    def _on_drag_start(self, _event: tk.Event) -> None:
        try:
            self._drag_offset = (_event.x_root - self.winfo_x(),
                                 _event.y_root - self.winfo_y())
        except Exception:  # noqa: BLE001
            pass

    def _on_drag_move(self, _event: tk.Event) -> None:
        try:
            off = getattr(self, "_drag_offset", None)
            if not off or getattr(self, "_is_maximized", False):
                return
            self.geometry(f"+{_event.x_root - off[0]}+{_event.y_root - off[1]}")
        except Exception:  # noqa: BLE001
            pass

    def post_bootstrap_init(self) -> None:
        """Wird nach dem Bootstrap-Fenster aufgerufen.

        Zu diesem Zeitpunkt ist die Discord-Anmeldung abgeschlossen —
        das Konto-Feld der Sidebar wird jetzt zum ersten Mal mit echten
        Daten gefuellt und das Injektions-Gate geprueft.
        """
        self._apply_session()
        user = getattr(self._session, "user", None)
        if user is not None:
            logger.info("Client startet fuer %s", user.display_name)

    def report_fatal_error(self, error: BaseException) -> None:
        logger.error("Fataler Fehler: %s", error)
        logger.error(traceback.format_exc())
        try:
            messagebox.showerror(
                "Draxo Client — Fehler",
                f"Ein unerwarteter Fehler ist aufgetreten:\n\n{error}")
        except Exception:  # noqa: BLE001
            pass

    def _on_close(self) -> None:
        try:
            self._config_manager.update_window_geometry(
                self.winfo_width(), self.winfo_height(), self.winfo_x(), self.winfo_y())
        except Exception:  # noqa: BLE001
            pass
        self.destroy()
