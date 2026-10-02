"""
versions.py
-----------
Stellt die unterstützten Minecraft-Versionen für den Draxo Launcher bereit.

Quelle 1: Live-Abruf vom offiziellen Mojang-Versionsmanifest (immer aktuell,
          inkl. neuer Releases wie 26.x, ohne manuelle Pflege).
Quelle 2: Statische Fallback-Liste (1.17 … 26.2), falls offline.

Der Abruf läuft über version_provider.refresh() in einem Hintergrund-Thread;
die UI liest jederzeit über version_provider.get_versions() den aktuellen
Stand (thread-safe über eine Lock).
"""

from __future__ import annotations

import json
import logging
import threading
import urllib.request
from typing import Optional

logger = logging.getLogger("DraxoClient.versions")

MANIFEST_URL = "https://piston-meta.mojang.com/mc/game/version_manifest_v2.json"
USER_AGENT = {"User-Agent": "draxo-launcher/1.0"}

# Statische Fallback-Liste (neueste zuerst). Deckt 1.17 … 26.2 ab.
# Diese Liste wird NUR verwendet, wenn der Live-Abruf fehlschlägt.
STATIC_VERSIONS: list[str] = [
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

# Älteste unterstützte Version (Release >= diese wird angezeigt)
MIN_SUPPORTED = (1, 17)


def _parse_version_tuple(version: str) -> tuple:
    """'26.2' -> (26, 2) | '1.21.11' -> (1, 21, 11) | '1.21.4' -> (1, 21, 4)."""
    result = []
    for part in version.split("."):
        digits = ""
        for ch in part:
            if ch.isdigit():
                digits += ch
            else:
                break
        if digits:
            result.append(int(digits))
    return tuple(result)


def fetch_live_versions(timeout: int = 20) -> Optional[list[str]]:
    """Holt alle Release-Versionen ab 1.17 vom Mojang-Manifest (neueste zuerst).

    Liefert None bei Netzwerk-/Parsefehlern — dann nutzt der Aufrufer die
    statische Fallback-Liste.
    """
    try:
        req = urllib.request.Request(MANIFEST_URL, headers=USER_AGENT)
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            manifest = json.loads(resp.read().decode("utf-8"))
        versions = [
            v["id"]
            for v in manifest.get("versions", [])
            if v.get("type") == "release" and _parse_version_tuple(v["id"]) >= MIN_SUPPORTED
        ]
        # Das Manifest ist bereits neueste-zuerst sortiert; zur Sicherheit
        # noch einmal explizit sortieren (semantisch, nicht lexikografisch).
        versions.sort(key=_parse_version_tuple, reverse=True)
        return versions or None
    except Exception:  # noqa: BLE001
        logger.exception("Live-Versionsliste konnte nicht geladen werden.")
        return None


class VersionProvider:
    """Thread-sicherer Versions-Provider: Live-Abruf mit statischem Fallback."""

    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._versions: list[str] = list(STATIC_VERSIONS)

    def get_versions(self) -> list[str]:
        """Aktuelle Liste (Live-Ergebnis oder Fallback), neueste zuerst."""
        with self._lock:
            return list(self._versions)

    def refresh(self) -> list[str]:
        """Versucht den Live-Abruf; aktualisiert die Liste bei Erfolg.

        Liefert die aktuelle Liste zurück (Live oder unverändert Fallback).
        """
        live = fetch_live_versions()
        if live:
            with self._lock:
                self._versions = live
            return list(live)
        return self.get_versions()


# Einzelinstanz, die von ui.py importiert wird
version_provider = VersionProvider()
