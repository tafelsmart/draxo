#!/usr/bin/env python3
"""
patch_integrity.py - DLL-Integritaetspatch nach dem Build.

Lokalisiert die dedizierte .drsent-Section (definiert in src/core/integrity.cpp)
direkt ueber die PE-Section-Tabelle und schreibt den CRC32 der .text-Sektion
in die ersten 16 Bytes der Section.

Warum Section-basiert statt Magic-Search?
  - Nach dem ersten Patch sind die Magic-Bytes ersetzt; eine Suche nach den
    Magic-Bytes schlaegt dann fehl und der Build meldet falsche Warnungen.
  - Wenn die DLL nach dem Patch neu gelinkt wird (Launcher-Incremental-Build),
    stimmt der eingetragene CRC nicht mehr mit .text ueberein -> der Client
    wuerde sich zur Laufzeit als "manipuliert" selbst blockieren.
  - Diese Variante schreibt den FRISCHEN CRC IMMER in die Section und haelt
    die DLL damit in jedem Build konsistent.

Immer nicht-fatal (Exit-Code 0) - der Build bricht NIE ab.
"""
import sys, struct

MAGIC_A = 0xD7A0D7D0D7A0D7D0  # -> Bytes: D0 D7 A0 D7 D0 D7 A0 D7
MAGIC_B = 0xD7D0D7A0D7D0D7A0  # -> Bytes: A0 D7 D0 D7 A0 D7 D0 D7


def crc32(data):
    table = []
    for i in range(256):
        crc = i
        for _ in range(8):
            crc = 0xEDB88320 ^ (crc >> 1) if crc & 1 else crc >> 1
        table.append(crc)
    crc = 0xFFFFFFFF
    for b in data:
        crc = table[(crc ^ b) & 0xFF] ^ (crc >> 8)
    return crc ^ 0xFFFFFFFF


def pe_sections(dll):
    """Liefert Liste (name, raw_off, raw_sz, virt_sz)."""
    pe = struct.unpack_from('<I', dll, 0x3C)[0]
    num = struct.unpack_from('<H', dll, pe + 6)[0]
    opt = struct.unpack_from('<H', dll, pe + 20)[0]
    ss = pe + 24 + opt
    out = []
    for i in range(num):
        so = ss + i * 40
        name = dll[so:so + 8].rstrip(b'\x00').decode('latin1')
        vsz = struct.unpack_from('<I', dll, so + 8)[0]
        rsz = struct.unpack_from('<I', dll, so + 16)[0]
        roff = struct.unpack_from('<I', dll, so + 20)[0]
        out.append((name, roff, rsz, vsz))
    return out


def patch_dll(dll_path):
    with open(dll_path, 'rb') as f:
        dll = bytearray(f.read())

    secs = pe_sections(dll)

    text_sec = next((s for s in secs if s[0] == '.text'), None)
    sent_sec = next((s for s in secs if s[0] == '.drsent'), None)

    if not text_sec or not sent_sec:
        print(f'WARNUNG: .text/.drsent Section fehlt in {dll_path} '
              f'(Build wird NICHT abgebrochen).')
        print(f'  DLL size: {len(dll)}  Sections: {[s[0] for s in secs]}')
        print('  Integrity-Check wird zur Laufzeit uebersprungen.')
        return True  # NICHT-fatal

    _, troff, trsz, _ = text_sec
    _, soff, srsz, _ = sent_sec

    text_data = bytes(dll[troff:troff + trsz])
    h = crc32(text_data)
    packed = struct.pack('<II', h, h)  # 8 Bytes, zweimal = 16 Bytes

    # Alte Section-Bytes zeigen (Magic oder alter CRC)
    old = bytes(dll[soff:soff + 16])
    old_magic = old[:8] == struct.pack('<Q', MAGIC_A)
    old_crc = struct.unpack('<I', old[0:4])[0] if not old_magic else 0

    # Frischen CRC IMMER schreiben (auch wenn schon gepatcht -> korrigiert
    # Inkonsistenzen nach Incremental-Relinks)
    dll[soff:soff + 16] = packed * 2

    with open(dll_path, 'wb') as f:
        f.write(dll)

    if old_magic:
        print(f'Patched CRC32=0x{h:08X} bei .drsent 0x{soff:x} (Magic ersetzt)')
    elif old_crc and old_crc != h:
        print(f'Updated CRC32=0x{h:08X} bei .drsent 0x{soff:x} '
              f'(war 0x{old_crc:08X} - Inkonsistenz nach Relink korrigiert)')
    else:
        print(f'Patched CRC32=0x{h:08X} bei .drsent 0x{soff:x} (konsistent)')
    return True


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print('Usage: patch_integrity.py <draxo.dll>')
        sys.exit(0)
    patch_dll(sys.argv[1])
    sys.exit(0)  # immer 0 - niemals Build abbrechen
