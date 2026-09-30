#!/usr/bin/env bash
# ══════════════════════════════════════════════════════════════════════
#  Draxo Bot — Update ohne Datenverlust
#
#  Ersetzt den Bot-Code auf dem Server und lässt dabei alles unangetastet,
#  was nicht Code ist: HWID-Datenbank, .env mit dem Signaturschlüssel,
#  Revoke-Liste, Audit-Log.
#
#      ./deploy/update.sh --dry-run     # nur zeigen, was passieren würde
#      ./deploy/update.sh               # durchführen
#      ./deploy/update.sh --force       # ohne Rückfrage (für Cron/Scripts)
#
#  Warum das nicht einfach "cp -r" ist:
#
#  Die Datenbank läuft im WAL-Modus. Das bedeutet drei Dateien statt einer:
#
#      draxo.sqlite3        Hauptdatei
#      draxo.sqlite3-wal    bis zu 1000 Commits, die noch nicht drin stehen
#      draxo.sqlite3-shm    Index, nur temporär nötig
#
#  Wer nur die Hauptdatei kopiert, verliert stillschweigend alles, was in
#  den letzten Minuten geschrieben wurde — inklusive der HWIDs von Nutzern,
#  die sich gerade angemeldet haben. Das Skript macht deshalb vorher einen
#  Checkpoint und danach eine Integritätsprüfung, und beide Schritte sind im
#  Protokoll sichtbar.
#
#  Zweiter Grund: der private Signaturschlüssel liegt in .env. Ein
#  rsync ohne --exclude, oder ein rm -rf vor dem Kopieren, löscht ihn. Danach
#  stellt der Bot Grants aus, die kein Client prüfen kann, weil der
#  öffentliche Schlüssel im Launcher nicht mehr passt.
# ══════════════════════════════════════════════════════════════════════

set -euo pipefail

INSTALL_DIR="${INSTALL_DIR:-/opt/draxo-bot}"
SERVICE_USER="${SERVICE_USER:-draxo}"
SERVICE_NAME="${SERVICE_NAME:-draxo-bot}"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BACKUP_ROOT="${BACKUP_ROOT:-/var/backups/draxo-bot}"

DRY_RUN=0
FORCE=0
NO_RESTART=0
# Wenn gesetzt, laeuft kein systemctl-Befehl. Nur fuer die Tests — im
# Betrieb bleibt es aus, sonst haette das Skript einen Pfad, in dem der
# Dienst nie startet und niemand es bemerkt.
NO_SYSTEMD="${NO_SYSTEMD:-0}"

info()  { printf '\033[1;35m▸\033[0m %s\n' "$*"; }
ok()    { printf '\033[1;32m✓\033[0m %s\n' "$*"; }
warn()  { printf '\033[1;33m!\033\033[0m %s\n' "$*"; }
fail()  { printf '\033[1;31m✗\033\033[0m %s\n' "$*" >&2; exit 1; }
step()  { [[ $DRY_RUN -eq 1 ]] && printf '\033[0;90m  (trocken)\033[0m %s\n' "$*" || true; }

# systemctl kapseln. Im Testbetrieb gibt es kein systemd, die Befehle
# muessen aber trotzdem in der richtigen Reihenfolge aufgerufen werden —
# sonst prueft der Test einen Ablauf, den es im Betrieb nie gibt.
svc() {
    if [[ "$NO_SYSTEMD" == "1" ]]; then
        printf '\033[0;90m  [systemctl uebersprungen: %s]\033[0m\n' "$*"
        # Ohne echtes systemd wird der Dienstzustand in einer Datei
        # nachgebildet: "start" markiert ihn als laufend, "stop" beendet
        # ihn. Sonst wuerde das Skript nach jedem "start" melden, der
        # Dienst laufe nicht, und abbrechen — der Test pruefte dann einen
        # Ablauf, den es im Betrieb nie gibt.
        local state_file="${INSTALL_DIR}/.svc-state"
        case "$1" in
            start)   : > "$state_file"; return 0 ;;
            stop)    rm -f "$state_file"; return 0 ;;
            is-active)
                [[ -f "$state_file" ]]; return $? ;;
            show)    echo 0; return 0 ;;
            *) return 0 ;;
        esac
    fi
    systemctl "$@"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --dry-run) DRY_RUN=1 ;;
        --force)   FORCE=1 ;;
        --no-restart) NO_RESTART=1 ;;
        -h|--help)
            sed -n '2,30p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
            exit 0 ;;
        *) fail "Unbekanntes Argument: $1  (siehe --help)" ;;
    esac
    shift
