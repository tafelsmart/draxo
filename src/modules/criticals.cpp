#include "pch.h"
#include "core/strcrypt.h"
#include "modules/criticals.h"
#include "sdk/minecraft.h"
#include "sdk/entity.h"

Criticals::Criticals() : Module("Criticals", ModuleCategory::COMBAT, 0, "Makes every attack land as a critical hit") {
    defineBool("mini_jump", "Mini Jump", true);
    defineBool("packet", "Packet Crit", false);
    addSearchTag("crit");
    addSearchTag("damage");
}

void Criticals::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;

    // ── ⚠️ Detect warnings ─────────────────────────────────────────────
    if (m_boolSettings["packet"])
        addDetectWarning("Packet crits active", true,
            "Packet criticals send fake ground position — detectable by GrimAC/Vulcan. Mini Jump mode is safer.");

    bool atk = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    if (!atk) return;

    jobject p = CMinecraft::getPlayer();
    if (!p) return;
    CEntity pl(p);

    if (pl.isOnGround() && m_boolSettings["mini_jump"]) {
        CEntity::Vec3 d = pl.getDeltaMovement();
        d.y = 0.001;   // minimal hop = triggers critical
        pl.setDeltaMovement(d.x, d.y, d.z);
    }

    env->DeleteLocalRef(p);
}
