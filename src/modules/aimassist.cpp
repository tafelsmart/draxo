#include "pch.h"
#include "core/strcrypt.h"
#include "modules/aimassist.h"
#include "sdk/minecraft.h"
#include "sdk/entity.h"
#include "sdk/world.h"
#include <cmath>
#include <chrono>
#include <random>

AimAssist::AimAssist() : Module("Aim Assist", ModuleCategory::COMBAT, 0, "Smoothly pulls your crosshair toward nearby enemies for easier hits") {
    defineGroup("Aim");
    defineFloat("range",       "Range",          5.0f, 2.0f, 8.0f, "%.1f");
    defineFloat("fov",         "FOV",           90.0f, 15.0f, 360.0f, "%.0f");
    defineFloat("h_smooth",    "Horizontal Smooth", 4.0f, 0.5f, 20.0f, "%.1f");
    defineFloat("v_smooth",    "Vertical Smooth",   4.0f, 0.5f, 20.0f, "%.1f");
    defineGroupEnd();

    defineGroup("Attack");
    defineFloat("cps",         "CPS",           12.0f, 4.0f, 30.0f, "%.0f");
    defineFloat("cps_jitter",  "CPS Jitter %",   20.0f, 0.0f, 50.0f, "%.0f");
    defineBool("players",      "Players",        true);
    defineBool("mobs",         "Mobs",           false);
    defineBool("focus_only",   "Only Focused",    true);
    defineGroupEnd();

    defineGroup("TriggerBot");
    defineBool("triggerbot",   "TriggerBot",     false);
    defineMode("pvp_mode",     "PvP Mode",       0, {"1.8 PvP", "1.18 PvP"});
    defineGroupEnd();

    addSearchTag("aimbot");
    addSearchTag("smooth");
    addSearchTag("silent");
    addSearchTag("trigger");
    addSearchTag("autoattack");
}

float AimAssist::wrapAngleTo180(float angle) {
    angle = std::fmod(angle, 360.0f);
    if (angle >= 180.0f) angle -= 360.0f;
    if (angle < -180.0f) angle += 360.0f;
    return angle;
}

float AimAssist::getAngleDifference(float a, float b) {
    return wrapAngleTo180(a - b);
}