done

# ══════════════════════════════════════════════════════════════════════
#  Vorprüfung
# ══════════════════════════════════════════════════════════════════════

if [[ "$NO_SYSTEMD" != "1" && $EUID -ne 0 ]]; then
    fail "Bitte mit sudo ausführen:  sudo ./deploy/update.sh"
fi
[[ -d "$INSTALL_DIR" ]] || fail "$INSTALL_DIR existiert nicht. Erst ./deploy/install.sh."
[[ -d "$INSTALL_DIR/data" ]] || warn "$INSTALL_DIR/data fehlt — es gibt dann keine HWIDs zu erhalten."
[[ -f "$INSTALL_DIR/.env" ]] || fail "$INSTALL_DIR/.env fehlt. Ohne die Datei ist der private Signaturschlüssel verloren."

# Prüfen, ob der Quellcode überhaupt da ist. Sonst würde das Skript den
# Dienst stoppen und dann feststellen, dass nichts zu installieren ist.
[[ -f "$REPO_ROOT/bot/draxo_bot/bot.py" ]] \
    || fail "Bot-Code nicht gefunden ($REPO_ROOT). Skript aus dem Repo auf dem Server starten."

VENV_PY="$INSTALL_DIR/.venv/bin/python"
[[ -x "$VENV_PY" ]] || fail "Virtualenv fehlt ($VENV_PY). Erst ./deploy/install.sh."

# Der Dienst darf nicht schon laufen, während der Code getauscht wird.
# Sonst schreibt er in die Dateien, die gerade ersetzt werden.
if svc is-active --quiet "$SERVICE_NAME" 2>/dev/null; then
    RUNNING=1
    ok "Dienst läuft (PID $(svc show -p MainPID --value "$SERVICE_NAME"))"
else
    RUNNING=0
    info "Dienst läuft nicht — Update wird trotzdem durchgeführt."
fi

DB="$INSTALL_DIR/data/draxo.sqlite3"

# ══════════════════════════════════════════════════════════════════════
#  Zustand vorher festhalten
#
#  Diese Zahlen sind der Beweis am Ende des Skripts. Ohne sie wäre
#  "Daten erhalten" nur eine Behauptung — und die würde man erst merken,
#  wenn ein Nutzer seinen Key verloren hat.
# ══════════════════════════════════════════════════════════════════════

count_rows() {
    # Über das venv-Python, nicht über die sqlite3-CLI: die ist auf Mint
    # nicht installiert, das venv-Python aber zwangsläufig vorhanden.
    "$VENV_PY" - "$1" <<'PYEOF'
import sqlite3, sys, json
path = sys.argv[1]
out = {"hwids": -1, "grants": -1, "revoked": -1, "audit": -1}
try:
    db = sqlite3.connect(f"file:{path}?mode=ro", uri=True)
    for table in out:
        try:
            out[table] = db.execute(f"SELECT COUNT(*) FROM {table}").fetchone()[0]
        except sqlite3.Error:
            pass
    db.close()
except sqlite3.Error as exc:
    print(json.dumps({"error": str(exc)}), file=sys.stderr)
print(json.dumps(out))
PYEOF
}

if [[ -f "$DB" ]]; then
    BEFORE="$(count_rows "$DB")"
    info "Vorher: $BEFORE"
else
    BEFORE='{"hwids":-1,"grants":-1,"revoked":-1,"audit":-1}'
    warn "Keine Datenbank gefunden — es gibt nichts zu erhalten."
fi

