"""
updater.py
----------
Vollständige Auto-Update-Implementierung für den Draxo Client.

Ablauf
------
1. ``Updater.check_for_update()`` holt ``UPDATE_URL`` (GitHub Pages JSON)
   und vergleicht ``tag_name`` / ``version`` mit ``CURRENT_VERSION``.
2. Bei neuerer Version öffnet ``UpdateDialog`` ein modales CTkToplevel.
3. "Jetzt herunterladen" startet ``DownloadDialog`` (CTkProgressBar, Hintergrund-Thread).
4. Nach erfolgreichem Download ruft ``_restart_with_new_exe()`` ein temporäres
   .bat-Skript auf (Windows-Trick, um eine laufende .exe zu ersetzen) und
   beendet die aktuelle Instanz via ``sys.exit(0)``.

Format von version.json auf GitHub Pages
-----------------------------------------
{
    "version":      "1.1.0",
    "tag_name":     "v1.1.0",
    "download_url": "https://github.com/batotomato/draxo/releases/download/v1.1.0/DraxoLauncher.exe",
    "notes":        "Bugfixes und Performance-Verbesserungen"
}
"""

from __future__ import annotations

import json
import logging
import os
import re
import subprocess
import sys
import tempfile
import threading
import urllib.error
import urllib.request
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Optional

import customtkinter as ctk
from tkinter import messagebox

logger = logging.getLogger("DraxoClient.updater")

# ── Konstanten ─────────────────────────────────────────────────────────────────
# Primary: GitHub Releases API — FREE, unlimited bandwidth, no Netlify credits.
# The updater natively parses the GitHub API format (tag_name, assets[], body).
def _workdir() -> Path:
    """Projekt-Root — im gepackten Build der Ordner der EXE.

    Wird fuer ``.git/config`` gebraucht, also fuer einen echten Ordner auf
    der Platte. Im Bundle gibt es kein .git; das ist unkritisch, dann wird
    die Repository-Erkennung eben uebersprungen.
    """
    try:
        from utils import get_workdir
        return get_workdir()
    except Exception:  # pragma: no cover - nur bei direktem Import
        return Path(__file__).resolve().parent.parent


def _base_dir() -> Path:
    """Basisordner der eingebetteten Ressourcen.

    WICHTIG: nicht ``_workdir()``. Im Bundle liegt ``VERSION`` in
    ``sys._MEIPASS``, nicht neben der .exe. Mit ``_workdir()`` faellt die
    Versionsabfrage auf "1.0.0" zurueck — und die .exe laedt daraufhin
    bei jedem Start ein Update von sich selbst.
    """
    try:
        from utils import get_base_dir
        return get_base_dir()
    except Exception:  # pragma: no cover - nur bei direktem Import
        return Path(__file__).resolve().parent.parent


def _discover_github_repo() -> tuple[str, str]:
    """Ermittle owner/repo fuer den Release-Check.

    Reihenfolge:
      1. Umgebungsvariablen DRAXO_GH_OWNER / DRAXO_GH_REPO (fuer Builds
         in fremden Umgebungen)
      2. Die Git-Remote "origin" — so folgt der Updater automatisch
         einem umbenannten oder geforkten Repository, ohne dass hier
         eine URL gepflegt werden muss.
      3. Hartcodierter Fallback, falls kein .git vorhanden ist
         (z.B. wenn nur die .exe weitergereicht wurde).
    """
    owner = os.environ.get("DRAXO_GH_OWNER", "").strip()
    repo = os.environ.get("DRAXO_GH_REPO", "").strip()
    if owner and repo:
        return owner, repo

    # Git-Remote auslesen (funktioniert auch ohne das git-Binary nicht —
    # dann wird die .git/config direkt gelesen).
    remote_url = ""
    try:
        git_dir = _workdir() / ".git"
        cfg = git_dir / "config"
        if cfg.is_file():
            text = cfg.read_text(encoding="utf-8", errors="ignore")
            in_origin = False
            for line in text.splitlines():
                stripped = line.strip()
                if stripped.startswith("["):
                    in_origin = stripped.replace(" ", "").lower() == '[remote"origin"]'
                elif in_origin and stripped.startswith("url"):
                    remote_url = stripped.split("=", 1)[-1].strip()
                    break
    except Exception:  # noqa: BLE001
        remote_url = ""

    if remote_url:
        m = re.search(
            r"github\.com[/:]([^/]+)/([^/]+?)(?:\.git)?/?$", remote_url)
        if m:
            return m.group(1), m.group(2)

    return "tafelsmart", "draxo"


