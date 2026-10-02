# Domain einrichten (Spaceship → Cloudflare → Netlify + Tunnel)

Stand: 02.10.2026. Ziel ist, `draxo.netlify.app` und den Mint-Server
unter einer eigenen Domain zu betreiben — und die flüchtige
`trycloudflare.com`-Adresse abzulösen.

```
draxo.deinedomain.tld      →  CNAME  →  draxo.netlify.app     (Website)
www.draxo.deinedomain.tld  →  CNAME  →  draxo.netlify.app     (Website)
api.draxo.deinedomain.tld  →  CNAME  →  < Tunnel-ID>.cfargotunnel.com   (Bot)
```

Die Domainnamen unten sind Platzhalter. Ersetze `draxo.deinedomain.tld`
überall durch deine echte Domain.

---

## Warum Cloudflare und nicht Netlify als Nameserver

Netlify und Cloudflare wollen beide *authoritative* Nameserver sein. Das
geht für beide nicht. Wir geben die Nameserver an Cloudflare, weil dort
beides möglich ist:

* Die Website braucht von Netlify nur einen **CNAME**. Netlify
  akzeptiert externe DNS — es verlangt zusätzlich einen
  TXT-Nachweis, aber keine Nameserver-Umstellung.
* Der Bot braucht zwingend einen **Named Tunnel**. Der ist nur mit
  eigenem DNS stabil; ein Quick Tunnel bekommt bei jedem Neustart eine
  neue Adresse.

Wäre umgekehrt (Nameserver zu Netlify), hinge der Bot dauerhaft an
einer URL, die sich ändert, sobald der Tunnel neu startet.

---

## Schritt 1 — Zone bei Cloudflare anlegen

1. <https://dash.cloudflare.com> → **Add a site**
2. `draxo.deinedomain.tld` eingeben
3. Plan **Free** bestätigen
4. Cloudflare zeigt dir zwei Nameserver an, z. B.
   `anna.ns.cloudflare.com` und `bob.ns.cloudflare.com`
5. **Nichts weiter klicken** — du kommst erst weiter, wenn die
   Nameserver bei Spaceship gesetzt sind (Schritt 2)

## Schritt 2 — Nameserver bei Spaceship umstellen

1. <https://www.spaceship.com> → **Domain Portfolio** → deine Domain
2. **Nameservers** → **Custom nameservers**
3. Zwei Felder mit den Werten aus Schritt 1 füllen
4. **Speichern**

> Spaceship setzt diese Umstellung meist binnen weniger Stunden um,
> gelegentlich bis zu 48 Stunden. Nach dem Wechsel verschwinden die
> Nameserver-Einträge bei Cloudflare und die Zone wird aktiv.

**Zwischendurch nicht weiterarbeiten.** Die DNS-Einträge aus Schritt 3
lassen sich zwar anlegen, werden aber erst nach dem Nameserver-Wechsel
beachtet.

## Schritt 3 — DNS-Einträge bei Cloudflare

Unter **DNS → Records**:

| Type | Name | Ziel | Proxy | TTL |
|---|---|---|---|---|
| CNAME | `@` | `draxo.netlify.app` | **DNS only** (graue Wolke) | Auto |
| CNAME | `www` | `draxo.netlify.app` | **DNS only** | Auto |
| CNAME | `api` | `<TUNNEL-ID>.cfargotunnel.com` | **DNS only** | Auto |

Zwei Punkte, die gern schiefgehen:

* **Proxy aus.** Bei CNAME-Zielen von Netlify und Cloudflare-Tunneln
  muss die orange Wolfe auf *DNS only* stehen. Im Proxy-Modus
  verändert Cloudflare die Zertifikate und die Weiterleitungen von
  Netlify, und `/.netlify/functions/*` antwortet dann nicht mehr.
* **Tunnel-ID Platzhalter.** Den echten Wert bekommst du in Schritt 5.

## Schritt 4 — Domain bei Netlify anmelden

1. <https://app.netlify.com> → das Projekt **draxo**
2. **Domain management** → **Add a domain**
3. `draxo.deinedomain.tld` eintragen
4. Netlify zeigt dir zwei Werte:
   * einen **CNAME-Zielwert** (bei fremdem DNS meist `xyz.netlify.app`)
   * einen **TXT-Verifikationswert** für `_netlify`
5. Bei Cloudflare anlegen:
   * **CNAME** `_netlify` → der TXT-Wert wird als **TXT** angelegt, nicht CNAME
6. In Netlify auf **Verify DNS configuration** klicken
7. Danach `www` auf dieselbe Domain umleiten lassen
   (**Domain management → www → Redirect to apex** bzw. umgekehrt —
   hier ist die apex-Domain das Ziel)

Erst nach erfolgreicher Verifikation stellt Netlify automatisch ein
Zertifikat aus. Das kann 5–30 Minuten dauern.

## Schritt 5 — Named Tunnel für den Bot

Auf dem Linux-Rechner, auf dem der Bot läuft:

```bash
# 1) Einmalig: Login mit der Domain
cloudflared tunnel login

# 2) Tunnel anlegen
cloudflared tunnel create draxo-api

# 3) Tunnel-ID herausfinden und in die YAML eintragen
cloudflared tunnel list
```

`~/.cloudflared/config.yml` anlegen:

```yaml
tunnel: <TUNNEL-ID>
credentials-file: /home/clemens/.cloudflared/<TUNNEL-ID>.json

ingress:
  - hostname: api.draxo.deinedomain.tld
    service: http://127.0.0.1:8787
  - service: http_status:404
```

