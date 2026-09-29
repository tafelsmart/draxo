"""
builder_runner.py
-----------------
Führt den Build+Inject des Draxo Clients aus — vollständig STANDALONE.

Zwei Betriebsarten:

1. Dev-Modus (Python aus dem Repo):
   tools/vanilla_builder.py wird von der Festplatte geladen, alle Pfade
   zeigen in den Projekt-Root.

2. Frozen-Standalone (.exe, keine weiteren Dateien):
   tools.vanilla_builder ist als MODUL eingebettet (PyInstaller) — es wird
   kein tools/-Ordner mehr von der Festplatte gebraucht.
   Vorgefertigte DLLs liegen im Bundle unter _MEIPASS/prebuilt/ und werden
   beim ersten Inject in einen STABILEN Ordner neben der .exe entpackt
   (damit draxo_config.ini mit dem Lizenz-Key dauerhaft dort liegt).

Die DLL-Auflösung läuft über vanilla_builder.find_prebuilt_dll() — sie
prüft zuerst den stabilen Ordner, dann das Bundle.
"""

from __future__ import annotations

import importlib.util
import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Callable, Optional

try:
    from utils import get_workdir
except Exception:  # pragma: no cover - standalone
    def get_workdir() -> Path:
        return Path(sys.executable).resolve().parent if getattr(
            sys, "frozen", False) else Path(__file__).resolve().parent


# DLL-Dateiname je Flavor. Muss mit FLAVOR_DLL in tools/vanilla_builder.py
# übereinstimmen — die Zuordnung entscheidet, ob die zum Spiel passende
# Namensauflösung (obfuskiert / intermediary / offiziell) geladen wird.
_FLAVOR_DLL = {
    "vanilla": "draxo.dll",
    "fabric": "draxo_fabric.dll",
    "forge": "draxo_forge.dll",
    "neoforge": "draxo_forge.dll",
}


def _load_builder(workdir: Path):
    """Lädt den Builder — als MODUL (frozen) oder von Disk (dev)."""
    root = str(workdir.resolve())

    try:
        # ── Frozen-Standalone: tools ist ein Paket im Bundle ─────────
        from tools import vanilla_builder as mod  # noqa: PLC0415
    except ImportError:
        # ── Dev-Modus: von der Festplatte laden ─────────────────────
        builder_path = workdir / "tools" / "vanilla_builder.py"
        if not builder_path.exists():
            raise FileNotFoundError(f"Builder nicht gefunden: {builder_path}")
        spec = importlib.util.spec_from_file_location("vanilla_builder", builder_path)
        if spec is None or spec.loader is None:
            raise RuntimeError(f"Builder konnte nicht geladen werden: {builder_path}")
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)

    # Pfade auf den echten Projekt-Root umbiegen (Frozen: neben der .exe)
    mod.ROOT = root
    mod.SRC_DIR = os.path.join(root, "src")
    mod.MAPPINGS_H = os.path.join(root, "src", "config", "mappings.h")
    mod.CACHE_DIR = os.path.join(root, "tools", "cache")
    mod.BUILD_DIR = os.path.join(root, "build", "vanilla")
    mod.TRANS_SRC = os.path.join(root, "build", "vanilla_src")
    return mod


class _Sink:
    """Ersetzt stdout/stderr und reicht jede Zeile an einen Callback."""

    def __init__(self, callback: Callable[[str, str], None], stream_type: str):
        self._callback = callback
        self._stream_type = stream_type
        self._buffer = ""

    def write(self, data: str) -> int:
        if not data:
            return 0
        self._buffer += data
        while "\n" in self._buffer:
            line, self._buffer = self._buffer.split("\n", 1)
            if line:
                self._callback(line, self._stream_type)
        return len(data)

    def flush(self) -> None:  # noqa: D401
        if self._buffer:
            self._callback(self._buffer, self._stream_type)
            self._buffer = ""


