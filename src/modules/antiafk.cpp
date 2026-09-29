#include "pch.h"
#include "core/strcrypt.h"
#include "modules/antiafk.h"
#include "sdk/minecraft.h"
#include "sdk/entity.h"

AntiAFK::AntiAFK() : Module("AntiAFK", ModuleCategory::MISC, 0, "Prevents server AFK kicks with periodic movement") {
    setTickInterval(60);
    addSearchTag("idle");
    addSearchTag("kick");
}

void AntiAFK::onDisable() { m_wasMoving = false; }
void AntiAFK::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;
    jobject p = CMinecraft::getPlayer();
    if (!p) return;
    CEntity pl(p);
    static auto last = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count() > 5000) {
        last = now; m_wasMoving = !m_wasMoving;
        CEntity::Vec3 d = pl.getDeltaMovement();
        if (m_wasMoving) pl.setDeltaMovement(d.x + 0.001, d.y, d.z + 0.001);
        else pl.setDeltaMovement(d.x - 0.001, d.y, d.z - 0.001);
    }
    env->DeleteLocalRef(p);
}
