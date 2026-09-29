#include "pch.h"
#include "core/strcrypt.h"
#include "modules/airjump.h"
#include "sdk/minecraft.h"
#include "sdk/entity.h"

AirJump::AirJump() : Module("AirJump", ModuleCategory::MOVEMENT, 0, "Jump again while in mid-air (double jump)") {
    addSearchTag("double");
    addSearchTag("fly");
}
void AirJump::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;
    jobject p = CMinecraft::getPlayer();
    if (!p) return;
    CEntity pl(p);
    if ((GetAsyncKeyState(VK_SPACE)&0x8000) && !pl.isOnGround()) {
        CEntity::Vec3 d = pl.getDeltaMovement(); d.y = 0.42;
        pl.setDeltaMovement(d.x, d.y, d.z);
    }
    env->DeleteLocalRef(p);
}
