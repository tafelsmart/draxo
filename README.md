<div align="center">

# ⬡ Draxo Client

**Ein moderner Minecraft-Client für Windows — Launcher, Autoupdater und modulare Cheat-DLL in einem Projekt.**

[![Version](https://img.shields.io/badge/Version-1.1.0-8B5CF6?style=flat-square)](VERSION)
[![Python](https://img.shields.io/badge/Python-3.10%2B-3776AB?style=flat-square)](requirements.txt)
[![Lizenz](https://img.shields.io/badge/Lizenz-proprietär-EF4444?style=flat-square)](LICENSE)
[![Plattform](https://img.shields.io/badge/Plattform-Windows-0078D4?style=flat-square)](#)

**[Website](https://draxo.netlify.app)** · **[Changelog](https://draxo.netlify.app/changelog.html)** · **[Support](docs/SUPPORT_HWID_KEYGEN.md)**

</div>

---

## 📖 Inhalt

- [Überblick](#-überblick)
- [Schnellstart](#-schnellstart)
- [Projektstruktur](#-projektstruktur)
- [Wie der Inject funktioniert](#-wie-der-inject-funktioniert)
- [Versionsunterstützung](#-versionsunterstützung)
- [Lizenzsystem](#-lizenzsystem)
- [Build](#-build)
- [Auto-Updater](#-auto-updater)
- [Module](#-module)
- [Fehlerbehebung](#-fehlerbehebung)
- [Mitwirken](#-mitwirken)

---

## 🎯 Überblick

**Draxo** besteht aus zwei Komponenten, die zusammen als ein Produkt ausgeliefert werden:

| Komponente | Sprache | Zweck |
|---|---|---|
| **DraxoLauncher** | Python 3 · CustomTkinter | Die Oberfläche: Versionserkennung, Inject, Autoupdater, Lizenzanzeige |
| **draxo.dll** | C++20 · JNI · MinHook · ImGui | Der eigentliche Client — wird in den laufenden Minecraft-Prozess injiziert |

Der Launcher ist absichtlich **kein Minecraft-Launcher**: Er startet das Spiel nicht, sondern erkennt eine bereits laufende Instanz, baut (oder findet) die passende DLL und injiziert sie. Das Spiel selbst bleibt unangetastet — die DLL lebt nur im Prozessspeicher.

> **Unterstützte Minecraft-Versionen:** 1.17 → 26.x (Forge, NeoForge und Vanilla)

---

## 🚀 Schnellstart

### Launcher starten (Development)

```bash
git clone <dein-repo-url>
cd "Draxo Client"

python -m venv .venv
.venv\Scripts\activate
pip install -r requirements.txt

python launcher/draxo_launcher.py
```

Oder einfach [Starte Draxo Launcher.bat](Starte%20Draxo%20Launcher.bat) per Doppelklick.

### Inject

1. Minecraft starten (Vanilla, Forge oder NeoForge — **1.17 oder neuer**)
2. `DraxoLauncher.exe` (bzw. `python launcher/draxo_launcher.py`) öffnen
3. Der Launcher erkennt die laufende Instanz automatisch
4. **INJECT** klicken

Der Client öffnet sich mit dem `GUI`-Keybind (Standard: **Right Shift**) im Spiel.

---

## 📂 Projektstruktur

Es gibt bewusst **genau eine Quelle der Wahrheit** — den Projekt-Root. Dort liegen alle Pfade, die der Build braucht: `src/`, `tools/`, `imgui/`, `minhook-master/`, `assets/`, `VERSION` und `build/prebuilt/`. Die Python-Module liegen in `launcher/` und importieren sich untereinander flach; ein dünner Shim im Root legt `launcher/` in den `sys.path`.

```
Draxo Client/
│
├── 🐍 Launcher (Python) — alles in launcher/
│   ├── draxo_launcher.py      # Einstiegspunkt: Logging, Fehlerbehandlung, Fensterstart
│   ├── bootstrap.py           # Premium-Splash: Update-Check, Python-Suche, pip-Setup
│   ├── ui.py                  # Hauptfenster: Logo, Status, INJECT-Button
│   ├── auth_ui.py             # Discord-Anmeldefenster
│   ├── updater.py             # GitHub-Releases-Check, Download, Neustart via .bat
│   ├── process_detector.py    # Findet javaw.exe + Versions-/Flavor-Erkennung
│   ├── builder_runner.py      # Bindeglied Launcher → Builder (Dev / Frozen / Prebuilt)
│   ├── setup_manager.py       # Ersteinrichtung: Python finden, pip-Pakete installieren
│   ├── license_manager.py     # HWID + Grant-Validierung (nutzt license_signing)
│   ├── license_signing.py     # Ed25519-Grant: Format, Pruefen, oeffentlicher Schluessel
│   ├── discord_auth.py        # OAuth2 + PKCE, Token-Ablage via DPAPI, Dev-Modus
│   ├── versions.py            # Live-Minecraft-Versionen + statischer Fallback
│   ├── config.py              # Persistente config.json
│   ├── styles.py              # Design-Tokens (Farben, Fonts, Layout)
│   ├── widgets.py             # Wiederverwendbare UI-Bausteine
│   ├── animations.py          # Fade-In, Glow, Scale-In
│   ├── particles.py           # Partikel-Hintergrund
│   └── utils.py               # Pfadaufloesung, Logging, Bildladen
│
├── ⚙️ Build & Distribution
│   ├── scripts/build_exe.py           # PyInstaller-Build → dist/DraxoLauncher.exe
│   ├── scripts/dev_mode_hook.py       # Runtime-Hook der Dev-EXE
│   ├── draxo_launcher.spec            # Release-Spec (Assets, Prebuilt-DLLs, Imports)
│   ├── draxo_dev.spec                 # Dev-Spec (Release + dev_mode_hook)
│   ├── VERSION                        # Einzelne Quelle der Wahrheit fuer die Version
│   └── build/                         # prebuilt/<version>/draxo*.dll, vanilla/Release/
│
├── 🔧 Hilfsskripte — scripts/
│   ├── build_exe.py, release.py, license_flow.py, crash_test.py
│   └── _shot*.py, _capture.py         # Screenshot-Helfer
│
├── 🎮 Client (C++)
│   ├── CMakeLists.txt         # Baut draxo.dll (C++20, ImGui, MinHook)
│   ├── src/
│   │   ├── dllmain.cpp        # DLL-Einstieg, JNI-Init, Hook-Installation
│   │   ├── core/              # auth, hooks, JVM-Wrapper, Integrität, Logging
│   │   ├── modules/           # ~140 Cheat-Module (eines je .cpp/.h)
│   │   ├── render/            # ClickGUI, HUD, ESP-Renderer, Theme
│   │   ├── sdk/               # Minecraft- und Entity-Wrapper
│   │   ├── config/            # Mapping-Tabellen
│   │   └── jni_headers/       # Minimale JNI/JVMTI-Header
│   ├── imgui/                 # Dear ImGui (Vendor)
│   └── minhook-master/        # MinHook (Vendor)
│
├── 🛠️ Werkzeuge
│   ├── vanilla_builder.py     # Mappings laden, übersetzen, DLL bauen, injizieren
│   ├── prebuild_dlls.py       # Vorgefertigte DLLs für alle Versionen erzeugen
│   ├── patch_integrity.py     # Patcht den DLL-Integritäts-Hash nach dem Build
│   ├── cache_all_mappings.py  # Lädt Mappings für alle Versionen im Voraus
│   ├── keygen.php             # Web-Keygen (identischer Algorithmus wie die DLL)
│   └── keygen_reference.py    # Referenz-Keygen in Python
│
├── 🌐 Website
│   └── netlify/               # Landingpage, Keygen, Changelog (EN/DE/ES/FR)
│
└── 📄 Dokumentation
    ├── README.md
    ├── BUILD_README.md        # Build-Anleitung
    └── docs/                  # Support-Dokumente
```

---

## 💉 Wie der Inject funktioniert

```
  ┌──────────────┐
  │  Minecraft   │  ← läuft bereits (javaw.exe)
  │   (JVM)      │
  └──────┬───────┘
         │  Kommandozeile lesen
         ▼
  ┌──────────────┐   erkennt: Version (1.21.4) + Flavor (vanilla/forge/neoforge)
  │  Launcher    │
  └──────┬───────┘
         │  builder_runner.run_build()
         ▼
  ┌──────────────────────────────────────────────────────┐
  │ 1. Prebuilt-DLL gefunden?  → direkt injizieren       │
  │ 2. Im Bundle enthalten?   → neben .exe entpacken    │
  │ 3. Sonst: CMake + MSVC    → draxo.dll kompilieren   │
  └──────────────────────┬───────────────────────────────┘
                         │  CreateRemoteThread + LoadLibrary
                         ▼
  ┌──────────────────────────────────────────────────────┐
  │  draxo.dll                                            │
  │  • JNI-Hooks auf Minecraft-Methoden                  │
  │  • ImGui-ClickGUI (Overlay-Fenster)                   │
  │  • 140 Module über den GUI aktivierbar                │
  └──────────────────────────────────────────────────────┘
```

**Warum werden DLLs nie direkt aus dem PyInstaller-Bundle injiziert?**
`_MEIPASS` ist ein temporärer Ordner, der beim Beenden gelöscht wird. Die DLL schreibt ihre Konfiguration (inklusive Lizenz-Key) *neben sich* — aus dem Temp-Ordner wäre der Key nach jedem Start weg. Deshalb werden Prebuilt-DLLs beim ersten Inject in einen **stabilen Ordner neben der .exe** entpackt.

---

## 📦 Versionsunterstützung

Die Versionsliste wird **live** von Mojang geladen (`piston-meta.mojang.com`) — neue Releases erscheinen automatisch, ohne dass etwas gepflegt werden muss. Offline greift eine statische Fallback-Liste.

Die Erkennung ist bewusst streng: Nur Versionen, die entweder aus einer autoritativen Quelle stammen (`--version`-Flag, `versions/<id>/`-Pfad, `fmlloader-<mcver>-*.jar`) **oder** in der bekannten Liste stehen, werden akzeptiert. Dadurch werden Bibliotheksversionen wie `1.316.0.7372` (Von Minecraft-Ressourcen) niemals fälschlich als Spielversion erkannt.

| Flavor | Erkennung | Build-Variante |
|---|---|---|
| Vanilla | Standard-Kommandozeile | `draxo.dll`, obfuskierte Mappings |
| Forge | `fmlloader` / `forge` in der Cmdline | `draxo_forge.dll`, offizielle Namen |
| NeoForge | `neoforge` in der Cmdline | `draxo_forge.dll`, offizielle Namen |

---

## 🔑 Lizenzsystem

**Kein Account, kein Login — ein HWID-gebundener Key.**

1. Der Launcher zeigt die **HWID** deines Rechners (SHA256 über CPU-ID + Mainboard-Serial)
2. Key über [draxo.netlify.app/keygen.html](https://draxo.netlify.app/keygen.html) beziehen
3. Key im Launcher einfügen → wird nach `draxo_config.ini` geschrieben
4. Die DLL prüft den Key beim Inject (`auth::check()`) und deaktiviert Module ohne gültigen Key

```
HWID  = SHA256("CPU-ID|Mainboard-Serial")[:16]     → maschinengebunden
Key   = Crockford-Base32( Poly-XOR( HWID-Hash ∥ Expiry, 32-Byte-Secret ) )
         └ v1: 20 Zeichen  → dauerhaft
         └ v2: 26 Zeichen  → mit 4-Byte-Unix-Ablaufdatum
```

Der Algorithmus ist in drei Sprachen **byte-identisch** implementiert: `src/core/auth.cpp` (C++), [license_manager.py](license_manager.py) (Python) und `tools/keygen.php` (Web). **Änderungen an einer Stelle müssen in allen drei nachgezogen werden**, sonst funktioniert die Validierung nicht mehr.

Weitere Details: [docs/SUPPORT_HWID_KEYGEN.md](docs/SUPPORT_HWID_KEYGEN.md)

---

## 🏗️ Build

### Launcher → `DraxoLauncher.exe`

```bash
pip install pyinstaller
python scripts/build_exe.py
```

Ergebnis: `dist/DraxoLauncher.exe`

Das `.exe` ist **self-contained**: Python-Runtime, alle Launcher-Module, die C++-Quellen (`src/`, `CMakeLists.txt`, `minhook-master/`) und alle vorgefertigten DLLs sind eingebettet. Wer die `.exe` herunterlädt, braucht **kein Python und keine Build-Tools** — die Vorgefertigten DLLs decken 1.19.2 bis 26.2 ab.

Nach dem Build die Kopien synchron halten:

```bash
cp dist/DraxoLauncher.exe DraxoLauncher.exe          # Root-Kopie
cp dist/DraxoLauncher.exe netlify/DraxoLauncher.exe  # Website-Download
```

### DLL → `draxo.dll`

Braucht **Visual Studio Build Tools** (Workload *Desktop Development with C++*) und CMake ≥ 3.20.

```bash
# Einzelnen Version bauen
python tools/vanilla_builder.py --version 1.21.11

# Oder direkt via CMake
cmake -B build/vanilla -S . -A x64
cmake --build build/vanilla --config Release
```

### Alle Versionen im Voraus bauen

```bash
python tools/prebuild_dlls.py        # erzeugt build/prebuilt/<version>/draxo.dll
```

> **Wichtig:** `build/` niemals blind löschen — dort liegen die vorgefertigten DLLs **und** `draxo_config.ini` mit dem Lizenz-Key. `scripts/build_exe.py` löscht deshalb gezielt nur `dist/` und `build/draxo_launcher/`.

---

## 🔄 Auto-Updater

Der Launcher prüft beim Start (und der Bootstrap im Hintergrund) auf Updates:

1. **GitHub Releases API** (`api.github.com/repos/batotomato/draxo/releases/latest`) — kostenlos, unbegrenzt Bandbreite
2. **Netlify** (`draxo.netlify.app/version.json`) — Fallback, falls GitHub nicht erreichbar ist

Vor der Installation wird die Datei geprüft: MZ-Header, Mindestgröße 1 MB und Übereinstimmung mit `Content-Length`. Erst danach ersetzt ein temporäres `.bat`-Skript die laufende `.exe` und startet sie neu. Ein abgeschnittener Download kann den Launcher so **nicht** in einen unusable Zustand bringen.

---

## 🎮 Module

Über 140 Module, gruppiert nach Kategorie (Auszug):

| Kategorie | Beispiele |
|---|---|
| **Combat** | KillAura, AimAssist, TriggerBot, Criticals, AutoTotem, AutoArmor, Velocity |
| **Movement** | Speed (BHop/Strafe/Y-Port), Fly (Vanilla/Packet/Motion), Sprint, Step, Glide, Jesus, Safewalk |
| **Render** | ESP, Nametags, Tracers, Fullbright, XRay, HUD, BlockOverlay, Hitmarkers |
| **World** | Scaffold, Nuker, AutoTool, BedAura, ChestStealer, Blink, Freecam |
| **Utility** | ChatBypass, AntiAFK, ClickGUI-Theme, Presets, Notifications, InventoryCleaner |

Jedes Modul hat eigene Einstellungen, eigene Hotkeys und eine eigene Farbe. Presets (**Legit** / **Rage**) lassen sich speichern und laden.

---

## 🛠️ Fehlerbehebung

<details>
<summary><b>„Failed to load Python DLL"</b> beim Start der .exe</summary>

Fast immer **Windows Defender**. Selbstgebaute PyInstaller-Dateien werden häufig fälschlich blockiert:

1. Windows-Sicherheit → Viren- & Bedrohungsschutz → Einstellungen verwalten → Ausschlüsse
2. Ordner `C:\Draxo Client` ausschließen
3. `%TEMP%` ebenfalls ausschließen
4. `.exe` neu starten
</details>

<details>
<summary><b>„Minecraft nicht erkannt"</b></summary>

- Minecraft **1.17 oder neuer**? (ältere Versionen werden nicht unterstützt)
- Läuft der Prozess als `javaw.exe`? Der Launcher sucht `javaw.exe`, `java.exe` und `Minecraft.Windows.exe`
- Minecraft als Administrator gestartet? Der Launcher braucht dann dieselben Rechte
</details>

<details>
<summary><b>„Keine vorgefertigte DLL und keine Build-Tools"</b></summary>

Die Version ist nicht in `build/prebuilt/` vorhanden. Lösung:

```bash
python tools/vanilla_builder.py --version <version>
```

Das setzt Visual Studio Build Tools voraus. Alternativ `build/prebuilt/` im Bundle ergänzen.
</details>

<details>
<summary><b>Alte <code>_MEI</code>-Temp-Ordner blockieren den Start</summary>

`%TEMP%` öffnen und alle Ordner namens `_MEI*` löschen.
</details>

<details>
<summary><b>Key wird als ungültig erkannt</b></summary>

- HWID muss **nach dem ersten Inject** kopiert werden — die DLL schreibt den echten HWID erst beim Laden
- Key in **Großbuchstaben** einfügen
- Bei v2-Keys (26 Zeichen): prüfen, ob das Ablaufdatum noch nicht überschritten ist
</details>

---

## 🤝 Mitwirken

```bash
git clone <dein-repo-url>
cd "Draxo Client"
python -m venv .venv && .venv\Scripts\activate
pip install -r requirements.txt
```

**Regeln für Beiträge:**

- Änderungen **immer im Projekt-Root** machen — es gibt keine zweite Launcher-Kopie
- Der Key-Algorithmus muss in `auth.cpp`, `license_manager.py` **und** `keygen.php` identisch bleiben
- Keine `build/`- oder `dist/`-Artefakte einchecken (steht in `.gitignore`)
- Secrets (`discord_webhook.txt`, `draxo_config.ini`) niemals committen

---

## 📄 Projektstatus

| Komponente | Stand |
|---|---|
| Launcher (Python) | ✅ Stabil, v1.1.0 |
| Auto-Updater | ✅ GitHub + Netlify-Fallback |
| Versionserkennung | ✅ Live von Mojang, 1.17 – 26.x |
| DLL-Build | ✅ 28 Versionen vorgefertigt |
| ClickGUI & Module | ✅ ~140 Module |
| Lizenz-Gate im Launcher | ⚠️ Prüfung erfolgt aktuell in der DLL |

---

<div align="center">

**⬡ Draxo Client** — entwickelt für die Community.

*Geben Sie Achtung: Die Verwendung von Cheats in Multiplayer-Spielen verstößt gegen die
Nutzungsbedingungen von Mojang Studios und kann zum Serververbot führen.*

</div>
