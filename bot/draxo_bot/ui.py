"""Embeds und Views — das gesamte sichtbare Bot-Interface an einer Stelle.

Farben und Wortwahl sind an Launcher und Website angeglichen, damit der Bot
nicht wie ein Fremdkörper im eigenen Ökosystem wirkt.
"""

from __future__ import annotations

import datetime as dt

import discord

from .config import COLOR_ERROR, COLOR_MUTED, COLOR_PRIMARY, COLOR_SUCCESS, COLOR_WARN
from .signing import grants


def _footer(text: str) -> discord.Embed:
    e = discord.Embed(color=COLOR_PRIMARY)
    e.set_footer(text=text)
    return e


def welcome(author_name: str, key_hours: int) -> discord.Embed:
    e = discord.Embed(
        title=f"Willkommen bei {author_name}!",
        description=(
            "Schön, dass du da bist. Hier läuft alles rund um den **Draxo Client** — "
            "Support, Updates und dein persönlicher Lizenz-Key.\n\n"
            "Der einfachste Weg zu einem Key ist der Button unten. Du brauchst dafür "
            "nur deine HWID, die der Launcher dir unter **CONFIG → LICENSE** anzeigt."
        ),
        color=COLOR_PRIMARY,
    )
    e.add_field(name="So kommst du zu deinem Key", value=(
        "1. Öffne den Launcher und klicke auf **Injizieren**.\n"
        "2. Kopiere deine HWID (Button **📋 Copy** neben dem Feld).\n"
        "3. Klicke hier unten auf **Key holen** und füge sie ein.\n"
        f"4. Der Key gilt **{key_hours} Stunden** und ist an deine HWID gebunden."
    ), inline=False)
    e.add_field(name="Regeln", value=(
        "• Kein Weiterverkauf und keine geteilten Keys.\n"
        "• Kein Cheating in Servern, auf denen es verboten ist.\n"
        "• Bugs bitte im Support-Channel melden, nicht im Chat."
    ), inline=False)
    return e


def key_issued(key: str, expiry: int, hwid: str) -> discord.Embed:
    """Antwort auf eine erfolgreiche Key-Anfrage. Nur für den Anfragenden sichtbar."""
    when = (
        dt.datetime.fromtimestamp(expiry, dt.timezone.utc)
        if expiry
        else None
    )
    e = discord.Embed(
        title="Dein Key",
        description=(
            f"```\n{key}\n```\n"
            "Kopier ihn und trage ihn im Launcher unter **CONFIG → LICENSE** ein."
        ),
        color=COLOR_SUCCESS,
    )
    e.add_field(name="HWID", value=f"`{grants.mask_hwid(hwid)}`", inline=True)
    e.add_field(
        name="Gültig bis",
        value=when.strftime("%d.%m.%Y %H:%M UTC") if when else "unbefristet",
        inline=True,
    )
    e.add_field(
        name="Wichtig",
        value="Der Key ist an diese HWID gebunden. Auf einem anderen PC funktioniert er nicht.",
        inline=False,
    )
    e.set_footer(text="Nur für dich sichtbar — teile ihn mit niemandem.")
    return e


def error(message: str, hint: str = "") -> discord.Embed:
    e = discord.Embed(title="Geht nicht", description=message, color=COLOR_ERROR)
    if hint:
        e.add_field(name="Was du tun kannst", value=hint, inline=False)
    return e


def info(title: str, description: str, color: int = COLOR_MUTED) -> discord.Embed:
    return discord.Embed(title=title, description=description, color=color)


def not_ready() -> discord.Embed:
    return error(
        "Der Key-Dienst läuft gerade nicht.",
        "Der Betreiber muss `DRAXO_SIGNING_KEY` in der `.env` auf dem Server "
        "setzen. Bis dahin gibt es hier keine Keys.",
    )


def rate_limited(reset_at: int) -> discord.Embed:
    when = dt.datetime.fromtimestamp(reset_at, dt.timezone.utc)
    return error(
        "Du hast heute schon genug Keys geholt.",
        f"Du bekommst wieder einen um **{when.strftime('%d.%m. %H:%M UTC')}**. "
        "Dein bestehender Key läuft so lange weiter — du brauchst also keinen neuen.",
    )


def no_hwid() -> discord.Embed:
    return error(
        "Ich kenne deine HWID noch nicht.",
        "Führe zuerst `/hwid hwid:DEINE_HWID` aus — oder nutze den Button "
        "**Key holen** und trage sie dort ein.",
    )


def server_stats(member_count: int, online: int, keys: dict, bot_latency_ms: float) -> discord.Embed:
    e = discord.Embed(title="Server-Status", color=COLOR_PRIMARY)
    e.add_field(name="Mitglieder", value=f"{member_count} (**{online}** online)", inline=True)
    e.add_field(name="Ping", value=f"{bot_latency_ms:.0f} ms", inline=True)
    e.add_field(name="Registrierte HWIDs", value=str(keys.get("hwids", 0)), inline=True)
    e.add_field(name="Ausgestellte Keys", value=str(keys.get("issued", 0)), inline=True)
    e.add_field(name="Uptime", value=_uptime(), inline=True)
    return e


