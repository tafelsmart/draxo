# Einrichtung auf dem Linux-Server

Stand: geprüft am 30.09.2026. Dieser Text sagt, was **tatsächlich** läuft —
nicht, was laufen sollte.

---

## Kurzfassung: läuft es perfekt?

**Nein.** Drei von fünf Dingen sind fertig und geprüft, zwei fehlen
noch und liegen bei dir:

| Teil | Zustand |
|---|---|
| Bot-Code, Grant-Ausstellung, Signatur | ✅ fertig, 70/70 Tests grün |
| Netlify-Funktion | ⚠️ geschrieben und getestet, **nie deployed** |
| Website | ⚠️ online ist noch die alte Version 1.1.0 |
| Bot in Discord | ❌ **in keinem Server** (`guilds` = 0) |
| Server-Installation | ❌ nie durchgeführt |

Der Code ist fertig. Was fehlt, ist Deployment — und das kann ich nicht
für dich erledigen, weil es deine Zugangsdaten braucht.

Zwei Dinge, die ich **nicht** behaupten werde: dass der Bot auf deinem
Server läuft (nie getestet), und dass die Function online ist (404).

---

## Was ich von dir brauche

| # | Was | Woher | Warum |
|---|---|---|---|
| 1 | **SSH-Zugang** zum Laptop | Benutzer, IP oder Hostname | Installation |
| 2 | **sudo** dort | — | systemd, Systemuser |
| 3 | **Discord-Bot-Einladung** anklicken | Link unten | Bot ist in 0 Servern |
| 4 | **Netlify-Deploy** | Netlify-UI | Function ist online 404 |
| 5 | **API_TOKEN** | von mir generiert | Bot ↔ Function |

Den Token **hast du schon** — er ist gültig. Ich habe es geprüft:

```
Gateway:        erreichbar (1 Shard, 1000 Sitzungen)
Bot:            Draxo, ID 1554874371573813389
Server:         0        ← hier ist das Problem
```

### Schritt 3: Bot in den Server einladen

Ohne das funktioniert **nichts** — weder Slash-Commands noch
`guilds.join` aus dem Launcher:

```
https://discord.com/oauth2/authorize?client_id=1554874371573813389&permissions=403033088&scope=bot+applications.commands&guild_id=1534959905104986314&disable_guild_select=true
```

Antwortet der Bot auf die Einladung mit einem Fehler, ist er schon
drin — dann ist der Schritt erledigt.

---

## Der Server

Ersetze `draxo@laptop` durch deinen echten Benutzer.

### 1. Code übertragen

Auf **deinem** PC (nicht dem Server):

```bash
cd "Pfad/zum/Draxo Client"
scp -r bot/ license_signing.py draxo@laptop:/tmp/draxo-setup
```

Falls `scp` den Ordner anders anlegt, prüf mit `ls /tmp/draxo-setup`.

### 2. Installieren

Auf dem **Server**:

```bash
ssh draxo@laptop
cd /tmp/draxo-setup
sudo ./bot/deploy/install.sh
```

Das legt an: Systemuser `draxo`, Verzeichnis `/opt/draxo-bot`,
Virtualenv, systemd-Unit. **Die HWID-Datenbank wird dabei nicht
angelegt** — die entsteht beim ersten Start.

### 3. Konfiguration

```bash
sudo nano /opt/draxo-bot/.env
```

Diese fünf Zeilen müssen gefüllt sein:

```ini
DISCORD_TOKEN=<dein Bot-Token aus alles_bot.txt>

# Privater Schlüssel. Erzeugen mit:
#   cd /opt/draxo-bot && sudo -u draxo .venv/bin/python -m draxo_bot.signing keygen
DRAXO_SIGNING_KEY=<64 Hex-Zeichen>

# Für die Netlify-Funktion zwingend nötig
API_ENABLED=1
API_TOKEN=<langes Zufallswort, z. B. 48 Zeichen>
```

Optional, wenn du Discord-Server willst:

```ini
DISCORD_GUILD_ID=1534959905104986314
WELCOME_CHANNEL_ID=<Rechtsklick auf Channel → ID kopieren>
LOG_CHANNEL_ID=
MEMBER_ROLE_ID=
```

> **Wichtig zum Schlüssel:** Der private Teil muss in die `.env`, der
> öffentliche in `license_signing.py` im Repo — dort ist er bereits
> eingebaut. Beide müssen zusammenpassen, sonst stellt der Bot Grants
> aus, die der Launcher ablehnt. Das Update-Skript prüft das und startet
> den Dienst bei Abweichung **nicht**.

