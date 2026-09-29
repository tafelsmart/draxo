#include "pch.h"
#include "core/strcrypt.h"
#include "modules/autoclicker.h"
#include "sdk/minecraft.h"
#include "sdk/entity.h"
#include "core/jvm_wrapper.h"
#include <random>

AutoClicker::AutoClicker() : Module("AutoClicker", ModuleCategory::COMBAT, 0, "Clicks at a configurable rate with human-like jitter") {
    defineFloat("cps",         "CPS",          12.0f, 4.0f, 30.0f, "%.0f");
    defineFloat("cps_jitter",  "CPS Jitter %",  20.0f, 0.0f, 50.0f, "%.0f");
    defineBool("require_target","Require Target", false);
    defineBool("focus_only",   "Only Focused",   true);
    addSearchTag("click");
    addSearchTag("attack");
}

void AutoClicker::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;

    bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    if (!lmb) return;

    if (m_boolSettings["focus_only"]) {
        HWND h = FindWindowA("GLFW30", nullptr);
        if (!h || GetForegroundWindow() != h) return;
    }

    // One-time JNI init
    if (!s_jni) {
        s_mcCls = JvmWrapper::findClass(Mappings::Minecraft_Class);
        if (s_mcCls) {
            s_gmField = env->GetFieldID(s_mcCls, Mappings::MC_gameMode, Mappings::MC_gameMode_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_gmField = nullptr; }
            s_crosshair = env->GetFieldID(s_mcCls, Mappings::MC_crosshairPickEntity, Mappings::MC_crosshairPickEntity_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_crosshair = nullptr; }
        }
        s_gmCls = JvmWrapper::findClass(Mappings::GameMode_Class);
        if (s_gmCls) {
            s_gmAttack = env->GetMethodID(s_gmCls, Mappings::GameMode_attack, Mappings::GameMode_attack_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_gmAttack = nullptr; }
        }
        s_handCls = JvmWrapper::findClass(Mappings::InteractionHand_Class);
        if (s_handCls) {
            s_mainHand = env->GetStaticFieldID(s_handCls, Mappings::InteractionHand_MAIN_HAND,
                Mappings::InteractionHand_MAIN_HAND_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_mainHand = nullptr; }
        }
        jclass pCls = JvmWrapper::findClass(Mappings::Player_Class);
        if (pCls) {
            s_playerSwing = env->GetMethodID(pCls, Mappings::Player_swing, Mappings::Player_swing_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_playerSwing = nullptr; }
        }
        s_jni = true;
    }

    // Require target in crosshair
    if (m_boolSettings["require_target"] && s_crosshair) {
        jobject mc = CMinecraft::getInstance();
        if (!mc) return;
        jobject ent = env->GetObjectField(mc, s_crosshair);
        bool hasTarget = ent != nullptr;
        if (ent) env->DeleteLocalRef(ent);
        env->DeleteLocalRef(mc);
        if (!hasTarget) return;
    }

    // ── ⚠️ Detect warnings ─────────────────────────────────────────────
    float cps = m_floatSettings["cps"];
    float jitterPct = m_floatSettings["cps_jitter"] / 100.0f;
    if (cps > 15.0f)
        addDetectWarning("CPS > 15", true, "Sustained high CPS triggers Watchdog/GrimAC. Reduce to 12-14 for safety.");
    if (cps > 20.0f)
        addDetectWarning("CPS > 20", true, "Extreme CPS. All anticheats detect this. Max safe is 16-18.");
    if (m_floatSettings["cps_jitter"] < 5.0f)
        addDetectWarning("Jitter < 5%", true, "Near-perfect CPS timing is inhuman. Matrix/Vulcan flag this. Increase to 15-25%.");
    auto now = std::chrono::steady_clock::now();
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    static std::mt19937 rng(std::random_device{}());
    float jitter = 1.0f + (float)(rng() % 1000) / 1000.0f * jitterPct * 2.0f - jitterPct;
    long long delay = (long long)(1000.0f / (cps * jitter));
    if (delay < 20) delay = 20;

    if (ms - m_last >= delay && s_gmField && s_gmAttack) {
        m_last = ms;

        jobject mc = CMinecraft::getInstance();
        if (!mc) return;
        jobject playerObj = CMinecraft::getPlayer();
        if (!playerObj) { env->DeleteLocalRef(mc); return; }

        // Get crosshair entity
        jobject target = env->GetObjectField(mc, s_crosshair);
        if (target && !env->ExceptionCheck()) {
            jobject gm = env->GetObjectField(mc, s_gmField);
            if (gm && !env->ExceptionCheck()) {
                // Real attack: MultiPlayerGameMode.attack(player, entity)
                env->CallVoidMethod(gm, s_gmAttack, playerObj, target);
                if (env->ExceptionCheck()) env->ExceptionClear();
                env->DeleteLocalRef(gm);
            }

            // Swing arm
            if (s_handCls && s_mainHand && s_playerSwing) {
                jobject hand = env->GetStaticObjectField(s_handCls, s_mainHand);
                if (hand) {
                    env->CallVoidMethod(playerObj, s_playerSwing, hand);
                    if (env->ExceptionCheck()) env->ExceptionClear();
                    env->DeleteLocalRef(hand);
                }
            }
            env->DeleteLocalRef(target);
        } else {
            if (env->ExceptionCheck()) env->ExceptionClear();
        }

        env->DeleteLocalRef(playerObj);
        env->DeleteLocalRef(mc);
    }
}
