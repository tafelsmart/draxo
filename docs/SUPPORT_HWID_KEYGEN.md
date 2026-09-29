# Draxo Client — Support: HWID & Key-Generierung

> **Nur für internen Support.** Diese Anleitung ist nicht für Endkunden bestimmt.

---

## 1. Was ist die HWID?

Die Draxo-HWID ist ein 32-stelliger Hex-String, der deine Maschine **eindeutig identifiziert**.  
Sie wird beim Injizieren der DLL automatisch berechnet und im LICENSE-Bereich des Menüs angezeigt.

**Berechnungsformel (deterministisch, kein WMI, kein COM):**

```
HWID = SHA-256( CPU_Brand + "|" + C_VolumeSerial + "|" + MachineGuid ) [erste 16 Bytes → 32 Hex-Zeichen]
```

| Quelle | API | Was es ist |
|--------|-----|------------|
| **CPU Brand** | `CPUID (0x80000002–0x80000004)` | CPU-Modellname (z. B. `12th Gen Intel(R) Core(TM) i5-12500H`) |
| **Volume Serial** | `GetVolumeInformationA("C:\\")` | Seriennummer des C:-Laufwerks (z. B. `E22A7B90`) |
| **MachineGuid** | Registry `HKLM\SOFTWARE\Microsoft\Cryptography` | Eindeutige Windows-Installations-ID (GUID) |

**Alle drei Quellen sind prozessunabhängig** — die HWID ist in PowerShell, CMD und javaw.exe **immer gleich**.

---

## 2. Wann ändert sich die HWID?

Der Key ist **HWID-gebunden**. Die HWID ändert sich, wenn **mindestens eine** der drei Quellen anders ist:

| Ereignis | HWID ändert sich? |
|----------|-------------------|
| CPU-Tausch | ✅ Ja — CPU Brand ist anders |
| Windows-Neuinstallation | ✅ Ja — neue MachineGuid |
| C:-Laufwerk formatiert | ✅ Ja — neue Volume Serial |
| Windows-Update / Treiber-Update | ❌ Nein |
| Minecraft-Version wechseln | ❌ Nein |
| GPU-Tausch / RAM-Tausch | ❌ Nein |
| Draxo neu installieren | ❌ Nein |

**Faustregel:** CPU-Tausch, Windows-Neuinstallation oder C:-Formatierung → neuer Key nötig.

---

## 3. Woran erkennt der Kunde eine HWID-Änderung?

### Im Menü (CONFIG → LICENSE)

Seit dem HWID-Preview-Update sieht der Kunde **live**, ob sein Key zur Maschine passt:

| Anzeige | Bedeutung |
|---------|-----------|
| 🟢 `Key expects: A1B2…` + ✅ Match | Key passt — alles gut |
| 🔴 `Key expects: X9Y8…` + ⚠ Mismatch | **HWID hat sich geändert!** Neuer Key nötig |
| ⚠ `Invalid key format` | Key ist falsch geschrieben / beschädigt |

### Verhalten der DLL

- **Key ist INVALID** → Alle Module bleiben **LOCKED** (kein Modul lässt sich aktivieren)
- **Key ist EXPIRED** → Roter Banner, alle Module werden deaktiviert
- Das Menü selbst funktioniert immer (damit man den neuen Key eingeben kann)

---

## 4. Kunden-Support: Schritt-für-Schritt

Wenn ein Kunde meldet: *"Mein Key funktioniert nicht mehr"*:

### Schritt 1: HWID prüfen lassen

Bitte den Kunden:
1. DLL injizieren → Menü öffnen (`RSHIFT`/`INSERT`)
2. **CONFIG** Tab → nach unten scrollen zu **LICENSE**
3. Screenshot vom LICENSE-Bereich schicken

Dort steht:
- Die aktuelle HWID (z. B. `FB58633AE62CAD4C54FCBB74293D5E9F`)
- Ob der Key als INVALID/EXPIRED/ACTIVE angezeigt wird
- Die Live-Key-Vorschau (Match/Mismatch)

### Schritt 2: Ursache identifizieren

- **🔴 Mismatch** → HWID hat sich geändert (Hardware-Tausch / Windows neu installiert)
- **⚠ EXPIRED** → 24h-Key ist abgelaufen (normal bei Free-Usern)
- **Ungültiges Format** → Key falsch kopiert (Leerzeichen, Zeilenumbruch)

### Schritt 3: Neuen Key erstellen

Siehe Abschnitt 5.

---

## 5. Internes Dev-Tool: Key generieren (OHNE Linkvertise)

Für Support-Fälle (HWID-Tausch, Lifetime-Owner, Test-Keys) nutzt du das **interne Keygen-Tool**.  
Damit kannst du Keys **direkt in 2 Sekunden** erstellen — ohne Linkvertise-Werbeschritte.

### Voraussetzung

- Python 3.10+ installiert
- Zugriff auf den Draxo-Quellcode (mit `tools/keygen_reference.py`)

### 5.1 Einfacher Keygen (Python-Skript)

```bash
cd "C:/Draxo Client"
```