_OWNER, _REPO = _discover_github_repo()
GITHUB_OWNER: str = _OWNER
GITHUB_REPO: str = _REPO
GITHUB_API_URL: str = f"https://api.github.com/repos/{GITHUB_OWNER}/{GITHUB_REPO}/releases/latest"

# Fallback: Netlify version.json (only used if GitHub is unreachable).
NETLIFY_UPDATE_URL: str = "https://draxo.netlify.app/version.json"

# Primary check URL — GitHub first (free), Netlify as fallback.
UPDATE_URL: str = GITHUB_API_URL
FALLBACK_URL: str = NETLIFY_UPDATE_URL
def _load_current_version() -> str:
    _vfile = _base_dir() / "VERSION"
    if _vfile.exists():
        return _vfile.read_text(encoding="utf-8").strip() or "1.0.0"
    return "1.0.0"

CURRENT_VERSION: str = _load_current_version()
REQUEST_TIMEOUT: float = 7.0
DOWNLOAD_TIMEOUT: float = 120.0
CHUNK_SIZE: int = 65536  # 64 KiB

# Farben aus styles, aber ohne Import-Abhängigkeit (Styles sind Python-Module
# im gleichen Paket – wir wiederholen sie hier minimal, um zirkuläre
# Importe zu vermeiden, falls updater.py früh geladen wird).
_C_BG = "#0B0B10"
_C_SURFACE = "#15151F"
_C_ACCENT = "#8B5CF6"
_C_ACCENT_H = "#A855F7"
_C_SUCCESS = "#22C55E"
_C_ERROR = "#EF4444"
_C_TEXT = "#FFFFFF"
_C_MUTED = "#9CA3AF"
_C_BORDER = "#26263A"


# ── Datenklassen ───────────────────────────────────────────────────────────────

@dataclass
class UpdateInfo:
    """Ergebnis eines Update-Checks."""

    update_available: bool
    current_version: str
    latest_version: Optional[str] = None
    download_url: Optional[str] = None
    release_notes: Optional[str] = None
    error: Optional[str] = None
    # Zusätzliche kostenlose Download-Spiegel (catbox, R2, GitLab, ...).
    # Der Updater probiert download_url zuerst, dann jeden Spiegel der Reihe
    # nach, bis einer funktioniert.
    mirrors: Optional[list[str]] = None


# ── Kern-Logik ─────────────────────────────────────────────────────────────────

