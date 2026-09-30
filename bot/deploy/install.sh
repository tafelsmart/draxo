#!/usr/bin/env bash
# ══════════════════════════════════════════════════════════════════════
#  Draxo Bot — Installation auf einem Linux-System (systemd)
#
#  Auf dem Server ausführen, nicht auf deinem PC:
#      scp -r bot/ draxo@server:/tmp/draxo-bot
#      ssh draxo@server
#      cd /tmp/draxo-bot && sudo ./deploy/install.sh
#
#  Das Skript ist idempotent: ein zweiter Lauf überschreibt Dateien,
#  beendet den laufenden Dienst und startet ihn neu — ohne Datenverlust.
# ══════════════════════════════════════════════════════════════════════
set -euo pipefail

INSTALL_DIR="${INSTALL_DIR:-/opt/draxo-bot}"
SERVICE_USER="${SERVICE_USER:-draxo}"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

info()  { printf '\033[1;35m▸\033[0m %s\n' "$*"; }
ok()    { printf '\033[1;32m✓\033[0m %s\n' "$*"; }
warn()  { printf '\033[1;33m!\033[0m %s\n' "$*"; }
die()   { printf '\033[1;31m✗\033[0m %s\n' "$*" >&2; exit 1; }

[[ $EUID -eq 0 ]] || die "Bitte mit sudo ausführen."
[[ -f "$REPO_ROOT/bot/draxo_bot/bot.py" ]] || die "Ich finde den Bot-Code nicht ($REPO_ROOT). Skript aus dem bot/-Ordner starten."

# ── 1. Python ────────────────────────────────────────────────────────
PY=""
for candidate in python3.12 python3.11 python3.10 python3; do
    if command -v "$candidate" >/dev/null 2>&1; then
        if "$candidate" -c 'import sys; sys.exit(0 if sys.version_info >= (3, 10) else 1)'; then
            PY="$candidate"; break
        fi
    fi
done
[[ -n "$PY" ]] || die "Python 3.10 oder neuer wird benötigt. Auf Mint:  sudo apt install python3.12 python3.12-venv"
ok "Python: $($PY --version)"

if ! $PY -m venv --help >/dev/null 2>&1; then
    warn "venv fehlt — installiere es gleich mit."
    apt-get update -qq && apt-get install -y python3-venv
fi

# ── 2. Systemuser ────────────────────────────────────────────────────
if ! id -u "$SERVICE_USER" >/dev/null 2>&1; then
    info "Lege System-Benutzer '$SERVICE_USER' an …"
    useradd --system --no-create-home --shell /usr/sbin/nologin "$SERVICE_USER"
    ok "Benutzer '$SERVICE_USER' angelegt"
else
    ok "Benutzer '$SERVICE_USER' existiert bereits"
fi

# ── 3. Dateien kopieren ──────────────────────────────────────────────
info "Installiere nach $INSTALL_DIR …"
install -d -m 0755 -o "$SERVICE_USER" -g "$SERVICE_USER" "$INSTALL_DIR"
install -d -m 0750 -o "$SERVICE_USER" -g "$SERVICE_USER" "$INSTALL_DIR/data"

for item in draxo_bot requirements.txt .env.example; do
    [[ -e "$REPO_ROOT/bot/$item" ]] || die "Quelldatei fehlt: bot/$item"
    cp -r "$REPO_ROOT/bot/$item" "$INSTALL_DIR/"
done

# license_signing.py ist die gemeinsame Definition des Grant-Formats und
# liegt im Repo eine Ebene hoeher als der Bot. Ohne sie kann der Bot keine
# Grants ausstellen — der Pfad-Fallback in draxo_bot/signing.py findet sie
# auf dem Server sonst nicht.
[[ -f "$REPO_ROOT/license_signing.py" ]] || die "license_signing.py fehlt im Repo."
cp "$REPO_ROOT/license_signing.py" "$INSTALL_DIR/"
ok "license_signing.py installiert"
cp "$REPO_ROOT/bot/deploy/draxo-bot.service" /etc/systemd/system/draxo-bot.service
chown -R "$SERVICE_USER:$SERVICE_USER" "$INSTALL_DIR"
find "$INSTALL_DIR" -type d -exec chmod 0750 {} +
chmod 0700 "$INSTALL_DIR/data"

