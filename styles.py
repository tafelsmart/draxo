"""
styles.py
---------
Zentrale Design-Tokens für den Draxo Client.

Alle Farben, Schriften, Abstände und Eckenradien liegen an einer Stelle,
damit das komplette Look & Feel konsistent bleibt und nicht über dozens
Dateien verteilt gepflegt werden muss.

Die Palette ist bewusst neutral-dunkel gehalten (wie moderne Desktop-Apps),
mit dem violetten Draxo-Akzent als einziger Signalfarbe. Grün ist ausschliesslich
dem Injekt-Vorgang vorbehalten, damit Erfolg/Aktion sofort unterscheidbar ist.
"""

from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class Colors:
    """Alle im Launcher verwendeten Farben (Hex-Werte)."""

    # ── Basis: entsättigtes Dunkelgrau statt reinem Schwarz ─────────
    # Reines Schwarz (#000) wirkt auf OLED flimmernd und lässt Karten
    # nicht von der Fläche abheben.
    BACKGROUND: str = "#0A0A0C"
    SIDEBAR: str = "#0D0D10"
    SURFACE: str = "#141419"
    SURFACE_LIGHT: str = "#1C1C22"
    SURFACE_HOVER: str = "#24242C"

    # ── Akzent: Draxo-Lila ──────────────────────────────────────────
    ACCENT: str = "#8B5CF6"
    ACCENT_HOVER: str = "#9F7AFA"
    ACCENT_SOFT: str = "#1E1B2E"   # dezente Fläche für aktive Nav-Einträge
    ACCENT_GLOW: str = "#C084FC"

    # ── Injekt: die eine Aktion, die zählt ─────────────────────────
    INJECT_GREEN: str = "#22C55E"
    INJECT_GREEN_HOVER: str = "#16A34A"
    INJECT_GREEN_GLOW: str = "#4ADE80"
    INJECT_RED: str = "#EF4444"

    # ── Text: vier Stufen reichen für die visuelle Hierarchie ───────
    TEXT_PRIMARY: str = "#FFFFFF"
    TEXT_SECONDARY: str = "#A1A1AA"
    TEXT_MUTED: str = "#71717A"
    TEXT_DIM: str = "#52525B"

    # ── Status ─────────────────────────────────────────────────────
    SUCCESS: str = "#22C55E"
    WARNING: str = "#F59E0B"
    ERROR: str = "#EF4444"
    INFO: str = "#38BDF8"

    BORDER: str = "#232329"
    BORDER_SOFT: str = "#1A1A20"
    HOVER_OVERLAY: str = "#2A2A33"


@dataclass(frozen=True)
class Fonts:
    """Schriftfamilien & Größen."""

    FAMILY: str = "Segoe UI"
    FAMILY_BOLD: str = "Segoe UI Semibold"
    MONO: str = "Cascadia Mono"
    # Segoe MDL2 Assets liefert die sauberen UI-Icons mit, die Windows
    # ohnehin mitbringt. Nur unter Windows vorhanden — der Launcher ist
    # Windows-only, daher unkritisch.
    ICON: str = "Segoe MDL2 Assets"

    # Grotesk – Seitentitel
    DISPLAY_SIZE: int = 26
    TITLE_SIZE: int = 17
    # Semibold – Karten-Überschriften
    HEADING_SIZE: int = 15
    # Standard – Fließtext und Listen
    BODY_SIZE: int = 13
    SMALL_SIZE: int = 11
    TINY_SIZE: int = 10
    BUTTON_SIZE: int = 14
    ICON_SIZE: int = 15
    LOGO_SIZE: int = 16


@dataclass(frozen=True)
class Layout:
    """Abstände, Eckenradien und Größenkonstanten."""

    # ── Fenster ────────────────────────────────────────────────────
    WINDOW_WIDTH: int = 1040
    WINDOW_HEIGHT: int = 690
    CORNER_RADIUS_WINDOW: int = 14

    # ── Bereiche ───────────────────────────────────────────────────
    SIDEBAR_WIDTH: int = 236
    TITLEBAR_HEIGHT: int = 52

    # ── Karten ─────────────────────────────────────────────────────
    CARD_CORNER: int = 12
    CARD_PADDING: int = 20
    CARD_GAP: int = 14

    # ── Bedienelemente ─────────────────────────────────────────────
    RADIUS_SMALL: int = 8
    RADIUS_MEDIUM: int = 10
    RADIUS_LARGE: int = 12
    BUTTON_HEIGHT: int = 38
    BUTTON_HEIGHT_LG: int = 52
    NAV_HEIGHT: int = 38

    # ── Alte Bezeichner ────────────────────────────────────────────
    # Werden an mehreren Stellen im Code referenziert. Sie bleiben
    # erhalten, damit bestehende Stellen nicht brechen.
    CORNER_RADIUS_SMALL: int = 8
    CORNER_RADIUS_MEDIUM: int = 10
    CORNER_RADIUS_LARGE: int = 14
    PADDING_LARGE: int = 24
    PADDING_MEDIUM: int = 14
    PADDING_SMALL: int = 8
    WINDOW_MIN_WIDTH: int = 1040
    WINDOW_MIN_HEIGHT: int = 690
    WINDOW_DEFAULT_WIDTH: int = 1040
    WINDOW_DEFAULT_HEIGHT: int = 690
    LOGO_SIZE: tuple = (400, 267)
    BORDER_WIDTH: int = 1


# Kurzabkürzungen für den kompakten Zugriff im UI-Code
COLORS = Colors()
FONTS = Fonts()
LAYOUT = Layout()

# Minecraft-Versionen – als Fallback, wenn der Live-Abruf ausfällt.
# versions.py lädt parallel die aktuelle Liste von Mojang.
MINECRAFT_VERSIONS: list[str] = [
    "26.3",
    "26.2",
    "26.1.2",
    "26.1.1",
    "26.1",
    "1.21.11",
    "1.21.10",
    "1.21.9",
    "1.21.8",
    "1.21.7",
    "1.21.6",
    "1.21.5",
    "1.21.4",
    "1.21.3",
    "1.21.2",
    "1.21.1",
    "1.21",
    "1.20.6",
    "1.20.5",
    "1.20.4",
    "1.20.3",
    "1.20.2",
    "1.20.1",
    "1.20",
    "1.19.4",
    "1.19.3",
    "1.19.2",
    "1.19.1",
    "1.19",
    "1.18.2",
    "1.18.1",
    "1.18",
    "1.17.1",
    "1.17",
]

DEFAULT_VERSION: str = MINECRAFT_VERSIONS[0]
