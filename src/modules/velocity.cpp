#include "pch.h"
#include "core/strcrypt.h"
#include "core/ac_bypass.h"
#include "modules/velocity.h"
#include "modules/module_manager.h"
#include "sdk/minecraft.h"
#include "sdk/entity.h"
#include <cmath>

/*
 * Velocity — Humanisierte Knockback-Reduktion.
 *
 * Uses KBHumanizer (ac_bypass.h) for:
 *  → Multi-phase Reaction Curve (not flat reduction)
 *  → Per-hit randomization (±10%)
 *  → Overshoot pattern (20% chance, +15-30% over-correction)
 *  → Inertia smoothing (no hard jumps between ticks)
 *  → Natural decay after window end
 */

Velocity::Velocity() : Module("Velocity", ModuleCategory::COMBAT, 0, "Reduces or removes knockback — horizontal and vertical") {
    defineMode("mode", "Mode", 0, {"Reduce", "Cancel"});
    defineFloat("horizontal", "Horizontal %", 65.0f, 0.0f, 100.0f);
    defineFloat("vertical",   "Vertical %",   30.0f, 0.0f, 100.0f);
    defineBool("in_air_only", "Air Only", true);
    defineBool("randomize", "Humanize (±10%)", true);
    defineBool("skip_liquid", "Skip in Water/Lava", true);
    addSearchTag("kb");
    addSearchTag("knockback");
}

void Velocity::onEnable() {
    if (auto* a = ModuleManager::getModule("AntiKB"))
        if (a->isEnabled()) a->setEnabled(false);
    m_humanizer.reset();
}

void Velocity::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;
    jobject playerObj = CMinecraft::getPlayer();
    if (!playerObj) return;
    CEntity player(playerObj);

    CEntity::Vec3 d = player.getDeltaMovement();
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
        addDetectWarning("Cancel mode", true, "Near-full KB cancel is flagged by all anticheats. Reduce is the safe choice.");
    if (m_floatSettings["vertical"] > 60.0f)
        addDetectWarning("Vertical cancel > 60%", true, "Cancelling upward knockback looks like flying on Grim/Vulcan.");
    if (auto* a = ModuleManager::getModule("AntiKB"))
        if (a->isEnabled())
            addDetectWarning("AntiKB also enabled", true, "Velocity + AntiKB = double cancel = instant fly flag.");

    // ── Air-only check ───────────────────────────────────────────
    if (m_boolSettings["in_air_only"] && player.isOnGround()) {
        env->DeleteLocalRef(playerObj);
        return;
    }

    // ── Knockback detection → start humanized window ─────────────
    bool knocked = player.getHurtTime() > 0 || hSpeed > 0.4;
    bool idle    = !m_humanizer.isActive();
    if (knocked && idle) {
        m_humanizer.startWindow(6, h, v, m_boolSettings["randomize"]);
    }

    // ── Tick the humanizer ───────────────────────────────────────
    auto reduction = m_humanizer.tick();
    float hEff = reduction.horizontal;
    float vEff = reduction.vertical;

    // ── Condition overrides ──────────────────────────────────────
    if (m_boolSettings["skip_liquid"] && player.isInWater()) { hEff *= 0.5f; vEff *= 0.25f; }
    if (hEff > 0.98f) hEff = 0.98f;
    if (vEff > 0.90f) vEff = 0.90f;

    // ── Apply ────────────────────────────────────────────────────
    if (hEff > 0.001f) { d.x *= (1.0f - hEff); d.z *= (1.0f - hEff); }
    if (vEff > 0.001f) { d.y *= (1.0f - vEff); }
    player.setDeltaMovement(d.x, d.y, d.z);

    env->DeleteLocalRef(playerObj);
}
