#include "pch.h"
#include "core/strcrypt.h"
#include "modules/noportal.h"
#include "sdk/minecraft.h"
#include "core/jvm_wrapper.h"

NoPortal::NoPortal() : Module("NoPortal", ModuleCategory::RENDER, 0,
    "Removes the purple portal overlay distortion when going through nether portals.") {
    setTickInterval(1);
    addSearchTag("portal");
    addSearchTag("overlay");
    addSearchTag("nether");
}

void NoPortal::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;

    jobject player = CMinecraft::getPlayer();
    if (!player) return;

    // Entity.portalTime — set to 0 to remove overlay
    static jfieldID s_portalTime = nullptr;
    static bool s_tried = false;
    if (!s_tried) {
        s_tried = true;
        jclass entCls = JvmWrapper::findClass(Mappings::Entity_Class);
        if (entCls) {
            s_portalTime = env->GetFieldID(entCls, "portalTime", "I");
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                // 1.12.2: "timeInPortal"
                s_portalTime = env->GetFieldID(entCls, "timeInPortal", "I");
                if (env->ExceptionCheck()) env->ExceptionClear();
            }
        }
    }
    if (!s_portalTime) return;

    env->SetIntField(player, s_portalTime, 0);
    if (env->ExceptionCheck()) env->ExceptionClear();
}
