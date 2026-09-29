"""
release.py
----------
One-command release tool for the Draxo Client bootstrapper.

AUTO-UPDATER WORKFLOW
=====================
The launcher .exe you ship is a BOOTSTRAPPER: on every startup it reads
https://draxo.netlify.app/version.json, compares the version there with
its own, and if a newer one exists it downloads DraxoLauncher.exe from
the same site and replaces itself.

So the ONLY thing you need to do for every release is:

    1. Build the new launcher .exe (PyInstaller)  ->  my_new.exe
    2. Run:  python tools/release.py my_new.exe --version 1.1.0 --notes "..."
    3. Either upload netlify/ to Netlify (drag & drop) OR run with --github
       to push the .exe to GitHub Releases (FREE, unlimited bandwidth).
       Done - every bootstrapper updates itself on next start.

This script:
    - Copies your .exe  ->  netlify/DraxoLauncher.exe
    - Bumps netlify/version.json (version, tag_name, download_url, notes)
    - Appends a changelog entry (used by /changelog.html)
    - Prints exactly which files changed and what to upload

Usage:
    python tools/release.py <exe_path> [--version X.Y.Z] [--notes "..."] [--title "..."]
    python tools/release.py --current          # show current version.json state

Examples:
    python tools/release.py DraxoLauncher_v11.exe --version 1.1.0 --notes "Bugfixes + new modules"
    python tools/release.py dist/DraxoLauncher.exe --version 1.2.0 --notes "Watchdog bypass update"
"""

from __future__ import annotations

import argparse
import json
import shutil
import sys
from datetime import date
from pathlib import Path

PROJECT_DIR = Path(__file__).resolve().parent.parent
NETLIFY_DIR = PROJECT_DIR / "netlify"
VERSION_JSON = NETLIFY_DIR / "version.json"
LAUNCHER_EXE = NETLIFY_DIR / "DraxoLauncher.exe"
SITE_URL = "https://draxo.netlify.app"
GITHUB_OWNER = "batotomato"
GITHUB_REPO = "draxo"
GITHUB_TAG_PREFIX = "v"  # releases are tagged v<version>

# Discord-Webhook für automatische Release-Ankündigungen.
# Kanal: Discord-Server -> Kanal-Einstellungen -> Integrationen -> Webhooks ->
# "Neuen Webhook erstellen" -> URL kopieren. Leer lassen = kein Discord-Posting.
DISCORD_WEBHOOK_URL = ""  # <-- HIER DEINEN WEBHOOK EINTRAGEN


def load_version_json() -> dict:
    if not VERSION_JSON.exists():
        return {
            "version": "0.0.0",
            "tag_name": "v0.0.0",
            "download_url": f"{SITE_URL}/DraxoLauncher.exe",
            "mirrors": [],  # kostenlose Download-Spiegel, werden der Reihe nach probiert
            "notes": "",
            "changelog": [],
        }
    return json.loads(VERSION_JSON.read_text(encoding="utf-8"))


def show_current() -> None:
    data = load_version_json()
    print(f"Current version : {data.get('version')}")
    print(f"Download URL    : {data.get('download_url')}")
    mirrors = data.get("mirrors", [])
    mirror_str = " / ".join(mirrors) if mirrors else "(none)"
    print(f"Mirrors         : {len(mirrors)} {mirror_str}")
    print(f"Notes           : {data.get('notes', '')[:80]}")
    print(f"Changelog count : {len(data.get('changelog', []))}")