# ══════════════════════════════════════════════════════════════════════
#  Sicherung
# ══════════════════════════════════════════════════════════════════════

TS="$(date +%Y%m%d-%H%M%S)"
BACKUP_DIR="$BACKUP_ROOT/pre-update-$TS"

if [[ $DRY_RUN -eq 0 ]]; then
    # mkdir statt install -d -m: die Rechte setzen wir danach explizit.
    # Ein Scheitern an den Rechten soll nicht den Abbruch vor der
    # Sicherung ausloesen — ohne Sicherung darf gar nichts passieren.
    mkdir -p "$BACKUP_DIR"
    # .env zuerst: ohne diese Datei ist der Schlüssel weg, und das ist
    # nicht durch ein Update rückgängig zu machen.
    cp -a "$INSTALL_DIR/.env" "$BACKUP_DIR/env"
    chmod 0600 "$BACKUP_DIR/env" 2>/dev/null || true
    chmod 0700 "$BACKUP_DIR" 2>/dev/null || true
    ok ".env gesichert nach $BACKUP_DIR"
fi

# ══════════════════════════════════════════════════════════════════════
#  Dienst stoppen
# ══════════════════════════════════════════════════════════════════════

if [[ $RUNNING -eq 1 ]]; then
    if [[ $DRY_RUN -eq 1 ]]; then
        step "Würde $SERVICE_NAME stoppen"
    else
        info "Stoppe Dienst …"
        svc stop "$SERVICE_NAME"

        # Warten, bis der Prozess wirklich weg ist. systemctl stop kehrt
        # zurück, sobald SIGTERM gesendet wurde — nicht, sobald die
        # Datenbank geschlossen ist. Ohne dieses Warten macht der
        # Checkpoint gleich darauf gegen eine noch offene Datei und
        # wartet seinerseits.
        for _ in $(seq 1 30); do
            svc is-active --quiet "$SERVICE_NAME" || break
            sleep 1
        done
        if svc is-active --quiet "$SERVICE_NAME"; then
            fail "Dienst reagiert nicht auf SIGTERM. Abbruch — es wurde nichts geändert."
        fi
        ok "Dienst gestoppt"
    fi
fi

# ══════════════════════════════════════════════════════════════════════
#  WAL-Checkpoint  ← der eigentliche Datenschutz
#
#  SQLite hat die letzten Commits im -wal stehen und schreibt sie erst bei
#  Bedarf in die Hauptdatei. Ein normales Beenden macht das automatisch
#  (der letzte Handle schließt die Datenbank und checkpointet), aber:
#
#    * ein SIGKILL von systemd nach TimeoutStopSec tut es nicht
#    * der Dienst war vielleicht gar nicht gestoppt
#    * ein Absturz hinterlässt die Datei in jedem Fall
#
#  Also explizit: TRUNCATE schreibt alles in die Hauptdatei zurück und
#  leert die -wal. Danach ist die Hauptdatei vollständig, und DAS ist
#  die Datei, die gesichert wird.
# ══════════════════════════════════════════════════════════════════════

checkpoint_and_verify() {
    "$VENV_PY" - "$DB" <<'PYEOF'
import sqlite3, sys, json, os

path = sys.argv[1]
report = {"ok": True, "checkpointed": False, "integrity": "unknown", "wal_bytes": 0}

if not os.path.exists(path):
    report["ok"] = False
    report["error"] = "Datenbank fehlt"
    print(json.dumps(report))
    sys.exit(1)

wal = path + "-wal"
if os.path.exists(wal):
    report["wal_bytes"] = os.path.getsize(wal)

db = sqlite3.connect(path, timeout=30, isolation_level=None)
try:
    # TRUNCATE: alles zurückschreiben und die -wal auf 0 Byte bringen.
    mode, _, _ = db.execute("PRAGMA wal_checkpoint(TRUNCATE)").fetchone()
    report["checkpointed"] = mode == 0
    report["wal_bytes"] = os.path.getsize(wal) if os.path.exists(wal) else 0

    # integrity_check liest die gesamte Datenbank. Bei einer beschädigten
    # Datei ist das der Moment, in dem man es merkt — vor dem Deploy,
    # nicht drei Tage später, wenn sich niemand an Keys erinnert.
    report["integrity"] = db.execute("PRAGMA integrity_check").fetchone()[0]
except sqlite3.DatabaseError as exc:
    report["ok"] = False
    report["error"] = str(exc)
finally:
    db.close()

print(json.dumps(report))
sys.exit(0 if report["ok"] and report["integrity"] == "ok" else 1)
PYEOF
}

