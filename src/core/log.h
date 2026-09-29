#pragma once
#include <cstdarg>

/*
 * Log — Thread-sicherer Ringpuffer + DATEI-Logging für printf-Ausgaben.
 *
 * Alle printf(...) im Projekt landen hier UND auf der Konsole UND in
 * draxo_client.log (neben der DLL). Die Datei überlebt jede Sitzung,
 * damit Fehler nachträglich analysiert werden können.
 */
namespace Log {
    // Öffnet draxo_client.log neben der DLL (append) + Kopfzeile
    void init();

    // Schließt die Datei sauber (vor FreeLibrary rufen)
    void shutdown();

    // printf-Ersatz: formatiert, schreibt in Ringpuffer + stdout + Datei
    void write(const char* fmt, ...);

    // Letzte Zeilen (älteste zuerst), threadsicher kopiert
    int snapshot(char* out, int maxBytes);

    // Anzahl gespeicherter Zeilen
    int count();

    void clear();

    // Absoluter Pfad der Log-Datei (fürs Debug-Fenster)
    const char* filePath();
}
