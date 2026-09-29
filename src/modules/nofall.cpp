#include "pch.h"
#include "core/strcrypt.h"
#include "modules/nofall.h"
#include "sdk/minecraft.h"
#include "sdk/entity.h"

NoFall::NoFall() : Module("NoFall", ModuleCategory::PLAYER, 0, "Prevents fall damage with ground spoofing") {
    defineBool("ground_spoof", "Ground Spoof", true);
    addSearchTag("damage");
    addSearchTag("safe");
}

void NoFall::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;
    jobject p = CMinecraft::getPlayer();
    if (!p) return;
    CEntity pl(p);
    CEntity::Vec3 d = pl.getDeltaMovement();
    if (d.y < -0.5 && m_boolSettings["ground_spoof"]) pl.setOnGround(true);
    env->DeleteLocalRef(p);
}
