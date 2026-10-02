# -*- mode: python ; coding: utf-8 -*-
"""
draxo_launcher.spec
PyInstaller-Konfiguration für DraxoLauncher.exe

Erzeugt eine einzelne, fensterbasierte .exe (--onefile, --windowed),
die alle Ressourcen und Launcher-Module einbettet und ohne Python-
Installation per Doppelklick gestartet werden kann.

Build:
    python scripts/build_exe.py
  oder direkt:
    pyinstaller draxo_launcher.spec --noconfirm

Zur Ordnerstruktur
-------------------
Entry-Skript ist ``launcher/draxo_launcher.py``. Die Module importieren sich
untereinander flach (``from utils import ...``), deshalb steht ``launcher/``
in ``pathex`` — sonst findet PyInstaller sie nicht.

Es gibt bewusst KEINEN Shim im Projekt-Root: der hiesse wie das Modul, das
er importiert, und im Bundle waere das ein Zyklus
(``ImportError: cannot import name 'main' from partially initialized
module``). Stattdessen startet man aus dem Source direkt mit
``python launcher/draxo_launcher.py`` — Python legt das Skriptverzeichnis
selbst in ``sys.path[0]``.
"""

import os
from pathlib import Path
from PyInstaller.utils.hooks import collect_data_files, collect_submodules

BASE_DIR = Path(SPECPATH)
LAUNCHER_DIR = BASE_DIR / "launcher"

# ── Assets (Quell → Ziel-Ordner im Bundle) ────────────────────────────────────
_RAW_ASSETS = [
    ("assets/branding/image.png",  "assets/branding"),
    ("assets/branding/breites_logo_draxo.png", "assets/branding"),
    ("draxo.ico",  "."),
    ("VERSION",    "."),
    # Standalone-Build: eingebettete Quelle, damit die .exe allein alles kann
    ("src",        "src"),
    ("CMakeLists.txt", "."),
    ("minhook-master", "minhook-master"),
]
datas = []
for _src, _dst in _RAW_ASSETS:
    if (BASE_DIR / _src).exists():
        datas.append((str(BASE_DIR / _src), _dst))

# ── Vorgefertigte DLLs (build/prebuilt/<version>/draxo.dll) ───────────────────
# Jede DLL liegt im Bundle unter prebuilt/<version>/draxo.dll und wird beim
# ersten Inject in einen STABILEN Ordner neben der .exe entpackt (Config-Key!).
_PREBUILT_DIR = BASE_DIR / "build" / "prebuilt"
if _PREBUILT_DIR.exists():
    for _ver_dir in sorted(_PREBUILT_DIR.iterdir()):
        if not _ver_dir.is_dir():
            continue
        for _dll in _ver_dir.glob("draxo*.dll"):
            datas.append((str(_dll), os.path.join("prebuilt", _ver_dir.name)))

# customtkinter MUSS vollständig eingebettet sein (JSON-Themes)
datas += collect_data_files("customtkinter")

try:
    datas += collect_data_files("PIL")
except Exception:
    pass

# ── Hidden Imports ────────────────────────────────────────────────────────────
_hidden = [
    *collect_submodules("customtkinter"),
    # Pillow
    "PIL", "PIL.Image", "PIL.ImageTk", "PIL.ImageFilter",
    "PIL.ImageDraw", "PIL.ImageFont", "PIL._imaging",
    # psutil (plattformspezifisch)
    "psutil", "psutil._pswindows", "psutil._pslinux",
    "psutil._psosx", "psutil._psposix",
    # Windows-Effekte (optional)
    "pywinstyles",
    # tkinter
    "tkinter", "tkinter.ttk", "tkinter.messagebox",
    "tkinter.filedialog", "tkinter.font", "_tkinter",
    # Projekt-eigene Module — ALLE auflisten, damit PyInstaller sie findet.
    # Die liegen seit dem Umzug in launcher/ und werden von dort flach
    # importiert; sie stehen deshalb in hiddenimports unter ihrem Klarnamen.
    "draxo_launcher",
    "ui",
    "auth_ui",
    "bootstrap",
    "setup_manager",
    "animations",
    "particles",
    "process_detector",
    "config",
    "styles",
    "updater",
    "utils",
    "versions",
    "builder_runner",
    # Lizenzseite — von license_manager zwingend importiert
    "license_signing",
    "license_manager",
    "discord_auth",
    "widgets",
    # Builder als MODUL einbetten (Standalone: kein tools/-Ordner nötig)
    "tools",
    "tools.vanilla_builder",
    # Standardbibliothek
    "queue", "threading", "subprocess", "pathlib", "json",
    "logging", "logging.handlers", "traceback", "dataclasses",
    "typing", "urllib.request", "urllib.error", "urllib.parse",
    "urllib.response", "http.client", "email.message",
    "email.policy", "tempfile", "shutil", "random", "math",
]

block_cipher = None

a = Analysis(
    [str(LAUNCHER_DIR / "draxo_launcher.py")],
    # launcher/ zuerst: der Shim importiert die Module flach, PyInstaller
    # muss sie dort finden.
    pathex=[str(LAUNCHER_DIR), str(BASE_DIR)],
    binaries=[],
    datas=datas,
    hiddenimports=_hidden,
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[
        "matplotlib", "numpy", "scipy", "pandas",
        "IPython", "jupyter", "notebook", "pytest",
        "setuptools", "distutils", "pydoc", "doctest",
        "difflib", "unittest", "xml", "xmlrpc",
    ],
    noarchive=False,
    optimize=1,
)

pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

_ico = str(BASE_DIR / "assets" / "branding" / "draxo.ico")
_ico = _ico if Path(_ico).exists() else None

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.zipfiles,
    a.datas,
    [],
    name="DraxoLauncher",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=False,          # Kein UPX → kein Antivirus-Alarm
    upx_exclude=[],
    runtime_tmpdir=None,
    console=False,      # Kein cmd-Fenster
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon=_ico,
    uac_admin=False,    # Kein UAC – Launcher braucht keine Admin-Rechte
)
