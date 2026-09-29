#include "pch.h"
#include "core/strcrypt.h"
#include "modules/sprint.h"
#include "sdk/minecraft.h"
#include "sdk/entity.h"

Sprint::Sprint() : Module("Sprint", ModuleCategory::MOVEMENT, 0, "Keeps you sprinting — Normal or Omnisprint") {
    defineMode("mode", "Mode", 0, {"Normal", "Omnisprint"});
    addSearchTag("speed");
    addSearchTag("rage");
}

void Sprint::onDisable() {
    JNIEnv* env = JvmWrapper::getEnv();
    if (!env) return;
    jobject p = CMinecraft::getPlayer();
    if (p) { CEntity pl(p); pl.setDiscardFriction(false); env->DeleteLocalRef(p); }
}

void Sprint::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;

    // ── ⚠️ Detect warnings ─────────────────────────────────────────────
    int mode = m_intSettings["mode"];
    if (mode == 1)
        addDetectWarning("Omnisprint active", true,
            "Sprinting in all directions (strafing) is detectable by GrimAC/Matrix. Normal mode is safe.");

    jobject playerObj = CMinecraft::getPlayer();
    if (!playerObj) return;
    CEntity player(playerObj);

    bool omni = (mode == 1);  // Omnisprint = all directions
    bool fwd = (GetAsyncKeyState('W') & 0x8000) != 0;
    bool back= (GetAsyncKeyState('S') & 0x8000) != 0;
    bool left= (GetAsyncKeyState('A') & 0x8000) != 0;
    bool right=(GetAsyncKeyState('D')& 0x8000) != 0;

    bool moving = fwd || (omni && (back || left || right));

    if (moving && !player.isShiftKeyDown() && !player.horizontalCollision()) {
        player.setDiscardFriction(true);
        m_wasSprinting = true;
    } else if (m_wasSprinting) {
        player.setDiscardFriction(false);
        m_wasSprinting = false;
    }
    env->DeleteLocalRef(playerObj);
}
