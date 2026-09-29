#include "pch.h"
#include "core/strcrypt.h"
#include "modules/step.h"
#include "sdk/minecraft.h"
#include "sdk/entity.h"

Step::Step() : Module("Step", ModuleCategory::MOVEMENT, 0, "Automatically steps up blocks up to the set height") {
    defineFloat("height", "Height", 1.5f, 0.5f, 3.0f, "%.1f");
    addSearchTag("jump");
}

void Step::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;

    // ── ⚠️ Detect warnings ─────────────────────────────────────────────
    float h = m_floatSettings["height"];
    if (h > 2.0f)
        addDetectWarning("Height > 2.0 blocks", true,
            "Stepping more than 2 blocks is detected by Vulcan/Matrix as Speed/Fly. Reduce to 1.5-2.0.");
    if (h > 2.5f)
        addDetectWarning("Height > 2.5 blocks", true,
            "Impossible step height. All anticheats detect this. Max safe is 2.0 blocks.");

    jobject p = CMinecraft::getPlayer();
    if (!p) return;
    CEntity pl(p);
    if (pl.horizontalCollision() && pl.isOnGround()) {
        CEntity::Vec3 d = pl.getDeltaMovement();
        d.y = h * 0.42f;
        pl.setDeltaMovement(d.x, d.y, d.z);
    }
    env->DeleteLocalRef(p);
}
