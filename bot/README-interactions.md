# Interactions-Endpoint (Netlify Function)

Slash-Commands über HTTP statt über das Gateway. Discord schickt jede
Interaktion an eine URL, die du im Developer Portal einträgst — kein
Gateway, kein discord.py, kein Bot-Login nötig.

## Warum überhaupt

Zwei Gründe, die beide praktisch sind:

**Der Bot läuft auch, wenn Discord das Gateway nicht erreichbar hat.**
Server-Ausfälle, Wartungsfenster, IP-Sperren. Ein HTTP-Endpunkt auf Netlify
ist eine unabhängige Strecke.

**Der private Schlüssel bleibt, wo er hingehört.** Das ist der eigentliche
Grund, und er ist nicht verhandelbar: die Funktion liegt in einem Netlify-
Projekt, in das jedes Teammitglied mit Deploy-Rechten schreiben kann und
das jede Netlify-Umgebungsvariable exportieren kann. Läge dort der private
Ed25519-Seed, wäre genau das Leck wieder da, das die Umstellung auf
asymmetrische Signaturen beseitigt hat — nur an einer Stelle, die man
leichter übersieht.

```
Discord ──POST──▶  Netlify Function  ──Bearer──▶  Linux-Server
                    prüft Signatur                     hält den Seed
                    kennt kein Geheimnis               signiert
                    zeigt Embeds                       speichert
```

## Einrichtung in drei Schritten

### 1. Server: API aktivieren

In `/opt/draxo-bot/.env`:

```ini
API_ENABLED=1
API_BIND=127.0.0.1
API_PORT=8787
API_TOKEN=<langer Zufallswert>
```

Dann `sudo systemctl restart draxo-bot`.

Prüfen, ob der Server mintet (ohne Discord):

```bash
curl -s -X POST http://127.0.0.1:8787/api/v1/issue \
  -H "Authorization: Bearer $API_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"discord_id":"1534959905104986314","hwid":"3F2A9C4D8B1E7A05C6D9F2B3A4C5D6E7"}'
```

Antwortet mit `{"ok":true,"grant":"DRAXO3-…"}`, funktioniert der Server.

Der Token gehört **nicht** ins offene Netz. `API_BIND=127.0.0.1` und ein
Reverse-Proxy mit TLS davor:

```caddy
bot.example.com {
    reverse_proxy 127.0.0.1:8787
}
```

### 2. Netlify: drei Umgebungsvariablen

Site settings → Environment variables:

| Variable | Wert |
|---|---|
| `DISCORD_PUBLIC_KEY` | aus General Information der Application |
| `DRAXO_MINT_URL` | `https://bot.example.com` (ohne `/api/v1`) |
| `DRAXO_MINT_TOKEN` | derselbe Wert wie `API_TOKEN` in der `.env` |

`DRAXO_SIGNING_KEY` gehört **nicht** hierhin. Ein Test
(`tests/test_interactions.py`) schlägt fehl, falls doch.

### 3. Developer Portal: URL eintragen

General Information → **Interactions Endpoint URL**:

```
https://draxo.netlify.app/.netlify/functions/interactions
```

Speichern. Discord schickt sofort einen PING. Bleibt die Antwort aus, meldet
Discord *„could not be verified"* — das sieht nach einem Bot-Problem aus,
liegt aber fast immer am Endpoint.

Lokal prüfen, bevor du dich aufs Portal verlässt:

```bash
curl -i -X POST http://localhost:8888/.netlify/functions/interactions \
  -H "Content-Type: application/json" \
  -d '{"id":"1","application_id":"1","type":1,"token":"x","version":1}'
```

Antwortet mit **401**, ist alles richtig: die Signatur fehlt, also wird
abgelehnt. Antwortet mit 200 ohne Signaturprüfung, ist die Prüfung kaputt.

## Betriebsmodi

Der Bot kann beides:

