"""
license_flow.py
----------------
Standalone script for the Draxo Client license key flow.
Displays the user's HWID and provides Linkvertise links to get a key.

Usage:
    python license_flow.py                  # Interactive mode
    python license_flow.py --hwid           # Print HWID only
    python license_flow.py --activate KEY   # Write key to DLL config

Linkvertise Flow:
    1. User copies HWID from launcher
    2. User clicks Linkvertise link(s) -> watches 3 ads
    3. After ads, user lands on keygen page -> receives 24h key
    4. User pastes key in launcher -> activates -> inject
"""

import sys
import webbrowser
from pathlib import Path

# ====== CONFIGURATION (YOUR LINKVERTISE LINKS GO HERE) =================
# Replace these with YOUR actual Linkvertise URLs.
# Each URL should point to YOUR keygen page with {HWID} in the URL.
# The user must complete 3 separate ads to get a 24-hour key.
#
# FORMAT: https://linkvertise.com/YOUR_ID/draxo-key?hwid={HWID}

LINKVERTISE_LINKS = [
    "https://linkvertise.com/6255141/BaT3NYhzY2vK?hwid={HWID}",
    "https://linkvertise.com/6255141/BaT3NYhzY2vK?hwid={HWID}",
    "https://linkvertise.com/6255141/BaT3NYhzY2vK?hwid={HWID}",
]

# Direct keygen URL (where Linkvertise redirects to after ads)
# When ENFORCE_LINKVERTISE is ON in keygen.php, users MUST come through
# the Linkvertise links above. For testing, set ENFORCE_LINKVERTISE=false.
KEYGEN_URL = "https://draxo.netlify.app/keygen.html"

# ====== PATHS ==========================================================
_PROJECT_DIR = Path(__file__).resolve().parent.parent
DLL_DIR = _PROJECT_DIR / "build" / "vanilla" / "Release"
CONFIG_FILE = DLL_DIR / "draxo_config.ini"


def get_hwid():
    """Read HWID from the DLL config file."""
    try:
        if CONFIG_FILE.exists():
            text = CONFIG_FILE.read_text(encoding="utf-8", errors="ignore")
            for line in text.splitlines():
                # Die DLL schreibt License.hwid (Laufzeit-HWID);
                # License._hwid ist nur ein Fallback (ältere Builds).
                if line.startswith("License.hwid=") or line.startswith("License._hwid="):
                    return line.split("=", 1)[1].strip()
                # Also try reading from DLL log output
                if "HWID:" in line:
                    # Extract HWID from log line: "[Draxo] HWID: ABC123..."
                    parts = line.split("HWID:")
                    if len(parts) > 1:
                        return parts[1].strip()
        return "(run inject first to display HWID)"
    except Exception:
        return "(error reading HWID)"


def activate_key(key: str) -> bool:
    """Write license key to the DLL config file."""
    if not CONFIG_FILE.parent.exists():
        print(f"ERROR: DLL directory not found: {CONFIG_FILE.parent}")
        print("Run the launcher and inject at least once first.")
        return False

    CONFIG_FILE.parent.mkdir(parents=True, exist_ok=True)

    lines = []
    found = False
    if CONFIG_FILE.exists():
        lines = CONFIG_FILE.read_text(
            encoding="utf-8", errors="ignore"
        ).splitlines()

    new_lines = []
    for line in lines:
        if line.startswith("License.key="):
            new_lines.append(f"License.key={key}")
            found = True
        else:
            new_lines.append(line)
    if not found:
        new_lines.append(f"License.key={key}")

    CONFIG_FILE.write_text("\n".join(new_lines), encoding="utf-8")
    print(f"\n[OK] License key written to:")
    print(f"     {CONFIG_FILE}")
    print("\nNext: Inject Draxo in Minecraft -> all modules unlocked!")
    return True


def open_linkvertise(hwid: str, step: int = 1):
    """Open a Linkvertise link in the default browser."""
    if step < 1 or step > len(LINKVERTISE_LINKS):
        print(f"Invalid step: {step}")
        return
    url = LINKVERTISE_LINKS[step - 1].replace("{HWID}", hwid)
    print(f"\nOpening Ad {step}/{len(LINKVERTISE_LINKS)} in browser...")
    print(f"   URL: {url}")
    webbrowser.open(url)


def dev_keygen(hwid: str, hours: int) -> str:
    """Generate a Draxo license key using the internal keygen (no Linkvertise).
    This mirrors the C++ algorithm in src/core/auth.cpp exactly."""
    import sys as _sys
    _tools_dir = _PROJECT_DIR / "tools"
    _sys.path.insert(0, str(_tools_dir))
    from keygen_reference import generate_key
    return generate_key(hwid, hours)


def main():
    print("=" * 58)
    print("  DRAXO CLIENT - License Manager")
    print("=" * 58)
    print()
    print("  3 Linkvertise ads = 1x 24-hour license key")
    print("  The key is bound to YOUR machine (HWID-locked)")
    print()

    hwid = get_hwid()
    print(f"  HWID: {hwid}")
    print()
    print("-" * 58)
    print("  HOW TO GET A KEY:")
    print()
    print("  1. Copy your HWID from above")
    print("  2. Open the Linkvertise links below (complete ALL 3 ads)")
    print("  3. After the 3rd ad, your 24h key appears")
    print("  4. Paste the key here or in the Draxo menu (CONFIG > LICENSE)")
    print("-" * 58)
    print()
    print("  YOUR LINKVERTISE LINKS (complete all 3):")
    print()
    for i, link in enumerate(LINKVERTISE_LINKS, 1):
        print(f"  Ad {i}: {link}")
    print()
    print("  Direct keygen page (for testing):")
    print(f"  {KEYGEN_URL}")
    print()
    print("-" * 58)
    print()
    print("  DEV TOOLS (internal only):")
    print('  python license_flow.py --keygen HWID HOURS')
    print('    Example: python license_flow.py --keygen FB58633AE62CAD4C54FCBB74293D5E9F 24')
    print('    HOURS=0 for lifetime key')
    print()
    print("  Already have a key? Paste it here:")
    print('  python license_flow.py --activate "DRAXO-XXXXX-XXXXX-XXXXX-XXXXX-XXXXX-X"')
    print()
    print("  OR: In the Draxo menu -> CONFIG tab -> LICENSE -> paste key")
    print()


if __name__ == "__main__":
    if "--activate" in sys.argv:
        idx = sys.argv.index("--activate")
        if idx + 1 < len(sys.argv):
            activate_key(sys.argv[idx + 1])
        else:
            print("Usage: python license_flow.py --activate <KEY>")
    elif "--keygen" in sys.argv:
        idx = sys.argv.index("--keygen")
        if idx + 2 < len(sys.argv):
            hwid = sys.argv[idx + 1]
            hours = int(sys.argv[idx + 2])
            key = dev_keygen(hwid, hours)
            label = "LIFETIME" if hours == 0 else f"{hours}h"
            print(f"\n  [{label}] Key for HWID {hwid}:")
            print(f"  {key}")
        else:
            print("Usage: python license_flow.py --keygen <HWID> <HOURS>")
            print("  HOURS=0 for lifetime key")
            print("  Example: python license_flow.py --keygen FB58633A... 24")
    elif "--hwid" in sys.argv:
        print(get_hwid())
    else:
        main()