void AimAssist::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;

    // One-time JNI init — real attack via MultiPlayerGameMode.attack()
    if (!s_jni) {
        s_mcCls = JvmWrapper::findClass(Mappings::Minecraft_Class);
        if (s_mcCls) {
            s_gmField = env->GetFieldID(s_mcCls, Mappings::MC_gameMode, Mappings::MC_gameMode_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_gmField = nullptr; }
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
            // 1.18 PvP: getAttackStrengthScale(float) → 0..1 how ready the weapon is
            s_attackScale = env->GetMethodID(pCls, Mappings::Player_getAttackStrengthScale,
                                              Mappings::Player_getAttackStrengthScale_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_attackScale = nullptr; }
        }
        s_jni = true;
    }

    jobject mc = CMinecraft::getInstance();
    if (!mc) return;

    jobject playerObj = CMinecraft::getPlayer();
    if (!playerObj) { env->DeleteLocalRef(mc); return; }

    CEntity player(playerObj);
    float range   = m_floatSettings["range"];
    float fovDeg  = m_floatSettings["fov"];
    float hSmooth = m_floatSettings["h_smooth"];
    float vSmooth = m_floatSettings["v_smooth"];
    float cps     = m_floatSettings["cps"];
    float jitterPct = m_floatSettings["cps_jitter"] / 100.0f;
    bool targetP  = m_boolSettings["players"];
    bool targetM  = m_boolSettings["mobs"];
    bool focusOnly = m_boolSettings["focus_only"];

    // ── ⚠️ Detect warnings ─────────────────────────────────────────────
    if (fovDeg > 120.0f)
        addDetectWarning("FOV > 120 deg", true, "360-degree aim range is detectable by GrimAC/Matrix. Reduce to 90-120 for safety.");
    if (hSmooth < 2.0f)
        addDetectWarning("Smoothness < 2", true, "Instant snap-to-target is humanly impossible. Vulcan flags this. Raise to 3-5.");
    if (range > 6.0f)
        addDetectWarning("Range > 6 blocks", true, "Aim assist at extreme range is detectable. Reduce to 4-5.");

    if (focusOnly) {
        HWND h = FindWindowA("GLFW30", nullptr);
        if (!h || GetForegroundWindow() != h) {
            env->DeleteLocalRef(playerObj);
            env->DeleteLocalRef(mc);
            return;
        }
    }

    double px = player.getX(), py = player.getY() + 1.62, pz = player.getZ();
    float pYaw = player.getYaw(), pPitch = player.getPitch();

    // ── Target selection ──────────────────────────────────────────
    jobject worldObj = CMinecraft::getWorld();
    CEntity* bestTarget = nullptr;
    float bestFov = fovDeg;

    if (worldObj) {
        auto entities = CWorld::getAllEntities(worldObj);

        for (auto& entity : entities) {
            if (entity.getId() == player.getId() || !entity.isAlive() || !entity.isLiving()) continue;
            if (entity.isArmorStand()) continue;
            bool isP = entity.isPlayer();
            if (isP && !targetP) continue;
            if (!isP && !targetM) continue;

            double ex = entity.getX();
            double ey = entity.getY() + 1.0;
            double ez = entity.getZ();
            double dx = ex - px, dy = ey - py, dz = ez - pz;
            double dist = std::sqrt(dx*dx + dy*dy + dz*dz);
            if (dist > range) continue;

            float yawTo = (float)(std::atan2(dz, dx) * 180.0 / 3.14159265) - 90.0f;
            float yawDiff = std::abs(getAngleDifference(pYaw, yawTo));
            if (yawDiff < bestFov) { bestFov = yawDiff; bestTarget = &entity; }
        }
        env->DeleteLocalRef(worldObj);
    }

    // ── Smooth aim toward target ──────────────────────────────────
    if (bestTarget) {
        double tx = bestTarget->getX();
        double ty = bestTarget->getY() + 1.0;
        double tz = bestTarget->getZ();
        double dx = tx - px, dy = ty - py, dz = tz - pz;
        double hd = std::sqrt(dx*dx + dz*dz);
        float tYaw   = (float)(std::atan2(dz, dx) * 180.0 / 3.14159265) - 90.0f;
        float tPitch = (float)(-std::atan2(dy, hd) * 180.0 / 3.14159265);

        float yawDiff   = getAngleDifference(tYaw, pYaw);
        float pitchDiff = tPitch - pPitch;

        // Higher smoothness = slower aim (more human-like)
        float hStep = std::min(1.0f, 1.0f / hSmooth);
        float vStep = std::min(1.0f, 1.0f / vSmooth);
        player.setYaw(pYaw + yawDiff * hStep);
        player.setPitch(pPitch + pitchDiff * vStep);

        // ── TriggerBot: auto-attack when aimed close enough ──────
        bool triggerBot = m_boolSettings["triggerbot"];
        int  pvpMode   = m_intSettings["pvp_mode"];
        if (triggerBot && std::abs(yawDiff) < 15.0f && std::abs(pitchDiff) < 15.0f) {
            // ── 1.18 PvP: wait for weapon cooldown (max damage) ───
            if (pvpMode == 1 && s_attackScale) {
                float scale = env->CallFloatMethod(playerObj, s_attackScale, 0.0f);
                if (env->ExceptionCheck()) { env->ExceptionClear(); scale = 1.0f; }
                if (scale < 0.93f) {
                    // Cooldown not ready yet — don't attack, just keep aiming
                    env->DeleteLocalRef(playerObj);
                    env->DeleteLocalRef(mc);
                    return;
                }
            }

            auto now = std::chrono::steady_clock::now();
            long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count();

            static std::mt19937 rng(std::random_device{}());
            float jitter = 1.0f + (float)(rng() % 1000) / 1000.0f * jitterPct * 2.0f - jitterPct;
            long long delay = (long long)(1000.0f / (cps * jitter));
            if (delay < 20) delay = 20;

            if (ms - m_lastAttack >= delay && s_gmField && s_gmAttack) {
                m_lastAttack = ms;

                jobject gm = env->GetObjectField(mc, s_gmField);
                if (gm && !env->ExceptionCheck()) {
                    // Real attack: MultiPlayerGameMode.attack(player, entity)
                    env->CallVoidMethod(gm, s_gmAttack, playerObj, bestTarget->getObject());
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
            }
        }
    }

    env->DeleteLocalRef(playerObj);
    env->DeleteLocalRef(mc);
}
