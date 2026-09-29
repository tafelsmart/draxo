#!/usr/bin/env python3
"""
prebuild_dlls.py — Baut vorgefertigte draxo.dll für ALLE unterstützten
Versionen (1.17 … 26.2) einmal vorab.

Warum: Der Inject soll OHNE Build-Toolchain (CMake/MSVC) sofort gehen.
Die fertigen DLLs landen in:

    build/prebuilt/<version>/draxo.dll        (Vanilla, obfuskiert)
    build/prebuilt/<version>/draxo_forge.dll  (Forge/NeoForge 1.17+)

Der Launcher prüft vor jedem Build, ob für die erkannte Version schon eine
vorgefertigte DLL existiert — wenn ja, wird direkt injiziert (kein Build).

Beispiele:
  python tools/prebuild_dlls.py --all                  # alle Versionen
  python tools/prebuild_dlls.py --versions 1.21.11,26.2
  python tools/prebuild_dlls.py --all --include-forge  # auch Forge-DLLs
  python tools/prebuild_dlls.py --all --force          # alles neu bauen

Das Skript ist resumierbar: Bereits gebaute Versionen werden übersprungen.
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def _load_builder():
    """Lädt tools/vanilla_builder.py als Modul (wie builder_runner)."""
    builder_path = ROOT / "tools" / "vanilla_builder.py"
    spec = importlib.util.spec_from_file_location("vanilla_builder", builder_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Builder nicht gefunden: {builder_path}")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    root = str(ROOT)
    mod.ROOT = root
    mod.SRC_DIR = os.path.join(root, "src")
    mod.MAPPINGS_H = os.path.join(root, "src", "config", "mappings.h")
    mod.CACHE_DIR = os.path.join(root, "tools", "cache")
    mod.BUILD_DIR = os.path.join(root, "build", "vanilla")
    mod.TRANS_SRC = os.path.join(root, "build", "vanilla_src")
    return mod


def _static_versions() -> list[str]:
    """Fallback-Liste 1.17–26.2 (muss versions.py entsprechen)."""
    try:
        sys.path.insert(0, str(ROOT))
        from versions import STATIC_VERSIONS  # type: ignore[import-not-found]

        return list(STATIC_VERSIONS)
    except Exception:  # noqa: BLE001
        return [
            "26.2", "26.1.2", "26.1.1", "26.1",
            "1.21.11", "1.21.10", "1.21.9", "1.21.8", "1.21.7", "1.21.6",
            "1.21.5", "1.21.4", "1.21.3", "1.21.2", "1.21.1", "1.21",
            "1.20.6", "1.20.5", "1.20.4", "1.20.3", "1.20.2", "1.20.1", "1.20",
            "1.19.4", "1.19.3", "1.19.2", "1.19.1", "1.19",
            "1.18.2", "1.18.1", "1.18",
            "1.17.1", "1.17",
        ]


def _translate(mod, version: str, forge: bool) -> None:
    """Erzeugt build/vanilla_src mit der richtigen mappings.h für die Version."""
    if forge:
        # Forge/NeoForge 1.17+: offizielle Namen == Laufzeitnamen
        with open(mod.MAPPINGS_H, encoding="utf-8") as f:
            translated = f.read()
    else:
        mapping_path = mod.download_client_mappings(version)
        if mapping_path:
            class_map, methods, fields = mod.parse_mappings(mapping_path)
            with open(mod.MAPPINGS_H, encoding="utf-8") as f:
                orig = f.read()
            translated = mod.translate_mappings(orig, class_map, methods, fields, version)
        else:
            with open(mod.MAPPINGS_H, encoding="utf-8") as f:
                translated = f.read()
    mod._sync_tree(mod.SRC_DIR, mod.TRANS_SRC)  # noqa: SLF001
    mod._write_if_changed(  # noqa: SLF001
        os.path.join(mod.TRANS_SRC, "config", "mappings.h"), translated
    )


def _build_one(mod, version: str, forge: bool, force: bool):
    """Baut EINE Version. Liefert (path, sha256, size) oder None wenn übersprungen."""
    out_dir = ROOT / "build" / "prebuilt" / version
    out_name = "draxo_forge.dll" if forge else "draxo.dll"
    out_dll = out_dir / out_name
    if out_dll.exists() and out_dll.stat().st_size > 1000 and not force:
        print(f"[skip] {version} ({'forge' if forge else 'vanilla'}) — existiert bereits")
        # Trotzdem als manifestfähig zurückmelden (bereits gebaut)
        digest = hashlib.sha256(out_dll.read_bytes()).hexdigest()
        return str(out_dll), digest, out_dll.stat().st_size

    tag = f"{version} ({'forge' if forge else 'vanilla'})"
    print(f"\n[===] Baue {tag} …")
    _translate(mod, version, forge)
    dll = mod.build(version)
    out_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(dll, out_dll)
    digest = hashlib.sha256(out_dll.read_bytes()).hexdigest()
    size = out_dll.stat().st_size
    print(f"[+] Fertig: {out_dll} ({size // 1024} KiB, sha256 {digest[:12]}…)")
    return str(out_dll), digest, size


def _save_manifest(entries: dict) -> None:
    manifest = ROOT / "build" / "prebuilt" / "manifest.json"
    manifest.parent.mkdir(parents=True, exist_ok=True)
    manifest.write_text(json.dumps(entries, indent=2, sort_keys=True), encoding="utf-8")


def _load_manifest() -> dict:
    manifest = ROOT / "build" / "prebuilt" / "manifest.json"
    if manifest.exists():
        try:
            return json.loads(manifest.read_text(encoding="utf-8"))
        except Exception:  # noqa: BLE001
            pass
    return {}


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--all", action="store_true", help="Alle Versionen 1.17–26.2")
    ap.add_argument("--versions", help="Komma-getrennt, z. B. 1.21.11,26.2")
    ap.add_argument("--include-forge", action="store_true",
                    help="Auch Forge/NeoForge-Varianten bauen")
    ap.add_argument("--force", action="store_true", help="Vorhandene DLLs neu bauen")
    args = ap.parse_args()

    if args.versions:
        versions = [v.strip() for v in args.versions.split(",") if v.strip()]
    elif args.all:
        versions = _static_versions()
    else:
        ap.print_help()
        sys.exit(1)

    mod = _load_builder()
    entries = _load_manifest()
    total = len(versions)
    done = 0
    failed: list[str] = []
    for version in versions:
        done += 1
        try:
            result = _build_one(mod, version, forge=False, force=args.force)
            if result:
                path, digest, size = result
                entries.setdefault(version, {})["vanilla"] = {
                    "dll": path, "sha256": digest, "size": size}
            if args.include_forge:
                if mod._ver_tuple(version) >= (26, 0):  # noqa: SLF001
                    # 26.x ist nicht obfuskiert → Forge-DLL identisch mit Vanilla
                    src = ROOT / "build" / "prebuilt" / version / "draxo.dll"
                    dst = ROOT / "build" / "prebuilt" / version / "draxo_forge.dll"
                    if src.exists() and not dst.exists():
                        shutil.copy2(src, dst)
                        digest = hashlib.sha256(dst.read_bytes()).hexdigest()
                        entries.setdefault(version, {})["forge"] = {
                            "dll": str(dst), "sha256": digest, "size": dst.stat().st_size}
                        print(f"[copy] {version} forge = vanilla (nicht obfuskiert)")
                else:
                    result = _build_one(mod, version, forge=True, force=args.force)
                    if result:
                        path, digest, size = result
                        entries.setdefault(version, {})["forge"] = {
                            "dll": path, "sha256": digest, "size": size}
        except Exception as exc:  # noqa: BLE001
            print(f"[FEHLER] {version}: {exc}")
            failed.append(version)
        # Manifest nach JEDER Version schreiben → resumierbar + Status sichtbar
        _save_manifest(entries)
        print(f"[fortschritt] {done}/{total} Versionen")
    print(f"\n[+] Prebuild abgeschlossen. {len(versions) - len(failed)} ok, "
          f"{len(failed)} fehlgeschlagen: {failed}")


if __name__ == "__main__":
    main()
