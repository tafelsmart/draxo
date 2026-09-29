@echo off
title Draxo Client Launcher
cd /d "%~dp0"
python draxo_launcher.py
if errorlevel 1 (
    echo.
    echo [Fehler] Der Launcher konnte nicht gestartet werden.
    echo Pruefe: python --version  und  pip install -r requirements.txt
    pause
)
