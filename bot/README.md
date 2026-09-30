# Draxo Discord Bot

Discord-Bot für **Draxo Client | FREE** — Community, Begrüßung und
serverseitige Key-Ausgabe. Läuft auf deinem Linux-Server unter systemd,
nicht auf deinem PC.

---

## Was der Bot macht

| | |
|---|---|
| **Begrüßung** | Neues Mitglied bekommt im Welcome-Channel ein Panel mit Knöpfen |
| **Grant holen** | Knopf oder `/key` → HWID abfragen → signierter Grant per **DM** |
| **Rate-Limit** | `KEY_MAX_PER_DAY` Grants pro Nutzer und Tag, serverseitig in SQLite |
| **Auto-Rolle** | Optional, für neue Mitglieder |
| **Protokoll** | Jede Ausstellung geht in `LOG_CHANNEL_ID`; HWIDs maskiert, Grants gekürzt |
| **Widerruf** | `grants.revoke()` sperrt einen Fingerabdruck — der Launcher erfährt es über `/api/v1/verify` |
| **Diagnose** | `/status`, `/serverinfo`, `/ping`, `/hilfe` |

Befehle: `/key` · `/hwid` · `/status` · `/serverinfo` · `/hilfe` · `/ping`

---

## Installation

Auf deinem Rechner, nicht auf dem Server:

```bash
# 1) Bot-Ordner auf den Server kopieren
scp -r bot/ draxo@dein-server:/tmp/draxo-bot

# 2) Auf dem Server
ssh draxo@dein-server
cd /tmp/draxo-bot
sudo ./deploy/install.sh
```

Das Skript legt an: System-Benutzer `draxo`, Verzeichnis `/opt/draxo-bot`,
Virtualenv, systemd-Unit, und startet den Dienst — sofern das Token schon
in der `.env` steht.

**Danach einmalig die `.env` ausfüllen:**

```bash
sudo nano /opt/draxo-bot/.env
sudo systemctl restart draxo-bot
sudo journalctl -u draxo-bot -f
```

### Was in die `.env` muss

| Variable | Pflicht | Bedeutung |
|---|:--:|---|
| `DISCORD_TOKEN` | **ja** | Bot → Reset Token im Developer Portal |
| `DISCORD_GUILD_ID` | empfohlen | Sonst werden Slash-Commands global verteilt (bis zu 1 h Verzögerung) |
| `DRAXO_KEY_SECRET` | für Keys | 64 Hex-Zeichen, siehe unten |
| `WELCOME_CHANNEL_ID` | für Begrüßung | Rechtsklick auf Channel → ID kopieren |
| `LOG_CHANNEL_ID` | optional | Protokollchannel |
| `MEMBER_ROLE_ID` | optional | Rolle über der Bot-Rolle |

### Privilegierter Intent

Für die Begrüßung braucht der Bot den **Server-Mitglieder-Intent**:

Developer Portal → Application → Bot → **Privileged Intents** →
*Server Members Intent* **AN** → 2× Speichern.

Ohne diesen Intent startet der Bot normal, `/key` und alle Befehle
funktionieren — nur `on_member_join` feuert nicht. Alternative:
`WELCOME_ON_JOIN=0` setzen, dann wird der Intent gar nicht angefordert.

---

## Das Key-Secret

Der Bot mintet Keys im **exakt gleichen Format**, das der Launcher gegen die
HWID prüft. Damit das funktioniert, muss `DRAXO_KEY_SECRET` derselbe Wert
sein, den auch der Launcher verwendet. Auslesen:

```bash
python -c "import sys; sys.path.insert(0,'.'); import license_manager; \
           print(license_manager._recon_secret().hex())"
```

Das Kommando steht auch fertig in `.env.example`.

Ohne `DRAXO_KEY_SECRET` startet der Key-Teil bewusst **nicht**: `/key` und
der Key-Knopf antworten mit einem Klartext-Hinweis statt still zu scheitern.

---

## Betrieb

```bash
sudo systemctl status  draxo-bot     # läuft es?
sudo systemctl restart draxo-bot     # nach .env-Änderung
sudo journalctl -u draxo-bot -f      # Log live
sudo journalctl -u draxo-bot -p err  # nur Fehler
```

Der Dienst läuft gehärtet: eigener System-Benutzer, `ProtectSystem=strict`,
nur `./data` beschreibbar, kein `sudo`-Zugriff, kein Zugriff auf `/home`,
Syscall-Filter und `MemoryDenyWriteExecute`. Für Diagnose nicht als `root`
starten, sondern:

```bash
sudo -u draxo journalctl -u draxo-bot -f
```

### Entfernen

```bash
sudo ./deploy/uninstall.sh
```

Sichert `.env` und Datenbank nach `/var/backups/draxo-bot` und entfernt den
Dienst. **Das Bot-Token selbst bleibt gültig** — das widerrufst du im
Developer Portal über *Reset Token*, oder du löschst die Application ganz.

---

## Aufbau

```
bot/
├── draxo_bot/
│   ├── bot.py       Einstiegspunkt, Events, Gateway-Lebenszyklus
│   ├── commands.py  Slash-Commands (6)
│   ├── service.py   Rate-Limit, Ausgabe, Logging — ohne discord.py testbar
│   ├── signing.py   Ed25519: Schlüssel laden, Grants ausstellen
│   ├── api.py       HTTP: /verify, /publickey, /health
│   ├── store.py     SQLite: HWIDs, Rate-Limit, Audit-Log
│   ├── ui.py        Embeds und Views — das gesamte sichtbare Interface
│   └── config.py    Umgebung → Config, mit Validierung
├── deploy/
│   ├── draxo-bot.service
│   ├── install.sh
│   └── uninstall.sh
├── .env.example
└── requirements.txt
```

