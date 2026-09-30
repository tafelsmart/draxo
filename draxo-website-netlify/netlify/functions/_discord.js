/**
 * Discord-Interaktionen: Grundlagen.
 *
 * Zwei Dinge stecken hier drin, die man leicht als Selbstverständlichkeit
 * übersieht und die dann still falsch sind:
 *
 *  1. Die Signaturprüfung. Discord schickt *jede* Anfrage an den
 *     Interactions-Endpoint, egal wer sie ausgelöst hat. Ohne Prüfung kann
 *     jeder im Internet `/key` aufrufen und sich Grants für fremre
 *     Discord-IDs ausstellen lassen.
 *
 *  2. Der private Schlüssel ist hier *nicht*. Diese Funktion ist Router und
 *     Formular, nicht Minter. Das Minten passiert über DRAXO_MINT_URL auf dem
 *     Server, wo der private Seed in der .env liegt.
 *
 * Warum die Signaturprüfung über `crypto.verify` und nicht über eine
 * Bibliothek: Netlify Functions bringen keine Dependencies mit, und der
 * Aufbau ist drei Zeilen. `verify(null, ...)` ist der Ed25519-Modus von
 * Node — bei null-Digest werden keine Argumente gehasht.
 */

const crypto = require("crypto");

/** Interaction-Typen (was Discord uns schickt). */
const InteractionType = {
  PING: 1,
  APPLICATION_COMMAND: 2,
  MESSAGE_COMPONENT: 3,
  APPLICATION_COMMAND_AUTOCOMPLETE: 4,
  MODAL_SUBMIT: 5,
};

/**
 * Interaction-Callback-Typen (was wir zurückschicken).
 *
 * Nur die, die hier tatsächlich vorkommen. `CHANNEL_MESSAGE_WITH_SOURCE`
 * statt der veralteten 3, und `MODAL` ist 9, nicht 7 — 7 ist UPDATE_MESSAGE.
 * Diese Zahlen stehen in der Discord-Doku an drei verschiedenen Stellen
 * und werden gern verwechselt.
 */
const CallbackType = {
  PONG: 1,
  CHANNEL_MESSAGE_WITH_SOURCE: 4,
  DEFERRED_CHANNEL_MESSAGE_WITH_SOURCE: 5,
  MODAL: 9,
};

/** Message-Flag für "nur für den Aufrufer sichtbar". */
const EPHEMERAL = 64;

/** Nichts antworten, was den Nutzer nicht sehen soll. */
const NO_MENTIONS = { parse: [] };

/**
 * Wie weit ein Zeitstempel von jetzt entfernt sein darf.
 *
 * Ohne diese Schranke ist jede gültige Signatur unbegrenzt wiederverwendbar:
 * ein Aufrufer, der einmal eine `/key`-Anfrage sieht, kann sie beliebig oft
 * wiederholen. Die Signatur bleibt gültig, weil sie gültig war — genau das
 * ist der Angriff, den nur der Zeitbezug aufhält.
 */
const MAX_TIMESTAMP_AGE_S = 300;

/**
 * Discord-Ed25519-Signatur prüfen.
 *
 * Signiert wird `timestamp + rawBody` — der Zeitstempel als *Text*, direkt
 * angehängt, ohne Trennzeichen. Body neu zu serialisieren ist der
 * häufigste Fehler und fällt nicht auf, weil die Prüfung dann schlicht
 * fehlschlägt und niemand den Grund sieht.
 *
 * @param {Buffer} rawBody  der unveränderte Request-Body
 * @param {string} signature  Hex aus X-Signature-Ed25519
 * @param {string} timestamp  Dezimaler String aus X-Signature-Timestamp
 * @param {string} publicKeyHex  64 Hex-Zeichen aus dem Developer Portal
 * @returns {boolean}
 */
