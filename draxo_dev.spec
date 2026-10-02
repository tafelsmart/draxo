# -*- mode: python ; coding: utf-8 -*-
"""
draxo_dev.spec
PyInstaller-Konfiguration für DraxoDev.exe — die Entwickler-Variante.

Identisch zu draxo_launcher.spec, mit einem Unterschied: der
Runtime-Hook scripts/dev_mode_hook.py setzt beim Start DRAXO_DEV_MODE=1.
Dadurch ueberspringt die .exe das Discord-Anmeldefenster und das
Injektions-Gate.

Das ist KEIN Offline-Modus. Netz, Update-Check, Versionsliste und die
Lizenzpruefung laufen unveraendert — siehe scripts/dev_mode_hook.py.

Build:
    python scripts/build_exe.py --dev
  oder direkt:
    pyinstaller draxo_dev.spec --noconfirm

Zum Zurueckschalten: die normale .exe ist getrennt gebaut und kennt den
Hook nicht. Fuer einen einmaligen Test laesst sich der Modus auch zur
Laufzeit erzwingen:
    set DRAXO_DEV_MODE=1 && dist\\DraxoLauncher.exe
"""

import os
from pathlib import Path
from PyInstaller.utils.hooks import collect_data_files, collect_submodules

BASE_DIR = Path(SPECPATH)
LAUNCHER_DIR = BASE_DIR / "launcher"

# ── Assets (Quell → Ziel-Ordner im Bundle) ────────────────────────────────────
_RAW_ASSETS = [
    ("assets/branding/image.png", "assets/branding"),
    ("assets/branding/breites_logo_draxo.png", "assets/branding"),
    ("draxo.ico", "."),
    ("VERSION", "."),
    ("src", "src"),
    ("CMakeLists.txt", "."),
    ("minhook-master", "minhook-master"),
]
datas = []
for _src, _dst in _RAW_ASSETS:
    if (BASE_DIR / _src).exists():
        datas.append((str(BASE_DIR / _src), _dst))

# ── Vorgefertigte DLLs (build/prebuilt/<version>/draxo.dll) ───────────────────
_PREBUILT_DIR = BASE_DIR / "build" / "prebuilt"
if _PREBUILT_DIR.exists():
    for _ver_dir in sorted(_PREBUILT_DIR.iterdir()):
        if not _ver_dir.is_dir():
            continue
        for _dll in _ver_dir.glob("draxo*.dll"):
            datas.append((str(_dll), os.path.join("prebuilt", _ver_dir.name)))

datas += collect_data_files("customtkinter")

try:
    datas += collect_data_files("PIL")
except Exception:
    pass

# ── Hidden Imports ────────────────────────────────────────────────────────────
_hidden = [
    *collect_submodules("customtkinter"),
    "PIL", "PIL.Image", "PIL.ImageTk", "PIL.ImageFilter",
    "PIL.ImageDraw", "PIL.ImageFont", "PIL._imaging",
    "psutil", "psutil._pswindows", "psutil._pslinux",
    "psutil._psosx", "psutil._psposix",
    "pywinstyles",
    "tkinter", "tkinter.ttk", "tkinter.messagebox",
    "tkinter.filedialog", "tkinter.font", "_tkinter",
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
    "license_signing",
    "license_manager",
    "discord_auth",
    "widgets",
    "tools",
    "tools.vanilla_builder",
    "queue", "threading", "subprocess", "pathlib", "json",
    "logging", "logging.handlers", "traceback", "dataclasses",
    "typing", "urllib.request", "urllib.error", "urllib.parse",
    "urllib.response", "http.client", "email.message",
    "email.policy", "tempfile", "shutil", "random", "math",
]

block_cipher = None

a = Analysis(
    [str(LAUNCHER_DIR / "draxo_launcher.py")],
    pathex=[str(LAUNCHER_DIR), str(BASE_DIR)],
    binaries=[],
    datas=datas,
    hiddenimports=_hidden,
    hookspath=[],
    hooksconfig={},
    # ── Der Unterschied zur Release-.exe ──────────────────────────────
    runtime_hooks=[str(BASE_DIR / "scripts" / "dev_mode_hook.py")],
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
    name="DraxoDev",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=False,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon=_ico,
    uac_admin=False,
)