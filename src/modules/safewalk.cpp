#include "pch.h"
#include "core/strcrypt.h"
#include "modules/safewalk.h"
#include "sdk/minecraft.h"
#include "sdk/entity.h"

SafeWalk::SafeWalk() : Module("SafeWalk", ModuleCategory::MOVEMENT, 0, "Prevents you from walking off block edges") {
    addSearchTag("bridge");
    addSearchTag("sneak");
}
void SafeWalk::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;
    jobject p = CMinecraft::getPlayer();
    if (p) { CEntity pl(p); if (pl.isOnGround()) pl.setDiscardFriction(true); env->DeleteLocalRef(p); }
}
