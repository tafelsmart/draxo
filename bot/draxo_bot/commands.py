"""Slash-Commands.

Eigener Baum statt ``Bot.tree``, damit der Bot ein reiner ``discord.Client``
bleibt: ein reiner Slash-Bot braucht weder Message-Content-Intent noch
Präfix-Befehle, und beides erzeugt nur unnötige Warnungen und Berechtigungen.

Seit discord.py 2.7 verlangt ``CommandTree`` den Client im Konstruktor, ein
Baum auf Modulebene ist also nicht mehr moeglich. Deshalb werden die Befehle
hier als lose Funktionen dekoriert und in ``ALL_COMMANDS`` gesammelt;
``DraxoBot.__init__`` haengt sie einzeln an seinen eigenen Baum.
"""

from __future__ import annotations

import time

import discord
from discord import app_commands

from . import ui
from .config import COLOR_PRIMARY
from .signing import grants
from .service import relative

ALL_COMMANDS: list[app_commands.Command] = []


def _bot(interaction: discord.Interaction):
    """Der Client haengt am Interaction — kein globaler Zustand noetig."""
    return interaction.client


@app_commands.command(name="key", description="Holt einen Lizenz-Key für deine HWID.")
@app_commands.describe(hwid="Deine HWID aus dem Launcher (CONFIG → LICENSE)")
async def cmd_key(interaction: discord.Interaction, hwid: str | None = None) -> None:
    await interaction.response.defer(ephemeral=True, thinking=True)
    client = _bot(interaction)
    target = (hwid or client.store.get_hwid(interaction.user.id) or "").strip()
    if not target:
        await interaction.followup.send(embed=ui.no_hwid(), ephemeral=True)
        return
    # Der Key landet als DM — niemand sonst sieht ihn.
    async def _dm(embed: discord.Embed) -> None:
        await interaction.user.send(embed=embed)

    await client.grant_service.issue(interaction.user.id, target, sender=_dm)


@app_commands.command(name="hwid", description="Legt deine HWID für /key fest.")
@app_commands.describe(hwid="32 Zeichen aus dem Launcher", clear="Vergisst die gespeicherte HWID")
async def cmd_hwid(
    interaction: discord.Interaction, hwid: str | None = None, clear: bool = False
) -> None:
    client = _bot(interaction)

    if clear:
        client.store.clear_hwid(interaction.user.id)
        client.store.audit(str(interaction.user), "hwid.cleared")
        await interaction.response.send_message(
            embed=ui.info("Vergessen", "Deine gespeicherte HWID ist gelöscht."),
            ephemeral=True,
        )
        return

    if not hwid:
        stored = client.store.get_hwid(interaction.user.id)
        body = (
            f"Gespeicherte HWID: `{stored}`"
            if stored
            else "Ich habe noch keine HWID von dir. Übergib sie mit `/hwid hwid:…`."
        )
        await interaction.response.send_message(embed=ui.info("Deine HWID", body), ephemeral=True)
        return

    try:
        normalised = grants.normalise_hwid(hwid)
    except grants.GrantError as exc:
        await interaction.response.send_message(
            embed=ui.error(str(exc), "Kopiere sie mit dem **Copy**-Knopf im Launcher."),
            ephemeral=True,
        )
        return

    client.store.set_hwid(interaction.user.id, normalised)
    client.store.audit(str(interaction.user), "hwid.set")
    await interaction.response.send_message(
        embed=ui.info(
            "Gespeichert",
            f"Deine HWID `{normalised}` liegt hinterlegt. "
            "Mit `/key` bekommst du jederzeit einen neuen.",
        ),
        ephemeral=True,
    )


@app_commands.command(name="status", description="Zeigt deine gespeicherte HWID und den letzten Key.")
async def cmd_status(interaction: discord.Interaction) -> None:
    client = _bot(interaction)
    stored = client.store.get_hwid(interaction.user.id)
    row = client.store.last_issue(interaction.user.id)

    embed = ui.info("Dein Status", "", COLOR_PRIMARY)
    embed.add_field(
        name="HWID", value=f"`{stored}`" if stored else "nicht hinterlegt", inline=True
    )
    if row:
        expired = bool(row["expiry"]) and row["expiry"] < time.time()
        embed.add_field(name="Letzter Key", value=f"`{row['key_preview']}…`", inline=True)
        embed.add_field(
            name="Gültig bis",
            value=relative(row["expiry"]) + (" **(abgelaufen)**" if expired else ""),
            inline=True,
        )
    else:
        embed.add_field(name="Letzter Key", value="noch keiner", inline=True)

    await interaction.response.send_message(embed=embed, ephemeral=True)


@app_commands.command(name="serverinfo", description="Zahlen und Fakten zum Server.")
async def cmd_serverinfo(interaction: discord.Interaction) -> None:
    client = _bot(interaction)
    guild = interaction.guild
    if guild is None:
        await interaction.response.send_message(
            embed=ui.error("Kenne deinen Server nicht."), ephemeral=True
        )
        return
    online = sum(1 for m in guild.members if m.status is not discord.Status.offline)
    await interaction.response.send_message(
        embed=ui.server_stats(
            guild.member_count or 0, online, client.store.stats(), client.latency * 1000
        ),
        ephemeral=True,
    )


@app_commands.command(name="hilfe", description="Alle Befehle auf einen Blick.")
async def cmd_help(interaction: discord.Interaction) -> None:
    embed = ui.info("Draxo Bot", "So kommst du zu deinem Key:", COLOR_PRIMARY)
    embed.add_field(name="`/key`", value="Key für deine hinterlegte HWID anfordern", inline=False)
    embed.add_field(
        name="`/hwid hwid:…`", value="HWID hinterlegen (nur für dich sichtbar)", inline=False
    )
    embed.add_field(name="`/hwid clear:true`", value="Hinterlegte HWID löschen", inline=False)
    embed.add_field(name="`/status`", value="HWID und Ablauf deines letzten Keys", inline=False)
    embed.add_field(name="`/serverinfo`", value="Mitglieder, Ping, Key-Statistik", inline=False)
    embed.set_footer(text="Oder benutze den Button „Key holen“ im Welcome-Channel.")
    await interaction.response.send_message(embed=embed, ephemeral=True)


@app_commands.command(name="ping", description="Latenz zum Discord-Gateway.")
async def cmd_ping(interaction: discord.Interaction) -> None:
    await interaction.response.send_message(
        embed=ui.info("Pong", f"{interaction.client.latency * 1000:.0f} ms"), ephemeral=True
    )

ALL_COMMANDS.extend([cmd_key, cmd_hwid, cmd_status, cmd_serverinfo, cmd_help, cmd_ping])
