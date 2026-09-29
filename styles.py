"""
styles.py
---------
Zentrale Design-Konstanten für den Draxo Client Launcher.
Enthält Farben, Schriftarten, Abstände und Eckenradien, damit das
komplette Look & Feel an einer einzigen Stelle gepflegt werden kann.
"""

from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class Colors:
    """Alle im Launcher verwendeten Farben (Hex-Werte)."""

    # ── Basis: dunkler Horion-Look ───────────────────────────────────
    BACKGROUND: str = "#111111"
    SECONDARY: str = "#1A1A1A"
    SURFACE: str = "#232323"
    SURFACE_LIGHT: str = "#2E2E2E"

    # ── Accent: Lila (Draxo-Branding) ────────────────────────────────
    ACCENT: str = "#8B5CF6"
    ACCENT_HOVER: str = "#A855F7"
    ACCENT_GLOW: str = "#C084FC"

    # ── Inject-Button: Grün (wie Horion) ─────────────────────────────
    INJECT_GREEN: str = "#499159"
    INJECT_GREEN_HOVER: str = "#57A968"
    INJECT_GREEN_GLOW: str = "#7BC98E"

    TEXT_PRIMARY: str = "#FFFFFF"
    TEXT_SECONDARY: str = "#B3B3B3"
    TEXT_MUTED: str = "#6E6E6E"

    SUCCESS: str = "#22C55E"
    WARNING: str = "#F59E0B"
    ERROR: str = "#EF4444"
    INFO: str = "#38BDF8"

    BORDER: str = "#2F2F2F"
    CONSOLE_BG: str = "#0A0A0A"


@dataclass(frozen=True)
class Fonts:
    """Schriftfamilien & Größen."""

    FAMILY: str = "Segoe UI"
    FAMILY_BOLD: str = "Segoe UI Semibold"
    MONO: str = "Cascadia Mono"

    TITLE_SIZE: int = 26
    SUBTITLE_SIZE: int = 13
    HEADING_SIZE: int = 16
    BODY_SIZE: int = 13
    SMALL_SIZE: int = 11
    BUTTON_SIZE: int = 18
    CONSOLE_SIZE: int = 11


@dataclass(frozen=True)
class Layout:
    """Abstände, Eckenradien und Größenkonstanten."""

    CORNER_RADIUS_LARGE: int = 20
    CORNER_RADIUS_MEDIUM: int = 14
    CORNER_RADIUS_SMALL: int = 9

    PADDING_LARGE: int = 24
    PADDING_MEDIUM: int = 16
    PADDING_SMALL: int = 10

    WINDOW_MIN_WIDTH: int = 560
    WINDOW_MIN_HEIGHT: int = 640
    WINDOW_DEFAULT_WIDTH: int = 560
    WINDOW_DEFAULT_HEIGHT: int = 760

    LOGO_SIZE: tuple = (400, 267)   # breites_logo_draxo.png (3:2)

    BORDER_WIDTH: int = 1


COLORS = Colors()
FONTS = Fonts()
LAYOUT = Layout()

MINECRAFT_VERSIONS: list[str] = [
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
    "1.21.0",
]

DEFAULT_VERSION: str = MINECRAFT_VERSIONS[0]