### 4. Starten

```bash
sudo systemctl restart draxo-bot
sudo journalctl -u draxo-bot -f
```

Läuft er, siehst du:

```
Angemeldet als Draxo#2549
  Server: <dein Server> (…, … Mitglieder)
Signaturschlüssel stimmt überein
```

### 5. Prüfen, ohne Discord

Das ist der schnellste Test, ob der Server wirklich mintet:

```bash
curl -s -X POST http://127.0.0.1:8787/api/v1/issue \
  -H "Authorization: Bearer <API_TOKEN>" \
  -H "Content-Type: application/json" \
  -d '{"discord_id":"1534959905104986314","hwid":"3F2A9C4D8B1E7A05C6D9F2B3A4C5D6E7"}'
```

Antwortet das mit `"ok":true` und einem `DRAXO3-…`, funktioniert der
Bot vollständig — ohne Discord, ohne Gateway.

---

## Netlify

1. `website/` als Site deployen
2. Unter **Environment variables** setzen:

| Variable | Wert |
|---|---|
| `DISCORD_PUBLIC_KEY` | `8ca7…` (aus `alles_bot.txt`) |
| `DRAXO_MINT_URL` | öffentliche Adresse des Servers |
| `DRAXO_MINT_TOKEN` | derselbe Wert wie `API_TOKEN` |

**`DRAXO_SIGNING_KEY` gehört dort nicht hin.** Die Function hält
absichtlich keinen privaten Schlüssel — sie holt sich Grants über
`/api/v1/issue`. Läge der Seed in Netlify, wäre genau das Leck wieder
da, das die Umstellung auf asymmetrische Signaturen beseitigt hat.

3. Im Developer Portal: **Interactions Endpoint URL**

```
https://draxo.netlify.app/.netlify/functions/interactions
```

Discord schickt beim Speichern einen PING. Bleibt die Antwort aus, meldet
Discord *„could not be verified"* — das sieht nach einem Bot-Problem
aus, liegt aber am Endpoint.

### Ohne Domain: SSH-Tunnel

Hat der Laptop keine öffentliche Adresse, ist `DRAXO_MINT_URL` nicht
erreichbar. Dann auf dem Laptop:

```bash
ssh -R 80:localhost:8787 localhost
```

…und in Netlify `DRAXO_MINT_URL` auf die zugewiesene subdomain setzen.
Funktioniert, ist aber nur für Tests — sobald der Laptop schläft, ist
der Bot offline.

---

## Spätere Updates

```bash
sudo ./bot/deploy/update.sh --dry-run    # erst ansehen
sudo ./bot/deploy/update.sh
```

Das Skript tauscht nur den Code aus. HWID-Datenbank, `.env` und
Audit-Log bleiben, wo sie sind — es macht vorher einen SQLite-Checkpoint
und vergleicht am Ende die Zeilenzahlen. Details in
[bot/README.md](README.md#updates).

---

## Wenn etwas nicht geht

**Dienst startet nicht**
```bash
sudo journalctl -u draxo-bot -n 40
```

**„Signaturschlüssel passt nicht zusammen"**
Privater Teil in der `.env` und öffentlicher in `license_signing.py`
gehören nicht zusammen. Neuen Seed erzeugen, öffentlichen Teil ins Repo,
neuen Launcher bauen.

**Bot antwortet nicht auf /key**
Prüfe in dieser Reihenfolge: (1) Ist der Bot im Server? (2) Steht die
Interactions-URL im Portal? (3) Antwortet `curl` auf Port 8787?
(4) Stimmt `DRAXO_MINT_URL` mit der echten Adresse überein?

**Netlify-Log**
Site → Functions → `interactions` → Logs. Bei `401` stimmt
`DISCORD_PUBLIC_KEY` nicht; bei `500` fehlt sie.

---

## Was nicht getestet ist

Ich sage es lieber jetzt als später:

- **Der Dienst auf Mint** — der Code ist getestet, die Installation nicht
- **Die Function im echten Netlify** — lokal mit echten Discord-Payloads
  geprüft, nie online
- **Ein End-to-End-Lauf über echtes Discord** — lokal läuft die Kette
  von Function → Bot-Server → signierter Grant, aber gegen die echte
  Discord-API ging noch nichts durch
- **Die Website** — online ist 1.1.0, im Repo ist 1.2.0
