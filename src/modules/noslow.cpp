#include "pch.h"
#include "core/strcrypt.h"
#include "modules/noslow.h"
#include "sdk/minecraft.h"
#include "sdk/entity.h"

NoSlow::NoSlow() : Module("NoSlow", ModuleCategory::MOVEMENT, 0, "Removes movement slowdown while eating, blocking or bowing") {
    defineBool("eating",   "Eating",   true);
    defineBool("blocking", "Blocking", true);
    defineBool("bowing",   "Bowing",   true);
    addSearchTag("item");
    addSearchTag("use");
}

void NoSlow::onDisable() {
    JNIEnv* env = JvmWrapper::getEnv();
    if (!env) return;
    jobject p = CMinecraft::getPlayer();
    if (p) { CEntity pl(p); pl.setDiscardFriction(false); env->DeleteLocalRef(p); }
}

void NoSlow::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;
    bool rmb = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    bool apply = (rmb && m_boolSettings["blocking"]) ||
                 (rmb && m_boolSettings["eating"])   ||
                 (lmb && m_boolSettings["bowing"]);
    if (apply) {
        jobject p = CMinecraft::getPlayer();
        if (p) { CEntity pl(p); pl.setDiscardFriction(true); env->DeleteLocalRef(p); }
    }
}
