/**
 * Verbindung zum Mint-Server.
 *
 * Das ist der einzige Weg, auf dem diese Funktion einen Grant bekommt — und
 * es ist der einzige Grund, warum die Architektur überhaupt hält: der
 * private Ed25519-Seed liegt in der `.env` auf deinem Linux-Server, nicht in
 * den Netlify-Umgebungsvariablen.
 *
 * Würde die Funktion selbst signieren, müsste `DRAXO_SIGNING_KEY` als
 * Netlify-Secret gesetzt werden. Das ist keine „zusätzliche Sicherheit für
 * den Betrieb", sondern die Rückkehr zum alten Zustand: ein Geheimnis, das
 * in einer Umgebung liegt, die jeder deployende Account-Admin exportieren
 * kann. Genau das Leck, das die Umstellung auf asymmetrische Signaturen
 * beseitigt hat, wäre wieder da — nur schwerer zu finden.
 */

/** Wie lange der Mint-Server maximal antworten darf. */
const TIMEOUT_MS = 2500;

/**
 * Mint-Server ansprechen.
 *
 * Der Timeout liegt bewusst unter Discords 3-Sekunden-Frist für die erste
 * Antwort. Lieber eine ehrliche Fehlermeldung an den Nutzer als eine
 * stille Fehlermeldung, die Discord nach 3 Sekunden selbst erzeugt.
 */
async function call(endpoint, path, body) {
  const base = String(endpoint ?? "").replace(/\/+$/, "");
  if (!base) {
    throw Object.assign(new Error("DRAXO_MINT_URL ist nicht gesetzt."), { code: "no_url" });
  }
  const token = process.env.DRAXO_MINT_TOKEN;
  if (!token) {
    throw Object.assign(new Error("DRAXO_MINT_TOKEN ist nicht gesetzt."), {
      code: "no_token",
    });
  }

  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
  try {
    const response = await fetch(base + path, {
      method: "POST",
      headers: {
        "content-type": "application/json",
        authorization: `Bearer ${token}`,
      },
      body: JSON.stringify(body),
      signal: controller.signal,
    });

    const text = await response.text();
    let data;
    try {
      data = JSON.parse(text);
    } catch {
      throw Object.assign(
        new Error(`Antwort ist kein JSON (HTTP ${response.status}).`),
        { code: "bad_json" }
      );
    }

    if (!response.ok) {
      throw Object.assign(
        new Error(data.reason || `Mint-Server antwortet mit HTTP ${response.status}.`),
        { code: "http", status: response.status }
      );
    }
    return data;
  } catch (error) {
    if (error.name === "AbortError") {
      throw Object.assign(new Error("Der Mint-Server antwortet nicht rechtzeitig."), {
        code: "timeout",
      });
    }
    throw error;
  } finally {
    clearTimeout(timer);
  }
}

/**
 * HWID eines Nutzers nachsehen.
 *
 * Damit `/key` ohne Argument und `/status` überhaupt etwas anzeigen können:
 * im Gateway-Modus liest der Bot das aus seiner SQLite, hier aus dem
 * Mint-Server. Wird der Server nicht erreicht, ist das kein Fehler — dann
 * eben keine hinterlegte HWID.
 */
async function lookup(discordId) {
  try {
    return await call(process.env.DRAXO_MINT_URL, "/api/v1/hwid", {
      discord_id: discordId,
    });
  } catch {
    return { ok: false, hwid: null, last: null };
  }
}

/** Einen Grant ausstellen lassen. */
function issue(discordId, hwid) {
  return call(process.env.DRAXO_MINT_URL, "/api/v1/issue", {
    discord_id: discordId,
    hwid,
  });
}

/** HWID hinterlegen bzw. löschen. */
function setHwid(discordId, hwid) {
  return call(process.env.DRAXO_MINT_URL, "/api/v1/hwid", {
    discord_id: discordId,
    hwid,
    clear: !hwid,
  });
}

module.exports = { lookup, issue, setHwid, call };
