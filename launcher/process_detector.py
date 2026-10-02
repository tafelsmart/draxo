"""
process_detector.py
--------------------
Enthält zwei Klassen:

- ProcessDetector: prüft, ob ein Minecraft-Prozess (javaw.exe oder
  Minecraft.Windows.exe) aktuell läuft.
- SystemMonitor: liest CPU- und RAM-Auslastung über psutil aus.

Beide Klassen sind bewusst leichtgewichtig und werfen niemals
Exceptions nach außen, damit die GUI niemals wegen eines
Monitoring-Fehlers einfrieren oder abstürzen kann.
"""

from __future__ import annotations

import logging
import re
from dataclasses import dataclass
from typing import Optional

import psutil

logger = logging.getLogger("DraxoClient.process_detector")

MINECRAFT_PROCESS_NAMES: tuple[str, ...] = (
    "javaw.exe",
    "java.exe",
    "minecraft.windows.exe",
)

# Erkennt echte Versions-IDs: 1.17, 1.21.11, 26.2, 26.1.2 …
_VERSION_RE = re.compile(r"^(?:1\.\d+(?:\.\d+)?|2[6-9]\.\d+(?:\.\d+)?)$")


def _normalize_version(candidate: str) -> Optional[str]:
    """Liest eine reine Versions-ID aus einem Kandidaten (Profilname,
    Pfadsegment …) heraus. Liefert None, wenn nichts Vernünftiges drin ist."""
    if not candidate:
        return None
    cand = candidate.strip()
    # Direkte Treffer: '26.2', '1.21.11', '1.12.2'
    if _VERSION_RE.match(cand):
        return cand
    # 'versions/26.2/...' oder 'NeoForge-21.11.5' → längsten Teilstring-Treffer
    m = re.search(r"(?:1\.\d+|2[6-9]\.\d+)(?:\.\d+)?", cand)
    return m.group(0) if m else None


def _best_known_match(candidate: str, known_versions: list[str]) -> Optional[str]:
    """Ordnet einen Kandidaten der längsten bekannten Version zu.
    Verhindert, dass aus '1.21.11' ein falsches '1.21.1' wird."""
    if not known_versions:
        return candidate
    hits = [v for v in known_versions if (v in candidate or candidate in v)]
    return max(hits, key=len) if hits else None


@dataclass
class MinecraftStatus:
    """Ergebnis einer Minecraft-Prozessprüfung."""

    running: bool
    process_name: Optional[str] = None
    pid: Optional[int] = None


@dataclass
class MinecraftInstance:
    """Eine konkret erkannte Minecraft-Instanz (Version + Flavor + PID)."""

    version: Optional[str]
    flavor: str
    pid: Optional[int] = None
    process_name: Optional[str] = None

    @property
    def label(self) -> str:
        flavor = FLAVOR_LABELS.get(self.flavor, self.flavor)
        return f"{self.version or '?'} · {flavor}"


@dataclass
class SystemUsage:
    """Ergebnis einer System-Auslastungsmessung."""

    cpu_percent: float
    ram_percent: float


def _cmdline_flavor(cmdline: list[str]) -> str:
    """Erkennt den Instanz-Typ aus einer Kommandozeile.

    Reihenfolge ist entscheidend — siehe Kommentar zu den Fallstricken.
    Rückgabe: "vanilla" | "fabric" | "forge" | "neoforge"
    """
    joined = " ".join(cmdline).lower()
    if not joined:
        return "vanilla"

    # Fabric Loader: eindeutige Marker. MUSS vor der Forge-Prüfung stehen,
    # weil eine Fabric-Instanz Mods laden kann, deren Name "forge" enthält.
    if "fabric-loader" in joined or "net.fabricmc" in joined or "fabric.gameVersion" in joined:
        return "fabric"

    # Labymod 4 startet Minecraft selbst; forge-Verzeichnisse im Classpath
    # sind nur Addon-Stubs und kein Forge-Indikator.
    if "net.labymod" in joined or "labymod" in joined:
        return "vanilla"

    if "neoforge" in joined:
        return "neoforge"
    if "fmlloader" in joined or "cpw.mods" in joined:
        return "forge"
    if "forge" in joined:
        return "forge"
    return "vanilla"