def _toolchain_ok(mod) -> bool:
    """Prüft, ob CMake + MSVC/VS (für den DLL-Build) vorhanden sind.

    Nur relevant, wenn keine vorgefertigte DLL existiert. Bei fehlendem
    CMake wird eine stille Installation via winget versucht.
    """
    try:
        cmake = mod.find_cmake()
        if cmake:
            return True
        print("[!] CMake nicht gefunden — installiere via winget …")
        cmd = [
            "winget", "install", "--id", "Kitware.CMake",
            "--exact", "--silent", "--accept-package-agreements",
            "--accept-source-agreements", "--disable-interactivity",
        ]
        _no_win = subprocess.CREATE_NO_WINDOW if hasattr(subprocess, "CREATE_NO_WINDOW") else 0
        try:
            subprocess.run(cmd, creationflags=_no_win, timeout=300, check=False)
        except Exception:  # noqa: BLE001
            pass
        return bool(mod.find_cmake())
    except Exception:  # noqa: BLE001
        return False


def _is_bundle_path(path: str) -> bool:
    """True, wenn der Pfad im temporären PyInstaller-Bundle (_MEIPASS) liegt.

    DLLs dürfen NICHT direkt aus _MEIPASS injiziert werden: draxo_config.ini
    (Lizenz-Key) wird vom Spiel NEBEN die DLL geschrieben. _MEIPASS ist ein
    Temp-Ordner und wird beim Beenden der .exe gelöscht → Key wäre weg.
    """
    bundle = getattr(sys, "_MEIPASS", None)
    if not bundle:
        return False
    try:
        return Path(path).resolve().is_relative_to(Path(bundle).resolve())
    except Exception:  # noqa: BLE001
        return False


def _extract_prebuilt(workdir: Path, version: str, forge: bool,
                      flavor: Optional[str] = None) -> Optional[str]:
    """Entpackt eine vorgefertigte DLL aus dem Bundle in den stabilen Ordner.

    Rückgabe: Pfad zur entpackten DLL oder None, wenn keine im Bundle liegt.
    """
    bundle = getattr(sys, "_MEIPASS", None)
    if not bundle:
        return None
    if flavor is None:
        flavor = "forge" if forge else "vanilla"
    name = _FLAVOR_DLL.get(flavor, "draxo.dll")
    src = Path(bundle) / "prebuilt" / version / name
    if not src.exists():
        return None
    dst_dir = workdir / "prebuilt" / version
    dst_dir.mkdir(parents=True, exist_ok=True)
    dst = dst_dir / name
    # Nur kopieren, wenn fehlt oder anders (mtime/größe) — kein Key-Verlust
    if not dst.exists() or dst.stat().st_size != src.stat().st_size:
        shutil.copy2(src, dst)
        print(f"[+] Vorgefertigte DLL entpackt: {dst}")
    return str(dst)


def _ensure_build_chain(workdir: Path) -> None:
    """Entpackt die eingebettete Build-Quelle (src/, CMakeLists.txt,
    minhook-master/) aus dem Bundle in den Arbeitsordner — nur wenn der
    Build tatsächlich gebraucht wird und die Dateien fehlen.
    """
    bundle = getattr(sys, "_MEIPASS", None)
    if not bundle:
        return
    bundle = Path(bundle)
    # Entpackt nur, wenn NICHT bereits vorhanden (nicht überschreiben)
    for rel in ("src", "CMakeLists.txt", "minhook-master"):
        src = bundle / rel
        if not src.exists():
            continue
        dst = workdir / rel
        if dst.exists():
            continue
        print(f"[+] Entpacke Build-Dateien: {rel}")
        if src.is_dir():
            shutil.copytree(src, dst)
        else:
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)


def prebuilt_available(version: str, flavor: str) -> tuple[bool, str]:
    """Gibt es für Version+Flavor eine einsatzbereite DLL?

    Liefert (verfügbar, hinweis). Der Hinweis erklärt, was beim Build
    passiert — die UI zeigt ihn an, damit klar ist, warum eine Auswahl
    noch keinen Inject ermöglicht.
    """
    if not version or version == "?":
        return False, "Keine Version gewählt."
    label = {"vanilla": "Vanilla", "fabric": "Fabric",
             "forge": "Forge", "neoforge": "NeoForge"}.get(flavor, flavor)
    try:
        mod = _load_builder(get_workdir())
        path = mod.find_prebuilt_dll(version, flavor=flavor)
    except Exception:  # noqa: BLE001
        # Builder nicht ladbar (z.B. im eingebetteten Bundle) — nicht
        # spekulieren, nur den neutralen Hinweis geben.
        return False, f"{version} · {label}: keine vorgefertigte DLL (wird gebaut)"
    if path:
        return True, f"●  {version} · {label} — bereit"
    return False, (
        f"{version} · {label}: keine vorgefertigte DLL — "
        "wird beim Inject aus dem Quellcode gebaut")