# ── 4. Konfiguration ─────────────────────────────────────────────────
if [[ ! -f "$INSTALL_DIR/.env" ]]; then
    cp "$INSTALL_DIR/.env.example" "$INSTALL_DIR/.env"
    chown "$SERVICE_USER:$SERVICE_USER" "$INSTALL_DIR/.env"
    chmod 0600 "$INSTALL_DIR/.env"
    warn "Jetzt $INSTALL_DIR/.env ausfüllen (siehe Zusammenfassung weiter unten) und neu starten:"
    warn "    nano $INSTALL_DIR/.env"
    warn "    systemctl restart draxo-bot"
else
    ok "Bestehende .env bleibt unverändert"
fi

# ── 5. Virtualenv ────────────────────────────────────────────────────
info "Richte Virtualenv ein …"
sudo -u "$SERVICE_USER" $PY -m venv "$INSTALL_DIR/.venv"
sudo -u "$SERVICE_USER" "$INSTALL_DIR/.venv/bin/pip" install --quiet --upgrade pip
sudo -u "$SERVICE_USER" "$INSTALL_DIR/.venv/bin/pip" install --quiet -r "$INSTALL_DIR/requirements.txt"
ok "Abhängigkeiten installiert"

# ── 6. systemd ───────────────────────────────────────────────────────
# systemd braucht den Pfad zur .env ohne ${...}-Syntax — dort wird jede
# Zeile als exakte Zuweisung gelesen, Expansion findet nicht statt.
grep -qE '^\s*EnvironmentFile' /etc/systemd/system/draxo-bot.service \
    && ok "EnvironmentFile ist gesetzt" \
    || warn "EnvironmentFile fehlt in der Unit — Token wird nicht geladen"

systemctl daemon-reload
systemctl enable draxo-bot >/dev/null 2>&1

if grep -qE '^DISCORD_TOKEN=.+' "$INSTALL_DIR/.env" 2>/dev/null && \
   grep -qE '^DRAXO_SIGNING_KEY=.+' "$INSTALL_DIR/.env" 2>/dev/null; then
    info "Token und Signing-Key gesetzt — Dienst wird gestartet."
    systemctl restart draxo-bot
    sleep 3
    if systemctl is-active --quiet draxo-bot; then
        ok "Draxo Bot läuft (PID $(systemctl show -p MainPID --value draxo-bot))"
    else
        warn "Dienst läuft nicht. Log:"
        systemctl --no-pager --lines=25 status draxo-bot || true
        exit 1
    fi
else
    warn "DISCORD_TOKEN oder DRAXO_SIGNING_KEY fehlt noch — Dienst wurde nicht gestartet."
fi

cat <<'SUMMARY'

─────────────────────────────────────────────────────────────────
 Nächste Schritte
─────────────────────────────────────────────────────────────────
 1. Signing-Key und Token eintragen:
        sudo nano /opt/draxo-bot/.env
    Noetig sind mindestens DISCORD_TOKEN, DISCORD_GUILD_ID und
    DRAXO_SIGNING_KEY.

    Den Signing-Key einmalig erzeugen:
        cd /opt/draxo-bot
        sudo -u draxo .venv/bin/python -m draxo_bot.signing keygen

    Der private Wert kommt in die .env. Der oeffentliche muss in
    license_signing.py (SERVER_PUBLIC_KEY_HEX) im Repo stehen und in einen
    neuen Launcher-Build. Beide muessen uebereinstimmen — sonst stellt der
    Bot Grants aus, die der Client zurueckweist.

 2. Privilegierten Intent aktivieren (nur für Begrüßung nötig):
    Developer Portal → deine Application → Bot → Privileged Intents
    → „Server Members Intent“ AN → 2× Speichern.
    Ohne diesen Intent läuft der Bot normal, begrüßt aber niemanden.
    Steht WELCOME_ON_JOIN=0, wird er nicht gebraucht.

 3. Channel-IDs eintragen (Rechtsklick auf den Channel → ID kopieren):
    WELCOME_CHANNEL_ID=  LOG_CHANNEL_ID=  MEMBER_ROLE_ID=

 4. Neustart und Kontrolle:
        sudo systemctl restart draxo-bot
        sudo journalctl -u draxo-bot -f

 5. Slash-Commands erscheinen evtl. erst nach ein paar Sekunden.
    Falls nicht:
        sudo systemctl restart draxo-bot

 6. Auto-Rolle: Die Rolle muss ÜBER der Bot-Rolle stehen,
    sonst darf der Bot sie nicht vergeben.

─────────────────────────────────────────────────────────────────
SUMMARY