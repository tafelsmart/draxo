#include "pch.h"
#include "core/strcrypt.h"
#include "modules/fastladder.h"
#include "sdk/minecraft.h"
#include "sdk/entity.h"

FastLadder::FastLadder() : Module("FastLadder", ModuleCategory::MOVEMENT, 0, "Climbs ladders noticeably faster") {
    defineFloat("speed", "Speed", 2.0f, 0.1f, 5.0f, "%.1f");
    addSearchTag("climb");
    addSearchTag("vine");
}

void FastLadder::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;

    // ── ⚠️ Detect warnings ─────────────────────────────────────────────
    float spd = m_floatSettings["speed"];
    if (spd > 2.0f)
        addDetectWarning("Speed > 2x", true, "Very fast ladder climb — detectable by GrimAC/Vulcan. Reduce to 1.5-2.0 for safety.");
    if (spd > 3.5f)
        addDetectWarning("Speed > 3.5x", true, "Impossible ladder speed. All ACs detect this. Reduce to 2.5 or below.");

    jobject p = CMinecraft::getPlayer();
    if (!p) return;
    CEntity pl(p);
    if (pl.horizontalCollision() && (GetAsyncKeyState('W')&0x8000||GetAsyncKeyState('S')&0x8000)) {
        CEntity::Vec3 d = pl.getDeltaMovement();
        d.y = (GetAsyncKeyState('W')&0x8000) ? m_floatSettings["speed"]*0.3f : -m_floatSettings["speed"]*0.3f;
        pl.setDeltaMovement(d.x, d.y, d.z);
    }
    env->DeleteLocalRef(p);
}
