#include "pch.h"
#include "core/strcrypt.h"
#include "modules/jesus.h"
#include "sdk/minecraft.h"
#include "sdk/entity.h"

Jesus::Jesus() : Module("Jesus", ModuleCategory::MOVEMENT, 0, "Lets you walk on water without sinking") {
    defineBool("solid", "Solid Walk", true);
    defineFloat("bounce", "Bounce", 0.11f, 0.0f, 0.3f, "%.2f");
    addSearchTag("water");
    addSearchTag("walk");
}

void Jesus::onEnable()  { m_wasOnGround = false; }
void Jesus::onDisable() { m_wasOnGround = false; }

void Jesus::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;

    // ── ⚠️ Detect warnings ─────────────────────────────────────────────
    float bounce = m_floatSettings["bounce"];
    if (bounce > 0.15f)
        addDetectWarning("Bounce > 0.15", true,
            "High water bounce looks like flying — detectable by GrimAC/Vulcan. Keep at 0.10-0.13.");
    if (bounce > 0.25f)
        addDetectWarning("Bounce > 0.25", true,
            "Extreme bounce on water. All ACs detect this as Fly.");

    jobject playerObj = CMinecraft::getPlayer();
    if (!playerObj) return;
    CEntity player(playerObj);
    if (!player.isInWater() || !m_boolSettings["solid"] || player.isShiftKeyDown()) {
        env->DeleteLocalRef(playerObj); return;
    }
    if (player.horizontalCollision()) { env->DeleteLocalRef(playerObj); return; }
    CEntity::Vec3 d = player.getDeltaMovement();
    if (d.y < 0.0) {
        d.y = bounce;
        player.setDeltaMovement(d.x, d.y, d.z);
    }
    if (!m_wasOnGround) { player.setOnGround(true); m_wasOnGround = true; }
    env->DeleteLocalRef(playerObj);
}
