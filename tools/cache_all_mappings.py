#!/usr/bin/env python3
"""
Pre-cache ALL Minecraft client mappings (1.17 – 1.21.x).
26.x versions are NOT obfuscated — they need no mappings.

Run this ONCE after adding a new version to the launcher.
It downloads missing mappings from Mojang's official API and
caches them in tools/cache/ for instant builds.

Usage:
    python tools/cache_all_mappings.py          # all versions
    python tools/cache_all_mappings.py --dry-run  # show what's missing
"""

import json
import os
import sys
import time
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CACHE_DIR = os.path.join(ROOT, "tools", "cache")
MANIFEST_URL = "https://piston-meta.mojang.com/mc/game/version_manifest_v2.json"

# All versions we claim to support (must match versions.py)
TARGET_VERSIONS = [
    "1.21.11", "1.21.10", "1.21.9", "1.21.8", "1.21.7",
    "1.21.6", "1.21.5", "1.21.4", "1.21.3", "1.21.2", "1.21.1", "1.21",
    "1.20.6", "1.20.5", "1.20.4", "1.20.3", "1.20.2", "1.20.1", "1.20",
    "1.19.4", "1.19.3", "1.19.2", "1.19.1", "1.19",
    "1.18.2", "1.18.1", "1.18",
    "1.17.1", "1.17",
]

# 26.x versions — no obfuscation, no mappings needed
NON_OBFUSCATED_PREFIXES = tuple(str(i) for i in range(26, 40))


def http_get(url, timeout=60):
    req = urllib.request.Request(url, headers={"User-Agent": "draxo-cache/1.0"})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return r.read()


def fetch_manifest():
    data = json.loads(http_get(MANIFEST_URL))
    # Build a lookup: version_id -> version_url
    lookup = {}
    for v in data.get("versions", []):
        if v.get("type") == "release":
            lookup[v["id"]] = v["url"]
    return lookup


def download_mappings(version, version_url, dry_run=False):
    """Download client_mappings for a single version. Returns True on success."""
    cache_path = os.path.join(CACHE_DIR, f"client_mappings_{version}.txt")

    if os.path.exists(cache_path) and os.path.getsize(cache_path) > 1000:
        print(f"  [OK] {version}: already cached ({os.path.getsize(cache_path)//1024} KiB)")
        return True

    if dry_run:
        print(f"  [MISSING] {version}: would download")
        return False

    try:
        vdata = json.loads(http_get(version_url))
        mappings_info = vdata.get("downloads", {}).get("client_mappings")
        if not mappings_info:
            print(f"  [SKIP] {version}: no client_mappings in manifest (likely unobfuscated)")
            return False

        url = mappings_info["url"]
        size = mappings_info.get("size", 0)
        print(f"  [DOWNLOAD] {version}: {size//1024} KiB ...", end=" ", flush=True)

        data = http_get(url, timeout=120)
        os.makedirs(CACHE_DIR, exist_ok=True)
        with open(cache_path, "wb") as f:
            f.write(data)
        print(f"OK ({len(data)//1024} KiB)")
        return True

    except Exception as e:
        print(f"FAILED: {e}")
        return False


def main():
    dry_run = "--dry-run" in sys.argv

    print("=" * 60)
    print("  DRAXO — Mapping Pre-Cache")
    print("=" * 60)
    print(f"Cache directory: {CACHE_DIR}")
    print(f"Target versions: {len(TARGET_VERSIONS)}")
    print()

    if not dry_run:
        os.makedirs(CACHE_DIR, exist_ok=True)

    print("[1/2] Fetching Mojang version manifest ...")
    manifest = fetch_manifest()
    print(f"      Found {len(manifest)} release versions in manifest.")
    print()

    print("[2/2] Downloading mappings ...")
    success = 0
    skipped = 0
    failed = 0

    for version in TARGET_VERSIONS:
        # 26.x: not obfuscated — skip
        if version.startswith(NON_OBFUSCATED_PREFIXES):
            print(f"  [SKIP] {version}: 26.x+ (no obfuscation — readable names)")
            skipped += 1
            continue

        if version not in manifest:
            print(f"  [SKIP] {version}: not in Mojang manifest (unreleased?)")
            skipped += 1
            continue

        result = download_mappings(version, manifest[version], dry_run=dry_run)
        if result:
            success += 1
        else:
            failed += 1

    print()
    print("=" * 60)
    print(f"  RESULTS: {success} cached, {skipped} skipped, {failed} failed")
    if failed > 0:
        print(f"  WARNING: {failed} versions could not be cached!")
        print(f"  These versions will fail to build until mappings are available.")
    print("=" * 60)


if __name__ == "__main__":
    main()
