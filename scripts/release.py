"""
release.py
----------
Vollständiges Release-Script für den Draxo Client.

Schritte (alle einzeln überspringbar):
  0. VERSION-Datei lesen
  1. .exe via build_exe.py bauen      (--skip-build)
  2. .exe prüfen
  3. Git-Tag erstellen und pushen     (--skip-git)
  4. GitHub-Release erstellen         (--skip-upload)
  5. version.json für GitHub Pages    (--skip-version-json)
  6. Discord-Webhook-Benachrichtigung (--skip-discord)

Discord-Webhook:
  Die Webhook-URL wird NIEMALS ins Repo committet. Sie kommt aus:
    a) Env-Variable:   set DRAXO_DISCORD_WEBHOOK=https://discord.com/api/webhooks/...
    b) Lokale Datei:   discord_webhook.txt im Projektordner (eine Zeile, gitignored)
  Ohne konfigurierten Webhook wird Schritt 6 einfach übersprungen.

Voraussetzungen für Upload:
  - GitHub CLI (gh) installiert und mit `gh auth login` eingeloggt
  - git konfiguriert, Remote `origin` gesetzt

Verwendung:
  python release.py                     → Vollständiger Release-Prozess
  python release.py --dry-run           → Zeigt alle Befehle ohne Ausführung
  python release.py --skip-build        → Nimmt vorhandene DraxoLauncher.exe
  python release.py --skip-git          → Kein Git-Tag
  python release.py --skip-upload       → Kein GitHub-Upload
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import traceback
import urllib.error
import urllib.request
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional

# Windows-Konsolen (cp1252) können Emojis/UTF-8 nicht immer ausgeben.
# Statt einem UnicodeEncodeError-Absturz: stdout auf UTF-8 mit Ersetzung umstellen.
try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:  # noqa: BLE001
    pass

BASE_DIR = Path(__file__).resolve().parent
VERSION_FILE = BASE_DIR / "VERSION"
SPEC_FILE = BASE_DIR / "draxo_launcher.spec"
DIST_DIR = BASE_DIR / "dist"
OUTPUT_EXE = DIST_DIR / "DraxoLauncher.exe"
VERSION_JSON = BASE_DIR / "version.json"

GITHUB_REPO = "batotomato/draxo"
GITHUB_PAGES_BASE = "https://draxo.netlify.app"
DISCORD_COLOR = 0x8B5CF6  # Draxo-Lila
DISCORD_WEBHOOK_ENV = "DRAXO_DISCORD_WEBHOOK"
DISCORD_WEBHOOK_FILE = BASE_DIR / "discord_webhook.txt"


# ── Hilfsfunktionen ────────────────────────────────────────────────────────────

def _hr() -> None:
    print("─" * 62)


def _log(tag: str, message: str) -> None:
    print(f"  [{tag}] {message}")


def _run(
    cmd: list[str],
    description: str,
    dry_run: bool = False,
    check: bool = True,
    capture: bool = False,
) -> subprocess.CompletedProcess:
    """Führt einen Shell-Befehl aus oder simuliert ihn im Dry-Run-Modus."""
    _log("CMD", description)
    print(f"        $ {' '.join(cmd)}")
    if dry_run:
        print("        [DRY-RUN] nicht ausgeführt.")
        return subprocess.CompletedProcess(cmd, 0, stdout="", stderr="")

    kwargs: dict = {"cwd": str(BASE_DIR)}
    if capture:
        kwargs["capture_output"] = True
        kwargs["text"] = True

    result = subprocess.run(cmd, **kwargs)
    if check and result.returncode != 0:
        raise RuntimeError(
            f"Befehl fehlgeschlagen (Exit {result.returncode}): {' '.join(cmd)}"
        )
    return result


def _tool_available(name: str) -> bool:
    if shutil.which(name) is None:
        _log("WARN", f"'{name}' nicht im PATH gefunden.")
        return False
    return True


# ── Schritte ───────────────────────────────────────────────────────────────────

def read_version() -> str:
    if not VERSION_FILE.exists():
        raise FileNotFoundError(
            f"VERSION-Datei nicht gefunden: {VERSION_FILE}\n"
            "Erstelle sie mit dem Inhalt '1.0.0'."
        )
    version = VERSION_FILE.read_text(encoding="utf-8").strip()
    if not version:
        raise ValueError("VERSION-Datei ist leer.")
    _log("VERSION", f"Aktuelle Version: {version}")
    return version


def build(dry_run: bool, skip: bool) -> None:
    print(f"\n{'[SKIP]' if skip else '[1/6]'} Build ...")
    if skip:
        _log("SKIP", "--skip-build gesetzt, verwende vorhandene .exe.")
        return

    if dry_run:
        _log("DRY-RUN", "build_exe.main() wird nicht aufgerufen.")
        return

    try:
        import build_exe
        result = build_exe.main()
        if result != 0:
            raise RuntimeError(f"build_exe.main() gab {result} zurück.")
    except Exception as exc:  # noqa: BLE001
        raise RuntimeError(f"Build fehlgeschlagen: {exc}") from exc


def verify(skip_build: bool) -> None:
    print("\n[2/6] .exe prüfen ...")
    if skip_build and not OUTPUT_EXE.exists():
        raise FileNotFoundError(
            f"DraxoLauncher.exe nicht gefunden unter: {OUTPUT_EXE}\n"
            "Führe zuerst 'python build_exe.py' aus."
        )
    if OUTPUT_EXE.exists():
        size_mb = OUTPUT_EXE.stat().st_size / 1_048_576
        _log("OK", f"DraxoLauncher.exe — {size_mb:.1f} MB")
    else:
        _log("SKIP", "(Build wurde übersprungen, kein Verify möglich.)")


def git_tag(version: str, dry_run: bool, skip: bool) -> None:
    print(f"\n{'[SKIP]' if skip else '[3/6]'} Git-Tag v{version} ...")
    if skip:
        _log("SKIP", "--skip-git gesetzt.")
        return
    if not _tool_available("git"):
        _log("SKIP", "git nicht verfügbar.")
        return

    # Prüfen, ob der Tag bereits existiert
    check = subprocess.run(
        ["git", "tag", "-l", f"v{version}"],
        cwd=str(BASE_DIR),
        capture_output=True,
        text=True,
    )
    if f"v{version}" in check.stdout:
        _log("INFO", f"Tag v{version} existiert bereits, wird übersprungen.")
    else:
        _run(["git", "tag", f"v{version}"], f"Tag v{version} erstellen", dry_run)

    _run(["git", "push", "origin", "--tags"], "Tags zu origin pushen", dry_run)


def github_release(version: str, dry_run: bool, skip: bool) -> None:
    print(f"\n{'[SKIP]' if skip else '[4/6]'} GitHub-Release v{version} ...")
    if skip:
        _log("SKIP", "--skip-upload gesetzt.")
        return
    if not _tool_available("gh"):
        _log("SKIP", "gh CLI nicht verfügbar. Installiere von https://cli.github.com/")
        return
    if not OUTPUT_EXE.exists() and not dry_run:
        _log("SKIP", "DraxoLauncher.exe nicht gefunden, Upload übersprungen.")
        return

    release_notes = (
        f"## Draxo Client v{version}\n\n"
        "### Installation\n"
        "1. `DraxoLauncher.exe` in deinen Draxo-Client-Ordner legen\n"
        "2. Doppelklick — fertig. Keine Python-Installation nötig.\n\n"
        "### Änderungen\nDetails und Release-Notes: https://draxo.netlify.app/changelog.html"
    )

    _run(
        [
            "gh", "release", "create",
            f"v{version}",
            str(OUTPUT_EXE),
            "--repo", GITHUB_REPO,
            "--title", f"Draxo v{version}",
            "--notes", release_notes,
        ],
        f"Release v{version} auf GitHub anlegen und .exe hochladen",
        dry_run,
    )
    _log("OK", f"Release: https://github.com/{GITHUB_REPO}/releases/tag/v{version}")


def write_version_json(version: str, dry_run: bool, skip: bool) -> None:
    print(f"\n{'[SKIP]' if skip else '[5/6]'} version.json aktualisieren ...")
    if skip:
        _log("SKIP", "--skip-version-json gesetzt.")
        return

    payload = {
        "version": version,
        "tag_name": f"v{version}",
        "download_url": (
            f"https://github.com/{GITHUB_REPO}/releases/download/"
            f"v{version}/DraxoLauncher.exe"
        ),
        "notes": f"Draxo Client v{version} — automatisch via release.py erstellt.",
    }
    content = json.dumps(payload, indent=2, ensure_ascii=False)

    if dry_run:
        _log("DRY-RUN", f"version.json würde enthalten:\n{content}")
        return

    VERSION_JSON.write_text(content, encoding="utf-8")
    _log("OK", f"version.json geschrieben: {VERSION_JSON}")

    print()
    print("  ┌─────────────────────────────────────────────────────────┐")
    print("  │  version.json muss in den gh-pages Branch committed     │")
    print("  │  werden, damit der Updater sie findet. Beispiel:        │")
    print("  │                                                         │")
    print("  │  git checkout gh-pages                                  │")
    print(f"  │  cp version.json .                                      │")
    print(f"  │  git add version.json                                   │")
    print(f"  │  git commit -m 'Update version to v{version}'              │")
    print(f"  │  git push                                               │")
    print(f"  │  git checkout main                                      │")
    print("  └─────────────────────────────────────────────────────────┘")


# ── Discord-Webhook ────────────────────────────────────────────────────────────────

def _load_webhook_url() -> Optional[str]:
    """Webhook-URL aus Env-Variable oder lokaler Datei (nie im Repo)."""
    url = os.environ.get(DISCORD_WEBHOOK_ENV, "").strip()
    if url:
        return url
    if DISCORD_WEBHOOK_FILE.exists():
        url = DISCORD_WEBHOOK_FILE.read_text(encoding="utf-8").strip()
        if url:
            return url
    return None


def _latest_changelog_entry(version: str) -> Optional[dict]:
    """Findet den Changelog-Eintrag der Version in netlify/version.json."""
    path = BASE_DIR / "netlify" / "version.json"
    if not path.exists():
        return None
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError):
        return None
    for entry in data.get("changelog", []):
        if str(entry.get("version")) == version:
            return entry
    return None


def _build_discord_payload(version: str, entry: Optional[dict]) -> dict:
    """Baut die Discord-Embed-Nachricht: Version, Highlights, Release Notes."""
    title = (entry or {}).get("title")
    if isinstance(title, dict):
        title = title.get("de") or title.get("en") or ""
    title = str(title or "")
    highlights = [str(h) for h in (entry or {}).get("highlights", []) if h]
    notes = [str(n) for n in (entry or {}).get("notes", []) if n]

    desc_parts: list[str] = []
    if title:
        desc_parts.append(f"**{title}**")
    if highlights:
        desc_parts.append("")
        desc_parts.append("**Highlights:**")
        desc_parts.extend(f"• {h}" for h in highlights)
    if notes:
        desc_parts.append("")
        desc_parts.append("**Release Notes:**")
        desc_parts.extend(f"• {n}" for n in notes[:8])
    if not desc_parts:
        desc_parts.append(f"Draxo Client v{version} ist da — viel Spaß!")

    description = "\n".join(desc_parts)
    if len(description) > 4000:
        description = description[:3997] + "…"

    return {
        "username": "Draxo Client",
        "embeds": [
            {
                "title": f"🚀 Draxo Client v{version}",
                "url": f"{GITHUB_PAGES_BASE}/changelog.html",
                "color": DISCORD_COLOR,
                "description": description,
                "fields": [
                    {
                        "name": "Version",
                        "value": f"`v{version}`",
                        "inline": True,
                    },
                    {
                        "name": "Download",
                        "value": f"[DraxoLauncher.exe]({GITHUB_PAGES_BASE}/DraxoLauncher.exe)",
                        "inline": True,
                    },
                ],
                "footer": {"text": "Draxo Client — batotomato"},
                "timestamp": datetime.now(timezone.utc).isoformat(),
            }
        ],
    }


def _post_discord(webhook_url: str, payload: dict, dry_run: bool) -> bool:
    """Postet die Nachricht an den Webhook. Gibt Erfolg zurück (nie fatal)."""
    body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
    if dry_run:
        pretty = json.dumps(payload, ensure_ascii=False, indent=2)
        _log("DRY-RUN", f"Discord-Nachricht würde gepostet:\n{pretty}")
        return True

    req = urllib.request.Request(
        webhook_url,
        data=body,
        method="POST",
        headers={
            "Content-Type": "application/json",
            "User-Agent": f"DraxoClient/release",
        },
    )
    try:
        with urllib.request.urlopen(req, timeout=15) as resp:
            if resp.status in (200, 204):
                _log("OK", "Discord-Webhook: Nachricht gesendet.")
                return True
            _log("WARN", f"Discord-Webhook: unerwarteter Status {resp.status}.")
            return False
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode("utf-8", errors="replace")[:300]
        _log("WARN", f"Discord-Webhook abgelehnt (HTTP {exc.code}): {detail}")
    except Exception as exc:  # noqa: BLE001
        _log("WARN", f"Discord-Webhook fehlgeschlagen: {exc}")
    return False


def discord_notify(version: str, dry_run: bool, skip: bool) -> None:
    print(f"\n{'[SKIP]' if skip else '[6/6]'} Discord-Webhook ...")
    if skip:
        _log("SKIP", "--skip-discord gesetzt.")
        return

    webhook_url = _load_webhook_url()
    if not webhook_url:
        _log(
            "SKIP",
            f"Kein Webhook konfiguriert. Setze {DISCORD_WEBHOOK_ENV} (Env) "
            f"oder lege {DISCORD_WEBHOOK_FILE.name} an.",
        )
        return

    entry = _latest_changelog_entry(version)
    payload = _build_discord_payload(version, entry)
    ok = _post_discord(webhook_url, payload, dry_run)
    if not ok and not dry_run:
        _log("WARN", "Discord-Benachrichtigung fehlgeschlagen — das Release selbst ist abgeschlossen.")


# ── Main ───────────────────────────────────────────────────────────────────────

def main() -> int:
    parser = argparse.ArgumentParser(
        description="Draxo Client — Release-Script",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument(
        "--dry-run", action="store_true",
        help="Zeigt alle Befehle, führt sie aber nicht aus",
    )
    parser.add_argument("--skip-build", action="store_true", help="Build überspringen")
    parser.add_argument("--skip-git", action="store_true", help="Git-Tag überspringen")
    parser.add_argument("--skip-upload", action="store_true", help="GitHub-Upload überspringen")
    parser.add_argument("--skip-version-json", action="store_true", help="version.json überspringen")
    parser.add_argument("--skip-discord", action="store_true", help="Discord-Webhook überspringen")
    args = parser.parse_args()

    _hr()
    print("  Draxo Client — Release-Script")
    if args.dry_run:
        print("  Modus: DRY-RUN — keine Änderungen werden vorgenommen")
    _hr()

    try:
        version = read_version()

        build(dry_run=args.dry_run, skip=args.skip_build)
        verify(skip_build=args.skip_build)
        git_tag(version=version, dry_run=args.dry_run, skip=args.skip_git)
        github_release(version=version, dry_run=args.dry_run, skip=args.skip_upload)
        write_version_json(version=version, dry_run=args.dry_run, skip=args.skip_version_json)
        discord_notify(version=version, dry_run=args.dry_run, skip=args.skip_discord)

        print()
        _hr()
        print(f"  ✓  Release v{version} erfolgreich{'  [DRY-RUN]' if args.dry_run else ''}!")
        print(f"     https://github.com/{GITHUB_REPO}/releases/tag/v{version}")
        _hr()
        return 0

    except KeyboardInterrupt:
        print("\n  [ABBRUCH] Durch Benutzer unterbrochen.")
        return 130
    except Exception as exc:  # noqa: BLE001
        print(f"\n  [FEHLER] {exc}", file=sys.stderr)
        print(traceback.format_exc(), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