function verifySignature(rawBody, signature, timestamp, publicKeyHex) {
  if (!signature || !timestamp || !publicKeyHex) return false;
  if (!/^\d{10,}$/.test(timestamp)) return false;
  if (!/^[0-9a-fA-F]{128}$/.test(signature)) return false;
  if (!/^[0-9a-fA-F]{64}$/.test(publicKeyHex)) return false;

  // Zu alt oder (nach pädagogischer Neigung) aus der Zukunft: beides heißt,
  // dass der Zeitstempel nicht zu dieser Anfrage gehört.
  const age = Math.abs(Date.now() / 1000 - Number(timestamp));
  if (age > MAX_TIMESTAMP_AGE_S) return false;

  try {
    // SPKI-Rahmen um den rohen 32-Byte-Schlüssel: SubjectPublicKeyInfo
    // für Ed25519 ist 12 Byte Prefix plus der Schlüssel. crypto.createPublicKey
    // nimmt nur DER, nicht rohe Bytes — deshalb der Rahmen.
    const prefix = Buffer.from("302a300506032b6570032100", "hex");
    const key = crypto.createPublicKey({
      key: Buffer.concat([prefix, Buffer.from(publicKeyHex, "hex")]),
      format: "der",
      type: "spki",
    });

    return crypto.verify(
      null,
      Buffer.concat([Buffer.from(timestamp, "ascii"), rawBody]),
      key,
      Buffer.from(signature, "hex")
    );
  } catch {
    // Ein kaputter Schlüssel darf nicht als "gültig" durchrutschen, und
    // darf auch keine Exception nach oben werfen: eine 500 bei der
    // URL-Validierung sieht für den Betreiber aus wie ein Discord-Problem.
    return false;
  }
}

/** Antwort mit einer Nachricht (und optional Embeds/Komponenten). */
function message(data) {
  return {
    statusCode: 200,
    headers: { "content-type": "application/json" },
    body: JSON.stringify({
      type: CallbackType.CHANNEL_MESSAGE_WITH_SOURCE,
      data: { ...NO_MENTIONS, ...data },
    }),
  };
}

/** Antwort, die nur der aufrufende Nutzer sieht. */
function ephemeral(data) {
  return message({ ...data, flags: EPHEMERAL });
}

/** PONG — die Antwort, mit der Discord die Endpoint-URL validiert. */
function pong() {
  return {
    statusCode: 200,
    headers: { "content-type": "application/json" },
    body: JSON.stringify({ type: CallbackType.PONG }),
  };
}

/** Modal als Antwort auf einen Knopfdruck. */
function modal(data) {
  return {
    statusCode: 200,
    headers: { "content-type": "application/json" },
    body: JSON.stringify({ type: CallbackType.MODAL, data }),
  };
}

/** Fehlerantwort an Discord. */
function fail(statusCode, reason) {
  return {
    statusCode,
    headers: { "content-type": "application/json" },
    body: JSON.stringify({ ok: false, reason }),
  };
}

/**
 * Nutzer-ID aus dem Interaction. In der Guild ist es `member.user.id`,
 * im DM `user.id` — und Discord liefert IDs als Strings, weil sie als
 * Snowflake sonst jede 2^53 sprengen.
 */
function userId(interaction) {
  const raw = interaction?.member?.user?.id ?? interaction?.user?.id;
  return raw ? String(raw) : null;
}

/** Anzeigename des Nutzers, mit Rückfall auf die ID. */
function userTag(interaction) {
  const user = interaction?.member?.user ?? interaction?.user;
  if (!user) return "unbekannt";
  return user.global_name || user.username || String(user.id);
}

/** Command-Name und Optionen aus einem APPLICATION_COMMAND-Payload. */
function commandOf(interaction) {
  const data = interaction?.data;
  if (!data) return { name: "", options: {} };
  const options = {};
  for (const option of data.options ?? []) {
    if (option && typeof option.name === "string") {
      options[option.name] = option.value;
    }
  }
  return { name: String(data.name ?? ""), options };
}

/** Werte aus einem Modal-Submit. */
function modalValues(interaction) {
  const values = {};
  for (const row of interaction?.data?.components ?? []) {
    for (const child of row?.components ?? []) {
      if (child?.custom_id && child.value !== undefined) {
        values[child.custom_id] = child.value;
      }
    }
  }
  return values;
}

module.exports = {
  EPHEMERAL,
  MAX_TIMESTAMP_AGE_S,
  InteractionType,
  CallbackType,
  verifySignature,
  message,
  ephemeral,
  pong,
  modal,
  fail,
  userId,
  userTag,
  commandOf,
  modalValues,
};