class Updater:
    """
    Kapselt HTTP-Check und Download-Logik.
    Alle Methoden sind thread-sicher und werfen niemals unbehandelte Exceptions.
    """

    def __init__(
        self,
        update_url: str = UPDATE_URL,
        current_version: str = CURRENT_VERSION,
        fallback_url: Optional[str] = FALLBACK_URL,
    ) -> None:
        self._update_url = update_url
        self._current_version = current_version
        self._fallback_url = fallback_url

    @property
    def current_version(self) -> str:
        return self._current_version

    # ── Update-Check ──────────────────────────────────────────────────────────

    def check_for_update(self) -> UpdateInfo:
        """
        Holt version.json vom Server und vergleicht mit der aktuellen Version.

        Akzeptierte JSON-Felder (beide Formate werden unterstützt):
        - ``tag_name``     (GitHub-API-Stil)  → Versionstring wie "v1.1.0"
        - ``version``      (eigener Stil)      → "1.1.0"
        - ``download_url`` oder ``assets[0].browser_download_url``
        - ``notes``        oder ``body``
        """
        # Try primary URL (GitHub API), fall back to Netlify version.json
        raw: Optional[str] = None
        last_error: Optional[str] = None
        for url in [self._update_url, getattr(self, "_fallback_url", None)]:
            if not url:
                continue
            try:
                req = urllib.request.Request(
                    url,
                    headers={
                        "User-Agent": f"DraxoClient/{self._current_version}",
                        "Accept": "application/vnd.github+json",
                    },
                )
                with urllib.request.urlopen(req, timeout=REQUEST_TIMEOUT) as resp:
                    raw = resp.read().decode("utf-8", errors="replace")
                    break  # success
            except urllib.error.URLError as exc:
                last_error = str(exc)
                logger.info("Update-Quelle nicht erreichbar (%s): %s", url, exc)
            except Exception as exc:  # noqa: BLE001
                last_error = str(exc)
                logger.exception("Unerwarteter Fehler beim Update-Check (%s).", url)

        if raw is None:
            return UpdateInfo(
                update_available=False,
                current_version=self._current_version,
                error=last_error or "Alle Update-Quellen nicht erreichbar",
            )

        try:
            data: dict = json.loads(raw)
        except json.JSONDecodeError as exc:
            logger.warning("version.json: ungültiges JSON (%s).", exc)
            return UpdateInfo(
                update_available=False,
                current_version=self._current_version,
                error="Ungültige Serverantwort",
            )

        # Versionstring extrahieren (tag_name oder version)
        raw_ver: str = str(data.get("tag_name") or data.get("version") or "").strip()
        latest = raw_ver.lstrip("v")  # "v1.1.0" → "1.1.0"

        if not latest:
            return UpdateInfo(
                update_available=False,
                current_version=self._current_version,
                error="Keine Versionsinformation im JSON.",
            )

        # Download-URL extrahieren
        dl_url: Optional[str] = data.get("download_url")
        if not dl_url:
            assets = data.get("assets", [])
            if assets and isinstance(assets, list):
                dl_url = assets[0].get("browser_download_url")

        notes: Optional[str] = data.get("notes") or data.get("body")

        # Download-Spiegel aus version.json (freie Hosts, kein GitHub nötig)
        mirrors: Optional[list[str]] = None
        raw_mirrors = data.get("mirrors")
        if isinstance(raw_mirrors, list):
            mirrors = [
                str(m).strip()
                for m in raw_mirrors
                if isinstance(m, str) and str(m).strip().startswith("http")
            ]
            if not mirrors:
                mirrors = None

        is_newer = self._is_newer(latest, self._current_version)
        logger.info(
            "Update-Check: lokal=%s server=%s newer=%s",
            self._current_version,
            latest,
            is_newer,
        )

        return UpdateInfo(
            update_available=is_newer,
            current_version=self._current_version,
            latest_version=latest,
            download_url=dl_url,
            release_notes=notes,
            mirrors=mirrors,
        )

    # ── Download ──────────────────────────────────────────────────────────────

    @staticmethod
    def _is_valid_exe(path: Path) -> bool:
        """
        Prüft, ob eine heruntergeladene Datei eine echte Windows-.exe ist
        und nicht z. B. eine HTML-Fehlerseite oder ein abgeschnittener
        Download. Eine gültige PE-Executable beginnt mit den Bytes 'MZ'
        und ist sinnvoll groß.
        """
        try:
            if not path.exists():
                return False
            size = path.stat().st_size
            if size < 1_000_000:  # < 1 MB → fast sicher keine echte .exe
                logger.warning("Integritätscheck: Datei zu klein (%d Bytes).", size)
                return False
            with path.open("rb") as fp:
                header = fp.read(2)
            if header != b"MZ":
                logger.warning("Integritätscheck: kein MZ-Header (%.2r).", header)
                return False
            return True
        except Exception:  # noqa: BLE001
            logger.exception("Integritätscheck fehlgeschlagen.")
            return False

    def download(
        self,
        url: str,
        destination: Path,
        progress_callback: Optional[Callable[[float], None]] = None,
    ) -> bool:
        """
        Lädt eine Datei von ``url`` nach ``destination`` herunter.

        ``progress_callback(fraction)`` wird mit float in [0.0, 1.0] aufgerufen,
        oder mit -1.0, wenn die Gesamtgröße unbekannt ist (indeterminate).
        Gibt ``True`` bei Erfolg zurück, ``False`` bei Fehler.
        """
        try:
            req = urllib.request.Request(
                url,
                headers={"User-Agent": f"DraxoClient/{self._current_version}"},
            )
            with urllib.request.urlopen(req, timeout=DOWNLOAD_TIMEOUT) as resp:
                content_type = (resp.headers.get("Content-Type") or "").lower()
                # Netlify/Server liefern bei 404/Fehlern HTML statt der .exe
                if "html" in content_type and "exe" not in url.lower():
                    logger.warning("Server lieferte HTML statt .exe (%s).", content_type)
                    return False
                content_length = int(resp.headers.get("Content-Length") or 0)
                downloaded = 0
                destination.parent.mkdir(parents=True, exist_ok=True)
                with destination.open("wb") as fp:
                    while True:
                        chunk = resp.read(CHUNK_SIZE)
                        if not chunk:
                            break
                        fp.write(chunk)
                        downloaded += len(chunk)
                        if progress_callback:
                            if content_length > 0:
                                progress_callback(downloaded / content_length)
                            else:
                                progress_callback(-1.0)  # indeterminate

            # WICHTIG: Integritätscheck VOR der Installation — sonst wird
            # eine kaputte Datei installiert und der Launcher startet nicht
            # mehr ("Failed to load Python DLL").
            # 1) Abgeschnittener Download: Größe != Content-Length
            if content_length > 0 and downloaded != content_length:
                logger.error(
                    "Download abgeschnitten: %d von %d Bytes — wird verworfen.",
                    downloaded, content_length,
                )
                try:
                    destination.unlink(missing_ok=True)
                except Exception:  # noqa: BLE001
                    pass
                return False
            # 2) Keine echte .exe (MZ-Header + Mindestgröße)
            if not self._is_valid_exe(destination):
                logger.error("Heruntergeladene Datei ist ungültig, wird verworfen.")
                try:
                    destination.unlink(missing_ok=True)
                except Exception:  # noqa: BLE001
                    pass
                return False

            if progress_callback:
                progress_callback(1.0)
            logger.info("Download abgeschlossen: %s (%d Bytes)", destination, downloaded)
            return True

        except Exception:  # noqa: BLE001
            logger.exception("Download von %s fehlgeschlagen.", url)
            try:
                destination.unlink(missing_ok=True)
            except Exception:  # noqa: BLE001
                pass
            return False

    # ── Restart ───────────────────────────────────────────────────────────────

    @staticmethod
    def restart_with_new_exe(new_exe: Path) -> None:
        """
        Ersetzt die laufende .exe durch ``new_exe`` und startet den Launcher
        neu. Unter Windows wird ein temporäres .bat-Skript erzeugt, das
        wartet bis die aktuelle Instanz beendet ist, dann die Dateien tauscht
        und die neue .exe startet.

        Im Dev-Modus (nicht frozen) wird die neue Datei direkt gestartet.
        """
        # Sicherheitsnetz: KEINE ungültige Datei installieren. Das hat zu
        # "Failed to load Python DLL"-Fehlern geführt, wenn ein Server eine
        # HTML-Fehlerseite statt der .exe geliefert hat.
        if not Updater._is_valid_exe(new_exe):
            logger.error("Update abgebrochen: heruntergeladene Datei ist ungültig.")
            try:
                new_exe.unlink(missing_ok=True)
            except Exception:  # noqa: BLE001
                pass
            messagebox.showerror(
                "Draxo Update",
                "Die heruntergeladene Update-Datei ist beschädigt und wurde\n"
                "nicht installiert. Bitte lade die neue Version manuell von\n"
                "draxo.netlify.app herunter.",
            )
            return

        if not getattr(sys, "frozen", False):
            # Im Python-Dev-Modus: einfach die neue .exe starten
            try:
                subprocess.Popen([str(new_exe)], close_fds=True)
            except Exception:  # noqa: BLE001
                logger.exception("Neue .exe konnte nicht gestartet werden.")
            sys.exit(0)
            return

        current_exe = Path(sys.executable).resolve()
        bat_lines = [
            "@echo off",
            "timeout /t 2 /nobreak >nul",
            f'move /y "{new_exe}" "{current_exe}"',
            f'start "" "{current_exe}"',
            'del "%~f0"',
        ]
        bat_path = current_exe.parent / "_draxo_update_helper.bat"
        try:
            bat_path.write_text("\r\n".join(bat_lines), encoding="ascii")

            create_flags = 0
            try:
                # CREATE_NO_WINDOW ist nur unter Windows definiert
                create_flags = subprocess.CREATE_NO_WINDOW  # type: ignore[attr-defined]
            except AttributeError:
                pass

            subprocess.Popen(
                ["cmd.exe", "/c", str(bat_path)],
                creationflags=create_flags,
                close_fds=True,
            )
        except Exception:  # noqa: BLE001
            logger.exception("Restart-Script konnte nicht erzeugt werden.")
            messagebox.showerror(
                "Draxo Update",
                f"Der automatische Neustart ist fehlgeschlagen.\n\n"
                f"Bitte ersetze die .exe manuell:\n{new_exe}",
            )
            return

        sys.exit(0)

    # ── Hilfsmethoden ─────────────────────────────────────────────────────────

    @staticmethod
    def _is_newer(remote: str, local: str) -> bool:
        """Gibt True zurück, wenn ``remote`` eine höhere Semver ist als ``local``."""

        def parse(v: str) -> tuple[int, ...]:
            parts = []
            for seg in v.split("."):
                digits = "".join(c for c in seg if c.isdigit())
                parts.append(int(digits) if digits else 0)
            return tuple(parts)

        r, l = parse(remote), parse(local)
        length = max(len(r), len(l))
        r += (0,) * (length - len(r))
        l += (0,) * (length - len(l))
        return r > l