| Modus | `API_ENABLED` | Verwendet |
|---|---|---|
| Gateway | 0 | nur der Bot-Prozess |
| HTTP | 1 | Netlify-Funktion → `/api/v1/issue` |
| Beides | 1 | je nach Aufruf |

Im Gateway-Modus antwortet der Bot selbst; im HTTP-Modus macht das die
Funktion. Beides parallel ist möglich und erzeugt keine Doppel-Ausgabe —
der Aufrufer entscheidet, welcher Weg benutzt wird.

Sobald eine Interactions-URL im Portal steht, liefert Discord Interaktionen
**nicht mehr** über das Gateway. Der Bot muss dann nicht mehr am Gateway
sein, um `/key` zu beantworten — nur noch für Welcome, Rollen und Logs.

## Befehle

| Befehl | Funktion | Gateway-Bot |
|---|---|---|
| `/key` | Grant für die hinterlegte HWID | ja |
| `/hwid` | HWID hinterlegen, anzeigen, löschen | ja |
| `/status` | HWID und letzter Grant | ja |
| `/hilfe` | Übersicht | ja |
| Button „Key holen" | Modal → Grant | ja |
| `/serverinfo` | Mitglieder, Ping, Statistik | ja |
| `/ping` | HTTP-Modus: Latenz zum Key-Server | nur Gateway |

`/serverinfo` fehlt in der Funktion, weil es Live-Zahlen aus dem Gateway
braucht. Die Zahl wäre sonst geraten.

## Was der Endpunkt prüft

Vor allem anderen die Ed25519-Signatur von Discord. Ohne diese Prüfung
kann jeder im Internet `/key` aufrufen und sich Grants für beliebige
Discord-IDs ausstellen lassen. Geprüft wird:

- **Signatur** — `X-Signature-Ed25519` gegen den Public Key der Application
- **Zeitstempel** — nicht älter als 5 Minuten, sonst ist die Signatur
  wiederverwendbar
- **Roh-Body** — der Body wird nie neu serialisiert; das würde die Bytes
  ändern und jede Prüfung unmöglich machen

Nicht geprüft wird, ob der Aufrufer ein Bot-Mitglied ist. Das ist Absicht:
eine Key-Ausgabe soll auch für jemanden funktionieren, der dem Server noch
nicht beigetreten ist. Das Rate-Limit (`KEY_MAX_PER_DAY`) ist die Schranke.

## Fehlersuche

**Discord speichert die URL nicht.** Im Netlify-Log nachsehen. Häufigste
Ursache: `DISCORD_PUBLIC_KEY` fehlt. Der Endpunkt antwortet dann mit 500
und einem Grund, statt zu raten.

**`/key` meldet „Der Key-Server antwortet gerade nicht".** Auf dem Server
prüfen: läuft `draxo-bot`? Ist `API_ENABLED=1`? Funktioniert der
Reverse-Proxy? Der Fehlercode steht im Klammertext der Meldung
(`timeout`, `no_token`, `no_url`, `http`).

**`401` bei jedem Aufruf.** `DISCORD_PUBLIC_KEY` stimmt nicht, oder Netlify
cacht die Antwort. Beide Fälle sind abgedeckt — `Cache-Control: no-store`
steht in `netlify.toml`.

**Alte Keys werden abgelehnt.** Erwartet: die Umstellung auf `DRAXO3-` war
ein Bruch. Für Details siehe „Was noch nicht abgesichert ist" in
[bot/README.md](README.md).

## Tests

```bash
python -m unittest tests.test_interactions      # 29 Endpunkt-Tests
python -m unittest tests.test_interactions_e2e  # Discord → Server → Launcher
python -m unittest tests.test_mint_server       # Server ohne Gateway
python -m unittest tests.test_ui_parity         # Python- und JS-Embeds gleich
```

Der E2E-Test startet den echten aiohttp-Server, lässt die echte
Node-Funktion darüber HTTP laufen und prüft den Grant mit
`license_signing.verify_grant` — dem Code, der auch in der EXE steckt.
