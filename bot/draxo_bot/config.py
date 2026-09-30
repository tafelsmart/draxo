"""Konfiguration für den Draxo-Bot — ausschließlich aus der Umgebung.

Bewusst kein Config-Modul mit Defaults im Code: jede sicherheitsrelevante
Einstellung muss explizit gesetzt sein, sonst startet der Bot nicht.
"""

from __future__ import annotations

import os
from dataclasses import dataclass
from pathlib import Path

# ── Markenfarben (mit dem Launcher/Website abgeglichen) ─────────────
COLOR_PRIMARY = 0x8B5CF6
COLOR_SUCCESS = 0x22C55E
COLOR_WARN = 0xF59E0B
COLOR_ERROR = 0xEF4444
COLOR_MUTED = 0x6B7280

BASE_DIR = Path(__file__).resolve().parent.parent


class ConfigError(RuntimeError):
    """Konfiguration ist unvollständig oder widersprüchlich."""


def _env(key: str, default: str | None = None) -> str | None:
    raw = os.environ.get(key)
    if raw is None:
        return default
    raw = raw.strip()
    return raw or default


def _env_int(key: str, default: int, *, lo: int, hi: int) -> int:
    raw = _env(key)
    if raw is None:
        return default
    try:
        value = int(raw)
    except ValueError as exc:
        raise ConfigError(f"{key} muss eine ganze Zahl sein, nicht {raw!r}") from exc
    if not lo <= value <= hi:
        raise ConfigError(f"{key} muss zwischen {lo} und {hi} liegen, ist aber {value}")
    return value


def _env_bool(key: str, default: bool) -> bool:
    raw = _env(key)
    if raw is None:
        return default
    return raw.lower() in {"1", "true", "yes", "on", "ja"}


@dataclass(frozen=True)
class Config:
    # ── Discord ────────────────────────────────────────────────────
    token: str
    guild_id: int | None
    welcome_channel_id: int | None
    log_channel_id: int | None
    member_role_id: int | None
    activity_text: str

    # ── Verhalten ──────────────────────────────────────────────────
    welcome_on_join: bool
    log_commands: bool

    # ── Grant-Ausgabe ──────────────────────────────────────────────
    # Privater Ed25519-Seed. Bleibt auf diesem Server.
    signing_key: bytes | None
    key_hours: int
    key_max_per_day: int
    bind_machine: bool
    db_path: Path

    # ── Optionale HTTP-API ─────────────────────────────────────────
    api_enabled: bool
    api_bind: str
    api_port: int
    api_token: str | None

    @property
    def keys_enabled(self) -> bool:
        """Ohne privaten Schluessel gibt es keine Grants — der Bot degradiert
        bewusst, statt etwas zu erfinden, das niemand pruefen kann."""
        return self.signing_key is not None


def load() -> Config:
    token = _env("DISCORD_TOKEN")
    if not token:
        raise ConfigError(
            "DISCORD_TOKEN fehlt. Kopiere .env.example nach .env und trage das "
            "Bot-Token ein (Developer Portal → Bot → Reset Token)."
        )

    seed_hex = _env("DRAXO_SIGNING_KEY")
    seed: bytes | None = None
    if seed_hex:
        try:
            seed = bytes.fromhex(seed_hex)
        except ValueError as exc:
            raise ConfigError(
                "DRAXO_SIGNING_KEY muss Hex sein (64 Zeichen = 32 Bytes)."
            ) from exc
        if len(seed) != 32:
            raise ConfigError(
                f"DRAXO_SIGNING_KEY muss genau 32 Bytes (64 Hex-Zeichen) sein, "
                f"hat aber {len(seed)}."
            )

    db_path = Path(_env("DRAXO_DB", str(BASE_DIR / "data" / "draxo.sqlite3")))

    return Config(
        token=token,
        guild_id=_env_int("DISCORD_GUILD_ID", 0, lo=0, hi=2**63 - 1) or None,
        welcome_channel_id=_env_int("WELCOME_CHANNEL_ID", 0, lo=0, hi=2**63 - 1) or None,
        log_channel_id=_env_int("LOG_CHANNEL_ID", 0, lo=0, hi=2**63 - 1) or None,
        member_role_id=_env_int("MEMBER_ROLE_ID", 0, lo=0, hi=2**63 - 1) or None,
        activity_text=_env("ACTIVITY_TEXT", "Draxo Client | FREE") or "",
        welcome_on_join=_env_bool("WELCOME_ON_JOIN", True),
        log_commands=_env_bool("LOG_COMMANDS", True),
        signing_key=seed,
        key_hours=_env_int("KEY_HOURS", 24, lo=1, hi=24 * 30),
        key_max_per_day=_env_int("KEY_MAX_PER_DAY", 1, lo=1, hi=50),
        bind_machine=_env_bool("BIND_HWID", True),
        db_path=db_path,
        api_enabled=_env_bool("API_ENABLED", False),
        api_bind=_env("API_BIND", "127.0.0.1") or "127.0.0.1",
        api_port=_env_int("API_PORT", 8787, lo=1024, hi=65535),
        api_token=_env("API_TOKEN"),
    )