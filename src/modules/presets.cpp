#include "pch.h"
#include "modules/module.h"
#include "modules/module_manager.h"

/* ──────────────────────────────────────────────────────────────────────
 *  Module Legit/Rage Presets — Single Source of Truth
 *
 *  "Legit" = human-like, conservative, safe on servers with strict AC
 *            (Hypixel, Minemen, MMC, BW).  Undetectable in long sessions.
 *  "Rage"  = aggressive, pushed to the limit but with bypass features
 *            still active.  For alt accounts or servers with weak AC.
 *
 *  Every module that has presets MUST be listed in both hasPresets()
 *  AND applyPreset() below.  No phantom settings — only writes keys the
 *  module constructor already defined.
 * ────────────────────────────────────────────────────────────────────── */

bool Module::hasPresets() const {
    // COMBAT
    if (m_name == "KillAura")   return true;
    if (m_name == "AimAssist")  return true;
    if (m_name == "Reach")      return true;
    if (m_name == "Velocity")   return true;
    if (m_name == "AntiKB")     return true;
    if (m_name == "AutoClicker")return true;
    if (m_name == "TriggerBot") return true;
    if (m_name == "HitBoxes")   return true;
    if (m_name == "Criticals")  return true;
    if (m_name == "WTap")       return true;
    // MOVEMENT
    if (m_name == "Speed")      return true;
    if (m_name == "Fly")        return true;
    if (m_name == "Timer")      return true;
    if (m_name == "Sprint")     return true;
    if (m_name == "Step")       return true;
    if (m_name == "NoSlow")     return true;
    if (m_name == "FastLadder") return true;
    if (m_name == "Glide")      return true;
    // RENDER
    if (m_name == "ESP")        return true;
    if (m_name == "Tracers")    return true;
    if (m_name == "X-Ray")      return true;
    // PLAYER
    if (m_name == "FastPlace")  return true;
    if (m_name == "FastBreak")  return true;
    if (m_name == "NoFall")     return true;
    // WORLD
    if (m_name == "Scaffold")   return true;
    if (m_name == "Nuker")      return true;
    if (m_name == "AutoFarm")   return true;
    // MISC
    if (m_name == "ChatBypass") return true;
    return false;
}

