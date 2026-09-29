#include "pch.h"
#include "core/strcrypt.h"
#include "modules/glide.h"
#include "sdk/minecraft.h"
#include "sdk/entity.h"

Glide::Glide() : Module("Glide", ModuleCategory::MOVEMENT, 0, "Slows your fall so you glide down gently") {
    defineFloat("fall_speed", "Fall Speed", -0.1f, -1.0f, 0.0f, "%.1f");
    addSearchTag("float");
    addSearchTag("slow");
}

void Glide::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;

    // ── ⚠️ Detect warnings ─────────────────────────────────────────────
    float fs = m_floatSettings["fall_speed"];
    if (fs > -0.03f)
        addDetectWarning("Fall speed near zero", true,
            "Very slow falling is detectable as Fly by GrimAC/Vulcan. Keep at -0.05 or lower for safety.");

    jobject p = CMinecraft::getPlayer();
    if (!p) return;
    CEntity pl(p);
    if (pl.isOnGround()) { env->DeleteLocalRef(p); return; }
    CEntity::Vec3 d = pl.getDeltaMovement();
    if (d.y < fs) {
        d.y = fs;
        pl.setDeltaMovement(d.x, d.y, d.z);
    }
    if (GetAsyncKeyState(VK_SPACE)&0x8000) {
        d.y = 0.2f; pl.setDeltaMovement(d.x, d.y, d.z);
    }
    env->DeleteLocalRef(p);
}
