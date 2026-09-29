"""
config.py
---------
Verwaltet die persistente Konfiguration des Draxo Client in einer
config.json-Datei: zuletzt gewählte Minecraft-Version, Fenstergröße
und Fensterposition. Die Datei wird automatisch angelegt, falls sie
nicht existiert, und robust gegen fehlerhafte/fehlende Werte geladen.
"""

from __future__ import annotations

import json
import logging
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any

from styles import DEFAULT_VERSION, LAYOUT
from utils import get_config_dir

logger = logging.getLogger("DraxoClient.config")

CONFIG_FILENAME = "config.json"


@dataclass
class AppConfig:
    """Datenmodell der gespeicherten Konfiguration."""

    last_version: str = DEFAULT_VERSION
    window_width: int = LAYOUT.WINDOW_DEFAULT_WIDTH
    window_height: int = LAYOUT.WINDOW_DEFAULT_HEIGHT
    window_x: int = -1
    window_y: int = -1
    theme: str = "dark"
    particles_enabled: bool = True
    volume_effects_enabled: bool = True
    # Ersteinrichtung: wird True gesetzt, wenn pip-Requirements erfolgreich
    # installiert wurden. Setzt der Nutzer dies auf False, folgt beim nächsten
    # Start eine erneute Prüfung.
    setup_done: bool = False
    # Pfad zur Python-Executable (leer = wird beim nächsten Start neu gesucht)
    python_path: str = ""

    def to_dict(self) -> dict[str, Any]:
        return asdict(self)

    @staticmethod
    def from_dict(data: dict[str, Any]) -> "AppConfig":
        defaults = AppConfig()
        return AppConfig(
            last_version=str(data.get("last_version", defaults.last_version)),
            window_width=int(data.get("window_width", defaults.window_width)),
            window_height=int(data.get("window_height", defaults.window_height)),
            window_x=int(data.get("window_x", defaults.window_x)),
            window_y=int(data.get("window_y", defaults.window_y)),
            theme=str(data.get("theme", defaults.theme)),
            particles_enabled=bool(data.get("particles_enabled", defaults.particles_enabled)),
            volume_effects_enabled=bool(
                data.get("volume_effects_enabled", defaults.volume_effects_enabled)
            ),
            setup_done=bool(data.get("setup_done", defaults.setup_done)),
            python_path=str(data.get("python_path", defaults.python_path)),
        )


class ConfigManager:
    """
    Kümmert sich um Laden, Speichern und Zugriff auf die Konfiguration.
    Wird als einzelne Instanz im Launcher verwendet (kein globaler State,
    sondern über das MainWindow-Objekt injiziert).
    """

    def __init__(self, config_dir: Path | None = None) -> None:
        self._config_dir: Path = config_dir if config_dir is not None else get_config_dir()
        self._config_path: Path = self._config_dir / CONFIG_FILENAME
        self._config: AppConfig = AppConfig()
        self.load()

    @property
    def config(self) -> AppConfig:
        return self._config

    @property
    def path(self) -> Path:
        return self._config_path

    def load(self) -> AppConfig:
        """Lädt die Konfiguration von der Festplatte oder erstellt Defaults."""
        if not self._config_path.exists():
            logger.info("Keine config.json gefunden, erstelle Standardkonfiguration.")
            self._config = AppConfig()
            self.save()
            return self._config

        try:
            with self._config_path.open("r", encoding="utf-8") as file_handle:
                raw_data = json.load(file_handle)
            self._config = AppConfig.from_dict(raw_data)
            logger.info("Konfiguration erfolgreich geladen: %s", self._config_path)
        except (json.JSONDecodeError, OSError) as error:
            logger.error("Konfiguration konnte nicht gelesen werden (%s). Nutze Defaults.", error)
            self._config = AppConfig()
            self.save()

        return self._config

    def save(self) -> None:
        """Speichert die aktuelle Konfiguration auf die Festplatte."""
        try:
            self._config_dir.mkdir(parents=True, exist_ok=True)
            with self._config_path.open("w", encoding="utf-8") as file_handle:
                json.dump(self._config.to_dict(), file_handle, indent=4, ensure_ascii=False)
            logger.debug("Konfiguration gespeichert: %s", self._config_path)
        except OSError:
            logger.exception("Konfiguration konnte nicht gespeichert werden.")

    def update_last_version(self, version: str) -> None:
        self._config.last_version = version
        self.save()

    def update_setup_done(self, done: bool, python_path: str = "") -> None:
        """Markiert die Ersteinrichtung als abgeschlossen und speichert den Python-Pfad."""
        self._config.setup_done = done
        if python_path:
            self._config.python_path = python_path
        self.save()

    def update_python_path(self, python_path: str) -> None:
        self._config.python_path = python_path
        self.save()

    def update_window_geometry(self, width: int, height: int, x: int, y: int) -> None:
        self._config.window_width = width
        self._config.window_height = height
        self._config.window_x = x
        self._config.window_y = y
        self.save()
