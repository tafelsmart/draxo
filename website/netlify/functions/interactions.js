/**
 * Discord Interactions-Endpoint.
 *
 * Eintragen im Developer Portal unter **General Information →
 * Interactions Endpoint URL**:
 *
 *     https://draxo.netlify.app/.netlify/functions/interactions
 *
 * Discord schickt beim Speichern sofort einen PING (Typ 1). Bleibt die
 * Antwort aus oder ist die Signaturprüfung nicht aktiv, speichert Discord
 * die URL nicht — und meldet das als „could not be verified", was nach
 * einem Bot-Problem klingt, aber fast immer am Endpoint liegt.
 *
 * Umgebungsvariablen (Netlify → Site settings → Environment):
 *
 *   DISCORD_PUBLIC_KEY   Public Key der Application (General Information)
 *   DRAXO_MINT_URL       https://bot.example.com  (der Linux-Server)
 *   DRAXO_MINT_TOKEN     derselbe Wert wie API_TOKEN in der .env des Bots
 *
 * Bewusst *nicht* hier: der private Signaturschlüssel. Siehe _mint.js.
 */

const {
  InteractionType,
  verifySignature,
  ephemeral,
  pong,
  modal,
  fail,
  userId,
  userTag,
  commandOf,
  modalValues,
} = require("./_discord.js");

const embeds = require("./_embeds.js");
const mint = require("./_mint.js");

/** custom_id des Knopfes und des Modals. Kurz, weil es im Payload steht. */
const ASK_KEY = "draxo:ask_key";
const HWID_FIELD = "hwid";

// ── Befehle ─────────────────────────────────────────────────────────────────

/**
 * /key — Grant für die hinterlegte HWID.
 *
 * Ohne HWID-Argument wird die gespeicherte genommen. Ohne gespeicherte
 * gibt es eine Antwort, die sagt, was zu tun ist — kein stilles Nichts.
 */
async function handleKey(interaction) {
  const id = userId(interaction);
  if (!id) return ephemeral({ embeds: [embeds.error("Kenne dein Konto nicht.")] });

  const { options } = commandOf(interaction);
  let hwid = typeof options.hwid === "string" ? options.hwid.trim() : "";

  if (!hwid) {
    const found = await mint.lookup(id);
    hwid = typeof found.hwid === "string" ? found.hwid : "";
  }
  if (!hwid) {
    return ephemeral({ embeds: [embeds.noHwid()] });
  }

  return issueAndReply(id, hwid);
}

/**
 * Gemeinsamer Pfad für /key, Knopf und Modal: Minten, dann antworten.
 *
 * An drei Stellen aufgerufen, damit Fehler, Maskierung und Ablaufformat an
 * allen drei identisch sind. Drei getrennte Aufrufe wären drei Stellen, an
 * denen ein Fix später nur zwei trifft.
 */
async function issueAndReply(id, hwid) {
  let result;
  try {
    result = await mint.issue(id, hwid);
  } catch (error) {
    // Der Server ist nicht erreichbar. Das ist ein Betriebsproblem, kein
    // Nutzerfehler — die Meldung sagt beides, damit klar ist, ob man
    // warten oder den Betreiber anschreiben muss.
    return ephemeral({
      embeds: [
        embeds.error(
          "Geht nicht",
          "Der Key-Server antwortet gerade nicht. Das liegt nicht an dir — " +
            "bitte in ein paar Minuten erneut versuchen. (" +
            (error.code ?? "unbekannt") +
            ")"
        ),
      ],
    });
  }

  if (!result || result.ok !== true || !result.grant) {
    return ephemeral({
      embeds: [
        embeds.error(
          "Grant nicht ausstellbar",
          String(result?.reason ?? "Der Server hat keinen Grant zurückgegeben.")
        ),
      ],
    });
  }

  return ephemeral({ embeds: [embeds.keyIssued(result.grant, result.expires_at, hwid)] });
}

