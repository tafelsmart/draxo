"""Grant-Service: Rate-Limit, Ausgabe, Protokoll.

Getrennt vom discord.py-Code, damit die Logik ohne Gateway testbar bleibt.
"""

from __future__ import annotations

import datetime as dt
import hashlib
import logging
from typing import Awaitable, Callable, Optional

import discord

from .config import Config
from .signing import Signer, SigningError, grants
from .store import Store
from . import ui

log = logging.getLogger("draxo.grants")

Notifier = Callable[[discord.Embed], Awaitable[None]]
# Im HTTP-Modus gibt es keinen Gateway-Client. Die Zustellung ist deshalb
# optional: die Netlify-Funktion liefert die Antwort selbst als
# Interaction-Response, statt sie dem Bot zu überlassen.
Sender = Callable[[discord.Embed], Awaitable[None]]


def fingerprint(payload: bytes, signature: bytes) -> str:
    """Kurze, stabile Kennung eines Grants — Grundlage für den Widerruf.

    Es ist bewusst *keine* Sicherheitsfunktion: sie hängt an der Signatur, die
    ohnehin nicht fälschbar ist. Sie dient nur dazu, den Server einen
    gesperrten Grant wiederzuerkennen.
    """
    return hashlib.sha256(b"draxo-grant-v1" + payload + signature).hexdigest()[:32]


class GrantService:
    def __init__(self, config: Config, store: Store, notify: Optional[Notifier] = None) -> None:
        self._cfg = config
        self._store = store
        self._notify = notify
        self._signer: Optional[Signer] = None
        if config.signing_key is not None:
            self._signer = Signer(config.signing_key)

    @property
    def enabled(self) -> bool:
        return self._signer is not None

    @property
    def public_key_hex(self) -> str:
        return self._signer.public_key_hex if self._signer else ""

    async def issue(
        self, discord_id: int, hwid: str, *, sender: Optional[Sender] = None
    ) -> tuple[Optional[str], int, Optional[discord.Embed]]:
        """Stellt einen Grant aus.

        ``discord_id`` ist bewusst nur die Zahl: der Aufrufer kann eine
        Interaction-ID aus einem HTTP-Payload sein, zu dem es gar keinen
        Gateway-Client gibt.

        Rückgabe: ``(grant, expires_at, fehler)``. Bei Erfolg ist ``fehler``
        ``None`` und der Grant wurde über ``sender`` zugestellt, falls einer
        übergeben wurde. Bei einem Fehler ist der Grant ``None`` und
        ``fehler`` das Embed, das der Aufrufer in *seinem* Kontext zeigen
        soll — ein Modal, ein Slash-Befehl und ein JSON-Endpunkt brauchen alle
        eine eigene Antwort, und eine DM allein reicht nicht, wenn der Nutzer
        seine DMs abgestellt hat.
        """
        if self._signer is None:
            embed = ui.not_ready()
            await self._deliver(sender, embed)
            return None, 0, embed

        normalised = ""
        if self._cfg.bind_machine:
            try:
                normalised = grants.normalise_hwid(hwid)
            except grants.GrantError as exc:
                self._store.audit(str(discord_id), "grant.invalid_hwid", str(exc))
                embed = ui.error(
                    str(exc), "Kopiere sie mit dem **Copy**-Knopf im Launcher."
                )
                await self._deliver(sender, embed)
                return None, 0, embed

        reset_at = self._store.next_quota_reset(
            discord_id, window=1, per_day=self._cfg.key_max_per_day
        )
        if reset_at:
            self._store.audit(str(discord_id), "grant.ratelimited")
            embed = ui.rate_limited(reset_at)
            await self._deliver(sender, embed)
            return None, 0, embed

        try:
            issued = self._signer.issue(
                discord_id=discord_id,
                hwid=normalised,
                hours=self._cfg.key_hours,
            )
        except SigningError as exc:
            self._store.audit(str(discord_id), "grant.failed", str(exc))
            embed = ui.error("Der Server konnte den Grant nicht ausstellen.", str(exc))
            await self._deliver(sender, embed)
            return None, 0, embed

        payload, signature = grants.unpack(issued.token)
        self._store.set_hwid(
            discord_id, normalised or self._store.get_hwid(discord_id) or ""
        )
        self._store.record_grant(
            user_id=discord_id,
            discord_id=discord_id,
            hwid=normalised,
            fingerprint=fingerprint(payload, signature),
            hwid_hash=issued.hwid_hash,
            expiry=issued.expires_at,
        )
        self._store.audit(str(discord_id), "grant.issued", issued.token[:12])
        log.info(
            "Grant ausgestellt: %s… (user_id=%s, hwid=%s)",
            issued.token[:12],
            discord_id,
            grants.mask_hwid(normalised) if normalised else "—",
        )

        await self._deliver(
            sender, ui.key_issued(issued.token, issued.expires_at, normalised)
        )
        if self._notify:
            try:
                await self._notify(
                    ui.key_logged_by_id(discord_id, issued.token[:12], issued.expires_at)
                )
            except Exception:  # Log darf die Ausgabe nie kippen
                log.exception("Grant-Log konnte nicht gesendet werden")
        return issued.token, issued.expires_at, None

    async def _deliver(self, sender: Optional[Sender], embed: discord.Embed) -> None:
        """Stellt ein Embed zu — über den übergebenen Sender, sonst gar nicht.

        Im Gateway-Modus wird ein ``sender`` übergeben, der per DM zustellt.
        Im HTTP-Modus gibt es keinen: der Aufrufer zeigt das Embed selbst als
        Interaction-Response, und ein „zusätzlich per DM" existiert gar nicht.

        Fehlgeschlagene Zustellung ist bewusst kein Fehlschlag der Ausgabe —
        der Grant ist längst signiert und im Audit-Log. Sonst würde ein
        deaktivierter DM-Filter einen gültigen Grant verwerfen.
        """
        if sender is None:
            return
        try:
            await sender(embed)
        except Exception:
            log.exception("Zustellung des Embeds fehlgeschlagen")


def relative(unix: int) -> str:
    if not unix:
        return "unbefristet"
    return dt.datetime.fromtimestamp(unix, dt.timezone.utc).strftime("%d.%m.%Y %H:%M UTC")