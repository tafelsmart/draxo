"""
widgets.py
----------
UI-Bausteine, die mehrere Fenster brauchen.

Hier liegen die Teile, die sowohl das Hauptfenster (ui.py) als auch
das Anmeldefenster (auth_ui.py) verwenden:

    Tooltip        — Kurzhinweis nach kurzer Verweilzeit
    DiscordMark    — Discord-Signatur als Vektor
    icon_button    — quadratischer Knopf mit Symbol und Tooltip
    card           — Standardfläche mit Rand

Sie liegen bewusst in einem eigenen Modul, damit die beiden Fenster
nicht voneinander abhängen müssen.
"""

from __future__ import annotations

import logging
import tkinter as tk
import webbrowser
from typing import Optional

import customtkinter as ctk

from styles import COLORS, FONTS, LAYOUT

logger = logging.getLogger("DraxoClient.widgets")

from discord_auth import GUILD_INVITE

# Segoe-MDL2-Glyphen (liegt Windows bei)
#
# Wichtig: als Escape-Sequenz schreiben, nicht als echtes Zeichen.
# Ein eingeschobenes PUA-Zeichen laeuft still durch und liefert im
# Fenster die falschen Symbole.
ICON_HOME = "\ue80f"
ICON_INJECT = "\ue768"
ICON_UPDATE = "\ue896"
ICON_INFO = "\ue946"
ICON_MINIMIZE = "\ue921"
ICON_MAXIMIZE = "\ue922"
ICON_RESTORE = "\ue8bb"
ICON_CLOSE = "\ue8bb"
ICON_USER = "\ue77b"
ICON_REFRESH = "\ue72c"
ICON_DISCORD = "\ue908"
ICON_CHECK = "\ue73e"
ICON_WARN = "\ue7ba"
ICON_GAME = "\ue7fc"
ICON_GLOBE = "\ue774"
ICON_GITHUB = "\ue902"
ICON_COPY = "\ue8c8"
ICON_SHIELD = "\ue72a"
ICON_MORE = "\ue74d"
ICON_ROCKET = "\ue135"
ICON_FOLDER = "\ue8b7"
ICON_STAR = "\ue734"
ICON_CHEVRON = "\ue76c"

# ── Externe Ziele ────────────────────────────────────────────────
#: Ziel-Server — kommt aus discord_auth, damit es nur eine Quelle gibt.
DISCORD_INVITE = GUILD_INVITE
WEBSITE_URL = "https://draxo.netlify.app"
GITHUB_URL = "https://github.com/tafelsmart/draxo"



class DiscordMark(ctk.CTkFrame):
    """Die Discord-Signatur als Vektor.

    Segoe MDL2 Assets liefert kein Discord-Logo, und eine geladene
    Grafik wäre eine weitere Abhängigkeit. Die Signatur besteht aus
    fünf Ellipsen und skaliert verlustfrei.

    ``bg`` muss die Farbe des Untergrunds sein — Tk-Canvas kann keine
    echte Transparenz, deshalb wird das Auge in dieser Farbe
    „ausgeschnitten“.
    """

    def __init__(self, master, size: int = 18, fg: str = COLORS.TEXT_PRIMARY,
                 bg: str = COLORS.SURFACE, **kwargs) -> None:
        super().__init__(master, width=size, height=size,
                         fg_color="transparent", **kwargs)
        self.pack_propagate(False)
        canvas = tk.Canvas(self, width=size, height=size,
                           highlightthickness=0, bd=0, bg=bg)
        canvas.pack()
        k = size / 20.0
        # Kopf zuerst, Ohren darüber — so verschmelzen sie zu einer
        # Silhouette, statt als zwei Kreise hervorzustehen.
        canvas.create_oval(0.6 * k, 4.2 * k, 19.4 * k, 17.6 * k, fill=fg, outline="")
        canvas.create_oval(2.4 * k, 1.4 * k, 8.2 * k, 5.6 * k, fill=fg, outline="")
        canvas.create_oval(11.8 * k, 1.4 * k, 17.6 * k, 5.6 * k, fill=fg, outline="")
        canvas.create_oval(6.3 * k, 8.3 * k, 9.1 * k, 11.1 * k, fill=bg, outline="")
        canvas.create_oval(10.9 * k, 8.3 * k, 13.7 * k, 11.1 * k, fill=bg, outline="")


class Tooltip:
    """Minimaler Tooltip — erscheint nach kurzer Verweilzeit."""

    DELAY_MS = 550

    def __init__(self, widget: tk.Misc, text: str) -> None:
        self._widget = widget
        self._text = text
        self._after_id: Optional[str] = None
        self._window: Optional[tk.Toplevel] = None
        widget.bind("<Enter>", self._schedule, add="+")
        widget.bind("<Leave>", self._hide, add="+")
        widget.bind("<ButtonPress>", self._hide, add="+")

    def _schedule(self, _event: tk.Event) -> None:
        self._cancel()
        self._after_id = self._widget.after(self.DELAY_MS, self._show)

    def _cancel(self) -> None:
        if self._after_id is not None:
            try:
                self._widget.after_cancel(self._after_id)
            except Exception:  # noqa: BLE001
                pass
            self._after_id = None

    def _show(self) -> None:
        self._after_id = None
        if self._window is not None:
            return
        try:
            x = self._widget.winfo_rootx() + self._widget.winfo_width() // 2
            y = self._widget.winfo_rooty() + self._widget.winfo_height() + 8
            win = tk.Toplevel(self._widget)
            win.wm_overrideredirect(True)
            win.wm_geometry(f"+{x - 45}+{y}")
            win.configure(bg=COLORS.ACCENT)
            tk.Label(
                win, text=self._text, bg=COLORS.ACCENT, fg="#FFFFFF",
                font=(FONTS.FAMILY, 9), padx=8, pady=3,
            ).pack()
            self._window = win
        except Exception:  # noqa: BLE001
            self._window = None

    def _hide(self, _event: Optional[tk.Event] = None) -> None:
        self._cancel()
        if self._window is not None:
            try:
                self._window.destroy()
            except Exception:  # noqa: BLE001
                pass
            self._window = None


def icon_button(parent: ctk.CTkBaseClass, glyph: str, tip: str,
                command) -> ctk.CTkButton:
    """Quadratischer Icon-Knopf mit Tooltip.

    ``glyph`` darf leer sein — dann erwartet der Aufrufer ein Kind-Widget
    (z. B. :class:`DiscordMark`) und setzt es selbst mittig hinein.
    """
    btn = ctk.CTkButton(
        parent, text=glyph, command=command,
        width=34, height=32, corner_radius=LAYOUT.RADIUS_SMALL,
        fg_color="transparent", hover_color=COLORS.SURFACE_HOVER,
        text_color=COLORS.TEXT_MUTED, font=(FONTS.ICON, 13),
    )
    Tooltip(btn, tip)
    return btn


def card(parent: ctk.CTkBaseClass, **kwargs) -> ctk.CTkFrame:
    """Standardfläche mit dezentem Rand."""
    return ctk.CTkFrame(
        parent,
        fg_color=COLORS.SURFACE,
        corner_radius=LAYOUT.CARD_CORNER,
        border_width=1,
        border_color=COLORS.BORDER,
        **kwargs,
    )


def open_url(url: str) -> None:
    """Öffnet einen Link im Standardbrowser und verschluckt Fehler still."""
    try:
        webbrowser.open(url)
    except Exception:  # noqa: BLE001
        logger.exception("Link konnte nicht geöffnet werden: %s", url)
