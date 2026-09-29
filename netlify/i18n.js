/* ═══════════════════════════════════════════════════════════════════
   DRAXO CLIENT — Site Configuration
   ──────────────────────────────────────────────────────────────────
   ⚠️  HIER DEINE ECHTEN WERTE EINTRAGEN ⚠️

   1. discordInviteUrl:
      Discord-Server → Server-Name → Einstellungen → „Einladungen“ →
      „Link kopieren“ → z. B. https://discord.gg/dein-code

   2. discordGuildId (für den Live-Online-Zähler):
      Discord-Server → Server-Einstellungen → „Widget“ →
      „Server-Widget aktivieren“ einschalten → „Server-ID“ kopieren.
      Leer lassen = Online-Zähler bleibt ausgeblendet.
   ═══════════════════════════════════════════════════════════════════ */
window.DRAXO_CONFIG = {
    discordInviteUrl: 'https://discord.gg/2JTPwRmPZw',
    discordGuildId: '1534959905104986314'
};

// ──────────────────────────────────────────────────────────────────
// ✅ BEREIT FÜR DEPLOY: discordInviteUrl + discordGuildId sind gesetzt.
// ──────────────────────────────────────────────────────────────────


/* ═══════════════════════════════════════════════════════════════════
   DRAXO CLIENT — i18n Engine (EN / DE / ES / FR)
   ──────────────────────────────────────────────────────────────────
   1. Auto-detect: manual choice (localStorage) > IP geolocation
      (country -> language) > browser language.
   2. Everything with [data-i18n] gets translated. Dynamic parts
      re-render via the window.i18nOnChange(lang) hook.
   3. <select id="langSelect"> in the page lets users override.
   ═══════════════════════════════════════════════════════════════════ */
