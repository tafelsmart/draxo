#include "pch.h"
#include "core/strcrypt.h"
#include "modules/dolphin.h"
#include "sdk/minecraft.h"
#include "sdk/entity.h"
#include <cmath>

Dolphin::Dolphin() : Module("Dolphin", ModuleCategory::MOVEMENT, 0, "Swims through water at high speed") {
    defineFloat("speed", "Speed", 2.0f, 0.1f, 5.0f, "%.1f");
    addSearchTag("water");
    addSearchTag("swim");
}

void Dolphin::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;
    jobject p = CMinecraft::getPlayer();
    if (!p) return;
    CEntity pl(p);
    if (!pl.isInWater()) { env->DeleteLocalRef(p); return; }
    float spd = m_floatSettings["speed"] * 0.15f;
    CEntity::Vec3 d = pl.getDeltaMovement();
    float yaw = pl.getYaw(); double yr = yaw * 3.14159265 / 180.0;
    if (GetAsyncKeyState('W')&0x8000) { d.x -= sin(yr)*spd; d.z += cos(yr)*spd; }
    if (GetAsyncKeyState(VK_SPACE)&0x8000) d.y = spd;
    pl.setDeltaMovement(d.x, d.y, d.z);
    env->DeleteLocalRef(p);
}
