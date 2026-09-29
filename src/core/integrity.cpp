/*
 * integrity.cpp - Sentinel-Definition fuer die DLL-Integritaetspruefung.
 *
 * Die 16 Magic-Bytes liegen in einer DEDIZIERTEN PE-Section (.drsent).
 * __declspec(allocate) + volatile erzwingt, dass der Compiler sie als
 * rohe Daten im Binary ablegt - er kann sie NICHT in Code-Immediates
 * falten (das war die Ursache fuer sporadisch fehlende Sentinels).
 *
 * Das Post-Build-Skript tools/patch_integrity.py sucht diese 16 Bytes
 * und ueberschreibt sie mit dem CRC32 der .text-Sektion.
 */

#include "core/integrity.h"

#pragma section(".drsent", read)
__declspec(allocate(".drsent"))
const volatile uint64_t g_drsentinel[2] = {
    0xD7A0D7D0D7A0D7D0ULL,
    0xD7D0D7A0D7D0D7A0ULL
};