FREE_HOSTS = """
KOSTENLOSE DOWNLOAD-MIRRORS (kein GitHub-Account der Kunden nötig)
===================================================================
Der Updater probiert download_url zuerst, dann jeden Eintrag in mirrors[]
der Reihe nach, bis ein Download klappt. Alle Quellen hier sind 100% gratis:

  1) catbox.moe            - KEIN Account, KEIN Login nötig. Einfach die .exe
     auf https://catbox.moe/ hochladen (Drag & Drop) -> fertige URL kopieren.
     Achtung: keine feste Versionshistorie, URL ändert sich pro Upload.

  2) GitHub Releases (public Repo) - Wenn batotomato/draxo PUBLIC ist, kann
     JEDER ohne Account die .exe laden:  https://github.com/batotomato/draxo/releases
     (dieser Mirror funktioniert nur bei public Repos!)

  3) Cloudflare R2 Public Bucket   - 10 GB Speicher gratis, 0 Egress-Gebühren.
     Bucket auf "Public Access" stellen -> Objekt-URL ist direkt ladbar.

  4) archive.org                 - Upload über die Website, dauerhafte URLs,
     komplett kostenlos, unbegrenztes Bandwidth.

So nutzt du es: Nach jedem Release die .exe bei einem (oder mehreren) freien
Host hochladen und die URL mit --mirror angeben:

    python tools/release.py dist/DraxoLauncher.exe --version 1.2.0 \\
        --mirror https://files.catbox.moe/abc123.exe \\
        --mirror https://github.com/batotomato/draxo/releases/download/v1.2.0/DraxoLauncher.exe

Netlify hostet dann nur noch die WINZIGE version.json (~1 KB) -> kaum Credits.
Die schwere .exe liegt auf den kostenlosen Hosts.
"""


def bump_version(exe_path: Path, version: str, notes: str, title: str) -> None:
    if not exe_path.exists():
        print(f"[ERROR] EXE not found: {exe_path}")
        sys.exit(1)

    data = load_version_json()
    old_version = data.get("version", "0.0.0")

    # 1. Copy the new .exe into netlify/
    print(f"[1/3] Copying {exe_path.name} -> netlify/DraxoLauncher.exe")
    shutil.copy2(exe_path, LAUNCHER_EXE)
    size_kb = LAUNCHER_EXE.stat().st_size / 1024
    print(f"      OK ({size_kb:.0f} KB)")

    # 2. Bump version.json
    print(f"[2/3] Bumping version.json  {old_version} -> {version}")
    data["version"] = version
    data["tag_name"] = f"v{version}"
    data["download_url"] = f"{SITE_URL}/DraxoLauncher.exe"
    data.setdefault("mirrors", [])
    if notes:
        data["notes"] = notes

    entry = {
        "version": version,
        "date": date.today().isoformat(),
        "tag": "latest",
        "title": {"en": title, "de": title, "es": title, "fr": title},
        "highlights": [notes.split("|")[0].strip()] if notes and "|" in notes else [],
        "notes": [notes] if notes else [],
    }
    if not entry["highlights"]:
        entry["highlights"] = ["Launcher Update"]
    data["changelog"].insert(0, entry)
    VERSION_JSON.write_text(json.dumps(data, indent=4, ensure_ascii=False), encoding="utf-8")

    # 3. Summary
    print(f"[3/3] Release ready - {version}")
    print()
    print("  UPLOAD THESE TO NETLIFY (drag & drop netlify/ folder):")
    print(f"    netlify/version.json   (only ~1 KB -> barely uses credits)")
    print()
    if data.get("mirrors"):
        print("  Bootstrappers download from these FREE mirrors (no Netlify credits):")
        for m in data["mirrors"]:
            print(f"    - {m}")
    else:
        print("  TIP: Add free mirrors with --mirror <url> so the .exe is NOT")
        print("       served from Netlify (saves credits). See FREE_HOSTS above.")
    print()
    print(f"  version.json  ->  version {version}")
    print(f"  Changelog page:  /changelog.html  (reads version.json)")