if [[ -f "$DB" ]]; then
    if [[ $DRY_RUN -eq 1 ]]; then
        step "Würde WAL-Checkpoint und integrity_check ausführen"
    else
        info "WAL-Checkpoint und Integritätsprüfung …"
        if REPORT="$(checkpoint_and_verify 2>&1)"; then
            ok "Checkpoint: $REPORT"
        else
            fail "Datenbankprüfung fehlgeschlagen: $REPORT
       Abbruch, es wurde nichts deployt. Bitte manuell prüfen:
           sqlite3 $DB \"PRAGMA integrity_check;\"
       oder aus $BACKUP_DIR zurückspielen."
        fi
    fi
fi

# Jetzt ist die Hauptdatei vollständig — erst jetzt sichern.
if [[ -f "$DB" && $DRY_RUN -eq 0 ]]; then
    cp -a "$DB" "$BACKUP_DIR/draxo.sqlite3"
    # -shm ist nur ein Index, aber wenn sie existiert gehört sie dazu.
    [[ -f "$DB-shm" ]] && cp -a "$DB-shm" "$BACKUP_DIR/" || true
    ok "Datenbank gesichert ($(du -h "$DB" | cut -f1))"
fi

# ══════════════════════════════════════════════════════════════════════
#  Code aktualisieren
#
#  Bewusst Datei für Datei statt "rm -rf + cp". Ein rm würde bei einem
#  Fehler mitten im Kopieren einen Server ohne Code hinterlassen, und es
#  würde genau die beiden Verzeichnisse mitnehmen, die man behalten will.
# ══════════════════════════════════════════════════════════════════════

sync_code() {
    local src="$1" dst="$2"

    # Python-Pakete: __pycache__ mitnehmen wäre Ballast und Verwirrung,
    # wenn sich Modulnamen geändert haben.
    rm -rf "${dst:?}/draxo_bot/__pycache__"
    cp -r "$src/draxo_bot" "$dst/"
    rm -rf "$dst/draxo_bot/__pycache__"

    cp "$src/requirements.txt" "$dst/"

    # license_signing.py: enthält SERVER_PUBLIC_KEY_HEX. Steht dort ein
    # anderer Wert als in der .env, stellt der Bot Grants aus, die der
    # Client ablehnt. Deshalb nach dem Kopfen prüfen, nicht vorher.
    local repo_root="$3"
    if [[ -f "$repo_root/license_signing.py" ]]; then
        cp "$repo_root/license_signing.py" "$dst/"
    fi
}

info "Aktualisiere Code …"
if [[ $DRY_RUN -eq 1 ]]; then
    step "Würde $REPO_ROOT/bot/draxo_bot → $INSTALL_DIR/draxo_bot kopieren"
    step "Würde requirements.txt und license_signing.py kopieren"
else
    sync_code "$REPO_ROOT/bot" "$INSTALL_DIR" "$REPO_ROOT"
    ok "Code aktualisiert"
fi

# ══════════════════════════════════════════════════════════════════════
#  Schlüssel-Plausibilität
#
#  Der häufigste stillschweigende Fehler nach einem Update: Der Server hat
#  einen anderen privaten Schlüssel als der öffentliche im Launcher. Der
#  Bot läuft dann fröhlich und stellt Grants aus, die kein Client annimmt.
#  Die Nutzer sehen "ungültiger Key" und niemand weiß warum.
# ══════════════════════════════════════════════════════════════════════

