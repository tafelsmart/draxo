#include "pch.h"
#include "core/strcrypt.h"
#include "modules/triggerbot.h"
#include "sdk/minecraft.h"
#include "sdk/entity.h"
#include "core/jvm_wrapper.h"
#include <random>

TriggerBot::TriggerBot() : Module("TriggerBot", ModuleCategory::COMBAT, 0, "Attacks the instant your crosshair is over an enemy") {
    defineFloat("cps",       "CPS",        12.0f, 4.0f, 30.0f, "%.0f");
    defineFloat("cps_jitter","CPS Jitter %", 20.0f, 0.0f, 50.0f, "%.0f");
    defineFloat("range",     "Range",       4.5f, 2.0f, 8.0f, "%.1f");
    defineBool("focus_only", "Only Focused", true);
    addSearchTag("autoattack");
    addSearchTag("crosshair");
}

void TriggerBot::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;

    // One-time JNI init — real attack via MultiPlayerGameMode.attack()
    if (!s_jniInit) {
        s_mcCls = JvmWrapper::findClass(Mappings::Minecraft_Class);
        if (s_mcCls) {
            s_crosshair = env->GetFieldID(s_mcCls, Mappings::MC_crosshairPickEntity, Mappings::MC_crosshairPickEntity_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_crosshair = nullptr; }
            s_gmField = env->GetFieldID(s_mcCls, Mappings::MC_gameMode, Mappings::MC_gameMode_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_gmField = nullptr; }
        }
        s_gmCls = JvmWrapper::findClass(Mappings::GameMode_Class);
        if (s_gmCls) {
            s_gmAttack = env->GetMethodID(s_gmCls, Mappings::GameMode_attack, Mappings::GameMode_attack_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_gmAttack = nullptr; }
        }
        // Swing
        s_handCls = JvmWrapper::findClass(Mappings::InteractionHand_Class);
        if (s_handCls) {
            s_mainHand = env->GetStaticFieldID(s_handCls, Mappings::InteractionHand_MAIN_HAND,
                Mappings::InteractionHand_MAIN_HAND_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_mainHand = nullptr; }
        }
        jclass playerCls = JvmWrapper::findClass(Mappings::Player_Class);
        if (playerCls) {
            s_playerSwing = env->GetMethodID(playerCls, Mappings::Player_swing, Mappings::Player_swing_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_playerSwing = nullptr; }
        }
        s_jniInit = true;
    }

    if (!s_crosshair || !s_gmField || !s_gmAttack) return;

    if (m_boolSettings["focus_only"]) {
        HWND h = FindWindowA("GLFW30", nullptr);
        if (!h || GetForegroundWindow() != h) return;
    }

    jobject mc = CMinecraft::getInstance();
    if (!mc) return;

    jobject ent = env->GetObjectField(mc, s_crosshair);
    if (!ent || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(mc);
        return;
    }

    // Check if entity is within range
    CEntity target(ent);
    jobject playerObj = CMinecraft::getPlayer();
    if (!playerObj) {
        env->DeleteLocalRef(ent);
        env->DeleteLocalRef(mc);
        return;
    }
    CEntity player(playerObj);
    double dx = target.getX() - player.getX();
    double dy = target.getY() - player.getY();
    double dz = target.getZ() - player.getZ();
    double dist = sqrt(dx*dx + dy*dy + dz*dz);
    if (dist > m_floatSettings["range"]) {
        env->DeleteLocalRef(playerObj);
        env->DeleteLocalRef(ent);
        env->DeleteLocalRef(mc);
        return;
    }

    // ── ⚠️ Detect warnings ─────────────────────────────────────────────
    float cps = m_floatSettings["cps"];
    float jitterPct = m_floatSettings["cps_jitter"] / 100.0f;
    if (cps > 15.0f)
        addDetectWarning("CPS > 15", true, "High CPS TriggerBot is detectable. Reduce to 10-14 for safety.");
    if (m_floatSettings["cps_jitter"] < 5.0f)
        addDetectWarning("Jitter < 5%", true, "Perfect attack timing — Matrix detects this. Increase jitter to 15-25%.");
    if (m_floatSettings["range"] > 5.0f)
        addDetectWarning("Range > 5 blocks", true, "Attacking beyond visual range is detectable by Watchdog.");
    auto now = std::chrono::steady_clock::now();
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    static std::mt19937 rng(std::random_device{}());
    float jitter = 1.0f + (float)(rng() % 1000) / 1000.0f * jitterPct * 2.0f - jitterPct;
    long long delay = (long long)(1000.0f / (cps * jitter));

    static long long m_last = 0;
    if (ms - m_last >= delay) {
        m_last = ms;

        // Real attack via MultiPlayerGameMode.attack(player, entity)
        jobject gm = env->GetObjectField(mc, s_gmField);
        if (gm && !env->ExceptionCheck()) {
            env->CallVoidMethod(gm, s_gmAttack, playerObj, ent);
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(gm);
        }

        // Swing arm for visual feedback
        if (s_handCls && s_mainHand && s_playerSwing) {
            jobject hand = env->GetStaticObjectField(s_handCls, s_mainHand);
            if (hand) {
                env->CallVoidMethod(playerObj, s_playerSwing, hand);
                if (env->ExceptionCheck()) env->ExceptionClear();
                env->DeleteLocalRef(hand);
            }
        }
    }

    env->DeleteLocalRef(playerObj);
    env->DeleteLocalRef(ent);
    env->DeleteLocalRef(mc);
}
