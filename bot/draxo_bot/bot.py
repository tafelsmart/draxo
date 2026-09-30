"""Draxo Discord Bot — Einstiegspunkt.

Start:  python -m draxo_bot
Der Token kommt aus der Umgebung, nie aus dem Quelltext.
"""

from __future__ import annotations

import asyncio
import logging
import os
import random
import sys
from typing import Optional

import discord
from discord import app_commands

from . import commands as slash
from . import ui
from .api import LicenseAPI
from .config import Config, ConfigError, load
from .signing import grants  # noqa: F401
from .service import GrantService
from .store import Store

log = logging.getLogger("draxo")


class DraxoBot(discord.Client):
    """Reiner Slash-Command-Bot.

    Bewusst ``discord.Client`` statt ``commands.Bot``: es gibt keine
    Präfix-Befehle, und der Message-Content-Intent wäre unnötiges
    Berechtigungs-Rauschen im Log.
    """

    def __init__(self, config: Config) -> None:
        intents = discord.Intents.default()
        # Der Member-Intent ist privilegiert und muss im Developer Portal
        # aktiviert sein. Ohne ihn läuft der Bot trotzdem — on_member_join
        # feuert dann eben nicht, alle Befehle funktionieren normal.
        intents.members = config.welcome_on_join

        super().__init__(intents=intents)

        # discord.Client hat kein .tree, und seit discord.py 2.7 braucht
        # CommandTree den Client im Konstruktor. Der Baum wird hier
        # deshalb neu gebaut und die Befehle einzeln angehaengt.
        self.tree = app_commands.CommandTree(self)
        for command in slash.ALL_COMMANDS:
            self.tree.add_command(command)

        self.config = config
        self.store = Store(config.db_path)
        self.grant_service = GrantService(config, self.store, notify=self._log_to_channel)
        self.api = LicenseAPI(config, self.store, grant_service=self.grant_service)

    # ── Helfer ─────────────────────────────────────────────────────

    async def _log_to_channel(self, embed: discord.Embed) -> None:
        channel_id = self.config.log_channel_id
        if not channel_id:
            return
        channel = self.get_channel(channel_id)
        if isinstance(channel, discord.abc.Messageable):
            await channel.send(embed=embed)

    def _guild(self) -> Optional[discord.Guild]:
        if not self.config.guild_id:
            return None
        return self.get_guild(self.config.guild_id)

    # ── Events ─────────────────────────────────────────────────────

    async def setup_hook(self) -> None:
        guild = self._guild()
        if guild is None:
            # Zwei völlig verschiedene Ursachen landen hier — und die
            # unterscheiden sich in der Behebung, also nicht vermischen.
            if self.config.guild_id:
                log.error(
                    "DISCORD_GUILD_ID=%s ist gesetzt, aber der Bot ist nicht in "
                    "diesem Server. Einladung fehlt — Befehle werden deshalb "
                    "global registriert (kann bis zu einer Stunde dauern).",
                    self.config.guild_id,
                )
            else:
                log.warning(
                    "DISCORD_GUILD_ID fehlt — Slash-Commands werden global "
                    "registriert (kann bis zu einer Stunde dauern)."
                )
            await self.tree.sync()
            return
        self.tree.copy_to(guild)
        await self.tree.sync(guild=guild)
        log.info("Slash-Commands in %s synchronisiert.", guild.name)

    async def on_ready(self) -> None:
        log.info("Angemeldet als %s (%s)", self.user, self.user.id)
        for guild in self.guilds:
            log.info("  Server: %s (%s, %s Mitglieder)", guild.name, guild.id, guild.member_count or "?")
        await self.change_presence(
            activity=discord.Game(name=self.config.activity_text),
            status=discord.Status.online,
        )
        if not self.config.keys_enabled:
            log.warning(
                "DRAXO_SIGNING_KEY fehlt — /key und der Key-Knopf bleiben "
                "deaktiviert. Erzeugen: python -m draxo_bot.signing keygen"
            )
        elif self.grant_service.public_key_hex != grants.SERVER_PUBLIC_KEY_HEX:
            # Sonst stellt der Bot Grants aus, die der Client nicht pruefen kann.
            log.error(
                "Öffentlicher Schlüssel passt nicht zum eingebauten: Bot %s, "
                "Client %s. Neue Grants werden abgelehnt.",
                self.grant_service.public_key_hex[:16],
                grants.SERVER_PUBLIC_KEY_HEX[:16],
            )
        self.store.audit("bot", "ready", f"{self.user}@{self.user.id}")
        await self._post_welcome_panel()

    async def _post_welcome_panel(self) -> None:
        """Postet das Willkommens-Panel einmalig, nach Bot-Neustart idempotent."""
        channel_id = self.config.welcome_channel_id
        if not channel_id:
            return
        channel = self.get_channel(channel_id)
        if not isinstance(channel, discord.TextChannel):
            log.warning("Welcome-Channel %s nicht gefunden.", channel_id)
            return
        marker = f"<!-- draxo-welcome-panel:{self.user.id} -->"
        async for message in channel.history(limit=50):
            if marker in (message.content or ""):
                log.info("Welcome-Panel existiert bereits — nichts gepostet.")
                return
        await channel.send(content=marker, embed=ui.main_panel(self.user, self.config.key_hours), view=ui.WelcomeView())
        log.info("Welcome-Panel in #%s gepostet.", channel.name)

    async def on_member_join(self, member: discord.Member) -> None:
        if member.bot or not self.config.welcome_on_join:
            return
        guild = self._guild()
        if guild and member.guild.id != guild.id:
            return
        if self.config.member_role_id:
            role = member.guild.get_role(self.config.member_role_id)
            if role:
                try:
                    await member.add_roles(role, reason="Auto-Rolle beim Beitritt")
                except discord.Forbidden:
                    log.warning("Auto-Rolle fehlgeschlagen — Rolle über dem Bot?")
        if self.config.welcome_channel_id:
            channel = self.get_channel(self.config.welcome_channel_id)
            if isinstance(channel, discord.TextChannel):
                await channel.send(
                    member.mention,
                    embed=ui.member_joined(member),
                    view=ui.WelcomeView(),
                )
        self.store.audit(str(member), "member.joined", str(member.id))

    async def on_app_command_error(
        self, interaction: discord.Interaction, error: app_commands.AppCommandError
    ) -> None:
        if isinstance(error, app_commands.MissingPermissions):
            log.warning("Fehlende Rechte für %s von %s", interaction.command.qualified_name, interaction.user)
            await interaction.response.send_message(
                embed=ui.error(
                    "Dafür fehlen mir die Rechte.",
                    "Der Bot braucht dafür eine Administrator-Berechtigung.",
                ),
                ephemeral=True,
            )
            return
        log.exception("Fehler in %s", interaction.command.qualified_name, exc_info=error)
        try:
            await interaction.response.send_message(
                embed=ui.error("Da ist etwas schiefgelaufen.", "Bitte melde es im Support-Channel."),
                ephemeral=True,
            )
        except discord.InteractionResponded:
            pass