# ── Dialoge ────────────────────────────────────────────────────────────────────

class UpdateDialog(ctk.CTkToplevel):
    """
    Modales CTkToplevel, das dem Benutzer ein verfügbares Update anzeigt.
    Bietet "Jetzt herunterladen" und "Später erinnern".
    """

    WIDTH = 480
    HEIGHT = 300

    def __init__(self, parent: ctk.CTk, update_info: UpdateInfo) -> None:
        super().__init__(parent)
        self._parent = parent
        self._info = update_info
        self._configure_window()
        self._build_ui()

    def _configure_window(self) -> None:
        self.title("Update verfügbar")
        self.configure(fg_color=_C_BG)
        self.resizable(False, False)
        self.grab_set()  # modal
        self.lift()
        self.focus()

        # Zentriert über dem Parent
        try:
            px = self._parent.winfo_x() + (self._parent.winfo_width() - self.WIDTH) // 2
            py = self._parent.winfo_y() + (self._parent.winfo_height() - self.HEIGHT) // 2
            self.geometry(f"{self.WIDTH}x{self.HEIGHT}+{px}+{py}")
        except Exception:  # noqa: BLE001
            self.geometry(f"{self.WIDTH}x{self.HEIGHT}")

    def _build_ui(self) -> None:
        # ── Titel ──────────────────────────────────────────────────────────
        ctk.CTkLabel(
            self,
            text="🚀  Update verfügbar!",
            font=("Segoe UI Semibold", 20, "bold"),
            text_color=_C_ACCENT,
        ).pack(pady=(28, 6))

        # ── Versions-Badge ─────────────────────────────────────────────────
        badge_row = ctk.CTkFrame(self, fg_color="transparent")
        badge_row.pack(pady=4)

        ctk.CTkLabel(
            badge_row,
            text=f"v{self._info.current_version}",
            font=("Segoe UI", 13),
            text_color=_C_MUTED,
        ).pack(side="left", padx=4)

        ctk.CTkLabel(
            badge_row,
            text="→",
            font=("Segoe UI", 14, "bold"),
            text_color=_C_TEXT,
        ).pack(side="left", padx=6)

        ctk.CTkLabel(
            badge_row,
            text=f"v{self._info.latest_version}",
            font=("Segoe UI Semibold", 14, "bold"),
            text_color=_C_SUCCESS,
        ).pack(side="left", padx=4)

        # ── Release Notes ──────────────────────────────────────────────────
        notes = (self._info.release_notes or "").strip()
        if notes:
            note_box = ctk.CTkTextbox(
                self,
                height=72,
                fg_color=_C_SURFACE,
                text_color=_C_MUTED,
                corner_radius=10,
                font=("Segoe UI", 11),
                wrap="word",
                state="normal",
            )
            note_box.insert("1.0", notes[:400])
            note_box.configure(state="disabled")
            note_box.pack(padx=26, pady=(8, 4), fill="x")

        # ── Buttons ────────────────────────────────────────────────────────
        btn_row = ctk.CTkFrame(self, fg_color="transparent")
        btn_row.pack(pady=20)

        ctk.CTkButton(
            btn_row,
            text="Später",
            width=140,
            height=40,
            fg_color=_C_SURFACE,
            hover_color="#22222E",
            text_color=_C_MUTED,
            corner_radius=10,
            font=("Segoe UI", 13),
            command=self._on_later,
        ).pack(side="left", padx=8)

        ctk.CTkButton(
            btn_row,
            text="Jetzt herunterladen",
            width=200,
            height=40,
            fg_color=_C_ACCENT,
            hover_color=_C_ACCENT_H,
            text_color=_C_TEXT,
            corner_radius=10,
            font=("Segoe UI Semibold", 13, "bold"),
            command=self._on_download,
        ).pack(side="left", padx=8)

    def _on_later(self) -> None:
        logger.info("Update verschoben: v%s.", self._info.latest_version)
        self.destroy()

    def _on_download(self) -> None:
        if not self._info.download_url:
            messagebox.showerror(
                "Draxo Update",
                "Keine Download-URL in der Serverantwort gefunden.\n"
                "Bitte lade die neue Version manuell von der Website herunter.",
                parent=self,
            )
            return

        self.destroy()
        dl_dialog = DownloadDialog(
            parent=self._parent,
            url=self._info.download_url,
            version=self._info.latest_version or "?",
            mirrors=self._info.mirrors or [],
        )
        dl_dialog.start()


