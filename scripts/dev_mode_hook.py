"""
dev_mode_hook.py
----------------
PyInstaller-Runtime-Hook für die Dev-Variante.

Ein Runtime-Hook läuft in der gepackten .exe, *bevor* die Entry-Point-
Funktion aufgerufen wird. Damit ist DRAXO_DEV_MODE gesetzt, bevor
discord_auth und license_manager ihren ersten os.environ-Zugriff machen —
ohne dass irgendein Modul eine Sonderbehandlung für "wurde ich aus einer
BAT gestartet" braucht.

Wichtig: das ist kein Netz-Schalter. Update-Check, Versionsliste und die
Lizenzprüfung laufen unverändert. Es fehlt ausschließlich die Discord-
Anmeldung.

Der Hook ist über die .spec eingebunden; ohne ihn verhält sich die .exe
wie der normale Release-Build.
"""

import os

# setdefault, nicht Zuweisung: wenn jemand die Variable beim Start schon
# absichtlich auf "0" gesetzt hat, wird sie nicht überschrieben.
os.environ.setdefault("DRAXO_DEV_MODE", "1")