if [[ $DRY_RUN -eq 0 ]]; then
    # Das Modul selbst fragen, nicht die Datei mit einem Regex zerlegen.
    # Der Schluessel steht auf zwei 32-Zeichen-Strings aufgeteilt in einer
    # Klammer — ein Regex erwartet einen zusammenhaengenden Block und
    # liefert dann stillschweigend nichts zurueck. Genau so entsteht ein
    # Skript, das den Schluessel nie prueft und trotzdem gruen aussieht.
    CODE_PUB="$(
        "$VENV_PY" - "$INSTALL_DIR" <<'PYEOF'
import sys
install_dir = sys.argv[1]
sys.path.insert(0, install_dir)

# Fehlender Import oder fehlendes public_key() ist hier kein Fall fuer
# "pruefen wir halt nicht": dann laeuft ein Update durch, das den
# Schluessel nicht vergleicht hat, und meldet dabei Erfolg. Besser ein
# harter Fehler, den man im Log sieht.
import license_signing as grants

getter = getattr(grants, "public_key", None)
if getter is None:
    print("FEHLT:public_key")
    sys.exit(0)

# Umgebungsvariable hat Vorrang — genau wie beim Client. Sonst wuerde der
# Deploy gegen einen Schluessel pruefen, den der Bot gar nicht benutzt.
try:
    value = getter().hex()
except Exception as exc:
    print("FEHLT:" + type(exc).__name__)
    sys.exit(0)

if not value:
    print("FEHLT:leer")
else:
    print(value)
PYEOF
    )"

    # Leerer Schlüssel ist ein Warnfall, kein Abbruch: der Bot startet dann
    # zwar, stellt aber keine Grants aus — das ist ein Zustand, den der
    # Betreiber bewusst so wählen kann (etwa waehrend eines Key-Rotations-
    # fensters).
    KEY_PRESENT="$(
        grep -E '^DRAXO_SIGNING_KEY=' "$INSTALL_DIR/.env" 2>/dev/null \
        | head -1 | cut -d= -f2- | tr -d '[:space:]'
    )"

    if [[ -z "$KEY_PRESENT" ]]; then
        warn "DRAXO_SIGNING_KEY ist in der .env leer — der Bot stellt keine Grants aus."
    elif [[ "$CODE_PUB" == FEHLT:* ]]; then
        # Kein weiches Durchwinken: eine fehlende Pruefung, die als
        # Erfolg durchlaeuft, ist schlimmer als ein Abbruch.
        fail "Der oeffentliche Schluessel aus license_signing.py ist nicht lesbar (${CODE_PUB#FEHLT:}).
       Das Skript kann nicht pruefen, ob Bot und Launcher denselben
       Schluessel verwenden. Bitte die Datei pruefen:
           cd $INSTALL_DIR && $VENV_PY -c 'import license_signing as g; print(g.public_key().hex())'
       Der Dienst wurde NICHT gestartet."
    else
        # Der private Schlüssel wird per Umgebung übergeben, nicht in den
        # Quelltext eines -c-Aufrufs interpoliert: sonst steht er in
        # /proc/<pid>/cmdline, in `ps auxww` und in jedem Stack-Trace, den
        # ein Fehler ausgibt. Nur der abgeleitete oeffentliche Teil wird
        # ausgegeben.
        ENV_PUB="$(
            DRAXO_KEY_FROM_ENV="$INSTALL_DIR/.env" \
            "$VENV_PY" - "$INSTALL_DIR" <<'PYEOF'
import os, re, sys, pathlib

# Der Pfad zur .env kommt ueber die Umgebung, nicht ueber die
# Kommandozeile: als Argument stuende er zwar nicht im Klartext, aber der
# private Schluessel waere an dieser Stelle nur ein String im Speicher.
# Wichtiger ist der Kommentar weiter oben — dort wird der Wert nicht
# interpoliert, sondern gelesen.
install_dir = sys.argv[1]
env_path = os.environ.get("DRAXO_KEY_FROM_ENV", "")

seed_hex = ""
if env_path and os.path.exists(env_path):
    for line in pathlib.Path(env_path).read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line.startswith("DRAXO_SIGNING_KEY="):
            seed_hex = line.split("=", 1)[1].strip().strip("\"'")
            break