class DownloadDialog(ctk.CTkToplevel):
    """
    Zeigt den Download-Fortschritt der neuen .exe und startet nach
    Abschluss den Neustart-Vorgang.
    """

    WIDTH = 440
    HEIGHT = 230

    def __init__(self, parent: ctk.CTk, url: str, version: str, mirrors: Optional[list[str]] = None) -> None:
        super().__init__(parent)
        self._parent = parent
        self._url = url
        self._version = version
        self._mirrors = mirrors or []
        self._cancelled = False
        self._dest: Optional[Path] = None
        self._configure_window()
        self._build_ui()

    def _configure_window(self) -> None:
        self.title("Draxo Update — Download")
        self.configure(fg_color=_C_BG)
        self.resizable(False, False)
        self.grab_set()

        try:
            px = self._parent.winfo_x() + (self._parent.winfo_width() - self.WIDTH) // 2
            py = self._parent.winfo_y() + (self._parent.winfo_height() - self.HEIGHT) // 2
            self.geometry(f"{self.WIDTH}x{self.HEIGHT}+{px}+{py}")
        except Exception:  # noqa: BLE001
            self.geometry(f"{self.WIDTH}x{self.HEIGHT}")

        self.protocol("WM_DELETE_WINDOW", self._on_cancel)

    def _build_ui(self) -> None:
        ctk.CTkLabel(
            self,
            text=f"Lade v{self._version} herunter …",
            font=("Segoe UI Semibold", 16, "bold"),
            text_color=_C_TEXT,
        ).pack(pady=(28, 4))

        self._status_label = ctk.CTkLabel(
            self,
            text="Verbinde …",
            font=("Segoe UI", 12),
            text_color=_C_MUTED,
        )
        self._status_label.pack(pady=(0, 14))

        self._progress = ctk.CTkProgressBar(
            self,
            width=360,
            height=14,
            corner_radius=7,
            progress_color=_C_ACCENT,
            fg_color=_C_SURFACE,
            mode="indeterminate",
        )
        self._progress.pack(padx=40, pady=(0, 20))
        self._progress.start()

        self._cancel_btn = ctk.CTkButton(
            self,
            text="Abbrechen",
            width=160,
            height=38,
            fg_color=_C_SURFACE,
            hover_color="#22222E",
            text_color=_C_MUTED,
            corner_radius=10,
            font=("Segoe UI", 12),
            command=self._on_cancel,
        )
        self._cancel_btn.pack()

    def start(self) -> None:
        """Startet den Download im Hintergrund-Thread."""
        # Ziel-Datei: temporärer Ordner oder neben der .exe
        try:
            if getattr(sys, "frozen", False):
                dest_dir = Path(sys.executable).resolve().parent
            else:
                dest_dir = Path(tempfile.gettempdir())
            self._dest = dest_dir / "DraxoLauncher_update.exe"
        except Exception:  # noqa: BLE001
            self._dest = Path(tempfile.gettempdir()) / "DraxoLauncher_update.exe"

        thread = threading.Thread(
            target=self._download_thread,
            daemon=True,
            name="UpdateDownload",
        )
        thread.start()

    def _download_thread(self) -> None:
        """Läuft im Hintergrund-Thread. Probiert alle Download-Quellen
        (primäre URL zuerst, dann Spiegel) der Reihe nach, bis eine klappt."""
        updater = Updater()

        # Quelle 1: primäre URL, dann alle Spiegel (kostenlose Hosts)
        # dict.fromkeys dedupliziert und erhält die Reihenfolge
        candidates: list[str] = list(
            dict.fromkeys([self._url] + [m for m in self._mirrors if m])
        )

        ok = False
        for i, url in enumerate(candidates):
            if self._cancelled:
                return
            label = url.split("//")[-1][:40]
            if i == 0:
                self.after(0, lambda l=label: self._set_status(f"Lade von {l} …"))
            else:
                self.after(0, lambda l=label: self._set_status(f"Hauptquelle fehlgeschlagen — versuche Spiegel ({l}) …"))
            ok = updater.download(
                url=url,
                destination=self._dest,
                progress_callback=self._on_progress,
            )
            if ok:
                break
            # Download-Ziel für den nächsten Versuch zurücksetzen
            if self._dest and self._dest.exists():
                try:
                    self._dest.unlink(missing_ok=True)
                except Exception:  # noqa: BLE001
                    pass

        if self._cancelled:
            return

        if ok:
            self.after(0, self._on_download_complete)
        else:
            self.after(
                0,
                lambda: self._on_download_error(
                    "Download fehlgeschlagen (alle Quellen versucht)."
                ),
            )

    def _set_status(self, text: str) -> None:
        """Statuszeile im GUI-Thread setzen."""
        try:
            self._status_label.configure(text=text)
        except Exception:  # noqa: BLE001
            pass

    def _on_progress(self, fraction: float) -> None:
        """Thread-sicherer Fortschritts-Callback via after()."""
        if self._cancelled:
            return
        try:
            self.after(0, lambda f=fraction: self._apply_progress(f))
        except Exception:  # noqa: BLE001
            pass

    def _apply_progress(self, fraction: float) -> None:
        """Wird im GUI-Thread ausgeführt."""
        try:
            if fraction < 0:
                # Indeterminate: Größe unbekannt
                self._progress.configure(mode="indeterminate")
                self._status_label.configure(text="Lade …")
            else:
                self._progress.stop()
                self._progress.configure(mode="determinate")
                self._progress.set(fraction)
                pct = int(fraction * 100)
                self._status_label.configure(text=f"{pct}% heruntergeladen")
        except Exception:  # noqa: BLE001
            pass

    def _on_download_complete(self) -> None:
        try:
            self._progress.stop()
            self._progress.configure(mode="determinate")
            self._progress.set(1.0)
            self._status_label.configure(text="Download abgeschlossen. Starte neu …", text_color=_C_SUCCESS)
            self._cancel_btn.configure(state="disabled")
        except Exception:  # noqa: BLE001
            pass

        # Kurze Pause, damit der Benutzer die Meldung sieht, dann Neustart
        self.after(1400, self._trigger_restart)

    def _trigger_restart(self) -> None:
        try:
            self.destroy()
        except Exception:  # noqa: BLE001
            pass
        if self._dest and self._dest.exists():
            Updater.restart_with_new_exe(self._dest)
        else:
            messagebox.showerror(
                "Draxo Update",
                "Die heruntergeladene Datei wurde nicht gefunden.\n"
                "Bitte starte den Launcher manuell neu.",
            )

    def _on_download_error(self, message: str) -> None:
        try:
            self._progress.stop()
            self._status_label.configure(text=f"Fehler: {message}", text_color=_C_ERROR)
            self._cancel_btn.configure(text="Schließen")
        except Exception:  # noqa: BLE001
            pass
        logger.error("Update-Download-Fehler: %s", message)

    def _on_cancel(self) -> None:
        self._cancelled = True
        try:
            self._dest and self._dest.unlink(missing_ok=True)
        except Exception:  # noqa: BLE001
            pass
        try:
            self.destroy()
        except Exception:  # noqa: BLE001
            pass
