#include "pch.h"
#include "core/strcrypt.h"
#include "modules/hitboxes.h"
#include "sdk/minecraft.h"
#include "core/jvm_wrapper.h"
#include "config/mappings.h"

HitBoxes::HitBoxes() : Module("HitBoxes", ModuleCategory::COMBAT, 0, "Expands entity hitboxes to make targets easier to hit") {
    defineFloat("expand", "Expand", 0.3f, 0.0f, 2.0f, "%.1f");
    addSearchTag("reach");
    addSearchTag("range");
}

void HitBoxes::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;

    // ── ⚠️ Detect warnings ─────────────────────────────────────────────
    float exp = m_floatSettings["expand"];
    if (exp > 0.5f)
        addDetectWarning("Expand > 0.5 blocks", true,
            "Hitbox expansion above 0.5 blocks triggers reach checks on Watchdog/Vulcan.");
    if (exp > 1.0f)
        addDetectWarning("Expand > 1.0 blocks", true,
            "Extreme hitbox expansion — hits connect at impossible range. All ACs detect this.");

    // One-time JNI init
    if (!s_init) {
        jclass attrCls = JvmWrapper::findClass(Mappings::Attributes_Class);
        if (attrCls) {
            s_entityRange = env->GetStaticFieldID(attrCls,
                Mappings::Attributes_ENTITY_INTERACTION_RANGE,
                Mappings::Attributes_ENTITY_INTERACTION_RANGE_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_entityRange = nullptr; }
        }
        jobject pl = CMinecraft::getPlayer();
        if (pl) {
            jclass pc = env->GetObjectClass(pl);
            s_getAttr = env->GetMethodID(pc, Mappings::LivingEntity_getAttribute,
                                         Mappings::LivingEntity_getAttribute_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_getAttr = nullptr; }
            env->DeleteLocalRef(pc); env->DeleteLocalRef(pl);
        }
        jclass aiCls = JvmWrapper::findClass(Mappings::AttributeInstance_Class);
        if (aiCls) {
            s_setBase = env->GetMethodID(aiCls, Mappings::AttributeInstance_setBaseValue,
                                         Mappings::AttributeInstance_setBaseValue_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_setBase = nullptr; }
        }
        s_init = true;
    }

    if (!s_entityRange || !s_getAttr || !s_setBase) return;
    jclass attrCls = JvmWrapper::findClass(Mappings::Attributes_Class);
    if (!attrCls) return;

    jobject p = CMinecraft::getPlayer();
    if (!p) return;

    jobject holder = env->GetStaticObjectField(attrCls, s_entityRange);
    if (holder) {
        jobject ai = env->CallObjectMethod(p, s_getAttr, holder);
        if (ai) {
            double base = 3.0 + exp;
            env->CallVoidMethod(ai, s_setBase, base);
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(ai);
        }
        env->DeleteLocalRef(holder);
    }
    env->DeleteLocalRef(p);
}