void Module::applyPreset(const char* name) {
    bool legit = (strcmp(name, "Legit") == 0);
    bool rage  = (strcmp(name, "Rage") == 0);
    if (!legit && !rage) return;  // Custom preset — loaded via loadPreset

    /* ═══════════════════════════════════════════════════════════════════
     *  COMBAT
     * ═════════════════════════════════════════════════════════════════ */

    if (m_name == "KillAura") {
        // ── Targeting ─────────────────────────────────────────────
        m_floatSettings["range"]            = legit ? 3.2f    : 4.5f;
        m_floatSettings["fov"]              = legit ? 60.0f   : 180.0f;
        m_boolSettings["players"]           = legit ? true    : false;  // Legit: only players. Rage: all.
        m_boolSettings["mobs"]              = legit ? false   : true;
        m_boolSettings["focus_only"]        = legit ? true    : false;
        // ── Aim (humanized) ───────────────────────────────────────
        m_floatSettings["aim_speed"]        = legit ? 25.0f   : 60.0f;   // Legit: slow human aim
        m_boolSettings["silent_aim"]         = true;                       // always silent
        // ── Timing ────────────────────────────────────────────────
        m_floatSettings["cps"]              = legit ? 10.0f   : 16.0f;   // Legit CPS 10 cap
        m_intSettings["cooldown"]            = legit ? 18      : 14;       // Legit: 18-ticks cooldown
        m_floatSettings["reaction"]          = legit ? 150.0f  : 80.0f;   // Legit: 150ms human reaction
        // ── Undetect (humanization layer) ─────────────────────────
        m_floatSettings["aim_jitter"]        = legit ? 3.0f    : 1.5f;    // Legit: more jitter = human
        m_floatSettings["miss_chance"]       = legit ? 6.0f    : 0.0f;    // Legit: natural misses
        m_floatSettings["cps_jitter"]        = legit ? 25.0f   : 12.0f;
        m_floatSettings["cps_decay"]         = legit ? 0.5f    : 0.0f;
        // ── PvP Mode ──────────────────────────────────────────────
        m_intSettings["pvp_mode"]            = legit ? 1 : 0;              // 1=1.21 cooldown, 0=1.8 fast
        // ── Bypass features (all ON for undetectability) ──────────
        m_boolSettings["rage_jitter"]        = true;
        m_boolSettings["wd_sine_jitter"]     = true;
        m_boolSettings["wd_scheduler"]       = true;
        m_boolSettings["ac_bodypart"]        = true;
        m_boolSettings["ac_accelrot"]        = true;
        m_boolSettings["ac_ctxmiss"]         = true;
        m_boolSettings["ac_spread"]          = true;
        m_boolSettings["ac_grimac"]          = true;
        m_boolSettings["ac_fovfalloff"]      = true;
    }
    else if (m_name == "AimAssist") {
        m_floatSettings["speed"]   = legit ? 4.0f  : 15.0f;
        m_floatSettings["fov"]     = legit ? 60.0f : 180.0f;
    }
    else if (m_name == "Reach") {
        m_floatSettings["reach_distance"] = legit ? 3.15f : 4.0f;
        m_boolSettings["tp_reach"]        = legit ? false  : true;
    }
    else if (m_name == "Velocity") {
        m_floatSettings["horizontal"] = legit ? 85.0f : 30.0f;
        m_floatSettings["vertical"]   = legit ? 90.0f : 40.0f;
    }
    else if (m_name == "AntiKB") {
        m_floatSettings["horizontal"] = legit ? 65.0f : 15.0f;
        m_floatSettings["vertical"]   = legit ? 80.0f : 25.0f;
    }
    else if (m_name == "AutoClicker") {
        m_floatSettings["min_cps"] = legit ? 9.0f  : 16.0f;
        m_floatSettings["max_cps"] = legit ? 12.0f : 20.0f;
    }
    else if (m_name == "TriggerBot") {
        m_floatSettings["min_delay"] = legit ? 120.0f : 50.0f;
        m_floatSettings["max_delay"] = legit ? 180.0f : 90.0f;
    }
    else if (m_name == "HitBoxes") {
        m_floatSettings["expand"] = legit ? 0.12f : 0.55f;
    }
    else if (m_name == "Criticals") {
        m_boolSettings["mini_jump"] = legit ? true  : true;   // always on — required
        m_boolSettings["packet"]    = legit ? false : true;    // packet crits = more detectable
    }
    else if (m_name == "WTap") {
        m_floatSettings["hold_ms"]   = legit ? 180.0f : 80.0f;
        m_floatSettings["post_delay"] = legit ? 220.0f : 100.0f;
    }

    /* ═══════════════════════════════════════════════════════════════════
     *  MOVEMENT
     * ═════════════════════════════════════════════════════════════════ */

    else if (m_name == "Speed") {
        m_floatSettings["multiplier"] = legit ? 1.08f : 1.45f;
        if (m_intSettings.count("mode"))
            m_intSettings["mode"]     = legit ? 0 : 1;  // 0=Multiplier, 1=BHop/Strafe
    }
    else if (m_name == "Fly") {
        m_floatSettings["speed"] = legit ? 0.25f : 1.0f;
        if (m_intSettings.count("mode"))
            m_intSettings["mode"] = legit ? 0 : 1;  // 0=Vanilla, 1=Packet
    }
    else if (m_name == "Timer") {
        m_floatSettings["speed"] = legit ? 1.08f : 1.5f;
    }
    else if (m_name == "Sprint") {
        if (m_intSettings.count("mode"))
            m_intSettings["mode"] = legit ? 0 : 1;  // 0=Normal, 1=Omnisprint
    }
    else if (m_name == "Step") {
        m_floatSettings["height"] = legit ? 1.25f : 2.5f;
    }
    else if (m_name == "NoSlow") {
        // Legit: only eating slowdown removal (least suspicious)
        // Rage: remove ALL slowdown including block and bow
        m_boolSettings["eating"]   = true;
        m_boolSettings["blocking"] = legit ? false : true;
        m_boolSettings["bowing"]   = legit ? false : true;
    }
    else if (m_name == "FastLadder") {
        m_floatSettings["speed"] = legit ? 1.3f : 2.5f;
    }
    else if (m_name == "Glide") {
        m_floatSettings["fall_speed"] = legit ? -0.08f : -0.03f;
    }

    /* ═══════════════════════════════════════════════════════════════════
     *  RENDER
     * ═════════════════════════════════════════════════════════════════ */

    else if (m_name == "ESP") {
        m_boolSettings["players"] = true;
        m_boolSettings["mobs"]    = legit ? false : true;
        m_boolSettings["invis"]   = legit ? false : true;
    }
    else if (m_name == "Tracers") {
        m_boolSettings["players"]   = true;
        m_boolSettings["mobs"]      = legit ? false : true;
        m_floatSettings["max_range"] = legit ? 50.0f : 200.0f;
    }
    else if (m_name == "X-Ray") {
        m_floatSettings["radius"] = legit ? 20.0f : 80.0f;
    }

    /* ═══════════════════════════════════════════════════════════════════
     *  PLAYER
     * ═════════════════════════════════════════════════════════════════ */

    else if (m_name == "FastPlace") {
        m_floatSettings["delay"] = legit ? 1.0f : 0.0f;
    }
    else if (m_name == "FastBreak") {
        m_floatSettings["speed"] = legit ? 1.3f : 2.8f;
    }
    else if (m_name == "NoFall") {
        m_boolSettings["ground_spoof"] = legit ? true  : true;
        m_boolSettings["no_void"]      = legit ? true  : true;
    }

    /* ═══════════════════════════════════════════════════════════════════
     *  WORLD
     * ═════════════════════════════════════════════════════════════════ */

    else if (m_name == "Scaffold") {
        m_floatSettings["expand"]  = legit ? 1.0f : 3.0f;
        m_floatSettings["delay"]   = legit ? 80.0f : 35.0f;
        m_boolSettings["tower"]    = legit ? false : true;
        if (m_intSettings.count("mode"))
            m_intSettings["mode"]  = legit ? 0 : 2;   // 0=Legit, 2=Rage in scaffold
    }
    else if (m_name == "Nuker") {
        m_floatSettings["radius"]    = legit ? 3.0f : 6.0f;
        m_floatSettings["delay_min"] = legit ? 80.0f : 25.0f;
        if (m_intSettings.count("mode"))
            m_intSettings["mode"]    = legit ? 1 : 0;   // 1=Survival, 0=Creative
    }
    else if (m_name == "AutoFarm") {
        m_floatSettings["range"]   = legit ? 4.0f : 8.0f;
        m_floatSettings["delay"]   = legit ? 300.0f : 100.0f;
    }

    /* ═══════════════════════════════════════════════════════════════════
     *  MISC
     * ═════════════════════════════════════════════════════════════════ */

    else if (m_name == "ChatBypass") {
        // Always the same — just toggles the patch on/off
    }
}

void Module::applyStoredPreset() {
    applyPreset(simplePresetName());
}