(function () {
  'use strict';

  var LANGS = ['en', 'de', 'es', 'fr'];
  var SAVED_KEY = 'draxo_lang';

  /* ISO 3166-1 country code -> language. Everything not listed
     falls back to the browser language (English first). */
  var COUNTRY_LANG = {
    DE: 'de', AT: 'de', CH: 'de', LI: 'de', LU: 'de',
    ES: 'es', MX: 'es', AR: 'es', CO: 'es', CL: 'es', PE: 'es',
    VE: 'es', EC: 'es', BO: 'es', UY: 'es', PY: 'es', GT: 'es',
    HN: 'es', SV: 'es', NI: 'es', CR: 'es', PA: 'es', DO: 'es', CU: 'es',
    FR: 'fr', BE: 'fr', MC: 'fr'
  };

  var STRINGS = {

  /* ─────────────────────────── ENGLISH ─────────────────────────── */
  en: {
    title: 'Draxo Client — Premium Minecraft Utility',
    meta_desc: 'Draxo Client — the ultimate Minecraft client. Undetected, premium features, modern GUI. For 1.17 to 26.x.',

    nav_features: 'Features', nav_modules: 'Modules', nav_versions: 'Versions',
    nav_download: 'Download', nav_faq: 'FAQ', nav_download_btn: 'Download',

    hero_badge: '⚡ Undetected & Premium',
    hero_h1: 'The <span class="accent">Ultimate</span><br>Minecraft Client',
    btn_download: '⬇ Download', btn_key: '🔑 Get Key',
    meta_modules: 'Modules', meta_uptime: 'Uptime', meta_versions: 'Versions',
    meta_gui: 'GUI', meta_support: 'Support',

    tw: [
      '99.9% Undetectable — optimized for Watchdog, GrimAC & Vulcan.',
      '60 FPS Premium GUI with theme engine and HUD editor.',
      'Multi-version support: 1.17 to 26.x — 33 versions.',
      '8-layer HWID license system — uncrackable protection.',
      '49+ modules with Simple/Advanced modes and presets.'
    ],

    feat_label: 'Why Draxo', feat_title: 'Premium Features',
    feat_subtitle: 'Every detail engineered for the highest quality — from the anti-cheat engine to the GUI.',
    feat1_t: 'Undetectable Engine',
    feat1_d: 'Multi-layered anti-cheat bypass technology with packet scheduling, sine-wave jitter rotations and a randomization engine. Optimized for Watchdog, GrimAC, Vulcan & more.',
    feat2_t: 'Premium GUI',
    feat2_d: 'Discord-style design with smooth 60 FPS animations, theme engine, customizable colors, blur/glow effects and a full HUD editor.',
    feat3_t: '49+ Modules',
    feat3_d: 'KillAura, Scaffold, ESP, Tracers, Nuker, Speed, Fly, Reach, Velocity — all with Simple/Advanced modes and custom presets.',
    feat4_t: 'HWID License System',
    feat4_d: 'Uncrackable 8-layer protection with poly-XOR encryption, 3-slot consensus and runtime code integrity checks.',
    feat5_t: 'Multi-Version',
    feat5_d: 'Supports every Minecraft version from 1.17 to 26.x. Automatic mapping generation — no manual updates needed.',
    feat6_t: 'Performance',
    feat6_d: 'Optimized for minimal CPU/RAM usage. Lazy rendering, object pooling and efficient data structures — zero FPS loss.',

    mod_label: 'Modules', mod_title: 'Everything you need',
    mod_subtitle: 'From Combat to Movement to Visuals — every module with Simple/Advanced mode and presets.',
    mod_cat_combat: '⚔ Combat', mod_cat_movement: '🏃 Movement',
    mod_cat_visuals: '👁 Visuals', mod_cat_utility: '🧰 Utility',

    ver_label: 'Compatibility', ver_title: '33 supported versions',
    ver_subtitle: 'From 1.17 to 26.x — automatic mapping generation for every version.',
    ver_latest: '26.2 (latest)',

    dl_label: 'Get Started', dl_title: 'Download & Key',
    dl_subtitle: 'Download the launcher and get your 24h key via Linkvertise.',
    dl_btn_exe: '⬇ DraxoLauncher.exe', dl_btn_key: '🔑 Get Key',
    dl_card_title: 'How it works',
    dl_steps: '<strong>1.</strong> Download DraxoLauncher.exe and place it in your Draxo Client folder.<br><strong>2.</strong> Start the launcher — it finds Python automatically and installs dependencies.<br><strong>3.</strong> Copy your HWID from the license card (📋 Copy).<br><strong>4.</strong> Click <a href="keygen.html" style="color:var(--accent);">Get Key</a> — complete 3 Linkvertise ads.<br><strong>5.</strong> Enter the key in the Draxo menu (CONFIG → LICENSE → Activate).<br><strong>6.</strong> Choose your Minecraft version → INJECT → all modules unlocked!',
    dl_meta: '🔒 24h keys are bound to your HWID • 3 ads = 1 key',

    test_label: 'Community', test_title: 'What players say',
    test_subtitle: 'Draxo is used daily by hundreds of players on Minemen, Flamefrags & Hypixel.',
    test1_q: "\"The best client I've ever used. The KillAura is so smooth that even experienced staff don't notice. And the GUI looks like Discord — simply premium.\"",
    test1_name: 'Minemen Player', test1_role: 'Playing since v1.0',
    test2_q: "\"Finally a client that doesn't get instantly kicked on Flamefrags. The Watchdog bypass is no empty promise — the sine-wave jitter rotation is genius.\"",
    test2_name: 'Flamefrags Player', test2_role: 'Diamond Division',
    test3_q: "\"Multi-version support is a game changer. I play 1.8 on Minemen and 1.21 on Hypixel with the same client — everything works out of the box.\"",
    test3_name: 'Multi-Version Player', test3_role: '1.8 & 1.21 Main',

    faq_label: 'FAQ', faq_title: 'Frequently asked questions',
    faq_subtitle: 'Everything you need to know about Draxo.',
    faq1_q: 'Is Draxo really undetected?',
    faq1_a: 'Draxo uses a multi-layered anti-cheat engine with packet scheduling, sine-wave jitter rotations, a randomization engine and position-based micro offsets. We regularly test against Watchdog (Hypixel), GrimAC, Vulcan and Spartan. <strong>No cheat is 100% undetectable</strong> — use Draxo responsibly and avoid obvious rage settings on servers with active staff.',
    faq2_q: 'How do I get a key?',
    faq2_a: 'Visit the <a href="keygen.html" style="color:var(--accent);">keygen page</a>, enter your HWID (copy it from the launcher: 📋 Copy), complete 3 short Linkvertise ads and receive your 24h key. The key is bound to your hardware (HWID-locked).',
    faq3_q: 'Which Minecraft versions are supported?',
    faq3_a: "Draxo supports <strong>all versions from 1.17 to 26.x</strong>. The built-in mapping generator automatically creates the correct mappings for your version. That's 33+ versions in total.",
    faq4_q: 'How does the installation work?',
    faq4_a: 'Download DraxoLauncher.exe and place it in any folder. The launcher automatically finds Python, installs dependencies and injects the DLL into Minecraft. No complicated configuration needed.',
    faq5_q: 'Can I play on Hypixel with my main account?',
    faq5_a: '<strong>We strongly advise to never cheat on your main account.</strong> Use an alt account or test on servers like Minemen.club or Flamefrags.gg that are more tolerant towards clients. Hypixel bans permanently on detection.',
    faq6_q: 'What is the Simple/Advanced mode?',
    faq6_a: 'In <strong>Simple mode</strong>, every module has only 2 presets: Legit (safe, subtle) and Rage (maximum effect). Perfect for beginners. <strong>Advanced mode</strong> gives you full control over timing, range, jitter and all other parameters — for experienced users.',
    faq7_q: "My key doesn't work — what should I do?",
    faq7_a: 'The most common cause is a <strong>wrong HWID</strong>: the key is hard-bound to the HWID of your machine, which you see in the Draxo launcher (📋 Copy). Always use the exact same HWID in the keygen — not one from an old draxo_config.ini or another PC. Other causes: the key is <strong>expired</strong> (24h), your <strong>system clock</strong> is wrong, or you generated a second key for the same HWID (rate limit). First check that the HWID matches exactly (32 characters, uppercase). If activation still fails, check the <strong>support box</strong> on the keygen page — it links to our Discord, where we fix HWID issues within minutes.',

    footer_home: 'Home', footer_features: 'Features', footer_download: 'Download',
    footer_keygen: 'Keygen', footer_discord: 'Discord', footer_github: 'GitHub',
    footer_copyright: '© 2026 Draxo Client · batotomato · All rights reserved.',
    footer_disclaimer: 'Draxo is a third-party utility and is not affiliated with Mojang or Microsoft.',

    kg_title: 'Draxo Client — License Key',
    kg_meta_desc: 'Get your 24-hour Draxo Client license key. HWID-bound, undetected, premium Minecraft utility.',
    kg_subtitle: 'License Key Generator',
    kg_hwid_label: 'Your HWID',
    kg_hwid_placeholder: 'Paste your 32-character HWID...',
    kg_guide_summary: 'How to find your HWID',
    kg_guide_content: '<strong>Option 1 — Draxo Launcher</strong><br>Start the DraxoLauncher.exe. In the license card between the control panel and console, you\'ll see your HWID. Click <strong>📋 Copy</strong> and paste it here.<br><br><strong>Option 2 — After first injection</strong><br>Inject Draxo into Minecraft once. Your HWID is then saved in <code>build/vanilla/Release/draxo_config.ini</code>. Open that file and look for <code>License.hwid=...</code> — that\'s your 32-character HWID.<br><br><strong>Important</strong><br>The key is HWID-locked. Use the SAME HWID as in your Draxo Client — otherwise the key won\'t work.',
    kg_ads_title: 'Complete ALL 3 Ads to Get Your Key',
    kg_ads_warn: '⚠️ You MUST open all 3 ads above. The button unlocks after all 3 are clicked.',
    kg_ads_warn_sub: 'Each ad opens in a new tab. Close it and come back here for the next one.',
    kg_btn_lock_pre: 'Complete all 3 ads (', kg_btn_lock_post: ')',
    kg_btn_generate: 'Generate Key', kg_btn_generating: 'Generating…',
    kg_valid: 'Valid HWID',
    kg_need32: 'Need 32 characters (currently {n})',
    kg_result_title: '✅ Your 24-Hour License Key',
    kg_copy: '📋 Copy Key', kg_copied: '✓ Copied!',
    kg_bound: 'HWID-bound — only works on YOUR machine',
    kg_expire: '⏰ Expires: {d} UTC',
    kg_activate_title: 'How to activate:',
    kg_act1: 'Open the Draxo Launcher or inject the DLL into Minecraft',
    kg_act2: 'Open the menu (<code>RSHIFT</code> or <code>INSERT</code>)',
    kg_act3: 'Go to <code>CONFIG</code> tab → <code>LICENSE</code> section',
    kg_act4: 'Paste the key and click <strong>Activate</strong>',
    kg_error_prefix: '❌ Error: ', kg_error_default: 'Key generation failed',
    kg_need_ads: 'You must complete all {n} Linkvertise ads first. Open each ad and return here.',
    kg_rate_limit: 'Rate limit: 1 key per HWID per 24 hours.',
    kg_checkpoint_title: 'Invalid or Expired Link',
    kg_checkpoint_msg: 'This ad link has already been used or is invalid.<br>Return to the keygen page and try again.',
    kg_checkpoint_back: 'Go to Keygen',
    kg_ad_label_pre: 'Ad ', kg_ad_label_post: '',
    kg_open_ad_pre: 'Open Ad ', kg_open_ad_post: '',
    kg_ad_done: '✓ Ad Completed',
    kg_badge_done: 'Done', kg_badge_lv: 'Linkvertise',
    kg_back_home: '← Back to Draxo Homepage',
    kg_no_links: 'No Linkvertise links configured.',
    kg_support_title: 'Activation problems?',
    kg_support_text: "The key doesn't work or your HWID is rejected? Join our Discord — we'll solve your issue in minutes.",
    kg_support_tip: '💡 Most common mistake: HWID copied incorrectly — always copy from the launcher (📋 Copy), not from draxo_config.ini.',
    kg_support_tip_link: 'Show guide',

    btn_discord: '💬 Discord',
    discord_join: 'Join Discord',
    discord_online: '🟢 {n} online',
    discord_cta_title: 'Join the Community',
    discord_cta_text: 'Updates, support and giveaways — meet the Draxo community on Discord.',

    nav_changelog: 'Changelog', footer_changelog: 'Changelog',
    chg_badge: 'Release History',
    chg_title: 'Changelog',
    chg_subtitle: 'Every version of Draxo — what changed, what improved, what got fixed.',
    chg_current: 'Current version',
    chg_loading: 'Loading releases…',
    chg_error: 'Could not load version.json.',
    chg_retry: 'Retry',
    chg_badge_latest: 'Latest',
    chg_date: 'Released',
    chg_highlights: 'Highlights',
    chg_notes: 'Release Notes',
    chg_no_entries: 'No releases yet.',
    nav_legal: 'Legal', footer_legal: 'Legal',
    legal_title: 'Draxo Client — Legal',
    legal_meta_desc: 'Disclaimer, Terms of Service and Privacy Policy for the Draxo Minecraft Client.',
    legal_heading: 'Legal',
    legal_subtitle: 'Disclaimer, Terms of Service and Privacy Policy.',
    legal_disclaimer_title: 'Disclaimer',
    legal_disclaimer_text: '<p>Draxo Client ("Draxo") is a third-party utility for Minecraft. Draxo is <strong>not affiliated with, endorsed by, or connected to Mojang Studios, Microsoft Corporation, or any Minecraft server</strong>.</p><p>Draxo is provided "as is" without warranty of any kind. The developers of Draxo assume <strong>no liability</strong> for any consequences arising from its use, including but not limited to account bans, server bans, or other punitive actions by server administrators or anti-cheat systems.</p><p>Use of Draxo on multiplayer servers may violate those servers\' terms of service. <strong>You are solely responsible</strong> for ensuring your use of Draxo complies with all applicable rules and laws.</p>',
    legal_terms_title: 'Terms of Service',
    legal_terms_text: '<p>By using Draxo Client, you agree to:</p><ol><li>Draxo is distributed under a <strong>24-hour license key system</strong>. Each key is bound to a unique Hardware ID (HWID) and may not be transferred, shared, or resold.</li><li>License keys are obtained by completing Linkvertise advertisements. <strong>No monetary payment</strong> is required or accepted.</li><li><strong>No refunds</strong> are provided, as no monetary transactions take place.</li><li>You may not <strong>modify, reverse-engineer, decompile, or redistribute</strong> Draxo Client without explicit written permission.</li><li>The Draxo team reserves the right to <strong>revoke license keys</strong> at any time for any reason, including abuse or violation of these terms.</li><li>You must comply with the rules of any Minecraft server you play on while using Draxo.</li></ol>',
    legal_privacy_title: 'Privacy Policy',
    legal_privacy_text: '<p>Draxo Client collects:</p><ul><li><strong>Hardware ID (HWID)</strong>: A hash of your CPU and motherboard serial numbers, used exclusively for license key binding. This data never leaves your machine in identifiable form.</li><li><strong>localStorage Data</strong>: The keygen page stores ad click progress and temporary checkpoint tokens in your browser\'s localStorage. This data stays on your device.</li></ul><p><strong>What Draxo does NOT collect:</strong></p><ul><li>No personal information (name, email, IP address)</li><li>No Minecraft account credentials</li><li>No gameplay or chat data</li><li>No analytics or tracking cookies</li></ul><p>No data is shared with or sold to third parties. Linkvertise operates independently — please refer to their privacy policy for details.</p><p>Questions? Join our <a href="https://discord.gg/2JTPwRmPZw" style="color:var(--accent);">Discord server</a>.</p>'
  },

  /* ─────────────────────────── DEUTSCH ─────────────────────────── */
  de: {
    title: 'Draxo Client — Premium Minecraft-Utility',
    meta_desc: 'Draxo Client — Der ultimative Minecraft Client. Undetected, premium Features, moderne GUI. Für 1.17 bis 26.x.',

    nav_features: 'Features', nav_modules: 'Module', nav_versions: 'Versionen',
    nav_download: 'Download', nav_faq: 'FAQ', nav_download_btn: 'Download',

    hero_badge: '⚡ Undetected & Premium',
    hero_h1: 'Der <span class="accent">ultimative</span><br>Minecraft Client',
    btn_download: '⬇ Download', btn_key: '🔑 Key holen',
    meta_modules: 'Module', meta_uptime: 'Uptime', meta_versions: 'Versionen',
    meta_gui: 'GUI', meta_support: 'Support',

    tw: [
      '99.9% Undetectable — optimiert für Watchdog, GrimAC & Vulcan.',
      '60 FPS Premium GUI mit Theme-Engine und HUD-Editor.',
      'Multi-Version Support: 1.17 bis 26.x — 33 Versionen.',
      '8-Layer HWID-Lizenzsystem — uncrackbar geschützt.',
      '49+ Module mit Simple/Advanced-Modus und Presets.'
    ],

    feat_label: 'Warum Draxo', feat_title: 'Premium-Features',
    feat_subtitle: 'Jedes Detail wurde für höchste Qualität entwickelt — von der Anti-Cheat-Engine bis zur GUI.',
    feat1_t: 'Undetectable Engine',
    feat1_d: 'Mehrschichtige Anti-Cheat-Bypass-Technologie mit Packet-Scheduling, Sinus-Jitter-Rotationen und Randomisierungs-Engine. Optimiert für Watchdog, GrimAC, Vulcan & mehr.',
    feat2_t: 'Premium GUI',
    feat2_d: 'Discord-ähnliches Design mit flüssigen 60-FPS-Animationen, Theme-Engine, anpassbaren Farben, Blur/Glow-Effekten und vollständigem HUD-Editor.',
    feat3_t: '49+ Module',
    feat3_d: 'KillAura, Scaffold, ESP, Tracers, Nuker, Speed, Fly, Reach, Velocity — alle mit Simple/Advanced-Modus und individuellen Presets.',
    feat4_t: 'HWID-Lizenzsystem',
    feat4_d: 'Uncrackbares 8-Layer-Schutzsystem mit Poly-XOR-Verschlüsselung, 3-Slot-Consensus und Runtime-Code-Integritätsprüfung.',
    feat5_t: 'Multi-Version',
    feat5_d: 'Unterstützt alle Minecraft-Versionen von 1.17 bis 26.x. Automatische Mapping-Generierung — kein manuelles Update nötig.',
    feat6_t: 'Performance',
    feat6_d: 'Optimiert für minimale CPU-/RAM-Auslastung. Lazy Rendering, Objekt-Pooling und effiziente Datenstrukturen — kein FPS-Verlust.',

    mod_label: 'Module', mod_title: 'Alles was du brauchst',
    mod_subtitle: 'Von Combat über Movement bis Visuals — jedes Modul mit Simple/Advanced-Modus und Presets.',
    mod_cat_combat: '⚔ Combat', mod_cat_movement: '🏃 Movement',
    mod_cat_visuals: '👁 Visuals', mod_cat_utility: '🧰 Utility',

    ver_label: 'Kompatibilität', ver_title: '33 unterstützte Versionen',
    ver_subtitle: 'Von 1.17 bis 26.x — automatische Mapping-Generierung für jede Version.',
    ver_latest: '26.2 (neueste)',

    dl_label: 'Loslegen', dl_title: 'Download & Key',
    dl_subtitle: 'Lade den Launcher herunter und hole dir deinen 24h-Key über Linkvertise.',
    dl_btn_exe: '⬇ DraxoLauncher.exe', dl_btn_key: '🔑 Key holen',
    dl_card_title: 'So funktioniert\u2019s',
    dl_steps: '<strong>1.</strong> DraxoLauncher.exe herunterladen und in deinen Draxo-Client-Ordner legen.<br><strong>2.</strong> Launcher starten — er sucht automatisch Python und installiert Abhängigkeiten.<br><strong>3.</strong> HWID aus der Lizenz-Karte kopieren (📋 Copy).<br><strong>4.</strong> Auf <a href="keygen.html" style="color:var(--accent);">Key holen</a> klicken — 3 Linkvertise-Ads durchlaufen.<br><strong>5.</strong> Key im Draxo-Menü eingeben (CONFIG → LICENSE → Activate).<br><strong>6.</strong> Minecraft-Version wählen → INJECT → alle Module freigeschaltet!',
    dl_meta: '🔒 24h-Keys sind an deine HWID gebunden • 3 Werbungen = 1 Key',

    test_label: 'Community', test_title: 'Was Spieler sagen',
    test_subtitle: 'Draxo wird täglich von Hunderten Spielern auf Minemen, Flamefrags & Hypixel eingesetzt.',
    test1_q: '"Der beste Client, den ich je benutzt habe. Die KillAura ist so smooth, dass selbst erfahrene Staffs nichts merken. Und die GUI sieht aus wie Discord — einfach premium."',
    test1_name: 'Minemen-Spieler', test1_role: 'Seit v1.0 dabei',
    test2_q: '"Endlich ein Client, der auf Flamefrags nicht instant kicked. Der Watchdog-Bypass ist kein leeres Versprechen — die Sinus-Jitter-Rotation ist genial."',
    test2_name: 'Flamefrags-Spieler', test2_role: 'Diamond Division',
    test3_q: '"Die Multi-Version-Unterstützung ist ein Gamechanger. Ich spiele 1.8 auf Minemen und 1.21 auf Hypixel mit demselben Client — alles funktioniert out of the box."',
    test3_name: 'Multi-Version-Spieler', test3_role: '1.8 & 1.21 Main',

    faq_label: 'FAQ', faq_title: 'Häufige Fragen',
    faq_subtitle: 'Alles was du über Draxo wissen musst.',
    faq1_q: 'Ist Draxo wirklich undetected?',
    faq1_a: 'Draxo verwendet eine mehrschichtige Anti-Cheat-Engine mit Packet-Scheduling, Sinus-Jitter-Rotationen, Randomisierungs-Engine und positionsbasierten Micro-Offsets. Wir testen regelmäßig gegen Watchdog (Hypixel), GrimAC, Vulcan und Spartan. <strong>Kein Cheat ist zu 100% undetectable</strong> — benutze Draxo verantwortungsvoll und vermeide offensichtliche Rage-Einstellungen auf Servern mit aktivem Staff.',
    faq2_q: 'Wie bekomme ich einen Key?',
    faq2_a: 'Besuche die <a href="keygen.html" style="color:var(--accent);">Keygen-Seite</a>, gib deine HWID ein (aus dem Launcher kopieren: 📋 Copy), durchlaufe 3 kurze Linkvertise-Werbungen und erhalte deinen 24h-Key. Der Key ist an deine Hardware gebunden (HWID-locked).',
    faq3_q: 'Welche Minecraft-Versionen werden unterstützt?',
    faq3_a: 'Draxo unterstützt <strong>alle Versionen von 1.17 bis 26.x</strong>. Der eingebaute Mapping-Generator erstellt automatisch die passenden Mappings für deine Version. Insgesamt sind das 33+ Versionen.',
    faq4_q: 'Wie funktioniert die Installation?',
    faq4_a: 'Lade die DraxoLauncher.exe herunter und lege sie in einen beliebigen Ordner. Der Launcher sucht automatisch nach Python, installiert Abhängigkeiten und injiziert die DLL in Minecraft. Keine komplizierte Konfiguration nötig.',
    faq5_q: 'Kann ich auf Hypixel mit meinem Main-Account spielen?',
    faq5_a: '<strong>Wir empfehlen dringend, niemals mit deinem Main-Account zu cheaten.</strong> Nutze einen Alt-Account oder teste auf Servern wie Minemen.club oder Flamefrags.gg, die toleranter gegenüber Clients sind. Hypixel bannt bei Detection permanent.',
    faq6_q: 'Was ist der Simple/Advanced-Modus?',
    faq6_a: 'Im <strong>Simple-Modus</strong> hat jedes Modul nur 2 Presets: Legit (safe, unauffällig) und Rage (maximale Wirkung). Perfekt für Einsteiger. Der <strong>Advanced-Modus</strong> gibt dir volle Kontrolle über Timing, Reichweite, Jitter und alle anderen Parameter — für erfahrene Nutzer.',
    faq7_q: 'Mein Key funktioniert nicht — was tun?',
    faq7_a: 'Die häufigste Ursache ist eine <strong>falsche HWID</strong>: Der Key ist fest an die HWID deines Rechners gebunden, die du im Draxo Launcher siehst (📋 Copy). Verwende unbedingt dieselbe HWID im Keygen — nicht die aus einer alten draxo_config.ini oder von einem anderen Rechner. Andere Ursachen: Der Key ist <strong>abgelaufen</strong> (24h), die <strong>Systemzeit</strong> stimmt nicht, oder du hast für dieselbe HWID einen zweiten Key generiert (Rate-Limit). Prüfe zuerst, ob die HWID exakt übereinstimmt (32 Zeichen, Großbuchstaben). Klappt die Aktivierung immer noch nicht, schau in die <strong>Support-Box</strong> auf der Keygen-Seite — dort findest du unseren Discord, wo wir HWID-Probleme in Minuten lösen.',

    footer_home: 'Home', footer_features: 'Features', footer_download: 'Download',
    footer_keygen: 'Keygen', footer_discord: 'Discord', footer_github: 'GitHub',
    footer_copyright: '© 2026 Draxo Client · batotomato · Alle Rechte vorbehalten.',
    footer_disclaimer: 'Draxo ist ein Drittanbieter-Utility und nicht mit Mojang oder Microsoft verbunden.',

    kg_title: 'Draxo Client — Lizenz-Key',
    kg_meta_desc: 'Hol dir deinen 24-Stunden-Draxo-Client-Lizenz-Key. HWID-gebunden, undetected, Premium-Minecraft-Utility.',
    kg_subtitle: 'Lizenz-Key-Generator',
    kg_hwid_label: 'Deine HWID',
    kg_hwid_placeholder: 'Füge deine 32-stellige HWID ein...',
    kg_guide_summary: 'So findest du deine HWID',
    kg_guide_content: '<strong>Option 1 — Draxo Launcher</strong><br>Starte die DraxoLauncher.exe. In der Lizenz-Karte zwischen Steuerung und Konsole siehst du deine HWID. Klicke auf <strong>📋 Copy</strong> und füge sie hier ein.<br><br><strong>Option 2 — Nach der ersten Injection</strong><br>Injiziere Draxo einmal in Minecraft. Deine HWID wird dann in <code>build/vanilla/Release/draxo_config.ini</code> gespeichert. Öffne die Datei und suche nach <code>License.hwid=...</code> — das ist deine 32-stellige HWID.<br><br><strong>Wichtig</strong><br>Der Key ist HWID-gebunden. Verwende die GLEICHE HWID wie in deinem Draxo Client — sonst funktioniert der Key nicht.',
    kg_ads_title: 'Schließe ALLE 3 Werbungen ab, um deinen Key zu erhalten',
    kg_ads_warn: '⚠️ Du MUSST alle 3 Werbungen oben öffnen. Der Button wird erst nach allen 3 Klicks freigeschaltet.',
    kg_ads_warn_sub: 'Jede Werbung öffnet sich in einem neuen Tab. Schließe ihn und komm für die nächste hierher zurück.',
    kg_btn_lock_pre: 'Schließe alle 3 Werbungen ab (', kg_btn_lock_post: ')',
    kg_btn_generate: 'Key generieren', kg_btn_generating: 'Generiere…',
    kg_valid: 'Gültige HWID',
    kg_need32: '32 Zeichen benötigt (aktuell {n})',
    kg_result_title: '✅ Dein 24-Stunden-Lizenz-Key',
    kg_copy: '📋 Key kopieren', kg_copied: '✓ Kopiert!',
    kg_bound: 'HWID-gebunden — funktioniert nur auf DEINEM Rechner',
    kg_expire: '⏰ Läuft ab: {d} UTC',
    kg_activate_title: 'So aktivierst du:',
    kg_act1: 'Öffne den Draxo Launcher oder injiziere die DLL in Minecraft',
    kg_act2: 'Öffne das Menü (<code>RSHIFT</code> oder <code>INSERT</code>)',
    kg_act3: 'Gehe zu <code>CONFIG</code>-Tab → <code>LICENSE</code>-Bereich',
    kg_act4: 'Füge den Key ein und klicke auf <strong>Activate</strong>',
    kg_error_prefix: '❌ Fehler: ', kg_error_default: 'Key-Generierung fehlgeschlagen',
    kg_need_ads: 'Du musst zuerst alle {n} Linkvertise-Werbungen abschließen. Öffne jede Werbung und kehre hierher zurück.',
    kg_rate_limit: 'Limit: 1 Key pro HWID alle 24 Stunden.',
    kg_checkpoint_title: 'Ungültiger oder abgelaufener Link',
    kg_checkpoint_msg: 'Dieser Werbe-Link wurde bereits verwendet oder ist ungültig.<br>Kehre zur Keygen-Seite zurück und versuche es erneut.',
    kg_checkpoint_back: 'Zum Keygen',
    kg_ad_label_pre: 'Werbung ', kg_ad_label_post: '',
    kg_open_ad_pre: 'Werbung ', kg_open_ad_post: ' öffnen',
    kg_ad_done: '✓ Werbung abgeschlossen',
    kg_badge_done: 'Fertig', kg_badge_lv: 'Linkvertise',
    kg_back_home: '← Zurück zur Draxo-Startseite',
    kg_no_links: 'Keine Linkvertise-Links konfiguriert.',
    kg_support_title: 'Aktivierungsprobleme?',
    kg_support_text: "Der Key funktioniert nicht oder deine HWID wird abgelehnt? Tritt unserem Discord bei — wir lösen dein Problem in Minuten.",
    kg_support_tip: '💡 Häufigster Fehler: HWID falsch kopiert — immer aus dem Launcher kopieren (📋 Copy), nicht aus draxo_config.ini.',
    kg_support_tip_link: 'Anleitung zeigen',

    btn_discord: '💬 Discord',
    discord_join: 'Tritt Discord bei',
    discord_online: '🟢 {n} online',
    discord_cta_title: 'Werde Teil der Community',
    discord_cta_text: 'Updates, Support und Giveaways — triff die Draxo-Community auf Discord.',

    nav_changelog: 'Changelog', footer_changelog: 'Changelog',
    chg_badge: 'Versionsverlauf',
    chg_title: 'Changelog',
    chg_subtitle: 'Jede Draxo-Version — was sich geändert, verbessert und gefixt wurde.',
    chg_current: 'Aktuelle Version',
    chg_loading: 'Lade Releases…',
    chg_error: 'version.json konnte nicht geladen werden.',
    chg_retry: 'Erneut versuchen',
    chg_badge_latest: 'Neueste',
    chg_date: 'Veröffentlicht',
    chg_highlights: 'Highlights',
    chg_notes: 'Release-Notes',
    chg_no_entries: 'Noch keine Releases.',
    nav_legal: 'Rechtliches', footer_legal: 'Rechtliches',
    legal_title: 'Draxo Client — Rechtliches',
    legal_meta_desc: 'Haftungsausschluss, Nutzungsbedingungen und Datenschutzerklärung für den Draxo Minecraft Client.',
    legal_heading: 'Rechtliches',
    legal_subtitle: 'Haftungsausschluss, Nutzungsbedingungen und Datenschutzerklärung.',
    legal_disclaimer_title: 'Haftungsausschluss (Disclaimer)',
    legal_disclaimer_text: '<p>Draxo Client ("Draxo") ist ein Drittanbieter-Dienstprogramm für Minecraft. Draxo ist <strong>nicht mit Mojang Studios, Microsoft Corporation oder irgendeinem Minecraft-Server verbunden, unterstützt oder assoziiert</strong>.</p><p>Draxo wird ohne jegliche Gewährleistung bereitgestellt. Die Entwickler von Draxo übernehmen <strong>keine Haftung</strong> für etwaige Folgen der Nutzung, einschließlich Account-Sperren, Server-Ausschlüsse oder andere Sanktionen durch Server-Administratoren oder Anti-Cheat-Systeme.</p><p>Die Nutzung von Draxo auf Multiplayer-Servern kann gegen die Nutzungsbedingungen dieser Server verstoßen. <strong>Du bist allein verantwortlich</strong> dafür, dass deine Nutzung von Draxo allen geltenden Regeln und Gesetzen entspricht.</p>',
    legal_terms_title: 'Nutzungsbedingungen (Terms of Service)',
    legal_terms_text: '<p>Durch die Nutzung von Draxo Client stimmst du folgenden Bedingungen zu:</p><ol><li>Draxo wird über ein <strong>24-Stunden-Lizenzschlüssel-System</strong> vertrieben. Jeder Schlüssel ist an eine einmalige Hardware-ID (HWID) gebunden und darf nicht übertragen, geteilt oder weiterverkauft werden.</li><li>Lizenzschlüssel werden durch das Abschließen von Linkvertise-Werbungen erworben. Es ist <strong>keine Geldzahlung</strong> erforderlich oder möglich.</li><li>Es werden <strong>keine Rückerstattungen</strong> gewährt, da keine Geldtransaktionen stattfinden.</li><li>Du darfst Draxo Client ohne ausdrückliche schriftliche Genehmigung <strong>nicht modifizieren, zurückentwickeln, dekompilieren oder weiterverbreiten</strong>.</li><li>Das Draxo-Team behält sich das Recht vor, Lizenzschlüssel <strong>jederzeit aus beliebigem Grund zu widerrufen</strong>, einschließlich Missbrauch oder Verstoß gegen diese Bedingungen.</li><li>Du musst die Regeln jedes Minecraft-Servers einhalten, auf dem du Draxo verwendest.</li></ol>',
    legal_privacy_title: 'Datenschutzerklärung (Privacy Policy)',
    legal_privacy_text: '<p>Draxo Client erfasst folgende Daten:</p><ul><li><strong>Hardware-ID (HWID)</strong>: Ein Hash aus deiner CPU- und Motherboard-Seriennummer, ausschließlich zur Bindung des Lizenzschlüssels. Diese Daten verlassen deinen Rechner nie in identifizierbarer Form.</li><li><strong>localStorage-Daten</strong>: Die Keygen-Webseite speichert den Fortschritt der Werbe-Klicks und temporäre Checkpoint-Tokens im localStorage deines Browsers. Diese Daten bleiben auf deinem Gerät.</li></ul><p><strong>Was Draxo NICHT erfasst:</strong></p><ul><li>Keine persönlichen Daten (Name, E-Mail, IP-Adresse)</li><li>Keine Minecraft-Accountdaten</li><li>Keine Spieldaten oder Chat-Protokolle</li><li>Keine Analyse- oder Tracking-Cookies</li></ul><p>Es werden keine Daten an Dritte weitergegeben oder verkauft. Linkvertise arbeitet unabhängig — bitte beachte die Datenschutzerklärung von Linkvertise für Details.</p><p>Fragen? Tritt unserem <a href="https://discord.gg/2JTPwRmPZw" style="color:var(--accent);">Discord-Server</a> bei.</p>'
  },

  /* ─────────────────────────── ESPAÑOL ─────────────────────────── */
  es: {
    title: 'Draxo Client — Utilidad Premium de Minecraft',
    meta_desc: 'Draxo Client — el client definitivo de Minecraft. Indetectable, funciones premium, GUI moderna. Para 1.17 a 26.x.',

    nav_features: 'Características', nav_modules: 'Módulos', nav_versions: 'Versiones',
    nav_download: 'Descargar', nav_faq: 'FAQ', nav_download_btn: 'Descargar',

    hero_badge: '⚡ Indetectable y Premium',
    hero_h1: 'El <span class="accent">mejor</span><br>client de Minecraft',
    btn_download: '⬇ Descargar', btn_key: '🔑 Obtener Key',
    meta_modules: 'Módulos', meta_uptime: 'Uptime', meta_versions: 'Versiones',
    meta_gui: 'GUI', meta_support: 'Soporte',

    tw: [
      '99.9% indetectable — optimizado para Watchdog, GrimAC y Vulcan.',
      'GUI premium a 60 FPS con motor de temas y editor de HUD.',
      'Soporte multiversión: de 1.17 a 26.x — 33 versiones.',
      'Sistema de licencia HWID de 8 capas — protección imposible de crackear.',
      '49+ módulos con modos Simple/Avanzado y presets.'
    ],

    feat_label: 'Por qué Draxo', feat_title: 'Características Premium',
    feat_subtitle: 'Cada detalle diseñado para la máxima calidad — desde el motor anti-cheat hasta la GUI.',
    feat1_t: 'Motor Indetectable',
    feat1_d: 'Tecnología de bypass anti-cheat multicapa con programación de paquetes, rotaciones con jitter sinusoidal y motor de aleatorización. Optimizado para Watchdog, GrimAC, Vulcan y más.',
    feat2_t: 'GUI Premium',
    feat2_d: 'Diseño estilo Discord con animaciones fluidas a 60 FPS, motor de temas, colores personalizables, efectos de blur/glow y editor de HUD completo.',
    feat3_t: '49+ Módulos',
    feat3_d: 'KillAura, Scaffold, ESP, Tracers, Nuker, Speed, Fly, Reach, Velocity — todos con modos Simple/Avanzado y presets personalizados.',
    feat4_t: 'Sistema de Licencia HWID',
    feat4_d: 'Protección incraqueable de 8 capas con cifrado poly-XOR, consenso de 3 ranuras y verificación de integridad del código en tiempo de ejecución.',
    feat5_t: 'Multiversión',
    feat5_d: 'Compatible con todas las versiones de Minecraft de 1.17 a 26.x. Generación automática de mappings — sin actualizaciones manuales.',
    feat6_t: 'Rendimiento',
    feat6_d: 'Optimizado para un uso mínimo de CPU/RAM. Renderizado diferido, pooling de objetos y estructuras de datos eficientes — cero pérdida de FPS.',

    mod_label: 'Módulos', mod_title: 'Todo lo que necesitas',
    mod_subtitle: 'De Combat a Movement y Visuals — cada módulo con modo Simple/Avanzado y presets.',
    mod_cat_combat: '⚔ Combate', mod_cat_movement: '🏃 Movimiento',
    mod_cat_visuals: '👁 Visuales', mod_cat_utility: '🧰 Utilidades',

    ver_label: 'Compatibilidad', ver_title: '33 versiones compatibles',
    ver_subtitle: 'De 1.17 a 26.x — generación automática de mappings para cada versión.',
    ver_latest: '26.2 (última)',

    dl_label: 'Empezar', dl_title: 'Descarga y Key',
    dl_subtitle: 'Descarga el launcher y consigue tu key de 24h a través de Linkvertise.',
    dl_btn_exe: '⬇ DraxoLauncher.exe', dl_btn_key: '🔑 Obtener Key',
    dl_card_title: 'Cómo funciona',
    dl_steps: '<strong>1.</strong> Descarga DraxoLauncher.exe y colócala en tu carpeta de Draxo Client.<br><strong>2.</strong> Inicia el launcher — encuentra Python automáticamente e instala las dependencias.<br><strong>3.</strong> Copia tu HWID de la tarjeta de licencia (📋 Copy).<br><strong>4.</strong> Haz clic en <a href="keygen.html" style="color:var(--accent);">Obtener Key</a> — completa 3 anuncios de Linkvertise.<br><strong>5.</strong> Introduce la key en el menú de Draxo (CONFIG → LICENSE → Activate).<br><strong>6.</strong> Elige tu versión de Minecraft → INJECT → ¡todos los módulos desbloqueados!',
    dl_meta: '🔒 Las keys de 24h están vinculadas a tu HWID • 3 anuncios = 1 key',

    test_label: 'Comunidad', test_title: 'Lo que dicen los jugadores',
    test_subtitle: 'Draxo es usado a diario por cientos de jugadores en Minemen, Flamefrags e Hypixel.',
    test1_q: '"El mejor client que he usado. La KillAura es tan suave que ni el staff experimentado lo nota. Y la GUI parece Discord — simplemente premium."',
    test1_name: 'Jugador de Minemen', test1_role: 'Desde v1.0',
    test2_q: '"Por fin un client que no recibe kick al instante en Flamefrags. El bypass de Watchdog no es una promesa vacía — la rotación con jitter sinusoidal es genial."',
    test2_name: 'Jugador de Flamefrags', test2_role: 'Diamond Division',
    test3_q: '"El soporte multiversión es un cambio total. Juego 1.8 en Minemen y 1.21 en Hypixel con el mismo client — todo funciona sin configuración."',
    test3_name: 'Jugador Multiversión', test3_role: 'Main 1.8 y 1.21',

    faq_label: 'FAQ', faq_title: 'Preguntas frecuentes',
    faq_subtitle: 'Todo lo que necesitas saber sobre Draxo.',
    faq1_q: '¿Draxo es realmente indetectable?',
    faq1_a: 'Draxo usa un motor anti-cheat multicapa con programación de paquetes, rotaciones con jitter sinusoidal, motor de aleatorización y micro-desplazamientos posicionales. Probamos regularmente contra Watchdog (Hypixel), GrimAC, Vulcan y Spartan. <strong>Ningún cheat es 100% indetectable</strong> — usa Draxo con responsabilidad y evita ajustes rage evidentes en servidores con staff activo.',
    faq2_q: '¿Cómo consigo una key?',
    faq2_a: 'Visita la <a href="keygen.html" style="color:var(--accent);">página de keygen</a>, introduce tu HWID (cópiala del launcher: 📋 Copy), completa 3 anuncios cortos de Linkvertise y recibe tu key de 24h. La key está vinculada a tu hardware (HWID-locked).',
    faq3_q: '¿Qué versiones de Minecraft se soportan?',
    faq3_a: 'Draxo soporta <strong>todas las versiones de 1.17 a 26.x</strong>. El generador de mappings integrado crea automáticamente los mappings correctos para tu versión. En total son 33+ versiones.',
    faq4_q: '¿Cómo funciona la instalación?',
    faq4_a: 'Descarga DraxoLauncher.exe y colócala en cualquier carpeta. El launcher encuentra Python automáticamente, instala las dependencias e inyecta la DLL en Minecraft. No se necesita configuración complicada.',
    faq5_q: '¿Puedo jugar en Hypixel con mi cuenta principal?',
    faq5_a: '<strong>Recomendamos encarecidamente no hacer trampa nunca con tu cuenta principal.</strong> Usa una cuenta alternativa o prueba en servidores como Minemen.club o Flamefrags.gg, que son más tolerantes con los clients. Hypixel banea permanentemente al detectar.',
    faq6_q: '¿Qué es el modo Simple/Avanzado?',
    faq6_a: 'En el <strong>modo Simple</strong>, cada módulo solo tiene 2 presets: Legit (seguro, discreto) y Rage (máximo efecto). Perfecto para principiantes. El <strong>modo Avanzado</strong> te da control total sobre timing, alcance, jitter y todos los demás parámetros — para usuarios avanzados.',
    faq7_q: 'Mi clave no funciona — ¿qué hago?',
    faq7_a: 'La causa más común es una <strong>HWID incorrecta</strong>: la clave está vinculada a la HWID de tu equipo, que ves en el launcher de Draxo (📋 Copy). Usa siempre la misma HWID en el keygen — no la de un draxo_config.ini antiguo ni de otro PC. Otras causas: la clave <strong>expiró</strong> (24h), el <strong>reloj del sistema</strong> está mal, o generaste una segunda clave para la misma HWID (límite de tasa). Primero comprueba que la HWID coincida exactamente (32 caracteres, mayúsculas). Si la activación sigue fallando, revisa la <strong>caja de soporte</strong> en la página de keygen — enlaza a nuestro Discord, donde resolvemos problemas de HWID en minutos.',

    footer_home: 'Inicio', footer_features: 'Características', footer_download: 'Descargar',
    footer_keygen: 'Keygen', footer_discord: 'Discord', footer_github: 'GitHub',
    footer_copyright: '© 2026 Draxo Client · batotomato · Todos los derechos reservados.',
    footer_disclaimer: 'Draxo es una utilidad de terceros y no está afiliada con Mojang o Microsoft.',

    kg_title: 'Draxo Client — Clave de Licencia',
    kg_meta_desc: 'Obtén tu clave de licencia de 24 horas de Draxo Client. Vinculada a HWID, indetectable, utilidad premium de Minecraft.',
    kg_subtitle: 'Generador de Claves de Licencia',
    kg_hwid_label: 'Tu HWID',
    kg_hwid_placeholder: 'Pega tu HWID de 32 caracteres...',
    kg_guide_summary: 'Cómo encontrar tu HWID',
    kg_guide_content: '<strong>Opción 1 — Draxo Launcher</strong><br>Inicia DraxoLauncher.exe. En la tarjeta de licencia, entre el panel de control y la consola, verás tu HWID. Haz clic en <strong>📋 Copy</strong> y pégala aquí.<br><br><strong>Opción 2 — Después de la primera inyección</strong><br>Inyecta Draxo en Minecraft una vez. Tu HWID se guarda en <code>build/vanilla/Release/draxo_config.ini</code>. Abre ese archivo y busca <code>License.hwid=...</code> — esa es tu HWID de 32 caracteres.<br><br><strong>Importante</strong><br>La clave está vinculada al HWID. Usa la MISMA HWID que en tu Draxo Client — de lo contrario la clave no funcionará.',
    kg_ads_title: 'Completa los 3 anuncios para obtener tu clave',
    kg_ads_warn: '⚠️ DEBES abrir los 3 anuncios de arriba. El botón se desbloquea después de los 3 clics.',
    kg_ads_warn_sub: 'Cada anuncio se abre en una nueva pestaña. Ciérrala y vuelve aquí para el siguiente.',
    kg_btn_lock_pre: 'Completa los 3 anuncios (', kg_btn_lock_post: ')',
    kg_btn_generate: 'Generar Clave', kg_btn_generating: 'Generando…',
    kg_valid: 'HWID válida',
    kg_need32: 'Se necesitan 32 caracteres (actualmente {n})',
    kg_result_title: '✅ Tu clave de licencia de 24 horas',
    kg_copy: '📋 Copiar Clave', kg_copied: '✓ ¡Copiada!',
    kg_bound: 'Vinculada a HWID — solo funciona en TU equipo',
    kg_expire: '⏰ Expira: {d} UTC',
    kg_activate_title: 'Cómo activarla:',
    kg_act1: 'Abre el Draxo Launcher o inyecta la DLL en Minecraft',
    kg_act2: 'Abre el menú (<code>RSHIFT</code> o <code>INSERT</code>)',
    kg_act3: 'Ve a la pestaña <code>CONFIG</code> → sección <code>LICENSE</code>',
    kg_act4: 'Pega la clave y haz clic en <strong>Activate</strong>',
    kg_error_prefix: '❌ Error: ', kg_error_default: 'Error al generar la clave',
    kg_need_ads: 'Primero debes completar los {n} anuncios de Linkvertise. Abre cada anuncio y vuelve aquí.',
    kg_rate_limit: 'Límite: 1 clave por HWID cada 24 horas.',
    kg_checkpoint_title: 'Enlace inválido o expirado',
    kg_checkpoint_msg: 'Este enlace de anuncio ya fue utilizado o es inválido.<br>Vuelve a la página de keygen e inténtalo de nuevo.',
    kg_checkpoint_back: 'Ir al Keygen',
    kg_ad_label_pre: 'Anuncio ', kg_ad_label_post: '',
    kg_open_ad_pre: 'Abrir Anuncio ', kg_open_ad_post: '',
    kg_ad_done: '✓ Anuncio completado',
    kg_badge_done: 'Hecho', kg_badge_lv: 'Linkvertise',
    kg_back_home: '← Volver a la página de Draxo',
    kg_no_links: 'No hay enlaces de Linkvertise configurados.',
    kg_support_title: '¿Problemas de activación?',
    kg_support_text: "¿La clave no funciona o tu HWID fue rechazada? Únete a nuestro Discord — resolveremos tu problema en minutos.",
    kg_support_tip: '💡 Error más común: HWID copiada incorrectamente — siempre cópiala del launcher (📋 Copy), no del draxo_config.ini.',
    kg_support_tip_link: 'Mostrar guía',

    btn_discord: '💬 Discord',
    discord_join: 'Únete a Discord',
    discord_online: '🟢 {n} en línea',
    discord_cta_title: 'Únete a la comunidad',
    discord_cta_text: 'Actualizaciones, soporte y sorteos: conoce a la comunidad de Draxo en Discord.',

    nav_changelog: 'Registro de cambios', footer_changelog: 'Registro de cambios',
    chg_badge: 'Historial de versiones',
    chg_title: 'Changelog',
    chg_subtitle: 'Cada versión de Draxo: qué cambió, mejoró y se corrigió.',
    chg_current: 'Versión actual',
    chg_loading: 'Cargando versiones…',
    chg_error: 'No se pudo cargar version.json.',
    chg_retry: 'Reintentar',
    chg_badge_latest: 'Última',
    chg_date: 'Publicada',
    chg_highlights: 'Destacados',
    chg_notes: 'Notas de la versión',
    chg_no_entries: 'Aún no hay versiones.',
    nav_legal: 'Legal', footer_legal: 'Legal',
    legal_title: 'Draxo Client — Legal',
    legal_meta_desc: 'Aviso legal, Términos de Servicio y Política de Privacidad del Draxo Minecraft Client.',
    legal_heading: 'Legal',
    legal_subtitle: 'Aviso legal, Términos de Servicio y Política de Privacidad.',
    legal_disclaimer_title: 'Aviso Legal (Disclaimer)',
    legal_disclaimer_text: '<p>Draxo Client ("Draxo") es una utilidad de terceros para Minecraft. Draxo <strong>no está afiliado, respaldado ni conectado con Mojang Studios, Microsoft Corporation o ningún servidor de Minecraft</strong>.</p><p>Draxo se proporciona "tal cual" sin garantía de ningún tipo. Los desarrolladores de Draxo no asumen <strong>ninguna responsabilidad</strong> por las consecuencias derivadas de su uso, incluyendo bloqueos de cuenta, exclusiones de servidores u otras sanciones por parte de administradores o sistemas anti-trampas.</p><p>El uso de Draxo en servidores multijugador puede violar los términos de servicio de dichos servidores. <strong>Eres el único responsable</strong> de garantizar que tu uso de Draxo cumpla con todas las normas y leyes aplicables.</p>',
    legal_terms_title: 'Términos de Servicio (ToS)',
    legal_terms_text: '<p>Al usar Draxo Client, aceptas las siguientes condiciones:</p><ol><li>Draxo se distribuye bajo un <strong>sistema de clave de licencia de 24 horas</strong>. Cada clave está vinculada a un ID de hardware (HWID) único y no puede ser transferida, compartida o revendida.</li><li>Las claves se obtienen completando anuncios de Linkvertise. <strong>No se requiere ni se acepta ningún pago monetario</strong>.</li><li><strong>No se realizan reembolsos</strong>, ya que no hay transacciones monetarias.</li><li>No puedes <strong>modificar, aplicar ingeniería inversa, descompilar o redistribuir</strong> Draxo Client sin permiso explícito por escrito.</li><li>El equipo de Draxo se reserva el derecho de <strong>revocar claves de licencia</strong> en cualquier momento y por cualquier motivo, incluyendo abuso o violación de estos términos.</li><li>Debes cumplir las normas de cualquier servidor de Minecraft en el que uses Draxo.</li></ol>',
    legal_privacy_title: 'Política de Privacidad',
    legal_privacy_text: '<p>Draxo Client recopila:</p><ul><li><strong>ID de Hardware (HWID)</strong>: Un hash de los números de serie de tu CPU y placa base, utilizado exclusivamente para la vinculación de la clave de licencia. Estos datos nunca salen de tu equipo en forma identificable.</li><li><strong>Datos de localStorage</strong>: La página keygen almacena el progreso de los clics en anuncios y tokens temporales en el localStorage de tu navegador. Estos datos permanecen en tu dispositivo.</li></ul><p><strong>Lo que Draxo NO recopila:</strong></p><ul><li>Información personal (nombre, correo, dirección IP)</li><li>Credenciales de cuenta de Minecraft</li><li>Datos de juego o registros de chat</li><li>Cookies de análisis o seguimiento</li></ul><p>No se comparten ni venden datos a terceros. Linkvertise opera de forma independiente — consulta su política de privacidad para más detalles.</p><p>¿Preguntas? Únete a nuestro <a href="https://discord.gg/2JTPwRmPZw" style="color:var(--accent);">servidor de Discord</a>.</p>'
  },

  /* ─────────────────────────── FRANÇAIS ────────────────────────── */
  fr: {
    title: 'Draxo Client — Utilitaire Premium Minecraft',
    meta_desc: "Draxo Client — l'utilitaire Minecraft ultime. Indétectable, fonctionnalités premium, GUI moderne. Pour 1.17 à 26.x.",

    nav_features: 'Fonctionnalités', nav_modules: 'Modules', nav_versions: 'Versions',
    nav_download: 'Télécharger', nav_faq: 'FAQ', nav_download_btn: 'Télécharger',

    hero_badge: '⚡ Indétectable et Premium',
    hero_h1: 'Le <span class="accent">meilleur</span><br>client Minecraft',
    btn_download: '⬇ Télécharger', btn_key: '🔑 Obtenir une clé',
    meta_modules: 'Modules', meta_uptime: 'Uptime', meta_versions: 'Versions',
    meta_gui: 'GUI', meta_support: 'Support',

    tw: [
      '99,9 % indétectable — optimisé pour Watchdog, GrimAC et Vulcan.',
      'GUI premium à 60 FPS avec moteur de thèmes et éditeur de HUD.',
      'Support multiversion : 1.17 à 26.x — 33 versions.',
      'Système de licence HWID à 8 couches — protection incraquable.',
      '49+ modules avec modes Simple/Avancé et presets.'
    ],

    feat_label: 'Pourquoi Draxo', feat_title: 'Fonctionnalités Premium',
    feat_subtitle: "Chaque détail est conçu pour une qualité maximale — du moteur anti-cheat à la GUI.",
    feat1_t: 'Moteur Indétectable',
    feat1_d: "Technologie de bypass anti-cheat multicouche avec planification de paquets, rotations à jitter sinusoïdal et moteur de randomisation. Optimisé pour Watchdog, GrimAC, Vulcan et plus.",
    feat2_t: 'GUI Premium',
    feat2_d: "Design de type Discord avec animations fluides à 60 FPS, moteur de thèmes, couleurs personnalisables, effets de flou/lueur et éditeur de HUD complet.",
    feat3_t: '49+ Modules',
    feat3_d: 'KillAura, Scaffold, ESP, Tracers, Nuker, Speed, Fly, Reach, Velocity — tous avec modes Simple/Avancé et presets personnalisés.',
    feat4_t: 'Système de Licence HWID',
    feat4_d: "Protection incraquable à 8 couches avec chiffrement poly-XOR, consensus à 3 slots et vérification d'intégrité du code à l'exécution.",
    feat5_t: 'Multiversion',
    feat5_d: "Prend en charge toutes les versions de Minecraft de 1.17 à 26.x. Génération automatique des mappings — aucune mise à jour manuelle.",
    feat6_t: 'Performance',
    feat6_d: "Optimisé pour une utilisation minimale du CPU/RAM. Rendu paresseux, pooling d'objets et structures de données efficaces — zéro perte de FPS.",

    mod_label: 'Modules', mod_title: 'Tout ce dont vous avez besoin',
    mod_subtitle: "De Combat à Movement en passant par Visuals — chaque module avec mode Simple/Avancé et presets.",
    mod_cat_combat: '⚔ Combat', mod_cat_movement: '🏃 Mouvement',
    mod_cat_visuals: '👁 Visuels', mod_cat_utility: '🧰 Utilitaires',

    ver_label: 'Compatibilité', ver_title: '33 versions prises en charge',
    ver_subtitle: "De 1.17 à 26.x — génération automatique des mappings pour chaque version.",
    ver_latest: '26.2 (dernière)',

    dl_label: 'Commencer', dl_title: 'Téléchargement et clé',
    dl_subtitle: "Téléchargez le launcher et obtenez votre clé 24h via Linkvertise.",
    dl_btn_exe: '⬇ DraxoLauncher.exe', dl_btn_key: '🔑 Obtenir une clé',
    dl_card_title: 'Comment ça marche',
    dl_steps: "<strong>1.</strong> Téléchargez DraxoLauncher.exe et placez-la dans votre dossier Draxo Client.<br><strong>2.</strong> Lancez le launcher — il trouve Python automatiquement et installe les dépendances.<br><strong>3.</strong> Copiez votre HWID depuis la carte de licence (📋 Copy).<br><strong>4.</strong> Cliquez sur <a href=\"keygen.html\" style=\"color:var(--accent);\">Obtenir une clé</a> — complétez 3 publicités Linkvertise.<br><strong>5.</strong> Saisissez la clé dans le menu Draxo (CONFIG → LICENSE → Activate).<br><strong>6.</strong> Choisissez votre version de Minecraft → INJECT → tous les modules débloqués !",
    dl_meta: '🔒 Les clés 24h sont liées à votre HWID • 3 publicités = 1 clé',

    test_label: 'Communauté', test_title: 'Ce que disent les joueurs',
    test_subtitle: "Draxo est utilisé chaque jour par des centaines de joueurs sur Minemen, Flamefrags et Hypixel.",
    test1_q: '"Le meilleur client que j\'aie jamais utilisé. La KillAura est si fluide que même le staff expérimenté ne remarque rien. Et la GUI ressemble à Discord — simplement premium."',
    test1_name: 'Joueur Minemen', test1_role: 'Depuis la v1.0',
    test2_q: '"Enfin un client qui ne se fait pas kick instantanément sur Flamefrags. Le bypass Watchdog n\'est pas une promesse vide — la rotation à jitter sinusoïdal est géniale."',
    test2_name: 'Joueur Flamefrags', test2_role: 'Diamond Division',
    test3_q: '"Le support multiversion change tout. Je joue en 1.8 sur Minemen et en 1.21 sur Hypixel avec le même client — tout fonctionne sans configuration."',
    test3_name: 'Joueur Multiversion', test3_role: 'Main 1.8 et 1.21',

    faq_label: 'FAQ', faq_title: 'Questions fréquentes',
    faq_subtitle: 'Tout ce que vous devez savoir sur Draxo.',
    faq1_q: 'Draxo est-il vraiment indétectable ?',
    faq1_a: "Draxo utilise un moteur anti-cheat multicouche avec planification de paquets, rotations à jitter sinusoïdal, moteur de randomisation et micro-décalages positionnels. Nous testons régulièrement contre Watchdog (Hypixel), GrimAC, Vulcan et Spartan. <strong>Aucun cheat n'est 100 % indétectable</strong> — utilisez Draxo de manière responsable et évitez les réglages rage évidents sur les serveurs avec du staff actif.",
    faq2_q: 'Comment obtenir une clé ?',
    faq2_a: "Visitez la <a href=\"keygen.html\" style=\"color:var(--accent);\">page keygen</a>, saisissez votre HWID (copiez-la depuis le launcher : 📋 Copy), complétez 3 courtes publicités Linkvertise et recevez votre clé 24h. La clé est liée à votre matériel (HWID-locked).",
    faq3_q: 'Quelles versions de Minecraft sont prises en charge ?',
    faq3_a: "Draxo prend en charge <strong>toutes les versions de 1.17 à 26.x</strong>. Le générateur de mappings intégré crée automatiquement les mappings corrects pour votre version. Soit 33+ versions au total.",
    faq4_q: "Comment fonctionne l'installation ?",
    faq4_a: "Téléchargez DraxoLauncher.exe et placez-la dans n'importe quel dossier. Le launcher trouve Python automatiquement, installe les dépendances et injecte la DLL dans Minecraft. Aucune configuration compliquée nécessaire.",
    faq5_q: 'Puis-je jouer sur Hypixel avec mon compte principal ?',
    faq5_a: "<strong>Nous recommandons fortement de ne jamais tricher avec votre compte principal.</strong> Utilisez un compte secondaire ou testez sur des serveurs comme Minemen.club ou Flamefrags.gg, plus tolérants envers les clients. Hypixel bannit définitivement en cas de détection.",
    faq6_q: "Qu'est-ce que le mode Simple/Avancé ?",
    faq6_a: "En <strong>mode Simple</strong>, chaque module n'a que 2 presets : Legit (sûr, discret) et Rage (effet maximal). Parfait pour les débutants. Le <strong>mode Avancé</strong> vous donne un contrôle total sur le timing, la portée, le jitter et tous les autres paramètres — pour les utilisateurs expérimentés.",
    faq7_q: "Ma clé ne fonctionne pas — que faire ?",
    faq7_a: "La cause la plus fréquente est une <strong>mauvaise HWID</strong> : la clé est liée au HWID de votre machine, visible dans le launcher Draxo (📋 Copy). Utilisez toujours le même HWID dans le keygen — pas celui d'un ancien draxo_config.ini ni d'un autre PC. Autres causes : la clé a <strong>expiré</strong> (24 h), l'<strong>horloge système</strong> est incorrecte, ou vous avez généré une deuxième clé pour le même HWID (limite de débit). Vérifiez d'abord que le HWID correspond exactement (32 caractères, majuscules). Si l'activation échoue toujours, consultez la <strong>boîte d'assistance</strong> sur la page keygen — elle mène à notre Discord, où nous résolvons les problèmes de HWID en quelques minutes.",

    footer_home: 'Accueil', footer_features: 'Fonctionnalités', footer_download: 'Télécharger',
    footer_keygen: 'Keygen', footer_discord: 'Discord', footer_github: 'GitHub',
    footer_copyright: '© 2026 Draxo Client · batotomato · Tous droits réservés.',
    footer_disclaimer: "Draxo est un utilitaire tiers et n'est pas affilié à Mojang ou Microsoft.",

    kg_title: 'Draxo Client — Clé de Licence',
    kg_meta_desc: "Obtenez votre clé de licence Draxo Client de 24 heures. Liée au HWID, indétectable, utilitaire premium Minecraft.",
    kg_subtitle: 'Générateur de Clé de Licence',
    kg_hwid_label: 'Votre HWID',
    kg_hwid_placeholder: 'Collez votre HWID de 32 caractères...',
    kg_guide_summary: 'Comment trouver votre HWID',
    kg_guide_content: "<strong>Option 1 — Draxo Launcher</strong><br>Lancez DraxoLauncher.exe. Dans la carte de licence, entre le panneau de contrôle et la console, vous verrez votre HWID. Cliquez sur <strong>📋 Copy</strong> et collez-le ici.<br><br><strong>Option 2 — Après la première injection</strong><br>Injectez Draxo dans Minecraft une fois. Votre HWID est ensuite enregistré dans <code>build/vanilla/Release/draxo_config.ini</code>. Ouvrez ce fichier et cherchez <code>License.hwid=...</code> — c'est votre HWID de 32 caractères.<br><br><strong>Important</strong><br>La clé est liée au HWID. Utilisez le MÊME HWID que dans votre Draxo Client — sinon la clé ne fonctionnera pas.",
    kg_ads_title: 'Terminez les 3 publicités pour obtenir votre clé',
    kg_ads_warn: "⚠️ Vous DEVEZ ouvrir les 3 publicités ci-dessus. Le bouton se déverrouille après les 3 clics.",
    kg_ads_warn_sub: "Chaque publicité s'ouvre dans un nouvel onglet. Fermez-le et revenez ici pour la suivante.",
    kg_btn_lock_pre: 'Terminez les 3 publicités (', kg_btn_lock_post: ')',
    kg_btn_generate: 'Générer la Clé', kg_btn_generating: 'Génération…',
    kg_valid: 'HWID valide',
    kg_need32: '32 caractères requis (actuellement {n})',
    kg_result_title: '✅ Votre clé de licence de 24 heures',
    kg_copy: '📋 Copier la Clé', kg_copied: '✓ Copié !',
    kg_bound: 'Liée au HWID — ne fonctionne que sur VOTRE machine',
    kg_expire: '⏰ Expire : {d} UTC',
    kg_activate_title: "Comment l'activer :",
    kg_act1: 'Ouvrez le Draxo Launcher ou injectez la DLL dans Minecraft',
    kg_act2: 'Ouvrez le menu (<code>RSHIFT</code> ou <code>INSERT</code>)',
    kg_act3: "Allez dans l'onglet <code>CONFIG</code> → section <code>LICENSE</code>",
    kg_act4: 'Collez la clé et cliquez sur <strong>Activate</strong>',
    kg_error_prefix: '❌ Erreur : ', kg_error_default: 'Échec de la génération de la clé',
    kg_need_ads: "Vous devez d'abord terminer les {n} publicités Linkvertise. Ouvrez chaque publicité et revenez ici.",
    kg_rate_limit: 'Limite : 1 clé par HWID toutes les 24 heures.',
    kg_checkpoint_title: 'Lien invalide ou expiré',
    kg_checkpoint_msg: "Ce lien publicitaire a déjà été utilisé ou est invalide.<br>Revenez à la page keygen et réessayez.",
    kg_checkpoint_back: 'Aller au Keygen',
    kg_ad_label_pre: 'Publicité ', kg_ad_label_post: '',
    kg_open_ad_pre: 'Ouvrir la Publicité ', kg_open_ad_post: '',
    kg_ad_done: '✓ Publicité terminée',
    kg_badge_done: 'Terminé', kg_badge_lv: 'Linkvertise',
    kg_back_home: "← Retour à la page d'accueil Draxo",
    kg_no_links: 'Aucun lien Linkvertise configuré.',
    kg_support_title: "Problème d'activation ?",
    kg_support_text: "La clé ne fonctionne pas ou votre HWID a été rejetée ? Rejoignez notre Discord — nous résoudrons votre problème en quelques minutes.",
    kg_support_tip: "💡 Erreur la plus fréquente : HWID mal copiée — copiez-la toujours depuis le launcher (📋 Copy), pas depuis draxo_config.ini.",
    kg_support_tip_link: 'Afficher le guide',

    btn_discord: '💬 Discord',
    discord_join: 'Rejoindre Discord',
    discord_online: '🟢 {n} en ligne',
    discord_cta_title: 'Rejoignez la communauté',
    discord_cta_text: "Mises à jour, support et giveaways — rejoignez la communauté Draxo sur Discord.",

    nav_changelog: 'Journal des modifications', footer_changelog: 'Journal des modifications',
    chg_badge: 'Historique des versions',
    chg_title: 'Changelog',
    chg_subtitle: "Chaque version de Draxo — ce qui a changé, amélioré et corrigé.",
    chg_current: 'Version actuelle',
    chg_loading: 'Chargement des versions…',
    chg_error: 'Impossible de charger version.json.',
    chg_retry: 'Réessayer',
    chg_badge_latest: 'Dernière',
    chg_date: 'Publié le',
    chg_highlights: 'Points forts',
    chg_notes: 'Notes de version',
    chg_no_entries: 'Aucune version pour le moment.',
    nav_legal: 'Légal', footer_legal: 'Légal',
    legal_title: 'Draxo Client — Mentions Légales',
    legal_meta_desc: 'Avis de non-responsabilité, Conditions d\'utilisation et Politique de confidentialité du Draxo Minecraft Client.',
    legal_heading: 'Mentions Légales',
    legal_subtitle: 'Avis de non-responsabilité, Conditions d\'utilisation et Politique de confidentialité.',
    legal_disclaimer_title: 'Avis de non-responsabilité (Disclaimer)',
    legal_disclaimer_text: '<p>Draxo Client ("Draxo") est un utilitaire tiers pour Minecraft. Draxo n\'est <strong>pas affilié, soutenu ou lié à Mojang Studios, Microsoft Corporation ou à un quelconque serveur Minecraft</strong>.</p><p>Draxo est fourni "tel quel" sans aucune garantie. Les développeurs de Draxo déclinent <strong>toute responsabilité</strong> quant aux conséquences de son utilisation, y compris les bannissements de compte, les exclusions de serveur ou toute autre sanction de la part d\'administrateurs ou de systèmes anti-triche.</p><p>L\'utilisation de Draxo sur des serveurs multijoueurs peut enfreindre les conditions d\'utilisation de ces serveurs. <strong>Vous êtes seul responsable</strong> de vous assurer que votre utilisation de Draxo est conforme à toutes les règles et lois applicables.</p>',
    legal_terms_title: 'Conditions d\'utilisation (ToS)',
    legal_terms_text: '<p>En utilisant Draxo Client, vous acceptez les conditions suivantes :</p><ol><li>Draxo est distribué sous un <strong>système de clé de licence de 24 heures</strong>. Chaque clé est liée à un identifiant matériel (HWID) unique et ne peut être transférée, partagée ou revendue.</li><li>Les clés de licence sont obtenues en complétant des publicités Linkvertise. <strong>Aucun paiement monétaire</strong> n\'est requis ou accepté.</li><li><strong>Aucun remboursement</strong> n\'est effectué, car aucune transaction monétaire n\'a lieu.</li><li>Vous ne pouvez pas <strong>modifier, désosser, décompiler ou redistribuer</strong> Draxo Client sans autorisation écrite explicite.</li><li>L\'équipe Draxo se réserve le droit de <strong>révoquer les clés de licence</strong> à tout moment et pour toute raison, y compris en cas d\'abus ou de violation de ces conditions.</li><li>Vous devez respecter les règles de tout serveur Minecraft sur lequel vous utilisez Draxo.</li></ol>',
    legal_privacy_title: 'Politique de confidentialité',
    legal_privacy_text: '<p>Draxo Client collecte :</p><ul><li><strong>Identifiant matériel (HWID)</strong> : Un hash des numéros de série de votre CPU et de votre carte mère, utilisé exclusivement pour la liaison de la clé de licence. Ces données ne quittent jamais votre machine sous forme identifiable.</li><li><strong>Données localStorage</strong> : La page keygen stocke la progression des clics publicitaires et des jetons temporaires dans le localStorage de votre navigateur. Ces données restent sur votre appareil.</li></ul><p><strong>Ce que Draxo NE collecte PAS :</strong></p><ul><li>Aucune information personnelle (nom, email, adresse IP)</li><li>Aucun identifiant de compte Minecraft</li><li>Aucune donnée de jeu ou de chat</li><li>Aucun cookie d\'analyse ou de suivi</li></ul><p>Aucune donnée n\'est partagée avec ou vendue à des tiers. Linkvertise fonctionne de manière indépendante — veuillez consulter leur politique de confidentialité pour plus de détails.</p><p>Des questions ? Rejoignez notre <a href="https://discord.gg/2JTPwRmPZw" style="color:var(--accent);">serveur Discord</a>.</p>'
  }
  };

  /* ─────────────────────────── CORE LOGIC ──────────────────────── */
  var current = 'en';

  function browserLang() {
    var n = ((navigator.language || navigator.userLanguage || 'en') + '').toLowerCase();
    for (var i = 0; i < LANGS.length; i++) {
      if (n.indexOf(LANGS[i]) === 0) return LANGS[i];
    }
    return 'en';
  }

  /* Ask several free IP-geo endpoints (no key, CORS-enabled).
     Returns ISO country code or null. */
  async function geoCountry() {
    var endpoints = [
      'https://api.country.is/',
      'https://ipapi.co/json/',
      'https://ipwho.is/'
    ];
    for (var i = 0; i < endpoints.length; i++) {
      try {
        var ctrl = new AbortController();
        var timer = setTimeout(function () { try { ctrl.abort(); } catch (e) {} }, 2500);
        var res = await fetch(endpoints[i], { signal: ctrl.signal });
        clearTimeout(timer);
        var j = await res.json();
        if (j && j.country) return String(j.country).toUpperCase();
        if (j && j.country_code) return String(j.country_code).toUpperCase();
        if (j && j.countryCode) return String(j.countryCode).toUpperCase();
      } catch (e) { /* try next endpoint */ }
    }
    return null;
  }

  function detect() {
    var saved = null;
    try { saved = localStorage.getItem(SAVED_KEY); } catch (e) {}
    if (saved && LANGS.indexOf(saved) !== -1) return { lang: saved, source: 'saved' };
    return { lang: browserLang(), source: 'browser' };
  }

  function apply() {
    var l = current;
    var table = STRINGS[l] || STRINGS.en;

    if (document.documentElement) document.documentElement.lang = l;
    if (table.title) document.title = table.title;
    var meta = document.querySelector('meta[name="description"]');
    if (meta && table.meta_desc) meta.setAttribute('content', table.meta_desc);

    document.querySelectorAll('[data-i18n]').forEach(function (el) {
      var key = el.getAttribute('data-i18n');
      var attr = el.getAttribute('data-i18n-attr');
      if (key && table[key] !== undefined) {
        if (attr) el.setAttribute(attr, table[key]);
        else el.innerHTML = table[key];
      }
    });

    // Wire all Discord links from the central config (single source of truth)
    var discordUrl = (window.DRAXO_CONFIG || {}).discordInviteUrl;
    if (discordUrl) {
      document.querySelectorAll('[data-discord]').forEach(function (el) {
        el.setAttribute('href', discordUrl);
      });
    }

    // Re-render the live Discord counter in the new language
    renderDiscordCounters();

    var sel = document.getElementById('langSelect');
    if (sel) sel.value = l;

    if (window.i18nOnChange) {
      try { window.i18nOnChange(l); } catch (e) {}
    }
  }

  window.DraxoI18N = {
    t: function (key) {
      var table = STRINGS[current] || STRINGS.en;
      if (table[key] !== undefined) return table[key];
      if (STRINGS.en[key] !== undefined) return STRINGS.en[key];
      return key;
    },
    phrases: function () { return (STRINGS[current] || STRINGS.en).tw || STRINGS.en.tw; },
    lang: function () { return current; },
    dict: function () { return STRINGS; },
    setLang: function (l) {
      if (LANGS.indexOf(l) === -1) l = 'en';
      current = l;
      try { localStorage.setItem(SAVED_KEY, l); } catch (e) {}
      apply();
      return l;
    },
    init: function () {
      var d = detect();
      current = d.lang;
      apply(); // synchronous — no language flash
      startDiscordStatus();

      // Wire up the language dropdown (works on any page that has #langSelect)
      var sel = document.getElementById('langSelect');
      if (sel && !sel.dataset.i18nBound) {
        sel.dataset.i18nBound = '1';
        sel.addEventListener('change', function () {
          window.DraxoI18N.setLang(this.value);
        });
      }

      // No manual choice yet → refine with IP geolocation (country detection)
      if (d.source !== 'saved') {
        geoCountry().then(function (country) {
          if (!country) return;
          var lang = COUNTRY_LANG[country] || d.lang;
          if (lang !== current) {
            current = lang;
            apply();
          }
        });
      }
    }
  };

  // ── Discord live online counter (shared by every page) ────────────
  // Reads presence_count from the Discord Server Widget JSON. The guild
  // ID comes from window.DRAXO_CONFIG. Polls every 60s and hides itself
  // gracefully when unavailable or unconfigured. Only active on pages
  // that contain at least one element with class .discord-online.
  var discordLastCount = null;

  function renderDiscordCounters() {
    var els = document.querySelectorAll('.discord-online');
    if (!els.length) return;
    var label = window.DraxoI18N.t('discord_online');
    els.forEach(function (el) {
      if (discordLastCount == null) { el.style.display = 'none'; return; }
      el.style.display = 'inline-flex';
      el.innerHTML = '<span class="dot"></span>' +
        label.replace('{n}', discordLastCount.toLocaleString(current));
    });
  }

  function startDiscordStatus() {
    var cfg = window.DRAXO_CONFIG || {};
    if (!cfg.discordGuildId || !document.querySelectorAll('.discord-online').length) return;

    async function poll() {
      try {
        var count = null;
        var r = await fetch(
          'https://discord.com/api/guilds/' + encodeURIComponent(cfg.discordGuildId) + '/widget.json',
          { cache: 'no-store' }
        );
        if (r.ok) {
          var j = await r.json();
          count = (typeof j.presence_count === 'number') ? j.presence_count : null;
        }
        discordLastCount = count;
      } catch (e) {
        discordLastCount = null; // network / CORS / ad-blocker -> hide
      }
      renderDiscordCounters();
    }

    window.__discordStatus = { refresh: poll };
    poll();
    setInterval(poll, 60000);
  }

  /* Set the initial language synchronously so t() already works
     before DOMContentLoaded (needed by the keygen checkpoint page). */
  current = detect().lang;

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', function () { window.DraxoI18N.init(); });
  } else {
    window.DraxoI18N.init();
  }
})();
