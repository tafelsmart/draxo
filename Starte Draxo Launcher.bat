@echo off
REM ============================================================
REM  Draxo Client — Launcher starten
REM
REM  WICHTIG fuer die Programmierung:
REM   Diese Datei MUSS CRLF-Zeilenenden haben, kein LF. Mit LF
REM   bricht cmd.exe die Klammerbloecke ab.
REM
REM   Zwei cmd-Fallstricke sind hier bewusst vermieden:
REM   1) Verschachtelte Klammerbloecke: dort werden %VARIABLE% und
REM      "if defined" ZUM PARS-ZEITPUNKT ausgewertet, nicht zur
REM      Laufzeit. Wertet man darin eine Variable, die eine for-
REM      Schleife erst danach setzt, ist sie immer leer.
REM   2) Deshalb wird jede Pruefung ueber :label + goto gesteuert
REM      und nicht ueber Verschachtelung.
REM ============================================================

title Draxo Client Launcher
cd /d "%~dp0"

set "PY="
set "CAND="
set "CFGPATH="

REM ------------------------------------------------------------
REM 1) Kandidaten durchprobieren
REM    Reihenfolge: config.json, dann "py -3", dann "python".
REM    Jeder Kandidat wird nur akzeptiert, wenn er die Pakete auch
REM    wirklich hat. Das ist wichtig, weil "py -3" auf vielen
REM    Rechnern auf eine ANDERE Version zeigt als "python" —
REM    und dann fehlen die Pakete.
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
echo.
"%PY%" draxo_launcher.py
if errorlevel 1 goto :crashed
goto :done

REM ============================================================
REM  Fehlerbehandlung
REM ============================================================

:failed
echo.
echo [FEHLER] Kein einsatzbereites Python 3 gefunden.
echo.
echo   Es wurde eine Python-Installation gesucht, die die Pakete
echo   aus requirements.txt bereits installiert hat.
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