FLAVOR_LABELS = {
    "vanilla": "Vanilla",
    "fabric": "Fabric",
    "forge": "Forge",
    "neoforge": "NeoForge",
}


class ProcessDetector:
    """Erkennt, ob Minecraft aktuell auf dem System läuft."""

    @staticmethod
    def is_minecraft_running() -> MinecraftStatus:
        """
        Iteriert über alle laufenden Prozesse und prüft, ob einer der
        bekannten Minecraft-Prozessnamen enthalten ist.
        """
        try:
            for process in psutil.process_iter(attrs=["pid", "name"]):
                try:
                    name = (process.info.get("name") or "").lower()
                except (psutil.NoSuchProcess, psutil.AccessDenied):
                    continue

                if name in MINECRAFT_PROCESS_NAMES:
                    return MinecraftStatus(
                        running=True,
                        process_name=process.info.get("name"),
                        pid=process.info.get("pid"),
                    )
        except Exception:  # noqa: BLE001
            logger.exception("Fehler bei der Minecraft-Prozesserkennung.")

        return MinecraftStatus(running=False)

    @staticmethod
    def detect_minecraft_profile(
        known_versions: Optional[list[str]] = None,
    ) -> tuple[Optional[str], str]:
        """Ermittelt Version UND Instanz-Typ (vanilla/forge/neoforge) der
        laufenden Minecraft-Instanz aus der Prozess-Kommandozeile
        (--version <id>, versions/<id>/ Pfad oder Klassenpfad — z. B. Forge
        'fmlloader-1.20.1-47.4.20.jar').

        Es werden NUR Versionen akzeptiert, die in der bekannten Liste
        stehen (oder eine saubere eigenständige Versions-ID sind), damit
        Bibliotheks-Versionen wie '1.316.0.7372' nie fälschlich erkannt
        werden. Liefert (None, "vanilla"), wenn kein Minecraft läuft.
        """
        known = known_versions or []
        try:
            for process in psutil.process_iter(attrs=["pid", "name"]):
                try:
                    name = (process.info.get("name") or "").lower()
                except (psutil.NoSuchProcess, psutil.AccessDenied):
                    continue
                if name not in MINECRAFT_PROCESS_NAMES:
                    continue
                try:
                    cmdline = process.cmdline() or []
                except (psutil.NoSuchProcess, psutil.AccessDenied, psutil.ZombieProcess):
                    continue

                result = ProcessDetector._version_from_cmdline(cmdline, known)
                if result:
                    return result, _cmdline_flavor(cmdline)
        except Exception:  # noqa: BLE001
            logger.exception("Fehler bei der Minecraft-Versionserkennung.")
        return None, "vanilla"

    @staticmethod
    def detect_minecraft_version(known_versions: Optional[list[str]] = None) -> Optional[str]:
        """
        Ermittelt die Version der laufenden Minecraft-Instanz aus der
        Prozess-Kommandozeile (--version <id>, versions/<id>/ Pfad oder
        Klassenpfad — z.B. Forge 'fmlloader-1.20.1-47.4.20.jar').

        Es werden NUR Versionen akzeptiert, die in der bekannten Liste
        stehen (oder eine saubere eigenständige Versions-ID sind), damit
        Bibliotheks-Versionen wie '1.316.0.7372' nie fälschlich erkannt
        werden. Liefert None, wenn kein Minecraft läuft.
        """
        version, _flavor = ProcessDetector.detect_minecraft_profile(known_versions)
        return version

    @staticmethod
    def list_instances(known_versions: Optional[list[str]] = None) -> list["MinecraftInstance"]:
        """Alle laufenden Minecraft-Instanzen mit Version und Flavor.

        Wird von der Auswahl im Launcher verwendet, damit der User bei
        mehreren gleichzeitig laufenden Instanzen gezielt wählen kann
        (z.B. Vanilla 1.21.4 UND Fabric 1.21.4 offen).
        """
        known = known_versions or []
        instances: list[MinecraftInstance] = []
        try:
            for process in psutil.process_iter(attrs=["pid", "name"]):
                try:
                    name = (process.info.get("name") or "").lower()
                except (psutil.NoSuchProcess, psutil.AccessDenied):
                    continue
                if name not in MINECRAFT_PROCESS_NAMES:
                    continue
                try:
                    cmdline = process.cmdline() or []
                except (psutil.NoSuchProcess, psutil.AccessDenied, psutil.ZombieProcess):
                    continue

                # _version_from_cmdline liefert nur die Versions-ID (kein
                # Tupel) — der Flavor kommt separat aus der Kommandozeile.
                version = ProcessDetector._version_from_cmdline(cmdline, known)
                if not version:
                    continue
                instances.append(MinecraftInstance(
                    pid=process.info.get("pid"),
                    version=version,
                    flavor=_cmdline_flavor(cmdline),
                    process_name=process.info.get("name"),
                ))
        except Exception:  # noqa: BLE001
            logger.exception("Fehler beim Auflisten der Minecraft-Instanzen.")
        return instances

    @staticmethod
    def _version_from_cmdline(cmdline: list[str], known: list[str]) -> Optional[str]:
        """Liest die MC-Version aus einer Kommandozeile (mehrere Strategien).

        Streng: Es werden NUR Versionen akzeptiert, die in der bekannten
        Liste stehen ODER aus einer autoritativen Quelle stammen
        (--version-Flag, versions/<id>/ Pfad, fmlloader/forge-Jar).
        Reine Bibliotheks-Versionen wie '1.316.0.7372' oder '1.0'
        werden dadurch nie fälschlich erkannt.
        """
        known_set = set(known)
        candidates: list[str] = []

        # ── 1) Explizites --version <id> ─────────────────────────────
        for i, arg in enumerate(cmdline):
            if arg == "--version" and i + 1 < len(cmdline):
                candidates.append(cmdline[i + 1])

        # ── 2) versions/<id>/ im Pfad (offizieller Launcher) ──────────
        for arg in cmdline:
            m = re.search(r"versions[\\/]([^\\/\\s]+)[\\/]", arg)
            if m:
                candidates.append(m.group(1))

        # ── 3) Forge/NeoForge: fmlloader-<mcver>-<forgever>.jar ──────
        for arg in cmdline:
            for m in re.finditer(r"fmlloader-([\d.]+)-[\d.]+[^\\/\\s]*\.jar", arg):
                candidates.append(m.group(1))
            for m in re.finditer(r"forge-([\d.]+)[^\\/\\s]*\.jar", arg):
                candidates.append(m.group(1))

        # ── 4) Argumente, die exakt einer Versions-ID gleichen UND in
        #       der bekannten Liste stehen (kein Bibliotheks-False-Positiv) ──
        for arg in cmdline:
            cand = _normalize_version(arg)
            if cand and cand in known_set:
                candidates.append(cand)

        # ── Auswertung: bekannte Version gewinnt immer ───────────────
        for cand in candidates:
            norm = _normalize_version(cand)
            if not norm:
                continue
            if norm in known_set:
                logger.info("Minecraft-Version erkannt: %s", norm)
                return norm

        # ── Fallback: autoritative Quellen ohne bekannte Liste ───────
        for cand in candidates[:3]:  # nur --version / versions-Pfad / fmlloader
            norm = _normalize_version(cand)
            if norm and _VERSION_RE.match(norm):
                best = _best_known_match(norm, known)
                logger.info("Minecraft-Version erkannt: %s", best or norm)
                return best or norm
        return None


class SystemMonitor:
    """Liest CPU- und RAM-Auslastung des Systems aus."""

    def __init__(self) -> None:
        # Ein initialer Aufruf ohne Intervall "kalibriert" psutil,
        # damit spätere Aufrufe sofort sinnvolle Werte liefern.
        psutil.cpu_percent(interval=None)

    def get_usage(self) -> SystemUsage:
        """Liefert aktuelle CPU- und RAM-Auslastung in Prozent."""
        try:
            cpu = psutil.cpu_percent(interval=None)
            ram = psutil.virtual_memory().percent
            return SystemUsage(cpu_percent=cpu, ram_percent=ram)
        except Exception:  # noqa: BLE001
            logger.exception("Fehler beim Auslesen der Systemauslastung.")
            return SystemUsage(cpu_percent=0.0, ram_percent=0.0)