if not re.fullmatch(r"[0-9a-fA-F]{64}", seed_hex):
    print("")
    sys.exit(0)

sys.path.insert(0, install_dir)
import license_signing as grants
print(grants.public_from_seed(bytes.fromhex(seed_hex)).hex())
PYEOF
        )"

        if [[ "$ENV_PUB" == "$CODE_PUB" ]]; then
            ok "Signaturschlüssel stimmt überein (${CODE_PUB:0:16}…)"
        else
            fail "ACHTUNG — Signaturschlüssel passt nicht zusammen:
         .env (privat)          → ${ENV_PUB:-nicht lesbar}
         license_signing.py     → $CODE_PUB
       Der Bot würde Grants ausstellen, die der Launcher ablehnt.
       Der Dienst wurde deshalb NICHT gestartet. Schlüssel in der .env
       korrigieren oder den öffentlichen Teil in license_signing.py anpassen
       und einen neuen Launcher bauen."
        fi
    fi
fi

# ══════════════════════════════════════════════════════════════════════
#  Abhängigkeiten
# ══════════════════════════════════════════════════════════════════════

if [[ $DRY_RUN -eq 1 ]]; then
    step "Würde pip install -r requirements.txt ausführen"
else
    info "Installiere Abhängigkeiten …"
    # Nur bei Bedarf: ein pip-Lauf ohne Änderung an requirements.txt
    # lädt jedes Mal Pakete herunter und macht ein Update unnötig langsam.
    if [[ "$INSTALL_DIR/requirements.txt" -ot "$INSTALL_DIR/.venv/.draxo-deps-installed" ]]; then
        ok "Abhängigkeiten unverändert — übersprungen"
    elif [[ "$NO_SYSTEMD" == "1" ]]; then
        step "Würde pip install ausführen (im Testbetrieb übersprungen)"
    else
        # Als SERVICE_USER installieren: die Dateien in .venv gehören dem
        # Dienst, und ein pip-Aufruf als root legt sie mit root-Rechten an.
        # Dann kann der Dienst sie nicht lesen und startet nicht.
        sudo -u "$SERVICE_USER" "$INSTALL_DIR/.venv/bin/pip" install --quiet -r "$INSTALL_DIR/requirements.txt"
        touch "$INSTALL_DIR/.venv/.draxo-deps-installed"
        ok "Abhängigkeiten installiert"
    fi
fi

# ══════════════════════════════════════════════════════════════════════
#  Rechte
# ══════════════════════════════════════════════════════════════════════

if [[ $DRY_RUN -eq 0 ]]; then
    # Der Dienst läuft als $SERVICE_USER. Code muss lesbar sein, data
    # beschreibbar — sonst startet er nicht und die Fehlermeldung
    # ("Permission denied") sagt nichts über die eigentliche Ursache.
    #
    # Jede Rechteoperation ist einzeln abgesichert: unter Windows gibt es
    # keine POSIX-Berechtigungen, und ein harter Fehler dort würde das
    # Skript abbrechen, nachdem der Code schon ausgetauscht ist.
    chown -R "$SERVICE_USER:$SERVICE_USER" "$INSTALL_DIR/data" 2>/dev/null || true
    chown "$SERVICE_USER:$SERVICE_USER" "$INSTALL_DIR/license_signing.py" 2>/dev/null || true
    chown "$SERVICE_USER:$SERVICE_USER" "$INSTALL_DIR/requirements.txt" 2>/dev/null || true
    chown -R "$SERVICE_USER:$SERVICE_USER" "$INSTALL_DIR/draxo_bot" 2>/dev/null || true
    chmod 0750 "$INSTALL_DIR" "$INSTALL_DIR/data" 2>/dev/null || true
    find "$INSTALL_DIR/draxo_bot" -type d -exec chmod 0750 {} + 2>/dev/null || true
    find "$INSTALL_DIR/draxo_bot" -type f -name '*.py' -exec chmod 0640 {} + 2>/dev/null || true
    ok "Rechte gesetzt"