async def run() -> int:
    try:
        config = load()
    except ConfigError as exc:
        print(f"Konfigurationsfehler: {exc}", file=sys.stderr)
        return 2

    db_path = config.db_path
    if not os.access(db_path.parent, os.W_OK):
        print(
            f"Kein Schreibrecht für {db_path.parent} — der Benutzer des systemd-Dienstes "
            "kann die Datenbank nicht anlegen.",
            file=sys.stderr,
        )
        return 2

    logging.basicConfig(
        level=os.environ.get("LOG_LEVEL", "INFO").upper(),
        format="%(asctime)s %(levelname)-7s %(name)s: %(message)s",
    )
    logging.getLogger("discord").setLevel(logging.WARNING)
    random.seed()  # vermeidet identische Seeds über Neustarts hinweg

    bot = DraxoBot(config)
    async with bot:
        try:
            await bot.api.start()
            await bot.start(config.token)
        except discord.LoginFailure:
            print("DISCORD_TOKEN ist ungültig.", file=sys.stderr)
            return 3
        except asyncio.TimeoutError:
            print("Zeitüberschreitung beim Verbinden mit Discord.", file=sys.stderr)
            return 4
        finally:
            await bot.api.stop()
            bot.store.audit("bot", "shutdown")
            bot.store.close()
    return 0


def main() -> None:
    try:
        raise SystemExit(asyncio.run(run()))
    except KeyboardInterrupt:
        print("\nBeendet.")


if __name__ == "__main__":
    main()