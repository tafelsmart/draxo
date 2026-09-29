#include "pch.h"
#include "core/strcrypt.h"
#include "modules/blink.h"
#include "sdk/minecraft.h"
#include "sdk/entity.h"

Blink::Blink() : Module("Blink", ModuleCategory::EXPLOIT, 0, "Buffers your movement, then teleports when toggled off") {
    defineFloat("duration", "Duration (ms)", 2000.0f, 100.0f, 10000.0f, "%.0f");
    addSearchTag("lag");
    addSearchTag("teleport");
}

void Blink::onEnable()  {}
void Blink::onDisable() {}

void Blink::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;
    jobject p = CMinecraft::getPlayer();
    if (p) {
        CEntity pl(p);
        CEntity::Vec3 d = pl.getDeltaMovement();
        pl.setDeltaMovement(0, d.y, 0);
        pl.setDiscardFriction(true);
        env->DeleteLocalRef(p);
    }
}
