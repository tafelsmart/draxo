"""
utils.py
--------
Allgemeine Hilfsfunktionen, die vom gesamten Draxo Client verwendet werden:
Pfad-Auflösung (auch für PyInstaller-Builds), Bild-Laden, Fenster-Zentrierung
und kleine Formatierungs-Helfer.
"""

from __future__ import annotations

import logging
import os
import sys
import tkinter as tk
from pathlib import Path
from typing import Optional

from PIL import Image

logger = logging.getLogger("DraxoClient.utils")


def get_base_dir() -> Path:
    """
    Liefert das Verzeichnis, in dem eingebettete Ressourcen (image.png,
    draxo.ico, VERSION …) liegen.

    - Im PyInstaller-Bundle (.exe): sys._MEIPASS  (temporäres Entpack-Dir)
    - Im normalen Python-Betrieb:  Ordner der utils.py
    """
    if getattr(sys, "frozen", False) and hasattr(sys, "_MEIPASS"):
        return Path(sys._MEIPASS)  # type: ignore[attr-defined]
    return Path(os.path.dirname(os.path.abspath(__file__)))


def get_workdir() -> Path:
    """
    Liefert den Projekt-Root — also den Ordner, in dem ``tools/`` liegt
    und in dem ``python tools/vanilla_builder.py`` ausgeführt werden soll.

    Drei Fälle werden unterschieden:

    1. **Frozen (.exe)**: Die .exe liegt selbst im Projekt-Root
       (User hat DraxoLauncher.exe direkt in den Draxo-Client-Ordner gelegt).
       → ``Path(sys.executable).parent``

    2. **Dev-Modus, Launcher im Projekt-Root**: utils.py liegt auf gleicher
       Ebene wie ``tools/``.
       → ``Path(__file__).resolve().parent``

    3. **Dev-Modus, Launcher in Unterordner**: utils.py liegt eine Ebene
       unterhalb des Projekt-Roots (z. B. ``launcher/``).
       → ``Path(__file__).resolve().parent.parent``

    Als letzter Fallback wird das aktuelle Arbeitsverzeichnis verwendet.
    """
    if getattr(sys, "frozen", False):
        # Im Bundle liegt die .exe direkt im Projekt-Root
        return Path(sys.executable).resolve().parent

    # Dev-Modus: auto-detect
    here = Path(os.path.abspath(__file__)).resolve().parent
    for candidate in (here, here.parent, Path.cwd()):
        if (candidate / "tools").exists():
            return candidate

    # Letzter Fallback: Ordner der utils.py
    logger.warning(
        "tools/-Verzeichnis nicht gefunden. Nutze Fallback: %s", here
    )
    return here


def resource_path(filename: str) -> Path:
    """Löst den absoluten Pfad einer Ressource im App-Verzeichnis auf."""
    return get_base_dir() / filename


def get_config_dir() -> Path:
    """
    Verzeichnis, in dem die config.json abgelegt wird.
    Liegt im selben Ordner wie die Anwendung, damit sie portabel bleibt.
    """
    return get_base_dir()


def load_image_safe(path: Path, size: Optional[tuple[int, int]] = None) -> Optional[Image.Image]:
    """
    Lädt ein Bild sicher via Pillow. Gibt None zurück, falls die Datei
    nicht existiert oder nicht gelesen werden kann, statt eine Exception
    nach oben zu werfen.
    """
    try:
        if not path.exists():
            logger.warning("Bilddatei nicht gefunden: %s", path)
            return None
        image = Image.open(path)
        image = image.convert("RGBA")
        if size is not None:
            image = image.resize(size, Image.LANCZOS)
        return image
    except Exception:  # noqa: BLE001
        logger.exception("Fehler beim Laden des Bildes: %s", path)
        return None


def center_window(window: tk.Misc, width: int, height: int) -> None:
    """Zentriert ein Tkinter/CTk-Fenster auf dem Bildschirm."""
    try:
        screen_width = window.winfo_screenwidth()
        screen_height = window.winfo_screenheight()
        x = int((screen_width - width) / 2)
        y = int((screen_height - height) / 2)
        window.geometry(f"{width}x{height}+{x}+{y}")
    except Exception:  # noqa: BLE001
        logger.exception("Fenster konnte nicht zentriert werden.")


def format_percentage(value: float) -> str:
    """Formatiert einen Prozentwert einheitlich, z. B. '42%'."""
    try:
        return f"{value:.0f}%"
    except Exception:  # noqa: BLE001
        return "--%"


