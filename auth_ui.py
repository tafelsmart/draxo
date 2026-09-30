"""
auth_ui.py
----------
Anmeldefenster des Draxo Client.

Aufbau nach dem Muster der Referenz-Apps:

    ┌────────────────────────────────────────────────┐
    │  Draxo                                      – ✕ │
    │                                                │
    │                   [ Logo ]                     │
    │           Willkommen bei Draxo                 │
    │       Der beste Weg, den Client zu nutzen       │
    │                                                │
    │      ┌──────────────────────────────────┐      │
    │      │      Mit Discord anmelden         │      │
    │      └──────────────────────────────────┘      │
    │      ┌──────────────────────────────────┐      │
    │      │      Server ansehen               │      │
    │      └──────────────────────────────────┘      │
    │       Noch kein Discord-Konto? Jetzt erstellen │
    │                                                │
    │              ( )  (🌐)  (👤)                    │
    └────────────────────────────────────────────────┘

Zustände:

    idle     beide Knöpfe aktiv
    working  Knöpfe gesperrt, Hinweis auf den Browser, Abbruch möglich
    error    Fehlerkarte mit Klartext und erneut versuchen
    setup    noch keine Client-ID hinterlegt (siehe discord_auth)

Das Fenster blockiert: ``on_success`` feuert erst, wenn wirklich ein
Nutzer angemeldet ist. ``on_cancel`` feuert beim Schließen — der
Aufrufer entscheidet, was dann passiert.
"""

from __future__ import annotations

import io
import logging
import queue
import threading
import tkinter as tk
import urllib.request
from typing import Optional

import customtkinter as ctk
from PIL import Image

from discord_auth import (
    GUILD_INVITE,
    GUILD_NAME,
    NOT_CONFIGURED_HINT,
    DiscordSession,
    LoginResult,
    is_configured,
)
from styles import COLORS, FONTS, LAYOUT
from utils import center_window
from widgets import (
    ICON_CLOSE,
    ICON_GLOBE,
    ICON_MINIMIZE,
    ICON_USER,
    WEBSITE_URL,
    DiscordMark,
    card,
    icon_button,
    open_url,
)

logger = logging.getLogger("DraxoClient.authUI")

POLL_MS = 120


REGISTER_URL = "https://discord.com/register"

#: Berechtigungen, um die der Launcher bittet — wortgleich zu Discord.
PERMISSIONS: tuple[tuple[str, str], ...] = (
    ("identify", "Auf deinen Benutzernamen, Avatar und Banner zugreifen"),
    ("guilds.join", f"Dir Zugang zu {GUILD_NAME} geben"),
)

#: Was der Nutzer nach der Anmeldung bekommt.
PERKS: tuple[str, ...] = (
    "Updates und neue Versionen automatisch",
    "Support direkt über den Discord-Server",
    "Ankündigungen zu Änderungen am Client",
)


def load_avatar(url: str, size: int = 64) -> Optional[Image.Image]:
    """Lädt ein Discord-Avatarbild als quadratisches PIL-Bild.

    Gibt None zurück, wenn kein Bild vorhanden ist oder der Download
    klemmt — der Aufrufer zeigt dann die Initialen an.
    """
    if not url:
        return None
    try:
        request = urllib.request.Request(
            url, headers={"User-Agent": "DraxoClient/1.0"})
        with urllib.request.urlopen(request, timeout=8) as response:
            raw = response.read()
        image = Image.open(io.BytesIO(raw)).convert("RGBA")
    except Exception:  # noqa: BLE001
        logger.debug("Avatar konnte nicht geladen werden: %s", url)
        return None

    # Quadratisch zuschneiden, damit das Bild nicht verzerrt wirkt.
    side = min(image.size)
    left = (image.width - side) // 2
    top = (image.height - side) // 2
    return image.crop((left, top, left + side, top + side)).resize(
        (size, size), Image.LANCZOS)


