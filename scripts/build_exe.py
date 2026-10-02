"""
build_exe.py
------------
Erstellt die .exe-Dateien via PyInstaller.

Verwendung:
    python scripts/build_exe.py                  → DraxoLauncher.exe (Release)
    python scripts/build_exe.py --dev            → DraxoDev.exe (Dev-Variante)
    python scripts/build_exe.py --both           → beide nacheinander
    python scripts/build_exe.py --no-clean       → Ohne vorherigen Cleanup
    python scripts/build_exe.py --copy-to-root   → Kopiert .exe in Projekt-Root

Die Dev-Variante bindet scripts/dev_mode_hook.py ein und setzt damit beim
Start DRAXO_DEV_MODE=1 — sie ueberspringt die Discord-Anmeldung, laesst aber
Netz und Lizenzpruefung unangetastet.

Voraussetzungen:
    pip install pyinstaller
    (oder: pip install -r requirements.txt)
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import time
from pathlib import Path

# Windows-Konsolen-Encoding fixen: Die Box-Zeichen (─) in der Ausgabe
# crashen sonst mit cp1252. Ohne diesen Guard schlägt der Build fehl,
# bevor PyInstaller überhaupt startet.
if hasattr(sys.stdout, "reconfigure"):
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:  # noqa: BLE001
        pass
if hasattr(sys.stderr, "reconfigure"):
    try:
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")
    except Exception:  # noqa: BLE001
        pass

#: Dieses Skript liegt in scripts/, der Projekt-Root ist eine Ebene hoeher.
BASE_DIR = Path(__file__).resolve().parent.parent
DIST_DIR = BASE_DIR / "dist"
BUILD_DIR = BASE_DIR / "build"

#: Die beiden Bauvarianten. "work" ist der PyInstaller-Zwischenordner unter
#: build/ — der darf NICHT mit build/prebuilt oder build/vanilla kollidieren,
#: sonst loescht der Cleanup die vorgebauten DLLs.
VARIANTS: dict[str, dict[str, str]] = {
    "release": {
        "spec": "draxo_launcher.spec",
        "exe": "DraxoLauncher.exe",
        "work": "draxo_launcher",
    },
    "dev": {
        "spec": "draxo_dev.spec",
        "exe": "DraxoDev.exe",
        "work": "draxo_dev",
    },
}


# ── Hilfsfunktionen ────────────────────────────────────────────────────────────

def _log(prefix: str, message: str) -> None:
    print(f"  [{prefix}] {message}")


def _hr() -> None:
    print("─" * 62)


def clean_previous_build() -> None:
    """Löscht nur die PyInstaller-Artefakte aus einem früheren Durchlauf.

    WICHTIG: Der Ordner build/ enthält auch build/prebuilt/ (vorgefertigte
    DLLs) und build/vanilla/ (die gebaute draxo.dll + config). Diese dürfen
    NIEMALS gelöscht werden — sonst ist der Toolchain-freie Inject kaputt.
    Gelöscht werden nur dist/ und die beiden PyInstaller-Zwischenordner
    (build/draxo_launcher/ und build/draxo_dev/).
    """
    if DIST_DIR.exists():
        _log("CLEAN", f"Lösche {DIST_DIR.name}/ ...")
        shutil.rmtree(DIST_DIR, ignore_errors=True)
    for v in VARIANTS.values():
        pyi_work = BUILD_DIR / v["work"]
        if pyi_work.exists():
            _log("CLEAN", f"Lösche build/{pyi_work.name}/ ...")
            shutil.rmtree(pyi_work, ignore_errors=True)
    _log("CLEAN", "Fertig.")


def check_spec_file(spec_path: Path) -> None:
    if not spec_path.exists():
        raise FileNotFoundError(
            f"{spec_path.name} nicht gefunden: {spec_path}\n"
            "Stelle sicher, dass du build_exe.py aus dem Projekt ausführst."
        )
    _log("CHECK", f"Spec-Datei gefunden: {spec_path.name}")


def ensure_pyinstaller() -> None:
    """Installiert PyInstaller automatisch, falls es fehlt."""
    try:
        import PyInstaller
        _log("CHECK", f"PyInstaller {PyInstaller.__version__} gefunden.")
    except ImportError:
        _log("INSTALL", "PyInstaller nicht gefunden, installiere via pip ...")
        subprocess.check_call(
            [sys.executable, "-m", "pip", "install", "--quiet", "pyinstaller>=6.0.0"]
        )
        _log("INSTALL", "PyInstaller installiert.")


def ensure_dependencies() -> None:
    """Prüft, ob alle Pakete aus requirements.txt installiert sind."""
    req_file = BASE_DIR / "requirements.txt"
    if not req_file.exists():
        _log("CHECK", "requirements.txt nicht gefunden, überspringe Dependency-Check.")
        return
    _log("CHECK", "Installiere / aktualisiere Abhängigkeiten aus requirements.txt ...")
    result = subprocess.run(
        [sys.executable, "-m", "pip", "install", "--quiet", "-r", str(req_file)],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        _log("WARN", f"pip install gab Fehler zurück:\n{result.stderr.strip()}")
    else:
        _log("CHECK", "Abhängigkeiten OK.")


def run_pyinstaller(spec_path: Path) -> None:
    """Führt PyInstaller mit der .spec-Datei aus."""
    cmd = [
        sys.executable, "-m", "PyInstaller",
        str(spec_path),
        "--noconfirm",
        "--clean",
    ]
    _log("BUILD", f"Starte: {' '.join(cmd)}")
    start = time.perf_counter()
    result = subprocess.run(cmd, cwd=str(BASE_DIR))
    elapsed = time.perf_counter() - start

    if result.returncode != 0:
        raise RuntimeError(
            f"PyInstaller fehlgeschlagen (Exit-Code {result.returncode}). "
            f"Sieh dir die Ausgabe oben an."
        )
    _log("BUILD", f"PyInstaller abgeschlossen in {elapsed:.1f}s.")


def verify_output(exe_name: str) -> Path:
    """Prüft, ob die .exe erzeugt wurde, und gibt ihren Pfad zurück."""
    exe_path = DIST_DIR / exe_name
    if not exe_path.exists():
        raise FileNotFoundError(
            f"{exe_name} wurde nicht erzeugt.\n"
            f"Erwartet unter: {exe_path}"
        )
    size_mb = exe_path.stat().st_size / 1_048_576
    _log("OK", f"{exe_name} erstellt ({size_mb:.1f} MB)")
    _log("OK", f"Pfad: {exe_path}")
    return exe_path


def copy_to_root(exe_path: Path) -> None:
    """Kopiert die fertige .exe in den Projekt-Root (optional)."""
    dest = BASE_DIR / exe_path.name
    shutil.copy2(exe_path, dest)
    _log("COPY", f"{exe_path.name} → {dest}")


# ── Haupt-Funktion ─────────────────────────────────────────────────────────────

def build_variant(variant: str, copy_to_root_flag: bool = False) -> Path:
    """Baut genau eine Variante (release oder dev)."""
    spec = VARIANTS[variant]
    spec_path = BASE_DIR / spec["spec"]
    exe_name = spec["exe"]

    _hr()
    print(f"  Draxo Client — Build ({variant})")
    _hr()

    check_spec_file(spec_path)
    ensure_pyinstaller()
    ensure_dependencies()
    print("\n[4/5] .exe bauen ...")
    run_pyinstaller(spec_path)

    print("\n[5/5] Ergebnis prüfen ...")
    exe_path = verify_output(exe_name)

    if copy_to_root_flag:
        print("\n[+] Kopiere ins Projekt-Root ...")
        copy_to_root(exe_path)

    return exe_path


def main(no_clean: bool = False, copy_to_root_flag: bool = False,
         dev: bool = False, both: bool = False) -> int:
    variants = ["release", "dev"] if both else (["dev"] if dev else ["release"])

    if not no_clean:
        clean_previous_build()

    for variant in variants:
        build_variant(variant, copy_to_root_flag)

    _hr()
    print("  Build erfolgreich! Starten per Doppelklick:")
    for variant in variants:
        print(f"  → {DIST_DIR / VARIANTS[variant]['exe']}")
    _hr()
    return 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Draxo Client Build-Script")
    parser.add_argument("--no-clean", action="store_true", help="dist/ und build/ nicht löschen")
    parser.add_argument("--copy-to-root", action="store_true", help=".exe in Projekt-Root kopieren")
    parser.add_argument("--dev", action="store_true",
                        help="DraxoDev.exe bauen (Dev-Variante, DRAXO_DEV_MODE=1)")
    parser.add_argument("--both", action="store_true",
                        help="Release- und Dev-Variante nacheinander bauen")
    args = parser.parse_args()

    try:
        sys.exit(main(
            no_clean=args.no_clean,
            copy_to_root_flag=args.copy_to_root,
            dev=args.dev,
            both=args.both,
        ))
    except KeyboardInterrupt:
        print("\n  [ABBRUCH] Durch Benutzer unterbrochen.")
        sys.exit(130)
    except Exception as err:  # noqa: BLE001
        import traceback
        print(f"\n  [FEHLER] {err}", file=sys.stderr)
        print(traceback.format_exc(), file=sys.stderr)
        sys.exit(1)
