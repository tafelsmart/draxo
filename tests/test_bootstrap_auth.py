"""Prueft den Startablauf: Splash -> Anmeldung -> Haupt-Fenster.

Statt echten Discord zu beantworten, wird der Ablauf mit einer
kontrollierten Sitzung durchgespielt. Geprueft wird je Fall, dass das
Haupt-Fenster genau einmal startet, dass das Anmeldefenster nur bei
Bedarf aufgeht und dass die Sidebar danach den richtigen Konto-Text
zeigt.

Nur zur Entwicklung — pytest wird nicht gebraucht.
"""
from __future__ import annotations

import sys
import time
from pathlib import Path

# Wie in tests/test_discord_auth.py: Projektordner zuerst in den Pfad.
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from bootstrap import BootstrapWindow
from config import ConfigManager
from discord_auth import DiscordUser, LoginResult
from ui import MainWindow


class GeteilteSitzung:
    """Sitzung, die den Anmeldezustand nur simuliert."""

    def __init__(self, angemeldet_beim_start: bool = False) -> None:
        self._user = None
        self._gespeichert = angemeldet_beinem_start_wenn(angemeldet_beim_start)
        self.login_aufrufe = 0
        self.restore_aufrufe = 0

    @property
    def user(self):
        return self._user

    @property
    def signed_in(self):
        return self._user is not None

    @property
    def display_name(self):
        return self._user.display_name if self._user else "Nicht angemeldet"

    def restore(self) -> bool:
        self.restore_aufrufe += 1
        if self._gespeichert:
            self._user = self._nutzer()
        return self._gespeichert

    def login(self) -> LoginResult:
        self.login_aufrufe += 1
        self._user = self._nutzer()
        return LoginResult(ok=True, user=self._user, guild_joined=True,
                           guild_message="Du bist jetzt im Server.")

    def logout(self) -> None:
        self._user = None

    @staticmethod
    def _nutzer():
        return DiscordUser(id="42", username="clemens", global_name="Clemens",
                           guild_joined=True)


def angemeldet_beinem_start_wenn(wert: bool) -> bool:
    return wert


class Fall:
    """Ein Testfall: fuehrt den Ablauf aus und sammelt das Ergebnis."""

    def __init__(self, titel: str, gespeichert: bool, aktion: str) -> None:
        self.titel = titel
        self.gespeichert = gespeichert
        self.aktion = aktion        # "keine" | "login" | "cancel"
        self.starts = 0
        self.konto: str | None = None
        self.anmeldefenster_aufgegangen = False
        self.session = GeteilteSitzung(gespeichert)
        self._timeout = False


def einen_fall_pruefen(fall: Fall) -> None:
    haupt = MainWindow(session=fall.session)
    haupt.withdraw()

    def on_ready():
        fall.starts += 1
        # So macht es der Launcher: 500 ms nach dem Einblenden.
        def fertig():
            haupt.post_bootstrap_init()      # wie der Launcher
            fall.konto = haupt._account_name.cget("text")
            haupt.after(80, haupt.destroy)
        haupt.after(500, fertig)

    splash = BootstrapWindow(parent=haupt, config_manager=ConfigManager(),
                             on_ready=on_ready,
                             on_update_found=lambda info: None,
                             session=fall.session)

    start = time.monotonic()

    def schritt():
        fenster = getattr(splash, "_login_window", None)
        if fenster is not None:
            fall.anmeldefenster_aufgegangen = True
            if fall.aktion == "login":
                fall.session.login()
                fenster._on_result(LoginResult(ok=True, user=fall.session.user))
            else:
                fenster._on_close()
            return
        if fall.starts == 0:
            # Der Bootstrap-Thread braucht ein paar Sekunden (Update-Check,
            # Python-Suche). Erst nach einer Frist aufgeben.
            if time.monotonic() - start > 25.0:
                fall._timeout = True
                haupt.destroy()
            else:
                haupt.after(150, schritt)

    haupt.after(300, schritt)
    haupt.mainloop()


def pruefe(fall: Fall) -> tuple[bool, str]:
    einen_fall_pruefen(fall)
    if fall._timeout:
        return False, "Haupt-Fenster wurde nie gestartet"
    if fall.starts != 1:
        return False, f"Haupt-Fenster {fall.starts}x gestartet (statt 1x)"
    erwartet = "Clemens" if fall.aktion == "login" or fall.gespeichert \
        else "Nicht angemeldet"
    if fall.konto != erwartet:
        return False, f"Sidebar zeigt {fall.konto!r}, erwartet {erwartet!r}"
    if fall.gespeichert and fall.anmeldefenster_aufgegangen:
        return False, "Anmeldefenster trotz gespeicherter Anmeldung geoeffnet"
    if not fall.gespeichert and fall.aktion == "keine" \
            and not fall.anmeldefenster_aufgegangen:
        return False, "Anmeldefenster nicht geoeffnet"
    return True, (f"restore {fall.session.restore_aufrufe}x, "
                  f"login {fall.session.login_aufrufe}x, "
                  f"Sidebar {fall.konto!r}")


def main() -> int:
    faelle = [
        Fall("A) Anmeldung gespeichert -> Splash direkt zum Fenster",
             gespeichert=True, aktion="keine"),
        Fall("B) Keine Anmeldung -> Fenster -> Anmeldung erfolgreich",
             gespeichert=False, aktion="login"),
        Fall("C) Keine Anmeldung -> Fenster -> abgebrochen",
             gespeichert=False, aktion="cancel"),
    ]

    print("=" * 66)
    print("  Startablauf mit Discord-Anmeldung")
    print("=" * 66)

    fehler = 0
    for fall in faelle:
        print(f"\n  {fall.titel}")
        try:
            ok, info = pruefe(fall)
        except Exception as exc:  # noqa: BLE001
            ok, info = False, f"{type(exc).__name__}: {exc}"
        print(f"    {'[OK]     ' if ok else '[FEHLER] '}{info}")
        fehler += 0 if ok else 1

    print("\n" + "=" * 66)
    print(f"  {len(faelle) - fehler} von {len(faelle)} Faellen bestanden")
    return 1 if fehler else 0


if __name__ == "__main__":
    sys.exit(main())