def run_build(
    version: str,
    workdir: Path,
    output: Optional[Callable[[str, str], None]] = None,
    forge: bool = False,
    flavor: Optional[str] = None,
) -> tuple[int, Optional[str]]:
    """Führt den kompletten Build+Inject für Version+Flavor aus.

    flavor: "vanilla" | "fabric" | "forge" | "neoforge"
    ``forge`` bleibt als Rückwärtskompatibilität (impliziert flavor="forge").

    Reihenfolge:
      1. Vorgefertigte DLL (stabiler Ordner ODER Bundle)? → direkt injizieren
      2. Sonst: Toolchain prüfen + bauen (nur mit Visual Studio möglich)

    Rückgabe: (exit_code, error_message_or_None)
    """
    if flavor is None:
        flavor = "forge" if forge else "vanilla"
    flavor = flavor.lower()
    callback = output or (lambda line, typ: None)
    old_stdout, old_stderr = sys.stdout, sys.stderr
    old_argv = sys.argv

    try:
        mod = _load_builder(workdir)
        sys.stdout = _Sink(callback, "stdout")  # type: ignore[assignment]
        sys.stderr = _Sink(callback, "stderr")  # type: ignore[assignment]
        sys.argv = ["vanilla_builder", "--version", version]
        if flavor and flavor != "vanilla":
            sys.argv += ["--flavor", flavor]

        # ── 1) Vorgefertigte DLL? ────────────────────────────────────
        #    a) stabiler Ordner neben der .exe (z. B. aus früherem Inject)
        prebuilt = mod.find_prebuilt_dll(version, flavor=flavor)
        if prebuilt and not _is_bundle_path(prebuilt):
            print(f"[+] Vorgefertigte DLL gefunden ({flavor}): {prebuilt}")
            print("    Kein Build nötig — injiziere direkt.")
            try:
                mod.inject(prebuilt, version=version, flavor=flavor)
            except RuntimeError as exc:
                print(f"[!] Injection übersprungen: {exc}")
                return 1, str(exc)
            return 0, None

        #    b) Bundle (_MEIPASS) → in stabilen Ordner entpacken + injizieren
        #       (NIE direkt aus _MEIPASS injizieren — Config/Key ginge verloren)
        extracted = _extract_prebuilt(workdir, version, forge, flavor=flavor)
        if extracted:
            print(f"[+] Vorgefertigte DLL aus Bundle entpackt: {extracted}")
            print("    Kein Build nötig — injiziere direkt.")
            try:
                mod.inject(extracted, version=version, flavor=flavor)
            except RuntimeError as exc:
                print(f"[!] Injection übersprungen: {exc}")
                return 1, str(exc)
            return 0, None

        # ── 2) Keine vorgefertigte DLL → Toolchain-Build ────────────
        #    Im Frozen-Standalone zuerst die eingebettete Quelle entpacken
        print(f"[*] Keine vorgefertigte DLL für {version} ({flavor}) — "
              "starte Build aus dem Quellcode.")
        _ensure_build_chain(workdir)
        if not _toolchain_ok(mod):
            print("[!] Keine vorgefertigte DLL für diese Version im Bundle.")
            print("    Und CMake/Visual Studio Build Tools fehlen.")
            print("    Installation: https://visualstudio.microsoft.com/de/downloads/#build-tools")
            print("    (Workload: 'Desktop development with C++')")
            return 1, "Keine vorgefertigte DLL und keine Build-Tools vorhanden"

        mod.main()
        return 0, None
    except SystemExit as exc:
        code = exc.code if isinstance(exc.code, int) else 1
        return code, (str(exc) if exc.code else None)
    except Exception as exc:  # noqa: BLE001
        callback(str(exc), "stderr")
        return 1, str(exc)
    finally:
        sys.stdout, sys.stderr = old_stdout, old_stderr
        sys.argv = old_argv
