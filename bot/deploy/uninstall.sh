#!/usr/bin/env bash
# Draxo Bot — sauber entfernen. Datenbank und .env werden nach
# /var/backups/draxo-bot gesichert, nicht gelöscht.
set -euo pipefail

INSTALL_DIR="${INSTALL_DIR:-/opt/draxo-bot}"
SERVICE_USER="${SERVICE_USER:-draxo}"
BACKUP_DIR="/var/backups/draxo-bot"

printf '\033[1;31m▸\033[0m Stoppe Dienst …\n'
systemctl disable --now draxo-bot 2>/dev/null || true
rm -f /etc/systemd/system/draxo-bot.service
systemctl daemon-reload
rm -f /etc/systemd/system/draxo-bot.service

if [[ -d "$INSTALL_DIR" ]]; then
    install -d -m 0700 "$BACKUP_DIR"
    ts="$(date +%Y%m%d-%H%M%S)"
    [[ -f "$INSTALL_DIR/.env" ]] && cp -a "$INSTALL_DIR/.env" "$BACKUP_DIR/env-$ts"
    [[ -d "$INSTALL_DIR/data" ]] && cp -a "$INSTALL_DIR/data" "$BACKUP_DIR/data-$ts"
    printf '\033[1;32m✓\033[0m Sicherung nach %s\n' "$BACKUP_DIR"
    rm -rf "$INSTALL_DIR"
    printf '\033[1;32m✓\033[0m %s entfernt\n' "$INSTALL_DIR"
fi

if id -u "$SERVICE_USER" >/dev/null 2>&1; then
    userdel "$SERVICE_USER" && printf '\033[1;32m✓\033[0m Benutzer %s entfernt\n' "$SERVICE_USER"
fi

cat <<'NOTE'

Dienst deinstalliert. Was bleibt, ist absichtlich:
  • der Discord-Bot selbst (Token im Developer Portal widerrufen:
    Bot → Reset Token — das lässt sich nicht per systemd zurücknehmen)
  • die Backups mit Token, Datenbank und HWIDs unter /var/backups/draxo-bot

Wenn du den Bot endgültig aus dem Discord-Entwicklerportal entfernen willst:
  Application → Bot → Delete Application
NOTE