/** /hwid — HWID hinterlegen, anzeigen oder löschen. */
async function handleHwid(interaction) {
  const id = userId(interaction);
  if (!id) return ephemeral({ embeds: [embeds.error("Kenne dein Konto nicht.")] });

  const { options } = commandOf(interaction);

  if (options.clear === true) {
    await mint.setHwid(id, "").catch(() => null);
    return ephemeral({
      embeds: [embeds.info("Vergessen", "Deine gespeicherte HWID ist gelöscht.")],
    });
  }

  const raw = typeof options.hwid === "string" ? options.hwid.trim() : "";

  if (!raw) {
    const found = await mint.lookup(id);
    const stored = typeof found.hwid === "string" ? found.hwid : "";
    return ephemeral({
      embeds: [
        embeds.info(
          "Deine HWID",
          stored
            ? "Gespeicherte HWID: `" + stored + "`"
            : "Ich habe noch keine HWID von dir. Übergib sie mit `/hwid hwid:…`."
        ),
      ],
    });
  }

  if (!/^[0-9a-fA-F]{32}$/.test(raw.replace(/\s+/g, ""))) {
    return ephemeral({
      embeds: [
        embeds.error(
          "HWID muss 32 Zeichen haben und darf nur 0-9 und A-F enthalten.",
          "Kopiere sie mit dem **Copy**-Knopf im Launcher."
        ),
      ],
    });
  }

  const normalised = raw.replace(/\s+/g, "").toUpperCase();
  try {
    await mint.setHwid(id, normalised);
  } catch {
    return ephemeral({
      embeds: [embeds.error("Speichern fehlgeschlagen.", "Der Key-Server ist gerade nicht erreichbar.")],
    });
  }

  return ephemeral({
    embeds: [
      embeds.info(
        "Gespeichert",
        "Deine HWID `" + normalised + "` liegt hinterlegt. " +
          "Mit `/key` bekommst du jederzeit einen neuen."
      ),
    ],
  });
}

/** /status — gespeicherte HWID und letzter Grant. */
async function handleStatus(interaction) {
  const id = userId(interaction);
  if (!id) return ephemeral({ embeds: [embeds.error("Kenne dein Konto nicht.")] });

  const found = await mint.lookup(id);
  return ephemeral({
    embeds: [
      embeds.status(
        typeof found.hwid === "string" ? found.hwid : "",
        found.last ?? null
      ),
    ],
  });
}

/** /hilfe. */
async function handleHelp() {
  return ephemeral({ embeds: [embeds.help()] });
}

/**
 * /ping — Latenz zum Mint-Server statt zum Gateway.
 *
 * Im Gateway-Modus misst dieser Befehl das Gateway. Hier gibt es keins, und
 * eine erfundene Zahl wäre schlimmer als eine ehrliche: die Antwort lautet
 * deshalb, *was* gemessen wurde.
 */
async function handlePing() {
  const started = Date.now();
  const healthy = await mint.lookup("0").then((r) => r !== undefined);
  const ms = Date.now() - started;
  return ephemeral({
    embeds: [
      embeds.info(
        "Pong",
        healthy
          ? `HTTP-Modus — ${ms} ms bis zum Key-Server.`
          : "Der Key-Server ist gerade nicht erreichbar."
      ),
    ],
  });
}

/** Unbekannter Befehl. */
function handleUnknown(name) {
  return ephemeral({
    embeds: [
      embeds.error(
        `Den Befehl \`/${name}\` gibt es nicht.`,
        "Mit `/hilfe` siehst du, was der Bot kann."
      ),
    ],
  });
}

// ── Komponenten und Modal ──────────────────────────────────────────────────

/** Der „Key holen"-Knopf öffnet das Modal. */
function handleComponent(interaction) {
  const customId = interaction?.data?.custom_id;
  if (customId === ASK_KEY) {
    return modal({ ...embeds.keyModal("draxo:key_form") });
  }
  return ephemeral({ embeds: [embeds.error("Dieser Knopf ist nicht mehr gültig.")] });
}

