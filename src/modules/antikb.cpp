#include "pch.h"
#include "core/strcrypt.h"
#include "core/ac_bypass.h"
#include "modules/antikb.h"
#include "modules/module_manager.h"
#include "sdk/minecraft.h"
#include "sdk/entity.h"
#include <cmath>

/*
 * AntiKB — Humanisierte Knockback-Reduktion.
 *
 * Uses KBHumanizer (ac_bypass.h) for:
 *  → Multi-phase Reaction Curve (not flat reduction)
 *  → Per-hit randomization (±10%)
 *  → Overshoot pattern (20% chance, +15-30% over-correction)
 *  → Inertia smoothing (no hard jumps between ticks)
 *  → Natural decay after window end
 *
 * ACs that would otherwise flag constant KB reduction:
 *   GrimAC, Vulcan, Matrix, Spartan, Watchdog
 */

AntiKB::AntiKB() : Module("AntiKB", ModuleCategory::COMBAT, 0, "Reduces or cancels knockback during the hit window for better combos") {
    defineMode("mode", "Mode", 0, {"Reduce", "Cancel"});
    defineFloat("horizontal", "Horizontal %", 75.0f, 0.0f, 100.0f);
    defineFloat("vertical",   "Vertical %",   30.0f, 0.0f, 100.0f);
    defineInt("window", "Window Ticks", 8, 1, 15);
    defineBool("ground_only", "Ground Only", true);
    defineBool("randomize", "Humanize (±10%)", true);
    defineBool("skip_liquid", "Skip in Water/Lava", true);
    addSearchTag("velocity");
    addSearchTag("kb");
}

void AntiKB::onEnable() {
    if (auto* v = ModuleManager::getModule("Velocity"))
        if (v->isEnabled()) v->setEnabled(false);
    m_humanizer.reset();
}

void AntiKB::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;
    jobject p = CMinecraft::getPlayer();
    if (!p) return;
    CEntity pl(p);

    CEntity::Vec3 d = pl.getDeltaMovement();
    double hSpeed = std::sqrt(d.x * d.x + d.z * d.z);

    bool modeCancel = m_intSettings["mode"] == 1;
    float h = m_floatSettings["horizontal"] / 100.0f;
    float v = m_floatSettings["vertical"] / 100.0f;

    // ── Safety caps ──────────────────────────────────────────────
    if (modeCancel) {
        h = std::max(h, 0.85f); v = std::min(v, 0.60f);
    } else {
        h = std::min(h, 0.85f); v = std::min(v, 0.55f);
    }

    // ── Detect warnings ──────────────────────────────────────────
    if (modeCancel)
        addDetectWarning("Cancel mode", true, "Near-full KB cancel is flagged by all anticheats. Reduce (70-85%) is the safe choice.");
    if (m_floatSettings["vertical"] > 60.0f)
        addDetectWarning("Vertical cancel > 60%", true, "Cancelling upward knockback looks like flying on Grim/Vulcan.");
    if (m_floatSettings["horizontal"] > 85.0f)
        addDetectWarning("Horizontal cancel > 85%", true, "Very high KB reduction is detectable. 70-80% is safer.");
    if (auto* v = ModuleManager::getModule("Velocity"))
        if (v->isEnabled())
            addDetectWarning("Velocity also enabled", true, "AntiKB + Velocity = double cancel = instant fly flag.");

    // ── Knockback detection → start humanized window ─────────────
    bool knocked = pl.getHurtTime() > 0 || hSpeed > 0.4;
    bool idle    = !m_humanizer.isActive();
    if (knocked && idle) {
        m_humanizer.startWindow(
            m_intSettings["window"],
            h, v,
            m_boolSettings["randomize"]
        );
    }

    // ── Tick the humanizer ───────────────────────────────────────
    auto reduction = m_humanizer.tick();
    float hEff = reduction.horizontal;
    float vEff = reduction.vertical;

    // ── Condition overrides ──────────────────────────────────────
    bool inLiquid = m_boolSettings["skip_liquid"] && pl.isInWater();
    bool groundOk = !m_boolSettings["ground_only"] || pl.isOnGround();

    if (inLiquid) { hEff *= 0.5f; vEff *= 0.25f; }
    if (hEff > 0.98f) hEff = 0.98f;  // never full 100%
    if (vEff > 0.90f) vEff = 0.90f;

    // ── Apply ────────────────────────────────────────────────────
    if (groundOk && hEff > 0.001f) { d.x *= (1.0f - hEff); d.z *= (1.0f - hEff); }
    if (vEff > 0.001f)             { d.y *= (1.0f - vEff); }
    pl.setDeltaMovement(d.x, d.y, d.z);

    env->DeleteLocalRef(p);
}
