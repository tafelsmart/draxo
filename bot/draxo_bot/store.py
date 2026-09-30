"""Persistenz: HWID-Zuordnung, Rate-Limiting und Audit-Log.

Bewusst SQLite statt Postgres — der Bot läuft auf einem einzelnen Server und
darf dort keinen zusätzlichen Dienst verlangen. Der Pfad kommt aus der
Umgebung (``DRAXO_DB``).
"""

from __future__ import annotations

import sqlite3
import time
from pathlib import Path

_SCHEMA = """
CREATE TABLE IF NOT EXISTS hwids (
    user_id     INTEGER PRIMARY KEY,
    hwid        TEXT    NOT NULL,
    created_at  INTEGER NOT NULL
);

-- Ausgestellte Grants. Der Token selbst wird NICHT gespeichert: er ist
-- signiert und offline pruefbar, der Server muss ihn nicht kennen.
-- Gespeichert wird nur genug, um gezielt zu widerrufen.
CREATE TABLE IF NOT EXISTS grants (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id       INTEGER NOT NULL,
    discord_id    INTEGER NOT NULL,
    hwid          TEXT    NOT NULL DEFAULT '',
    hwid_hash     BLOB,
    fingerprint   TEXT    NOT NULL,
    expiry        INTEGER NOT NULL,
    revoked       INTEGER NOT NULL DEFAULT 0,
    created_at    INTEGER NOT NULL
);

CREATE INDEX IF NOT EXISTS grants_user_time ON grants (user_id, created_at);
CREATE INDEX IF NOT EXISTS grants_fingerprint ON grants (fingerprint);

CREATE TABLE IF NOT EXISTS audit (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    actor      TEXT    NOT NULL,
    action     TEXT    NOT NULL,
    detail     TEXT    NOT NULL DEFAULT '',
    created_at INTEGER NOT NULL
);
"""


class Store:
    def __init__(self, path: Path) -> None:
        self._path = path
        path.parent.mkdir(parents=True, exist_ok=True)
        # check_same_thread=False: die Verbindung wird im Haupt-Thread geöffnet,
        # aber aus der Event-Loop des Bots benutzt. Das ist sicher, weil
        # alle Zugriffe im selben Thread (der Loop) stattfinden und dadurch
        # serialisiert sind.
        self._db = sqlite3.connect(path, isolation_level=None, check_same_thread=False)
        self._db.row_factory = sqlite3.Row
        self._db.execute("PRAGMA journal_mode=WAL")
        self._db.execute("PRAGMA foreign_keys=ON")
        self._db.executescript(_SCHEMA)

    def close(self) -> None:
        self._db.close()

    # ── HWID-Zuordnung ─────────────────────────────────────────────

    def get_hwid(self, user_id: int) -> str | None:
        row = self._db.execute(
            "SELECT hwid FROM hwids WHERE user_id = ?", (user_id,)
        ).fetchone()
        return row["hwid"] if row else None

    def set_hwid(self, user_id: int, hwid: str) -> None:
        self._db.execute(
            "INSERT INTO hwids (user_id, hwid, created_at) VALUES (?, ?, ?) "
            "ON CONFLICT(user_id) DO UPDATE SET hwid = excluded.hwid, "
            "created_at = excluded.created_at",
            (user_id, hwid, int(time.time())),
        )

    def clear_hwid(self, user_id: int) -> None:
        self._db.execute("DELETE FROM hwids WHERE user_id = ?", (user_id,))

    # ── Rate-Limiting ──────────────────────────────────────────────

    def issued_since(self, user_id: int, since: int) -> int:
        row = self._db.execute(
            "SELECT COUNT(*) AS n FROM grants WHERE user_id = ? AND created_at >= ?",
            (user_id, since),
        ).fetchone()
        return int(row["n"])

    def next_quota_reset(self, user_id: int, window: int, per_day: int) -> int:
        """Unix-Zeitpunkt, zu dem wieder ein Key möglich ist (0 = sofort)."""
        now = int(time.time())
        used = self.issued_since(user_id, now - window * 86400)
        if used < per_day:
            return 0
        row = self._db.execute(
            "SELECT created_at FROM grants WHERE user_id = ? "
            "ORDER BY created_at ASC LIMIT 1",
            (user_id,),
        ).fetchone()
        if row is None:
            return 0
        return int(row["created_at"]) + window * 86400

    def record_grant(
        self,
        *,
        user_id: int,
        discord_id: int,
        hwid: str,
        fingerprint: str,
        hwid_hash: bytes,
        expiry: int,
    ) -> None:
        self._db.execute(
            "INSERT INTO grants (user_id, discord_id, hwid, hwid_hash, "
            "fingerprint, expiry, created_at) VALUES (?, ?, ?, ?, ?, ?, ?)",
            (user_id, discord_id, hwid, hwid_hash, fingerprint, expiry, int(time.time())),
        )

    def last_issue(self, user_id: int) -> sqlite3.Row | None:
        return self._db.execute(
            "SELECT * FROM grants WHERE user_id = ? ORDER BY created_at DESC LIMIT 1",
            (user_id,),
        ).fetchone()

    def recent_issues(self, limit: int = 5) -> list[sqlite3.Row]:
        return self._db.execute(
            "SELECT * FROM grants ORDER BY created_at DESC LIMIT ?", (limit,)
        ).fetchall()

    # ── Widerruf ───────────────────────────────────────────────────
    #
    # Signierte Grants sind offline nicht widerrufbar — das ist die Kehrseite
    # davon, kein Geheimnis im Client zu haben. Deshalb hält der Server den
    # Fingerabdruck: /verify fragt hier nach, und ein gesperrter Fingerabdruck
    # macht jeden Grant ungültig, auch ohne Netz beim Client.

    def is_revoked(self, fingerprint: str) -> bool:
        row = self._db.execute(
            "SELECT COUNT(*) AS n FROM grants WHERE fingerprint = ? AND revoked = 1",
            (fingerprint,),
        ).fetchone()
        return int(row["n"]) > 0

    def revoke(self, fingerprint: str) -> int:
        cursor = self._db.execute(
            "UPDATE grants SET revoked = 1 WHERE fingerprint = ?", (fingerprint,)
        )
        return cursor.rowcount

    def stats(self) -> dict:
        counts = {
            "hwids": "SELECT COUNT(*) AS n FROM hwids",
            "issued": "SELECT COUNT(*) AS n FROM grants",
            "revoked": "SELECT COUNT(*) AS n FROM grants WHERE revoked = 1",
        }
        return {k: int(self._db.execute(q).fetchone()["n"]) for k, q in counts.items()}

    # ── Audit ──────────────────────────────────────────────────────

    def audit(self, actor: str, action: str, detail: str = "") -> None:
        self._db.execute(
            "INSERT INTO audit (actor, action, detail, created_at) VALUES (?, ?, ?, ?)",
            (actor, action, detail[:500], int(time.time())),
        )