/** Das Modal schickt die HWID zurück — und stellt den Key aus. */
async function handleModal(interaction) {
  const id = userId(interaction);
  if (!id) return ephemeral({ embeds: [embeds.error("Kenne dein Konto nicht.")] });

  const values = modalValues(interaction);
  const raw = String(values[HWID_FIELD] ?? "").trim();

  if (!/^[0-9a-fA-F]{32}$/.test(raw.replace(/\s+/g, ""))) {
    return ephemeral({
      embeds: [
        embeds.error(
          "HWID muss 32 Zeichen haben und darf nur 0-9 und A-F enthalten.",
          "Kopiere sie mit dem **Copy**-Knopf im Launcher."
        ),
      ],
    });
  }

  const normalised = raw.replace(/\s+/g, "").toUpperCase();
  return issueAndReply(id, normalised);
}

// ── Handler ─────────────────────────────────────────────────────────────────

exports.handler = async (event) => {
  // Der Roh-Body ist Pflicht. Ein JSON.parse(event.body) und Neuserialisieren
  // ändert die Bytes (Schlüsselreihenfolge, Leerzeichen) und macht damit
  // jede Signaturprüfung unmöglich.
  const raw = Buffer.from(event.body ?? "", event.isBase64Encoded ? "base64" : "utf8");

  // Roh durchreichen. verifySignature baut den DER-Rahmen selbst und lehnt
  // einen unbrauchbaren Schlüssel ab, statt hier eine Exception zu werfen.
  const publicKey = String(process.env.DISCORD_PUBLIC_KEY ?? "").trim();
  if (!/^[0-9a-fA-F]{64}$/.test(publicKey)) {
    return fail(500, "DISCORD_PUBLIC_KEY fehlt oder ist kein 32-Byte-Hex.");
  }

  if (!verifySignature(
      raw,
      event.headers?.["x-signature-ed25519"],
      event.headers?.["x-signature-timestamp"],
      publicKey
    )) {
    // Kein Grundtext: wer eine Fälschung schickt, soll nicht erfahren,
    // welche der beiden Prüfungen fehlgeschlagen ist.
    return fail(401, "Ungültige Signatur.");
  }

  let interaction;
  try {
    interaction = JSON.parse(raw.toString("utf8"));
  } catch {
    return fail(400, "Body ist kein JSON.");
  }

  // PING muss vor allem anderen kommen: Discord validiert damit die URL.
  if (interaction.type === InteractionType.PING) {
    return pong();
  }

  try {
    switch (interaction.type) {
      case InteractionType.APPLICATION_COMMAND: {
        const { name } = commandOf(interaction);
        switch (name) {
          case "key": return await handleKey(interaction);
          case "hwid": return await handleHwid(interaction);
          case "status": return await handleStatus(interaction);
          case "hilfe": return await handleHelp(interaction);
          case "ping": return await handlePing(interaction);
          default: return handleUnknown(name);
        }
      }
      case InteractionType.MESSAGE_COMPONENT:
        return handleComponent(interaction);
      case InteractionType.MODAL_SUBMIT:
        return await handleModal(interaction);
      case InteractionType.APPLICATION_COMMAND_AUTOCOMPLETE:
        // Autovervollständigung wird nicht angeboten; eine leere Liste ist
        // die gültige Antwort und hält Discord bei Laune.
        return {
          statusCode: 200,
          headers: { "content-type": "application/json" },
          body: JSON.stringify({ type: 8, data: { choices: [] } }),
        };
      default:
        return fail(400, `Unbekannter Interaction-Typ ${interaction.type}.`);
    }
  } catch (error) {
    console.error("interactions: Fehler bei der Behandlung", {
      type: interaction?.type,
      user: userTag(interaction),
      error: error?.message ?? String(error),
    });
    return ephemeral({
      embeds: [
        embeds.error("Da ist etwas schiefgelaufen.", "Bitte melde es im Support-Channel."),
      ],
    });
  }
};

// Für die lokalen Tests: die Innerien ohne HTTP-Rahmen aufrufen.
exports._internals = {
  handleKey,
  handleHwid,
  handleStatus,
  handleHelp,
  handlePing,
  handleComponent,
  handleModal,
  issueAndReply,
  ASK_KEY,
};