`license_signing.py` liegt eine Ebene höher im Repo und wird bei der
Installation mit kopiert — Launcher und Server teilen sich damit eine
Definition des Formats statt zweier.

Die Tests liegen im Hauptprojekt und prüfen gegen den **echten** Launcher:

```bash
python -m unittest tests.test_grants -v
```

---

## Was noch nicht abgesichert ist

**Die DLL prüft die Signatur nicht.** [src/core/auth.cpp](../src/core/auth.cpp)
liest die Grant-Struktur (Ablauf, HWID-Bindung) und meldet sie — die
Ed25519-Verifikation fehlt dort. Der Beweis kommt vom Launcher. Das ist
zugunsten eines ungeprüften C++-Portes von Ed25519 so entschieden: rund 600
Zeilen Feldarithmetik, die nicht gegen die RFC-8032-Vektoren getestet sind,
fallen still positiv aus. Wer das schließen will, portiert `license_signing.py`
nach C++ **mit** Vektorentest — die Vektoren stehen in
`tests/test_grants.py`.

**Offline ist ein Grant nicht widerrufbar.** Signiert heißt: gültig bis zum
Ablauf, egal was der Server inzwischen sagt. Das ist die Kehrseite davon,
kein Geheimnis im Client zu haben. Mit `API_ENABLED=1` fragt der Launcher
den Bot und erfährt es sofort; ohne Netz wartet er das Ablaufdatum ab. Kurze
Laufzeiten (`KEY_HOURS`) verkürzen das Fenster.

**HWIDs liegen im Klartext** in `/opt/draxo-bot/data/draxo.sqlite3`. Das
braucht die Rate-Limit-Logik, heißt aber: wer die Datei liest, kennt alle
registrierten HWIDs. Das Audit-Log maskiert sie, die `hwids`-Tabelle nicht.

**Die Bot-Einladung fehlt.** `guilds.join` aus dem Launcher-OAuth kann nur
funktionieren, wenn die Application bereits Zugriff auf den Server hat. Der
Bot ist derzeit in keinem Server (`/users/@me/guilds` liefert `[]`), also
muss er über die Einladung aus der Anleitung hinzugefügt werden. Die Anmeldung
im Launcher funktioniert auch ohne den Auto-Beitritt.

**Alte Keys sind tot.** `DRAXO-…` wird abgelehnt, mit einem Text, der das
sagt. Wer noch welche hat, muss einen neuen Grant holen — was ohnehin
notwendig war, denn die alten waren frei erzeugbar.

---

## Updates

`./deploy/update.sh` tauscht den Code aus und lässt HWID-Datenbank,
`.env`, Revoke-Liste und Audit-Log unangetastet.

```bash
sudo ./deploy/update.sh --dry-run    # nur zeigen
sudo ./deploy/update.sh              # durchführen
```

Das Skript macht mehr als cp, und der Grund ist nicht Sorgfalt, sondern
SQLite: die Datenbank läuft im **WAL-Modus**, es gibt also drei Dateien
statt einer. Die letzten Commits stehen in `-wal` und werden erst bei
Bedarf in die Hauptdatei geschrieben. Wer nur `draxo.sqlite3` kopiert,
verliert stillschweigend alles, was in den letzten Minuten geschrieben
wurde — inklusive der HWIDs von Nutzern, die sich gerade angemeldet
haben. Das Skript macht vorher einen Checkpoint (`PRAGMA
wal_checkpoint(TRUNCATE)`), prüft die Integrität und vergleicht am Ende
die Zeilenzahlen mit dem Stand von vorher.

Zwei weitere Fallen, die es abfängt:

**Die `.env` wird nie angefasst.** Darin liegt der private
Signaturschlüssel. Ein `rsync` ohne Ausschluss oder ein `rm -rf` vor dem
Kopieren löscht ihn — danach stellt der Bot Grants aus, die kein Client
prüfen kann.

**Der Schlüssel wird verglichen.** Vor dem Start leitet das Skript den
öffentlichen Teil aus dem privaten Seed der `.env` ab und vergleicht ihn
mit `SERVER_PUBLIC_KEY_HEX` aus `license_signing.py`. Stimmen sie nicht
überein, startet der Dienst nicht. Ohne diese Prüfung läuft der Bot
fröhlich und stellt Grants aus, die der Launcher ablehnt — der Fehler
fällt erst auf, wenn sich Nutzer beschweren.

Nach jedem Lauf liegt eine Sicherung unter
`/var/backups/draxo-bot/pre-update-<Zeitstempel>/` (die `.env` und die
Datenbank). Die letzten fünf werden behalten.

---

## Interactions-Endpoint (ohne Gateway)

Slash-Commands laufen auch über eine Netlify Function, ohne dass der Bot am
Gateway sein muss. Anleitung und Hintergründe:
**[README-interactions.md](README-interactions.md)**.

Kurzfassung: `API_ENABLED=1` auf dem Server, drei Umgebungsvariablen in
Netlify, URL im Developer Portal eintragen. Die Funktion enthält
**keinen** privaten Schlüssel — sie holt sich Grants über `/api/v1/issue`.
Das ist der Grund, warum es diese Trennung überhaupt gibt: der Seed läge
sonst in einer Umgebung, die jeder mit Deploy-Rechten exportieren kann.