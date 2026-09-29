#include "pch.h"
#include "core/strcrypt.h"
#include "modules/freecam.h"
#include "sdk/minecraft.h"
#include "sdk/entity.h"

Freecam::Freecam() : Module("Freecam", ModuleCategory::PLAYER, 0, "Detaches the camera from your body to look around freely") {
    defineFloat("speed", "Speed", 1.0f, 0.1f, 5.0f, "%.1f");
    addSearchTag("camera");
    addSearchTag("spectator");
}

void Freecam::onEnable() {
    jobject p = CMinecraft::getPlayer();
    if (p) { CEntity pl(p); m_savedX=pl.getX();m_savedY=pl.getY();m_savedZ=pl.getZ(); JvmWrapper::getEnv()->DeleteLocalRef(p); }
}
void Freecam::onDisable() {}
void Freecam::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;
    jobject p = CMinecraft::getPlayer();
    if (!p) return;
    CEntity pl(p);
    float spd = m_floatSettings["speed"] * 0.5f;
    CEntity::Vec3 d = pl.getDeltaMovement();
    if (GetAsyncKeyState('W')&0x8000) d.z -= spd;
    if (GetAsyncKeyState('S')&0x8000) d.z += spd;
    if (GetAsyncKeyState('A')&0x8000) d.x -= spd;
    if (GetAsyncKeyState('D')&0x8000) d.x += spd;
    if (GetAsyncKeyState(VK_SPACE)&0x8000) d.y += spd;
    if (GetAsyncKeyState(VK_SHIFT)&0x8000) d.y -= spd;
    pl.setDeltaMovement(d.x, d.y, d.z);
    pl.setOnGround(true);
    env->DeleteLocalRef(p);
}