_STARTED_AT = dt.datetime.now(dt.timezone.utc)


def _uptime() -> str:
    delta = dt.datetime.now(dt.timezone.utc) - _STARTED_AT
    seconds = int(delta.total_seconds())
    days, rest = divmod(seconds, 86400)
    hours, rest = divmod(rest, 3600)
    minutes = rest // 60
    if days:
        return f"{days}d {hours}h {minutes}m"
    if hours:
        return f"{hours}h {minutes}m"
    return f"{minutes}m"


def key_logged_by_id(discord_id: int, key_preview: str, expiry: int) -> discord.Embed:
    """Öffentliches Protokoll im Log-Channel. Der Key selbst wird nie geloggt.

    Nimmt die reine ID statt eines ``discord.User``: im HTTP-Modus gibt es
    keinen Gateway-Client, aus dem ein User-Objekt käme. Für die Anzeige
    genügt die ID — sie ist ohnehin die eindeutige Kennung.
    """
    who = f"`{discord_id}`"
    when = (
        dt.datetime.fromtimestamp(expiry, dt.timezone.utc).strftime("%d.%m.%Y %H:%M UTC")
        if expiry
        else "unbefristet"
    )
    embed = discord.Embed(title="Key ausgestellt", color=COLOR_WARN)
    embed.add_field(name="Nutzer", value=who, inline=True)
    embed.add_field(name="Key", value=f"`{key_preview}…`", inline=True)
    embed.add_field(name="Gültig bis", value=when, inline=True)
    return embed


def member_joined(member: discord.Member) -> discord.Embed:
    return discord.Embed(
        title=f"{member} ist dabei",
        description=(
            "Herzlich willkommen! Kopiere dir deine HWID aus dem Launcher "
            "und hol dir hier deinen Key."
        ),
        color=COLOR_SUCCESS,
    )


# ── Views ───────────────────────────────────────────────────────────


class KeyRequest(discord.ui.Modal, title="Draxo — Key holen"):
    """Fragt die HWID ab und gibt den Key ausschliesslich dem Anfragenden."""

    hwid: discord.ui.TextInput = discord.ui.TextInput(
        label="Deine HWID",
        placeholder="32 Zeichen, z. B. 3F2A… (CONFIG → LICENSE → Copy)",
        min_length=32,
        max_length=32,
    )

    async def on_submit(self, interaction: discord.Interaction) -> None:  # noqa: D102
        from .signing import grants as g

        service = getattr(self, "grant_service", None)
        if service is None:
            await interaction.response.send_message(
                embed=not_ready(), ephemeral=True
            )
            return

        try:
            hwid = g.normalise_hwid(self.hwid.value)
        except g.GrantError as exc:
            await interaction.response.send_message(
                embed=error(str(exc), "Kopiere sie mit dem **Copy**-Knopf im Launcher."),
                ephemeral=True,
            )
            return

        async def _dm(embed: discord.Embed) -> None:
            await interaction.user.send(embed=embed)

        token, expiry, failure = await service.issue(
            interaction.user.id, hwid, sender=_dm
        )
        if token is None:
            # Der Dienst hat den Grund schon per DM geschickt; hier kommt er
            # noch einmal im Kontext des Modals an, damit der Nutzer ihn auch
            # sieht, wenn seine DMs geschlossen sind.
            await interaction.response.send_message(
                embed=failure or error("Der Grant konnte nicht ausgestellt werden."),
                ephemeral=True,
            )
            return

        await interaction.response.send_message(
            embed=key_issued(token, expiry, hwid), ephemeral=True
        )


class WelcomeView(discord.ui.View):
    """Die Leiste im Welcome-Channel.

    Der „Key holen“-Knopf kommt aus dem Dekorator; die beiden Link-Knöpfe
    werden hier von Hand hinzugefügt. Wichtig: kein add_item() für denselben
    Knopf wie im Dekorator, sonst erscheint er doppelt.
    """

    def __init__(self, timeout: float = 3600.0) -> None:
        super().__init__(timeout=timeout)
        self.add_item(
            discord.ui.Button(
                style=discord.ButtonStyle.link,
                label="Website",
                url="https://draxo.netlify.app/",
            )
        )
        self.add_item(
            discord.ui.Button(
                style=discord.ButtonStyle.link,
                label="Draxo Client",
                url="https://draxo.netlify.app/DraxoLauncher.exe",
            )
        )

    @discord.ui.button(style=discord.ButtonStyle.primary, label="Key holen", emoji="🔑")
    async def ask_key(
        self, interaction: discord.Interaction, button: discord.ui.Button
    ) -> None:
        service = getattr(interaction.client, "grant_service", None)
        if service is None:
            await interaction.response.send_message(embed=not_ready(), ephemeral=True)
            return
        modal = KeyRequest()
        modal.grant_service = service
        await interaction.response.send_modal(modal)


def main_panel(author: discord.abc.User, key_hours: int) -> discord.Embed:
    """Embed, das der Bot beim Start in den Welcome-Channel postet."""
    e = welcome(str(author), key_hours)
    e.set_thumbnail(url=author.display_avatar.url)
    e.set_footer(text=f"@{author.name} • Keys: {key_hours} h • https://draxo.netlify.app/")
    return e