Der `service` passt zur `.env` des Bots: `API_BIND=127.0.0.1`,
`API_PORT=8787`. Der Tunnel verbindet sich von außen nach innen,
deshalb bleibt die Bindung auf dem Loopback — **nicht** auf `0.0.0.0`
stellen, solange der Port nicht zusätzlich durch eine Firewall
begrenzt ist.

Dauerhaft als systemd-Dienst:

```bash
sudo cloudflared service install
sudo systemctl enable --now cloudflared
```

Den alten Quick Tunnel damit beenden — er lief nur manuell und ist nach
dieser Umstellung überflüssig.

## Deployen

Die Website liegt in `website/` im Projekt-Root. Der Ordner hieß früher
`draxo-website-netlify/` — im Netlify-Dashboard ist es trotzdem dieselbe
Site, erkennbar an der ID in `website/.netlify/state.json`
(`674509a4-4391-4182-a952-8e5e8cdbf8e4`). Ein `link` ist also nicht
nötig; ohne die Datei würde Netlify ein neues Projekt anlegen.

```cmd
cd /d "C:\Projekte\Minecraft-java\Draxo Client\website"
npx --yes netlify-cli@27.10.2 deploy --prod
```

Nach jedem Deploy prüfen, ob die Function noch antwortet — 401 ohne
Signatur ist das richtige Ergebnis:

```cmd
curl -s -o nul -w "%{http_code}\n" -X POST ^
  https://draxo.netlify.app/.netlify/functions/interactions
```

Deploys funktionieren nur über die CLI. Drag-und-drop im Dashboard
ignoriert den `netlify/`-Ordner und stellt eine Seite ohne Functions
bereit — die Interaktionen fallen dann still aus.

Die Website bleibt absichtlich flach (`index.html` im Wurzelverzeichnis,
Functions unter `netlify/functions/`). Netlify erwartet `publish = "."`;
ein Unterordnen würde den Deploy brechen.

## Schritt 6 — Umgebungsvariablen

**Netlify** (Site settings → Environment variables → dann neu deployen):

| Variable | Wert |
|---|---|
| `DRAXO_MINT_URL` | `https://api.draxo.deinedomain.tld` |
| `DRAXO_MINT_TOKEN` | der `API_TOKEN` aus `/opt/draxo-bot/.env` |

`DISCORD_PUBLIC_KEY` ist bereits gesetzt und bleibt unverändert.

**Nicht** vergessen: die Interaktions-URL im Discord-Developer-Portal
von `https://draxo.netlify.app/.netlify/functions/interactions` auf
`https://draxo.deinedomain.tld/.netlify/functions/interactions`
umstellen. Discord prüft die Erreichbarkeit beim Speichern und we'dert
den Dialog ab, wenn es nicht antwortet.

## Schritt 7 — Absichern

Die Tunnel-Adresse ist jetzt stabil und damit dauerhaft im Internet.
Der Mint-Endpunkt prüft einen `Authorization: Bearer`-Header, aber:

* `API_TOKEN` ist ein einzelnes Geheimnis ohne Ablauf. Ein Leck erlaubt
  das Ausstellen beliebiger Grants.
* **Niemals** `API_TOKEN` in den Chat, ins Repo oder ins Netlify-UI als
  Klartext in Logs.
* Für den Produktivbetrieb erwägenswert: eine zweite Schranke vor
  `POST /api/v1/verify`/`mint`, etwa eine IP-Allowlist, weil der Bot auf
  dem Laptop des Entwicklers läuft und nicht auf einem Server mit
  eigener Firewall.

## Prüfen

```bash
# Website
curl -sI https://draxo.deinedomain.tld | head -1
curl -sI https://draxo.deinedomain.tld/version.json | head -1

# Interaktions-Endpunkt: 401 ohne Signatur ist RICHTIG
curl -s -o /dev/null -w '%{http_code}\n' \
  -X POST https://draxo.deinedomain.tld/.netlify/functions/interactions

# Tunnel
curl -sI https://api.draxo.deinedomain.tld/ | head -1
```

Erwartet: `200` für die Website, `401` mit `{"error":"Ungültige Signatur."}`
für die Function — das bestätigt, dass `DISCORD_PUBLIC_KEY` angekommen ist
und die Ed25519-Prüfung läuft.

## Häufige Fehler

| Symptom | Ursache |
|---|---|
| Netlify zeigt "Domain nicht verifiziert" | TXT `_netlify` fehlt oder steht als CNAME |
| Website lädt, `/.netlify/functions/*` liefert 404 | Cloudflare-Proxy ist orange; auf *DNS only* stellen |
| Zertifikat-Fehler im Browser | Nameserver-Umstellung noch nicht durch; 24 h warten |
| Tunnel antwortet 502 | `API_BIND` steht auf `127.0.0.1`, aber `cloudflared` läuft in einem anderen Netzwerk-Container — bzw. umgekehrt |
| `curl` auf `api.` bekommt connection refused | Tunnel-Dienst läuft nicht: `systemctl status cloudflared` |
| Website läuft, Bot mintet weiter | `DRAXO_MINT_URL` in Netlify nicht neu deployed |
| Deploy meldet "Site not found" | `website/.netlify/state.json` fehlt — `link` im Ordner `website/` wiederholen |
| Deploy läuft, aber die Site ist eine neue | `link` wurde im falschen Ordner ausgeführt; dadurch ohne Site-ID deployt |
| Functions fehlen nach dem Deploy | Drag-and-drop benutzt; die Function liegt in `website/netlify/functions/` |
| `cd /d ...draxo-website-netlify` schlägt fehl | Der Ordner heißt jetzt `website/` |