def clamp(value: float, minimum: float, maximum: float) -> float:
    """Begrenzt einen Wert auf einen Bereich [minimum, maximum]."""
    return max(minimum, min(maximum, value))


def lerp(start: float, end: float, t: float) -> float:
    """Lineare Interpolation zwischen start und end anhand von t in [0, 1]."""
    return start + (end - start) * t


def hex_to_rgb(hex_color: str) -> tuple[int, int, int]:
    """Wandelt einen Hex-Farbwert (#RRGGBB) in ein RGB-Tupel um."""
    hex_color = hex_color.lstrip("#")
    return tuple(int(hex_color[i : i + 2], 16) for i in (0, 2, 4))  # type: ignore[return-value]


def rgb_to_hex(rgb: tuple[int, int, int]) -> str:
    """Wandelt ein RGB-Tupel in einen Hex-Farbwert um."""
    return "#{:02x}{:02x}{:02x}".format(
        int(clamp(rgb[0], 0, 255)),
        int(clamp(rgb[1], 0, 255)),
        int(clamp(rgb[2], 0, 255)),
    )


def blend_colors(color_a: str, color_b: str, t: float) -> str:
    """Blendet zwischen zwei Hex-Farben anhand des Faktors t in [0, 1]."""
    rgb_a = hex_to_rgb(color_a)
    rgb_b = hex_to_rgb(color_b)
    blended = tuple(lerp(rgb_a[i], rgb_b[i], t) for i in range(3))
    return rgb_to_hex(blended)  # type: ignore[arg-type]


def find_python_executable() -> Optional[str]:
    """
    Sucht eine nutzbare Python-3-Installation auf dem System.

    Reihenfolge:
    1. Namen im PATH: ``python``, ``python3``, ``py``
    2. Typische Windows-Installationspfade unter AppData und C:\\
    3. Gibt ``None`` zurück, wenn nichts gefunden wurde.

    Der Rückgabewert ist ein absoluter Pfad-String, der direkt in
    ``subprocess.Popen`` übergeben werden kann.
    """
    import shutil as _shutil

    # ── Schritt 1: PATH ────────────────────────────────────────────────────
    for name in ("python", "python3", "py"):
        path = _shutil.which(name)
        if not path:
            continue
        try:
            result = subprocess.run(
                [path, "--version"],
                capture_output=True,
                text=True,
                timeout=5,
            )
            # Akzeptiere nur Python 3
            version_text = (result.stdout + result.stderr).strip()
            if result.returncode == 0 and "Python 3" in version_text:
                return path
        except Exception:  # noqa: BLE001
            continue

    # ── Schritt 2: Typische Windows-Pfade ──────────────────────────────────
    import os as _os
    local_app = _os.environ.get("LOCALAPPDATA", "")
    candidates: list[Path] = []

    if local_app:
        py_base = Path(local_app) / "Programs" / "Python"
        if py_base.is_dir():
            for entry in sorted(py_base.iterdir(), reverse=True):
                exe = entry / "python.exe"
                if exe.exists():
                    candidates.append(exe)

    # Klassische systemweite Installationen
    for drive in ("C:/", "D:/"):
        for folder_name in (
            "Python312", "Python311", "Python310", "Python39",
            "Python313", "Python38",
        ):
            exe = Path(drive) / folder_name / "python.exe"
            if exe.exists():
                candidates.append(exe)

    for candidate in candidates:
        try:
            result = subprocess.run(
                [str(candidate), "--version"],
                capture_output=True,
                text=True,
                timeout=5,
            )
            if result.returncode == 0:
                return str(candidate)
        except Exception:  # noqa: BLE001
            continue

    return None



import subprocess  # noqa: E402 (für find_python_executable)


def setup_logging(log_file: Path) -> logging.Logger:
    """Richtet ein einfaches File+Console Logging für die App ein."""
    logger_root = logging.getLogger("DraxoClient")
    logger_root.setLevel(logging.DEBUG)

    if logger_root.handlers:
        return logger_root

    formatter = logging.Formatter(
        "%(asctime)s | %(levelname)-8s | %(name)s | %(message)s",
        datefmt="%H:%M:%S",
    )

    try:
        file_handler = logging.FileHandler(log_file, encoding="utf-8")
        file_handler.setFormatter(formatter)
        file_handler.setLevel(logging.DEBUG)
        logger_root.addHandler(file_handler)
    except Exception:  # noqa: BLE001
        pass

    console_handler = logging.StreamHandler(sys.stdout)
    console_handler.setFormatter(formatter)
    console_handler.setLevel(logging.INFO)
    logger_root.addHandler(console_handler)

    return logger_root