class WelcomeWindow(ctk.CTkToplevel):
    """Das Anmeldefenster. Blockierend bis zum Erfolg."""

    WIDTH = 900
    HEIGHT = 600

    def __init__(self, parent: ctk.CTk, session: DiscordSession,
                 on_success, on_cancel=None) -> None:
        super().__init__(parent)

        self._session = session
        self._on_success = on_success
        self._on_cancel = on_cancel
        self._queue: "queue.Queue[LoginResult]" = queue.Queue()
        self._state = "idle"
        self._drag_offset: Optional[tuple[int, int]] = None
        self._cancel_button: Optional[ctk.CTkButton] = None

        self._configure_window()
        self._build_ui()

        # Noch keine Client-ID? Dann sofort die Setup-Anleitung zeigen.
        self._set_state("setup" if not is_configured() else "idle")
        self._poll()

        self.protocol("WM_DELETE_WINDOW", self._on_close)

    # ══════════════════════════════════════════════════════════════
    #  Fenster
    # ══════════════════════════════════════════════════════════════

    def _configure_window(self) -> None:
        ctk.set_appearance_mode("dark")
        self.title("Draxo — Anmelden")
        self.geometry(f"{self.WIDTH}x{self.HEIGHT}")
        center_window(self, self.WIDTH, self.HEIGHT)
        self.minsize(self.WIDTH, self.HEIGHT)
        self.configure(fg_color="#010101")
        try:
            self.overrideredirect(True)
            self.attributes("-transparentcolor", "#010101")
        except Exception:  # noqa: BLE001
            logger.debug("Rahmenloses Fenster nicht möglich.")

        # Das Master-Fenster ist zu diesem Zeitpunkt noch versteckt — ein
        # daraus erzeugtes Toplevel erbt diesen Zustand und bliebe
        # unsichtbar, bis es sich wieder einblendet.
        self.deiconify()
        self.lift()
        self.after(60, self._bring_to_front)

    def _bring_to_front(self) -> None:
        """Blendet das Fenster endgültig ein und holt es nach vorn.

        CustomTkinter blendet ein Toplevel kurz aus, um die Farbe der
        Titelleiste zu setzen, und stellt danach den Zustand von
        zuvor wieder her. Weil das Master-Fenster beim Start noch
        versteckt ist, bliebe es dabei versteckt — deshalb wird hier
        ein zweites Mal eingeblendet.
        """
        try:
            self.deiconify()
            self.lift()
            self.focus_force()
        except Exception:  # noqa: BLE001
            pass

    def _on_close(self) -> None:
        """Schließen beendet den Vorgang ohne Anmeldung."""
        self._destroy_cancel_button()
        try:
            self.destroy()
        except Exception:  # noqa: BLE001
            pass
        if self._on_cancel is not None:
            self._on_cancel()

    # ══════════════════════════════════════════════════════════════
    #  Aufbau
    # ══════════════════════════════════════════════════════════════

    def _build_ui(self) -> None:
        shell = ctk.CTkFrame(self, fg_color=COLORS.BACKGROUND,
                             corner_radius=LAYOUT.CORNER_RADIUS_WINDOW)
        shell.pack(fill="both", expand=True)
        shell.grid_rowconfigure(1, weight=1)
        shell.grid_columnconfigure(0, weight=1)

        self._build_titlebar(shell)

        body = ctk.CTkFrame(shell, fg_color="transparent", corner_radius=0)
        body.grid(row=1, column=0, sticky="nsew")
        body.grid_columnconfigure(0, weight=1)

        stage = ctk.CTkFrame(body, fg_color="transparent", corner_radius=0)
        stage.grid(row=0, column=0, sticky="nsew", padx=56, pady=(6, 22))
        stage.grid_columnconfigure(0, weight=1)
        stage.grid_rowconfigure(1, weight=1)

        self._build_head(stage)
        self._build_actions(stage)
        self._build_footer(body)

    def _build_titlebar(self, parent: ctk.CTkFrame) -> None:
        bar = ctk.CTkFrame(parent, fg_color=COLORS.BACKGROUND, height=44,
                           corner_radius=LAYOUT.CORNER_RADIUS_WINDOW)
        bar.grid(row=0, column=0, sticky="ew", padx=1, pady=(1, 0))
        bar.pack_propagate(False)
        bar.grid_columnconfigure(1, weight=1)

        mark = ctk.CTkFrame(bar, width=24, height=24, corner_radius=7,
                            fg_color=COLORS.ACCENT, border_width=0)
        mark.grid(row=0, column=0, padx=(14, 9), pady=10)
        mark.pack_propagate(False)
        ctk.CTkLabel(mark, text="D", font=(FONTS.FAMILY_BOLD, 12, "bold"),
                     text_color="#FFFFFF").pack(expand=True)

        ctk.CTkLabel(
            bar, text="Draxo", height=24, anchor="w",
            font=(FONTS.FAMILY_BOLD, FONTS.LOGO_SIZE, "bold"),
            text_color=COLORS.TEXT_PRIMARY,
        ).grid(row=0, column=1, sticky="w")

        for col, (glyph, cmd, hover) in enumerate((
            (ICON_MINIMIZE, self._minimize, COLORS.HOVER_OVERLAY),
            (ICON_CLOSE, self._on_close, COLORS.INJECT_RED),
        ), start=2):
            btn = ctk.CTkButton(
                bar, text=glyph, command=cmd, width=42, height=44,
                corner_radius=0, fg_color="transparent", hover_color=hover,
                text_color=COLORS.TEXT_MUTED, font=(FONTS.ICON, 10),
            )
            btn.grid(row=0, column=col, sticky="nse")

        for widget in (bar, mark):
            widget.bind("<Button-1>", self._drag_start)
            widget.bind("<B1-Motion>", self._drag_move)

    def _build_head(self, parent: ctk.CTkFrame) -> None:
        head = ctk.CTkFrame(parent, fg_color="transparent", corner_radius=0)
        head.grid(row=0, column=0, sticky="ew", pady=(24, 0))

        mark = ctk.CTkFrame(head, width=56, height=56, corner_radius=16,
                            fg_color=COLORS.ACCENT, border_width=0)
        mark.pack(pady=(0, 16))
        mark.pack_propagate(False)
        ctk.CTkLabel(mark, text="D", font=(FONTS.FAMILY_BOLD, 26, "bold"),
                     text_color="#FFFFFF").pack(expand=True)

        ctk.CTkLabel(
            head, text="Willkommen bei Draxo", height=32,
            font=(FONTS.FAMILY_BOLD, 25, "bold"),
            text_color=COLORS.TEXT_PRIMARY, anchor="center",
        ).pack()
        ctk.CTkLabel(
            head, text="Der beste Weg, den Client zu nutzen.", height=18,
            font=(FONTS.FAMILY, FONTS.BODY_SIZE),
            text_color=COLORS.TEXT_MUTED, anchor="center",
        ).pack(pady=(2, 0))

    def _build_footer(self, parent: ctk.CTkFrame) -> None:
        foot = ctk.CTkFrame(parent, fg_color="transparent", corner_radius=0)
        foot.grid(row=1, column=0, sticky="s", pady=(0, 22))

        entries = (
            ("", "Discord-Server", GUILD_INVITE),
            (ICON_GLOBE, "Website", WEBSITE_URL),
            (ICON_USER, "Dein Konto", None),
        )
        for index, (glyph, tip, url) in enumerate(entries):
            btn = icon_button(foot, glyph, tip,
                              (lambda u=url: open_url(u)) if url else (lambda: None))
            btn.configure(width=38, height=36)
            if not glyph:
                DiscordMark(btn, size=17, fg=COLORS.TEXT_MUTED,
                            bg=COLORS.BACKGROUND).place(relx=0.5, rely=0.5,
                                                         anchor="center")
            btn.grid(row=0, column=index, padx=5)

    def _build_actions(self, parent: ctk.CTkFrame) -> None:
        middle = ctk.CTkFrame(parent, fg_color="transparent", corner_radius=0)
        middle.grid(row=1, column=0, sticky="nsew")
        middle.grid_columnconfigure(0, weight=1)
        middle.grid_rowconfigure(0, weight=1)

        holder = ctk.CTkFrame(middle, fg_color="transparent", corner_radius=0)
        holder.grid(row=0, column=0, sticky="nsew")
        holder.grid_columnconfigure(0, weight=1)
        # Wird fuer den Abbrechen-Knopf gebraucht (Zeile 4); die Karten
        # darunter liegen in Zeile 5, damit nichts ueberlagert wird.
        self._actions = holder

        BTN_W = 350

        # ── Primär: mit Discord anmelden ──
        self._login_button = ctk.CTkButton(
            holder, text="   Mit Discord anmelden", command=self._start_login,
            width=BTN_W, height=50, corner_radius=LAYOUT.RADIUS_MEDIUM,
            fg_color="#FFFFFF", hover_color="#E6E6EB",
            text_color="#101014",
            font=(FONTS.FAMILY_BOLD, FONTS.BUTTON_SIZE, "bold"),
        )
        self._login_button.grid(row=0, column=0, pady=(14, 12))
        DiscordMark(self._login_button, size=18, fg="#101014",
                    bg="#FFFFFF").place(relx=0.19, rely=0.5, anchor="center")

        # ── Sekundär: Server ansehen ──
        self._perks_button = ctk.CTkButton(
            holder, text="Server ansehen", command=self._toggle_perks,
            width=BTN_W, height=50, corner_radius=LAYOUT.RADIUS_MEDIUM,
            fg_color="transparent", hover_color=COLORS.SURFACE_HOVER,
            border_width=1, border_color=COLORS.BORDER,
            text_color=COLORS.TEXT_SECONDARY,
            font=(FONTS.FAMILY_BOLD, FONTS.BUTTON_SIZE, "bold"),
        )
        self._perks_button.grid(row=1, column=0, pady=(0, 12))

        # ── Konto erstellen ──
        self._register_button = ctk.CTkButton(
            holder, text="Noch kein Discord-Konto? Jetzt erstellen",
            command=lambda: open_url(REGISTER_URL),
            width=BTN_W, height=24, corner_radius=LAYOUT.RADIUS_SMALL,
            fg_color="transparent", hover_color=COLORS.SURFACE,
            text_color=COLORS.TEXT_MUTED,
            font=(FONTS.FAMILY, FONTS.SMALL_SIZE, "underline"),
        )
        self._register_button.grid(row=2, column=0)

        # ── Statuszeile ──
        self._status = ctk.CTkLabel(
            holder, text="", height=18,
            font=(FONTS.FAMILY, FONTS.SMALL_SIZE),
            text_color=COLORS.TEXT_MUTED, anchor="center",
            wraplength=560, justify="center",
        )
        self._status.grid(row=3, column=0, pady=(12, 0))

        # ── Fehlerkarte ──
        self._error_card = card(holder)
        self._error_title = ctk.CTkLabel(
            self._error_card, text="", height=20, anchor="w",
            font=(FONTS.FAMILY_BOLD, FONTS.HEADING_SIZE, "bold"),
            text_color=COLORS.ERROR,
        )
        self._error_title.grid(row=0, column=0, sticky="w", padx=18, pady=(14, 6))
        self._error_text = ctk.CTkLabel(
            self._error_card, text="", height=64, anchor="w", justify="left",
            wraplength=540, font=(FONTS.FAMILY, FONTS.SMALL_SIZE),
            text_color=COLORS.TEXT_SECONDARY,
        )
        self._error_text.grid(row=1, column=0, sticky="w", padx=18)
        self._retry_button = ctk.CTkButton(
            self._error_card, text="Erneut versuchen", command=self._start_login,
            width=160, height=34, corner_radius=LAYOUT.RADIUS_SMALL,
            fg_color=COLORS.ACCENT, hover_color=COLORS.ACCENT_HOVER,
            text_color="#FFFFFF", font=(FONTS.FAMILY, FONTS.SMALL_SIZE, "bold"),
        )
        self._retry_button.grid(row=2, column=0, sticky="w", padx=18, pady=(10, 16))

        # ── Setup-Hinweis: fehlende Client-ID ──
        self._setup_card = card(holder)
        ctk.CTkLabel(
            self._setup_card, text="Noch nicht eingerichtet", height=20,
            font=(FONTS.FAMILY_BOLD, FONTS.HEADING_SIZE, "bold"),
            text_color=COLORS.WARNING, anchor="w",
        ).grid(row=0, column=0, sticky="w", padx=18, pady=(14, 8))
        ctk.CTkLabel(
            self._setup_card, text=NOT_CONFIGURED_HINT, anchor="w",
            justify="left", wraplength=540, height=118,
            font=(FONTS.FAMILY, FONTS.SMALL_SIZE),
            text_color=COLORS.TEXT_SECONDARY,
        ).grid(row=1, column=0, sticky="w", padx=18)
        ctk.CTkButton(
            self._setup_card, text="Trotzdem starten — Injektion bleibt gesperrt",
            command=self._on_close, width=300, height=34,
            corner_radius=LAYOUT.RADIUS_SMALL,
            fg_color=COLORS.SURFACE_LIGHT, hover_color=COLORS.SURFACE_HOVER,
            text_color=COLORS.TEXT_SECONDARY,
            font=(FONTS.FAMILY, FONTS.SMALL_SIZE),
        ).grid(row=2, column=0, sticky="w", padx=18, pady=(10, 16))

        # ── Zusagen-Panel (aufklappbar) ──
        self._perk_panel = card(holder)
        self._perk_panel.grid_columnconfigure(1, weight=1)
        self._build_perks(self._perk_panel)

    def _build_perks(self, parent: ctk.CTkFrame) -> None:
        pad = 18

        ctk.CTkLabel(
            parent, text=f"Das bekommst du in {GUILD_NAME}", height=20,
            font=(FONTS.FAMILY_BOLD, FONTS.HEADING_SIZE, "bold"),
            text_color=COLORS.TEXT_PRIMARY, anchor="w",
        ).grid(row=0, column=0, columnspan=2, sticky="ew", padx=pad, pady=(pad, 8))

        row = 1
        for perk in PERKS:
            ctk.CTkLabel(
                parent, text="\u2713", width=16,
                font=(FONTS.FAMILY_BOLD, FONTS.BODY_SIZE, "bold"),
                text_color=COLORS.SUCCESS, anchor="w",
            ).grid(row=row, column=0, sticky="w", padx=(pad, 8), pady=1)
            ctk.CTkLabel(
                parent, text=perk, height=17, anchor="w",
                font=(FONTS.FAMILY, FONTS.SMALL_SIZE),
                text_color=COLORS.TEXT_SECONDARY,
            ).grid(row=row, column=1, sticky="w", padx=(0, pad), pady=1)
            row += 1

        row += 1
        ctk.CTkLabel(
            parent, text="Der Launcher bittet um genau diese Zugriffe:",
            height=17, anchor="w", font=(FONTS.FAMILY, FONTS.SMALL_SIZE),
            text_color=COLORS.TEXT_MUTED,
        ).grid(row=row, column=0, columnspan=2, sticky="ew", padx=pad, pady=(0, 4))
        row += 1

        for scope, text in PERMISSIONS:
            ctk.CTkLabel(
                parent, text=scope, width=104, anchor="w",
                font=(FONTS.MONO, FONTS.TINY_SIZE), text_color=COLORS.ACCENT,
            ).grid(row=row, column=0, sticky="w", padx=(pad, 8), pady=1)
            ctk.CTkLabel(
                parent, text=text, height=16, anchor="w",
                font=(FONTS.FAMILY, FONTS.TINY_SIZE), text_color=COLORS.TEXT_DIM,
            ).grid(row=row, column=1, sticky="w", padx=(0, pad), pady=1)
            row += 1

        row += 1
        ctk.CTkFrame(parent, height=pad, fg_color="transparent").grid(
            row=row, column=0, columnspan=2)

    # ══════════════════════════════════════════════════════════════
    #  Zustände
    # ══════════════════════════════════════════════════════════════

    def _set_state(self, state: str) -> None:
        self._state = state
        self._destroy_cancel_button()

        working = state == "working"
        busy = state in ("working", "setup")
        self._login_button.configure(state="disabled" if busy else "normal")
        self._perks_button.configure(state="disabled" if working else "normal")
        self._register_button.configure(state="disabled" if busy else "normal")

        self._error_card.grid_remove()
        self._setup_card.grid_remove()
        self._perk_panel.grid_remove()

        if state == "setup":
            self._setup_card.grid(row=5, column=0, columnspan=1,
                                  pady=(14, 0), sticky="ew")
            self._status.configure(
                text="Ohne Client-ID ist keine Anmeldung möglich.",
                text_color=COLORS.WARNING)
        elif state == "error":
            self._error_card.grid(row=5, column=0, pady=(14, 0), sticky="ew")
            self._status.configure(text="")
        elif state == "working":
            self._status.configure(
                text="Browser geöffnet — bitte dort bestätigen …",
                text_color=COLORS.ACCENT)
            # Im Grid statt frei platziert: sonst landet der Knopf auf den
            # Symbolen in der Fusszeile.
            self._cancel_button = ctk.CTkButton(
                self._actions, text="Abbrechen", command=self._on_close,
                width=130, height=30, corner_radius=LAYOUT.RADIUS_SMALL,
                fg_color="transparent", hover_color=COLORS.SURFACE_HOVER,
                border_width=1, border_color=COLORS.BORDER,
                text_color=COLORS.TEXT_MUTED,
                font=(FONTS.FAMILY, FONTS.TINY_SIZE),
            )
            self._cancel_button.grid(row=4, column=0, pady=(18, 0))
        else:
            self._status.configure(text="", text_color=COLORS.TEXT_MUTED)

    def _destroy_cancel_button(self) -> None:
        if self._cancel_button is not None:
            try:
                self._cancel_button.destroy()
            except Exception:  # noqa: BLE001
                pass
            self._cancel_button = None

    # ══════════════════════════════════════════════════════════════
    #  Aktionen
    # ══════════════════════════════════════════════════════════════

    def _toggle_perks(self) -> None:
        if self._perk_panel.winfo_manager():
            self._perk_panel.grid_remove()
        else:
            self._perk_panel.grid(row=5, column=0, pady=(16, 0), sticky="ew")

    def _start_login(self) -> None:
        if self._state == "working":
            return
        self._error_title.configure(text="")
        self._error_text.configure(text="")
        self._set_state("working")

        def worker() -> None:
            try:
                result = self._session.login()
            except Exception as exc:  # noqa: BLE001
                logger.exception("Anmeldung unerwartet fehlgeschlagen.")
                result = LoginResult(ok=False, error=str(exc))
            self._queue.put(result)

        threading.Thread(target=worker, daemon=True, name="DiscordLogin").start()

    def _poll(self) -> None:
        """Holt das Ergebnis des Anmelde-Threads ab."""
        try:
            while True:
                self._on_result(self._queue.get_nowait())
        except queue.Empty:
            pass
        except Exception:  # noqa: BLE001
            logger.exception("Fehler beim Verarbeiten der Anmelde-Nachricht.")
        finally:
            try:
                if self.winfo_exists():
                    self.after(POLL_MS, self._poll)
            except Exception:  # noqa: BLE001
                pass

    def _on_result(self, result: LoginResult) -> None:
        if not self.winfo_exists():
            return

        if result.ok:
            logger.info("Anmeldung erfolgreich: %s", self._session.display_name)
            self._destroy_cancel_button()
            try:
                self.destroy()
            except Exception:  # noqa: BLE001
                pass
            self._on_success(result)
            return

        logger.info("Anmeldung fehlgeschlagen: %s", result.error)
        self._set_state("error")
        self._error_title.configure(
            text="Abgebrochen" if result.cancelled else result.error_title)
        self._error_text.configure(text=result.error)

    # ══════════════════════════════════════════════════════════════
    #  Fenstertechnik
    # ══════════════════════════════════════════════════════════════

    def _drag_start(self, event: tk.Event) -> None:
        try:
            self._drag_offset = (event.x_root - self.winfo_x(),
                                 event.y_root - self.winfo_y())
        except Exception:  # noqa: BLE001
            pass

    def _drag_move(self, event: tk.Event) -> None:
        if not self._drag_offset:
            return
        try:
            self.geometry(f"+{event.x_root - self._drag_offset[0]}"
                          f"+{event.y_root - self._drag_offset[1]}")
        except Exception:  # noqa: BLE001
            pass

    def _minimize(self) -> None:
        try:
            self.withdraw()
        except Exception:  # noqa: BLE001
            pass