```python
import sys, hashlib
sys.path.insert(0, 'tools')
from keygen_reference import generate_key

# ── HIER DIE KUNDEN-HWID EINTRAGEN (aus dem Screenshot / Menü) ──
kunden_hwid = "FB58633AE62CAD4C54FCBB74293D5E9F"

# 24h-Key:
key_24h = generate_key(kunden_hwid, 24)
print(f"24h-Key : {key_24h}")

# Lifetime-Key (nur Owner!):
key_lifetime = generate_key(kunden_hwid, 0)
print(f"Lifetime: {key_lifetime}")
```

**Ausgabe:**
```
24h-Key : DRAXO-XXXXX-XXXXX-XXXXX-XXXXX-XXXXX-X
Lifetime: DRAXO-YYYYY-YYYYY-YYYYY-YYYYY-YYYYY-Y
```

### 5.2 Key an Kunden senden

Den Key per DM/Support-Ticket schicken. Der Kunde trägt ihn im Menü ein:

1. Menü → **CONFIG** → **LICENSE**
2. Key in das `License Key`-Feld pasten
3. **Live-Vorschau prüft sofort:** 🟢 ✅ Match muss erscheinen
4. Auf **Activate** klicken
5. Modul-Liste wird automatisch entsperrt

### 5.3 Quick-Keygen (Einzeiler für die Kommandozeile)

```bash
cd "C:/Draxo Client" && python -c "
import sys; sys.path.insert(0,'tools')
from keygen_reference import generate_key
print(generate_key('FB58633AE62CAD4C54FCBB74293D5E9F', 24))
"
```

Einfach die HWID ersetzen und ausführen — Key erscheint direkt im Terminal.

---

## 6. HWID des Kunden herausfinden (wenn er keinen Screenshot schicken kann)

### Variante A: PowerShell (funktioniert immer)

```powershell
$cpu = (Get-ItemProperty "HKLM:\HARDWARE\DESCRIPTION\System\CentralProcessor\0" -Name ProcessorNameString).ProcessorNameString.Trim()
$volSer = (Get-Volume -DriveLetter C).FileSystemLabel  # oder: cmd /c "vol C:" → Seriennummer

# MachineGuid
$mg = (Get-ItemProperty "HKLM:\SOFTWARE\Microsoft\Cryptography" -Name MachineGuid).MachineGuid

# Kombiniert:
$comb = "$cpu|$volSer|$mg"

# SHA-256 → erste 16 Bytes als Hex
$sha = [System.Security.Cryptography.SHA256]::Create()
$hash = $sha.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($comb))
$hwid = -join ($hash[0..15] | ForEach-Object { $_.ToString("X2") })
Write-Output "HWID: $hwid"
```

> ⚠️ **Wichtig:** Die PowerShell-Methode ist **nur als Fallback** gedacht. Die echte HWID steht **im Menü** und ist der einzig verlässliche Wert. PowerShell kann bei fehlenden Admin-Rechten oder leerer Volume-Serial andere Werte liefern.

### Variante B: Der Kunde liest sie aus der Config

Die DLL schreibt die HWID in `build/vanilla/Release/draxo_config.ini`:

```
License.hwid=FB58633AE62CAD4C54FCBB74293D5E9F
```

---

## 7. FAQ

### „Der Kunde hat einen Lifetime-Key, aber nach Windows-Neuinstallation geht er nicht."

→ HWID hat sich geändert (neue MachineGuid). **Neuen Lifetime-Key mit der neuen HWID generieren** (siehe 5.1, `generate_key(hwid, 0)`).

### „Ich will einem Tester einen 7-Tage-Key geben."

```python
generate_key(hwid, 7 * 24)  # 7 Tage = 168 Stunden
```

`expiry_hours` akzeptiert jeden Integer-Wert.

### „Der Key wird im Menü als INVALID angezeigt, obwohl er frisch generiert wurde."

1. HWID im Menü mit der HWID vergleichen, für die der Key erstellt wurde
2. Sind sie **unterschiedlich**? → Key für die richtige HWID neu generieren
3. Sind sie **gleich**? → Key auf Tippfehler prüfen (Leerzeichen, Bindestriche, Groß-/Kleinschreibung)

### „Kann ein Kunde die HWID fälschen?"

Nein. Die HWID-Berechnung läuft in der DLL (`auth.cpp`, `getHWID()`). Es gibt keinen User-seitigen Weg, die HWID zu manipulieren. Der Key ist an `SHA-256(CPUID + VolumeSerial + MachineGuid)` gebunden — alle drei Werte sind hardware- bzw. installationsseitig festgelegt.

---

## 8. Quick-Reference

| Aktion | Befehl / Ort |
|--------|-------------|
| **HWID anzeigen** | Menü → CONFIG → LICENSE (wird automatisch angezeigt) |
| **24h-Key generieren** | `python -c "from keygen_reference import generate_key; print(generate_key('HWID', 24))"` |
| **Lifetime-Key generieren** | `python -c "from keygen_reference import generate_key; print(generate_key('HWID', 0))"` |
| **Keygen-Tool** | `tools/keygen_reference.py` |
| **Key-Validierung (C++)** | `src/core/auth.cpp` → `validateKey()` |
| **HWID-Berechnung (C++)** | `src/core/auth.cpp` → `getHWID()` |
| **Config-Datei** | `build/vanilla/Release/draxo_config.ini` |

---

> **Letzte Aktualisierung:** August 2026  
> **Nur für Draxo-Team.** Nicht an Endkunden weitergeben.