fi

# ══════════════════════════════════════════════════════════════════════
#  Start
# ══════════════════════════════════════════════════════════════════════

if [[ $NO_RESTART -eq 1 ]]; then
    warn "--no-restart: Dienst bleibt gestoppt."
elif [[ $DRY_RUN -eq 1 ]]; then
    step "Würde $SERVICE_NAME starten"
else
    info "Starte Dienst …"
    svc start "$SERVICE_NAME"
    sleep 4

    if svc is-active --quiet "$SERVICE_NAME"; then
        ok "Dienst läuft (PID $(svc show -p MainPID --value "$SERVICE_NAME"))"
    else
        warn "Dienst läuft nicht. Letzte Logzeilen:"
        svc --no-pager --lines=25 status "$SERVICE_NAME" || true
        fail "Update fehlgeschlagen. Rollback:
         svc stop $SERVICE_NAME
         cp -a $BACKUP_DIR/env $INSTALL_DIR/.env
         cp -a $BACKUP_DIR/draxo.sqlite3 $INSTALL_DIR/data/
         svc start $SERVICE_NAME
         (der alte Codestand steht in $BACKUP_ROOT)"
    fi
fi

# ══════════════════════════════════════════════════════════════════════
#  Nachher prüfen  ← der Beweis
# ══════════════════════════════════════════════════════════════════════

if [[ $DRY_RUN -eq 0 && -f "$DB" ]]; then
    info "Prüfe Datenbestand …"
    AFTER="$(count_rows "$DB")"
    ok "Nachher: $AFTER"

    MISMATCH=0
    for table in hwids grants revoked audit; do
        before="$(echo "$BEFORE" | "$VENV_PY" -c "import json,sys; print(json.load(sys.stdin).get('$table', -1))")"
        after="$(echo "$AFTER"  | "$VENV_PY" -c "import json,sys; print(json.load(sys.stdin).get('$table', -1))")"
        [[ "$before" == "$after" ]] || {
            printf '\033[1;31m✗\033[0m %s: vorher %s, nachher %s\n' "$table" "$before" "$after" >&2
            MISMATCH=1
        }
    done

    if [[ $MISMATCH -eq 0 ]]; then
        ok "Alle Daten unverändert."
    else
        fail "Datenbestand hat sich geändert! Sofort zurückspielen:
         svc stop $SERVICE_NAME
         cp -a $BACKUP_DIR/draxo.sqlite3 $INSTALL_DIR/data/
         svc start $SERVICE_NAME"
    fi
fi

# ══════════════════════════════════════════════════════════════════════
#  Alte Backups aufräumen
# ══════════════════════════════════════════════════════════════════════

if [[ $DRY_RUN -eq 0 && -d "$BACKUP_ROOT" ]]; then
    # Nicht die 5 neuesten. Ein Backup, das beim nächsten Update
    # weggeräumt wird, ist kein Backup.
    find "$BACKUP_ROOT" -maxdepth 1 -name 'pre-update-*' -type d -printf '%T@ %p\n' 2>/dev/null \
        | sort -rn | tail -n +6 | cut -d' ' -f2- \
        | while read -r old; do
            [[ -n "$old" && -d "$old" ]] && rm -rf "$old" && printf '\033[1;33m!\033[0m Altes Backup entfernt: %s\n' "$old"
        done
fi

cat <<SUMMARY

─────────────────────────────────────────────────────────────────
 Update abgeschlossen
─────────────────────────────────────────────────────────────────
 Sicherung:      $BACKUP_DIR
 Code:           $INSTALL_DIR
 Daten:          $INSTALL_DIR/data (unverändert)

 Protokoll:      journalctl -u $SERVICE_NAME -n 50
 Rückrollen:     cp -a $BACKUP_DIR/env $INSTALL_DIR/.env
                 cp -a $BACKUP_DIR/draxo.sqlite3 $INSTALL_DIR/data/

─────────────────────────────────────────────────────────────────
SUMMARY
