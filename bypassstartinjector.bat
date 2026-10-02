@echo off
REM ============================================================
REM  Draxo Client - Dev-Variante (Start Injector)
REM
REM  Startet den Launcher fuer Entwicklung und Tests, ohne Discord-
REM  Anmeldung: das Anmeldefenster und das Injektions-Gate werden
REM  dabei uebergangen. Der Schalter ist die Umgebungsvariable
REM  DRAXO_DEV_MODE, gelesen in discord_auth.py (Sitzung) und
REM  license_manager.py (Lizenzstatus).
REM
REM  WICHTIG: Das ist KEIN Offline-Modus. Netz, Update-Check,
REM  Versionsliste und Lizenzpruefung laufen unveraendert weiter.
REM  Es fehlt ausschliesslich die Discord-Anmeldung.
REM
REM  WICHTIG fuer die Programmierung:
REM   Diese Datei MUSS CRLF-Zeilenenden haben, kein LF. Mit LF
REM   bricht cmd.exe die Klammerbloecke ab.
REM
REM   Die Python-Suche ist bewusst dieselbe wie in
REM   "Starte Draxo Launcher.bat": config.json, dann "py -3",
REM   dann "python". Jeder Kandidat wird nur akzeptiert, wenn er
REM   die Pakete aus requirements.txt auch wirklich hat.
REM
REM   Warum keine verschachtelten Klammerbloecke: dort werden
REM   %VARIABLE% und "if defined" zum Parse-Zeitpunkt ausgewertet,
REM   nicht zur Laufzeit. Wertet man darin eine Variable, die eine
REM   for-Schleife erst danach setzt, ist sie immer leer. Deshalb
REM   wird jede Pruefung ueber :label + goto gesteuert.
REM ============================================================

title Draxo Client - Dev-Variante
cd /d "%~dp0"

REM Der ganze Schalter.
set "DRAXO_DEV_MODE=1"

set "PY="
set "CAND="
set "CFGPATH="

REM ------------------------------------------------------------
REM 1) Kandidaten durchprobieren
REM ------------------------------------------------------------

REM 1a) Pfad aus config.json
for /f "usebackq delims=" %%P in (`python -c "import json;print(json.load(open('config.json')).get('python_path',''))" 2^>nul`) do set "CFGPATH=%%P"
if defined CFGPATH goto :try_cfgpath
goto :try_py

:try_cfgpath
if not exist "%CFGPATH%" goto :try_py
"%CFGPATH%" -c "import customtkinter, psutil, PIL" >nul 2>&1
if errorlevel 1 goto :try_py
set "PY=%CFGPATH%"
goto :found

REM 1b) Windows-Launcher
:try_py
for /f "usebackq delims=" %%P in (`py -3 -c "import sys; print(sys.executable)" 2^>nul`) do set "CAND=%%P"
if not defined CAND goto :try_python
"%CAND%" -c "import customtkinter, psutil, PIL" >nul 2>&1
if errorlevel 1 goto :try_python
set "PY=%CAND%"
goto :found

REM 1c) PATH
:try_python
for /f "usebackq delims=" %%P in (`python -c "import sys; print(sys.executable)" 2^>nul`) do set "CAND=%%P"
if not defined CAND goto :failed
"%CAND%" -c "import customtkinter, psutil, PIL" >nul 2>&1
if errorlevel 1 goto :failed
set "PY=%CAND%"

:found
echo Python: %PY%
echo Modus:  Dev-Variante (DRAXO_DEV_MODE=1) - keine Discord-Anmeldung noetig
echo.
"%PY%" launcher\draxo_launcher.py
if errorlevel 1 goto :crashed
goto :done

REM ============================================================
REM  Fehlerbehandlung
REM ============================================================

:failed
echo.
echo [FEHLER] Kein einsatzbereites Python 3 gefunden.
echo.
echo   Falls Python installiert ist, aber die Pakete fehlen:
echo       python -m pip install -r requirements.txt
echo.
echo   Falls Python fehlt, lade es herunter:
echo       https://www.python.org/downloads/
echo   Wichtig: den Haken bei "Add Python to PATH" setzen.
echo.
pause
exit /b 1

:crashed
echo.
echo [FEHLER] Der Launcher wurde unerwartet beendet.
echo.
echo   Die Details stehen in "draxo_client.log" in diesem Ordner.
echo.
pause
exit /b 1

:done
endlocal
exit /b 0