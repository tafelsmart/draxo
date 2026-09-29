"""
build_exe.py
------------
Erstellt DraxoLauncher.exe via PyInstaller.

Verwendung:
    python build_exe.py                    → Standard-Build
    python build_exe.py --no-clean         → Ohne vorherigen Cleanup
    python build_exe.py --copy-to-root     → Kopiert .exe in Projekt-Root

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

BASE_DIR = Path(__file__).resolve().parent
DIST_DIR = BASE_DIR / "dist"
BUILD_DIR = BASE_DIR / "build"
SPEC_FILE = BASE_DIR / "draxo_launcher.spec"
OUTPUT_EXE = DIST_DIR / "DraxoLauncher.exe"


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
    Gelöscht werden nur dist/ und build/draxo_launcher/ (PyInstaller-Work).
    """
    if DIST_DIR.exists():
        _log("CLEAN", f"Lösche {DIST_DIR.name}/ ...")
        shutil.rmtree(DIST_DIR, ignore_errors=True)
    pyi_work = BUILD_DIR / "draxo_launcher"
    if pyi_work.exists():
        _log("CLEAN", f"Lösche build/{pyi_work.name}/ ...")
        shutil.rmtree(pyi_work, ignore_errors=True)
    _log("CLEAN", "Fertig.")


def check_spec_file() -> None:
    if not SPEC_FILE.exists():
        raise FileNotFoundError(
            f"draxo_launcher.spec nicht gefunden: {SPEC_FILE}\n"
            "Stelle sicher, dass du build_exe.py aus dem Projektordner ausführst."
        )
    _log("CHECK", f"Spec-Datei gefunden: {SPEC_FILE.name}")


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


def run_pyinstaller() -> None:
    """Führt PyInstaller mit der .spec-Datei aus."""
    cmd = [
        sys.executable, "-m", "PyInstaller",
        str(SPEC_FILE),
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


def verify_output() -> Path:
    """Prüft, ob die .exe erzeugt wurde, und gibt ihren Pfad zurück."""
    if not OUTPUT_EXE.exists():
        raise FileNotFoundError(
            f"DraxoLauncher.exe wurde nicht erzeugt.\n"
            f"Erwartet unter: {OUTPUT_EXE}"
        )
    size_mb = OUTPUT_EXE.stat().st_size / 1_048_576
    _log("OK", f"DraxoLauncher.exe erstellt ({size_mb:.1f} MB)")
    _log("OK", f"Pfad: {OUTPUT_EXE}")
    return OUTPUT_EXE


def copy_to_root(exe_path: Path) -> None:
    """Kopiert die fertige .exe in den Projekt-Root (optional)."""
    dest = BASE_DIR / "DraxoLauncher.exe"
    shutil.copy2(exe_path, dest)
    _log("COPY", f"DraxoLauncher.exe → {dest}")


# ── Haupt-Funktion ─────────────────────────────────────────────────────────────

def main(no_clean: bool = False, copy_to_root_flag: bool = False) -> int:
    _hr()
    print("  Draxo Client — Build-Script")
    _hr()

    steps = [
        ("[1/5] Spec-Datei prüfen",       check_spec_file),
        ("[2/5] PyInstaller prüfen",       ensure_pyinstaller),
        ("[3/5] Abhängigkeiten prüfen",    ensure_dependencies),
        ("[4/5] .exe bauen",               run_pyinstaller),
    ]

    if not no_clean:
        steps.insert(0, ("[0/5] Aufräumen", clean_previous_build))

    for label, fn in steps:
        print(f"\n{label} ...")
        fn()

    print("\n[5/5] Ergebnis prüfen ...")
    exe_path = verify_output()

    if copy_to_root_flag:
        print("\n[+] Kopiere ins Projekt-Root ...")
        copy_to_root(exe_path)

    _hr()
    print("  Build erfolgreich! Starte mit Doppelklick:")
    print(f"  → {exe_path}")
    _hr()
    return 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Draxo Client Build-Script")
    parser.add_argument("--no-clean", action="store_true", help="dist/ und build/ nicht löschen")
    parser.add_argument("--copy-to-root", action="store_true", help=".exe in Projekt-Root kopieren")
    args = parser.parse_args()

    try:
        sys.exit(main(no_clean=args.no_clean, copy_to_root_flag=args.copy_to_root))
    except KeyboardInterrupt:
        print("\n  [ABBRUCH] Durch Benutzer unterbrochen.")
        sys.exit(130)
    except Exception as err:  # noqa: BLE001
        import traceback
        print(f"\n  [FEHLER] {err}", file=sys.stderr)
        print(traceback.format_exc(), file=sys.stderr)
        sys.exit(1)
