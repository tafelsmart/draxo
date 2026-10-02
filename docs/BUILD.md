# Draxo Launcher — Build-Anleitung

## SO BAUST DU DIE .EXE

```bash
# Im Projekt-Root (C:/Draxo Client):
python scripts/build_exe.py
```

Ergebnis: `dist/DraxoLauncher.exe`

## LAUNCHER STARTEN (Dev-Modus)

```bash
# Im Projekt-Root:
python launcher/draxo_launcher.py
# oder per Doppelklick auf: Starte Draxo Launcher.bat
```

## SOURCE OF TRUTH — ES GIBT NUR EINEN LAUNCHER-ORDNER

Alle Launcher-Dateien liegen im **Projekt-ROOT**:

- `launcher/draxo_launcher.py` (Einstiegspunkt)
- `ui.py`, `license_manager.py`, `updater.py`, `styles.py`,
  `utils.py`, `versions.py`, `config.py`, `animations.py`,
  `particles.py`, `process_detector.py`, `license_flow.py`

Der Build (`scripts/build_exe.py` + `draxo_launcher.spec`) liest ausschliesslich
aus dem Root. Es gibt KEINE zweite Quelle mehr (der alte `Claudelauncher/`
Ordner wurde geloescht, weil er zu veralteten Builds fuehrte).

**Aenderungen IMMER im Root machen!**

## DER LIZENZ-FLOW

1. Launcher starten -> Lizenz-Karte: ACTIVE / EXPIRED / LOCKED
2. HWID kopieren (Copy) -> Key auf draxo.netlify.app holen (3 Ads)
3. Key einfuegen -> Activate -> schreibt `License.key=...` nach
   `build/vanilla/Release/draxo_config.ini`
4. INJECT erst mit gueltigem Key aktiv (sonst LICENSE REQUIRED / KEY EXPIRED)
5. In Minecraft gibt es KEINE Key-Eingabe (7 Tabs). Die DLL prueft den Key
   beim Inject (`auth::check()`) und deaktiviert alle Module sonst.

## NACH DEM BUILD (3 Kopien synchron halten!)

```bash
cp dist/DraxoLauncher.exe DraxoLauncher.exe           # Root-Kopie
cp dist/DraxoLauncher.exe netlify/DraxoLauncher.exe   # Website-Download
```

## WICHTIG: "Failed to load Python DLL" Fehler

Wenn die .exe nach einem Update mit `Failed to load Python DLL
C:\Users\...\Temp\_MEI...\python310.dll` abstuerzt:

1. **Windows Defender blockiert die .exe** (haeufigste Ursache bei
   selbstgebauten PyInstaller-Dateien):
   - Windows-Sicherheit -> Viren- & Bedrohungsschutz ->
     Einstellungen verwalten -> Ausschlüsse -> Ausschluss hinzufuegen
     -> Ordner -> `C:\Draxo Client` auswaehlen
   - Optional auch `%TEMP%` ausschliessen
   - Danach die .exe neu starten

2. **Korrupter Update-Download** (seit v1.1.0 automatisch abgefangen):
   Der Updater prueft jetzt vor der Installation, ob die Datei eine
   echte .exe ist (MZ-Header, Groesse passt zu Content-Length).
   Kaputte Downloads werden verworfen statt installiert.

3. **Alte _MEI-Temp-Ordner aufraeumen**:
   `%TEMP%` oeffnen und alle `_MEI*` Ordner loeschen, falls welche
   haengen geblieben sind.
