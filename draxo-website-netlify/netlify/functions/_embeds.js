/**
 * Embeds und Views — die sichtbare Oberfläche im HTTP-Modus.
 *
 * Diese Datei ist ein Zwilling von `bot/draxo_bot/ui.py`. Das ist
 * absichtlich eine Doppelung und trotzdem richtig: die beiden Seiten laufen
 * auf verschiedenen Laufzeiten (Node auf Netlify, Python auf dem Server),
 * und ein gemeinsames Modul würde bedeuten, den Python-Bot von Node aus
 * aufzurufen — bei jedem Knopfdruck, mit Netzwerk und Latenz.
 *
 * Der Preis der Doppelung ist Drift. Deshalb prüft
 * `tests/test_interactions_parity.py`, dass Farben, Titel und Field-Namen
 * beider Seiten übereinstimmen. Wer hier etwas ändert, muss dort dasselbe
 * tun — und der Test sagt es einem, bevor ein Nutzer den Unterschied sieht.
 */

const COLORS = {
  primary: 0x8b5cf6,
  success: 0x22c55e,
  warn: 0xf59e0b,
  error: 0xef4444,
  muted: 0x6b7280,
};

const WEBSITE = "https://draxo.netlify.app/";
const LAUNCHER = "https://draxo.netlify.app/DraxoLauncher.exe";

/** Wie `ui.not_ready()` in Python. */
function notReady() {
  return error(
    "Der Key-Dienst läuft gerade nicht.",
    "Der Betreiber muss `DRAXO_SIGNING_KEY` in der `.env` auf dem Server " +
      "setzen. Bis dahin gibt es hier keine Keys."
  );
}

/** Wie `ui.error()`. */
function error(title, hint) {
  const embed = { title: "Geht nicht", description: title, color: COLORS.error };
  if (hint) {
    embed.fields = [
      { name: "Was du tun kannst", value: hint, inline: false },
    ];
  }
  return embed;
}

/** Wie `ui.info()`. */
function info(title, description, color = COLORS.muted) {
  return { title, description: description || "", color };
}

/** Wie `ui.key_issued()`. HWID wird maskiert, nie im Klartext. */
function keyIssued(key, expiresAt, hwid) {
  const when = expiresAt
    ? new Date(expiresAt * 1000)
        .toISOString()
        .replace("T", " ")
        .slice(0, 16) + " UTC"
    : "unbefristet";

  return {
    title: "Dein Key",
    description:
      "```\n" + key + "\n```\n" +
      "Kopier ihn und trage ihn im Launcher unter **CONFIG → LICENSE** ein.",
    color: COLORS.success,
    fields: [
      { name: "HWID", value: "`" + maskHwid(hwid) + "`", inline: true },
      { name: "Gültig bis", value: when, inline: true },
      {
        name: "Wichtig",
        value:
          "Der Key ist an diese HWID gebunden. Auf einem anderen PC " +
          "funktioniert er nicht.",
        inline: false,
      },
    ],
    footer: { text: "Nur für dich sichtbar — teile ihn mit niemandem." },
  };
}

/** Wie `grants.mask_hwid()`. */
function maskHwid(hwid) {
  const value = String(hwid ?? "");
  if (value.length <= 8) return "*".repeat(value.length);
  return value.slice(0, 4) + "…" + value.slice(-4);
}

/** Wie `ui.rate_limited()`. */
function rateLimited(resetAt) {
  const when = new Date(resetAt * 1000)
    .toISOString()
    .replace("T", " ")
    .slice(0, 16) + " UTC";
  return error(
    "Du hast heute schon genug Keys geholt.",
    "Du bekommst wieder einen um **" + when + "**. " +
      "Dein bestehender Key läuft so lange weiter — du brauchst also keinen neuen."
  );
}

/** Wie `ui.no_hwid()`. */
function noHwid() {
  return error(
    "Ich kenne deine HWID noch nicht.",
    "Führe zuerst `/hwid hwid:DEINE_HWID` aus — oder nutze den Button " +
      "**Key holen** und trage sie dort ein."
  );
}

/** Wie `cmd_help()`. */
function help() {
  const embed = info("Draxo Bot", "So kommst du zu deinem Key:", COLORS.primary);
  embed.fields = [
    { name: "`/key`", value: "Key für deine hinterlegte HWID anfordern", inline: false },
    { name: "`/hwid hwid:…`", value: "HWID hinterlegen (nur für dich sichtbar)", inline: false },
    { name: "`/hwid clear:true`", value: "Hinterlegte HWID löschen", inline: false },
    { name: "`/status`", value: "HWID und Ablauf deines letzten Keys", inline: false },
    { name: "`/hilfe`", value: "Diese Übersicht", inline: false },
  ];
  embed.footer = { text: "Oder benutze den Button „Key holen“ im Welcome-Channel." };
  return embed;
}

/** Wie `cmd_status()`, aufgebaut aus dem, was der Mint-Server meldet. */
function status(storedHwid, last) {
  const embed = info("Dein Status", "", COLORS.primary);
  const fields = [
    {
      name: "HWID",
      value: storedHwid ? "`" + storedHwid + "`" : "nicht hinterlegt",
      inline: true,
    },
  ];

  if (last && last.exists) {
    const expired = last.expired ? " **(abgelaufen)**" : "";
    fields.push(
      { name: "Letzter Key", value: "`" + last.preview + "…`", inline: true },
      {
        name: "Gültig bis",
        value: (last.expiresAt
          ? new Date(last.expiresAt * 1000)
              .toISOString()
              .replace("T", " ")
              .slice(0, 16) + " UTC"
          : "unbefristet") + expired,
        inline: true,
      }
    );
  } else {
    fields.push({ name: "Letzter Key", value: "noch keiner", inline: true });
  }

  embed.fields = fields;
  return embed;
}

/** Das Modal, das nach dem „Key holen"-Knopf aufgeht. */
function keyModal(customId) {
  return {
    custom_id: customId,
    title: "Draxo — Key holen",
    components: [
      {
        type: 1,
        components: [
          {
            type: 4,
            custom_id: "hwid",
            label: "Deine HWID",
            style: 2,
            placeholder: "32 Zeichen, z. B. 3F2A… (CONFIG → LICENSE → Copy)",
            min_length: 32,
            max_length: 32,
            required: true,
          },
        ],
      },
    ],
  };
}

/** Die Knopfleiste im Welcome-Channel. Wie `ui.WelcomeView`. */
function welcomeView(customId) {
  return [
    {
      type: 1,
      components: [
        {
          type: 2,
          custom_id: customId,
          style: 1,
          label: "Key holen",
          emoji: { name: "🔑" },
        },
        {
          // type 2 = Button, style 5 = Link. Beides verwechseln ist leicht:
          // "5" ist der Style eines Link-Knopfes, nicht sein Typ. Discord
          // lehnt type 5 ab, weil es keine Komponente dieser Art gibt.
          type: 2,
          style: 5,
          label: "Website",
          url: WEBSITE,
        },
        {
          type: 2,
          style: 5,
          label: "Draxo Client",
          url: LAUNCHER,
        },
      ],
    },
  ];
}

module.exports = {
  COLORS,
  WEBSITE,
  LAUNCHER,
  notReady,
  error,
  info,
  keyIssued,
  maskHwid,
  rateLimited,
  noHwid,
  help,
  status,
  keyModal,
  welcomeView,
};