def post_discord_release(version: str, notes: str, title: str) -> bool:
    """Postet eine Release-Ankündigung an den Discord-Webhook.
    Nutzt nur die Standardbibliothek (urllib) — keine extra Abhängigkeiten."""
    import urllib.request
    import urllib.error

    if not DISCORD_WEBHOOK_URL:
        print("[Discord] Kein Webhook gesetzt — übersprungen (tools/release.py → DISCORD_WEBHOOK_URL).")
        return False

    changelog_url = f"{SITE_URL}/changelog.html"
    content = (
        f"🚀 **Draxo Client v{version}** ist da!\n\n"
        f"**{title}**\n\n"
        f"{notes[:1000] if notes else 'Neues Release!'}\n\n"
        f"📋 Changelog: {changelog_url}\n"
        f"⬇ Download: {SITE_URL}"
    )
    payload = json.dumps({"content": content}).encode("utf-8")
    req = urllib.request.Request(
        DISCORD_WEBHOOK_URL,
        data=payload,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=15) as resp:
            if resp.status in (200, 204):
                print(f"[Discord] Release v{version} gepostet ✓")
                return True
    except urllib.error.HTTPError as exc:
        print(f"[Discord] HTTP-Fehler: {exc.code} {exc.read()[:200]!r}")
    except Exception as exc:  # noqa: BLE001
        print(f"[Discord] Fehler beim Posten: {exc}")
    return False


def upload_github_release(exe_path: Path, version: str, notes: str) -> None:
    """Create/update a GitHub Release with the new .exe using the gh CLI.
    Requires: gh installed + logged in (gh auth login). Free & unlimited bandwidth."""
    import subprocess
    tag = f"{GITHUB_TAG_PREFIX}{version}"
    print(f"[GitHub] Release {tag} -> {GITHUB_OWNER}/{GITHUB_REPO}")

    # Check gh availability
    if subprocess.run(["gh", "--version"], capture_output=True).returncode != 0:
        print("[ERROR] gh CLI not found. Install: winget install GitHub.cli")
        print("        Then: gh auth login")
        return False

    # Delete existing tag/release if re-releasing the same version
    subprocess.run(["gh", "release", "delete", tag, "--yes", "--cleanup-tag"],
                   capture_output=True)
    print(f"[GitHub] Uploading {exe_path.name} ({exe_path.stat().st_size/1024:.0f} KB)")
    cmd = ["gh", "release", "create", tag, str(exe_path), "--title", f"Draxo Client {version}", "--notes", notes or "Draxo Client release"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print(f"[ERROR] gh release create failed:\n{r.stderr}")
        return False
    print(f"[GitHub] OK: https://github.com/{GITHUB_OWNER}/{GITHUB_REPO}/releases/tag/{tag}")
    return True


def main() -> None:
    parser = argparse.ArgumentParser(description="Draxo launcher release tool")
    parser.add_argument("exe_path", nargs="?", type=Path, help="path to the NEW launcher .exe")
    parser.add_argument("--version", default="", help="new version, e.g. 1.1.0")
    parser.add_argument("--notes", default="", help="release notes (one line)")
    parser.add_argument("--title", default="Launcher Update", help="changelog title")
    parser.add_argument("--current", action="store_true", help="show current version.json state")
    parser.add_argument("--github", action="store_true", help="also upload to GitHub Releases (free, gh CLI)")
    parser.add_argument("--mirror", action="append", default=[], help="add a free download mirror URL (repeatable)")
    parser.add_argument("--list-free-hosts", action="store_true", help="show the free hosting guide")
    args = parser.parse_args()

    if args.list_free_hosts:
        print(FREE_HOSTS)
        return

    if args.current or args.exe_path is None:
        show_current()
        if args.exe_path is None and not args.current:
            parser.print_help()
        return

    if not args.version:
        print("[ERROR] Missing --version (e.g. --version 1.1.0)")
        sys.exit(1)

    # Mirrors anwenden, bevor version.json geschrieben wird
    if args.mirror:
        data = load_version_json()
        data.setdefault("mirrors", [])
        for m in args.mirror:
            if m not in data["mirrors"]:
                data["mirrors"].append(m)
        VERSION_JSON.write_text(json.dumps(data, indent=4, ensure_ascii=False), encoding="utf-8")
        print(f"[+] Mirrors added: {len(data['mirrors'])}")

    bump_version(args.exe_path, args.version, args.notes, args.title)

    if args.github:
        ok = upload_github_release(args.exe_path, args.version, args.notes)
        if ok:
            print("  Bootstrappers now check GitHub FIRST (free) — Netlify is fallback.")

    post_discord_release(args.version, args.notes, args.title)


if __name__ == "__main__":
    main()
