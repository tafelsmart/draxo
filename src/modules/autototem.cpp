#include "pch.h"
#include "core/strcrypt.h"
#include "modules/autototem.h"
#include "sdk/minecraft.h"
#include "core/jvm_wrapper.h"
#include "config/mappings.h"

AutoTotem::AutoTotem() : Module("AutoTotem", ModuleCategory::PLAYER, 0, "Automatically keeps a totem in your offhand") {
    setTickInterval(3);
    defineBool("while_moving", "While Moving", true);
    addSearchTag("offhand");
    addSearchTag("save");
}

void AutoTotem::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;
    jobject p = CMinecraft::getPlayer();
    if (!p) return;

    if (!s_init) {
        jclass playerCls = JvmWrapper::findClass(Mappings::Player_Class);
        if (playerCls) {
            s_getOffhand = env->GetMethodID(playerCls, Mappings::Player_getOffhandItem, Mappings::Player_getOffhandItem_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_getOffhand = nullptr; }
        }
        s_init = true;
    }
    if (!s_getOffhand) { env->DeleteLocalRef(p); return; }

    // Check offhand for totem
    jobject offhand = env->CallObjectMethod(p, s_getOffhand);
    if (offhand && !env->ExceptionCheck()) {
        jclass isCls = env->GetObjectClass(offhand);
        jmethodID getItem = env->GetMethodID(isCls, Mappings::ItemStack_getItem, Mappings::ItemStack_getItem_Sig);
        bool hasTotem = false;
        if (getItem && !env->ExceptionCheck()) {
            jobject item = env->CallObjectMethod(offhand, getItem);
            if (item && !env->ExceptionCheck()) {
                jclass iCls = env->GetObjectClass(item);
                jmethodID ts = env->GetMethodID(iCls, "toString", "()Ljava/lang/String;");
                if (ts && !env->ExceptionCheck()) {
                    jstring s = (jstring)env->CallObjectMethod(item, ts);
                    if (s && !env->ExceptionCheck()) {
                        std::string name = JvmWrapper::jstringToString(s);
                        if (name.find("totem") != std::string::npos) hasTotem = true;
                        env->DeleteLocalRef(s);
                    }
                    if (env->ExceptionCheck()) env->ExceptionClear();
                }
                env->DeleteLocalRef(iCls);
                env->DeleteLocalRef(item);
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(isCls);
        env->DeleteLocalRef(offhand);
        if (hasTotem) { env->DeleteLocalRef(p); return; }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();

    // Equip totem via F key — non-blocking (no Sleep on render thread)
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastF).count();
    if (elapsed > 500) {
        keybd_event('F', 0, 0, 0);
        m_fDown = true;
        m_lastF = now;
    }
    env->DeleteLocalRef(p);
}

void AutoTotem::onRender() {
    if (m_fDown) {
        keybd_event('F', 0, KEYEVENTF_KEYUP, 0);
        m_fDown = false;
    }
}
