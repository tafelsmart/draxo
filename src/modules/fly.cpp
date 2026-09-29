#include "pch.h"
#include "core/strcrypt.h"
#include "modules/fly.h"
#include "sdk/minecraft.h"
#include "sdk/entity.h"
#include <cmath>

Fly::Fly() : Module("Fly", ModuleCategory::MOVEMENT, 0, "Lets you fly — Vanilla, Packet or Motion mode") {
    defineMode("mode", "Mode", 0, {"Vanilla", "Packet", "Motion"});
    defineFloat("speed", "Speed", 1.0f, 0.1f, 5.0f, "%.1f");
    addSearchTag("flight");
    addSearchTag("air");
}

void Fly::onEnable() {
    JNIEnv* env = JvmWrapper::getEnv();
    jobject p = CMinecraft::getPlayer();
    if (p) { CEntity pl(p); pl.setDiscardFriction(true); env->DeleteLocalRef(p); }
}
void Fly::onDisable() {
    JNIEnv* env = JvmWrapper::getEnv();
    jobject p = CMinecraft::getPlayer();
    if (p) { CEntity pl(p); pl.setDiscardFriction(false); env->DeleteLocalRef(p); }
}

void Fly::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;

    // ── Detect warnings ──────────────────────────────────────────────
    int mode = m_intSettings["mode"];
    float speed = m_floatSettings["speed"];

    if (speed > 2.5f)
        addDetectWarning("Speed > 2.5", true,
            "Flight speed above 2.5x — Vulcan/GrimAC detect this instantly.");
    if (speed > 4.0f)
        addDetectWarning("Speed > 4.0", true,
            "Extreme flight speed. All anticheats flag this as impossible movement.");
    if (mode == 0 && speed > 1.5f)
        addDetectWarning("Vanilla Mode > 1.5x", true,
            "Vanilla fly at >1.5x speed is trivial for GrimAC to detect. Use Packet mode.");
    if (mode == 2)
        addDetectWarning("Motion Mode active", true,
            "Motion mode is the most detectable fly mode. Use Vanilla or Packet for safety.");

    jobject p = CMinecraft::getPlayer();
    if (!p) return;
    CEntity pl(p);

    float spd = speed * 0.4f;
    CEntity::Vec3 d = pl.getDeltaMovement();
    d.y = 0;

    bool fwd=(GetAsyncKeyState('W')&0x8000)!=0, back=(GetAsyncKeyState('S')&0x8000)!=0;
    bool left=(GetAsyncKeyState('A')&0x8000)!=0, right=(GetAsyncKeyState('D')&0x8000)!=0;
    bool jump=(GetAsyncKeyState(VK_SPACE)&0x8000)!=0, sneak=(GetAsyncKeyState(VK_SHIFT)&0x8000)!=0;
    if (jump) d.y = spd;
    if (sneak) d.y = -spd;

    float yaw = pl.getYaw(); double yr = yaw * 3.14159265 / 180.0;
    double sy=sin(yr), cy=cos(yr), mx=0, mz=0;
    if (fwd){mx-=sy;mz+=cy;} if(back){mx+=sy;mz-=cy;}
    if(left){mx+=cy;mz+=sy;} if(right){mx-=cy;mz-=sy;}
    double len=sqrt(mx*mx+mz*mz);
    if(len>0){mx/=len;mz/=len; d.x=mx*spd; d.z=mz*spd;}

    if (mode == (int)Mode::Vanilla) {
        // Vanilla: set onGround to trick the server into allowing flight
        pl.setDiscardFriction(true);
        pl.setOnGround(true);
        pl.setDeltaMovement(d.x, d.y, d.z);
    } else if (mode == (int)Mode::Packet) {
        // Packet: set position Y slightly upward each tick (packet-based)
        pl.setDiscardFriction(true);
        pl.setOnGround(true);
        if (!jump && !sneak) d.y = -0.002;  // Counter gravity
        pl.setDeltaMovement(d.x, d.y, d.z);
    } else {
        // Motion: raw motion manipulation (most detectable but smoothest)
        pl.setDiscardFriction(true);
        pl.setOnGround(false);  // Don't fake ground in motion mode
        // Allow falling naturally when no up/down key pressed
        if (!jump && !sneak) d.y = pl.getDeltaMovement().y;
        pl.setDeltaMovement(d.x, d.y, d.z);
    }

    env->DeleteLocalRef(p);
}
