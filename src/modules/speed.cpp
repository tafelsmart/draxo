#include "pch.h"
#include "core/strcrypt.h"
#include "modules/speed.h"
#include "sdk/minecraft.h"
#include "sdk/entity.h"
#include <cmath>

Speed::Speed() : Module("Speed", ModuleCategory::MOVEMENT, 0, "Increases movement speed — Multiplier, BHop or Strafe") {
    defineMode("mode", "Mode", 0, {"Multiplier", "BHop", "Strafe"});
    defineFloat("multiplier", "Speed Multiplier", 1.5f, 0.1f, 5.0f, "%.1f");
    defineFloat("bhop_speed", "BHop Speed",    0.38f, 0.10f, 1.0f, "%.2f");
    defineFloat("strafe_speed","Strafe Speed",  0.22f, 0.05f, 0.50f, "%.2f");
    addSearchTag("boost");
    addSearchTag("bhop");
    addSearchTag("strafe");
}

void Speed::onDisable() {
    JNIEnv* env = JvmWrapper::getEnv();
    if (!env) return;
    jobject p = CMinecraft::getPlayer();
    if (p) { CEntity pl(p); pl.setDiscardFriction(false); env->DeleteLocalRef(p); }
    m_bhopStage = 0;
}

void Speed::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;

    // ── Detect warnings ──────────────────────────────────────────────
    float mult = m_floatSettings["multiplier"];
    float bhop  = m_floatSettings["bhop_speed"];
    float strafe = m_floatSettings["strafe_speed"];
    int mode = m_intSettings["mode"];

    if (mode == 0 && mult > 1.8f)
        addDetectWarning("Multiplier > 1.8x", true,
            "Speed multiplier above 1.8x is flagged by GrimAC/Vulcan. Reduce to 1.5-1.8.");
    if (mode == 0 && mult > 2.5f)
        addDetectWarning("Multiplier > 2.5x", true,
            "Extreme speed multiplier. Watchdog will accumulate points rapidly.");
    if (mode == 1 && bhop > 0.45f)
        addDetectWarning("BHop Speed > 0.45", true,
            "High BHop speed triggers GrimAC physics-check. Stick to 0.35-0.42.");
    if (mode == 1 && bhop > 0.60f)
        addDetectWarning("BHop Speed > 0.60", true,
            "Extreme BHop speed — all ACs detect this instantly.");
    if (mode == 2 && strafe > 0.30f)
        addDetectWarning("Strafe Speed > 0.30", true,
            "High strafe speed is detectable by Vulcan/Matrix. Reduce to 0.20-0.28.");

    jobject playerObj = CMinecraft::getPlayer();
    if (!playerObj) return;
    CEntity player(playerObj);

    bool fwd = (GetAsyncKeyState('W') & 0x8000) != 0;
    bool back = (GetAsyncKeyState('S') & 0x8000) != 0;
    bool left = (GetAsyncKeyState('A') & 0x8000) != 0;
    bool right= (GetAsyncKeyState('D') & 0x8000) != 0;
    if (!fwd && !back && !left && !right) {
        env->DeleteLocalRef(playerObj); return;
    }

    float yaw = player.getYaw();
    double yr = yaw * (3.14159265 / 180.0);
    double sy = sin(yr), cy = cos(yr), mx = 0, mz = 0;
    if (fwd)  { mx -= sy; mz += cy; }
    if (back) { mx += sy; mz -= cy; }
    if (left) { mx += cy; mz += sy; }
    if (right){ mx -= cy; mz -= sy; }
    double len = sqrt(mx*mx + mz*mz);
    if (len > 0) { mx /= len; mz /= len; }

    CEntity::Vec3 delta = player.getDeltaMovement();

    if (mode == (int)Mode::BHop) {
        // BHop: auto-jump + speed boost on ground, maintain momentum in air
        float spd = m_floatSettings["bhop_speed"];
        if (player.isOnGround()) {
            m_bhopStage++;
            if (m_bhopStage >= 2) {
                player.setJumping(true);
                m_bhopStage = 0;
            }
            player.setDiscardFriction(true);
            double base = sqrt(delta.x*delta.x + delta.z*delta.z);
            if (base < 0.25) base = 0.25;
            double boost = base * (1.0 + spd * 0.6);
            player.setDeltaMovement(mx * boost, delta.y, mz * boost);
        } else {
            // In air: maintain horizontal speed
            double airSpeed = sqrt(delta.x*delta.x + delta.z*delta.z);
            if (airSpeed < spd * 0.8) {
                player.setDeltaMovement(mx * spd, delta.y, mz * spd);
            }
        }
    } else if (mode == (int)Mode::Strafe) {
        // Strafe: smooth speed increase while moving (ground only)
        float spd = m_floatSettings["strafe_speed"];
        player.setDiscardFriction(true);
        double base = sqrt(delta.x*delta.x + delta.z*delta.z);
        if (base < 0.25) base = 0.25;
        double boost = base * (1.0 + spd);
        if (boost > 1.2) boost = 1.2;  // Cap for legit feel
        player.setDeltaMovement(mx * boost, delta.y, mz * boost);
    } else {
        // Multiplier: simple speed boost
        player.setDiscardFriction(true);
        double baseSpeed = sqrt(delta.x*delta.x + delta.z*delta.z);
        if (baseSpeed < 0.3) baseSpeed = 0.3;
        player.setDeltaMovement(mx * baseSpeed * mult, delta.y, mz * baseSpeed * mult);
    }

    env->DeleteLocalRef(playerObj);
}
