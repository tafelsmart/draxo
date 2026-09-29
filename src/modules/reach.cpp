#include "pch.h"
#include "core/strcrypt.h"
#include "modules/reach.h"
#include "sdk/minecraft.h"
#include "sdk/entity.h"
#include "sdk/world.h"
#include "core/watchdog_bypass.h"
#include "core/ac_humanizer.h"
#include <cmath>

Reach::Reach() : Module("Reach", ModuleCategory::COMBAT, 0, "Extends your attack reach beyond the vanilla limit") {
    defineGroup("Range");
    defineFloat("reach_distance", "Reach Distance", 3.5f, 3.0f, 6.0f, "%.1f");
    defineBool("tp_reach", "TP Reach", true);
    defineGroupEnd();
    addSearchTag("range");
    addSearchTag("attack");
    addSearchTag("distance");
    addSearchTag("hit");
}

void Reach::onEnable() {
    // ── Reset humanization state ─────────────────────────────────
    m_reachFrames       = 0;
    m_overshootTimer    = (int)ac::humRand(30, 60);  // 0.6-1.2s until first overshoot
    m_overshootFrames   = 0;
    m_reachOvershootAmt = 0;
    m_curReachLevel     = 0;  // start at vanilla, ramp up
    m_prevReachDelta    = 0;
    m_lastReachSetMs    = 0;
}

void Reach::onDisable() {
    JNIEnv* env = JvmWrapper::getEnv();
    if (env) {
        setReach(env, 3.0, 4.5); // Vanilla defaults
    }
    m_curReachLevel = 0;
}

void Reach::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;

    // ── Humanized Reach Wobbler: ±0.03-0.08 blocks per-tick ─────
    // 2-octave noise + occasional micro-slips at higher amplitude
    // than the default ReachWobbler, as user requested stronger variation.
    static ac::ReachWobbler wobbler;
    static float wobblerDtAccum = 0;
    static bool wobblerInit = false;
    if (!wobblerInit) {
        wobbler.reset();
        wobblerInit = true;
    }
    wobblerDtAccum += 1.0f / 60.0f;
    // Amplify: multiply base wobble by 2-4× to get ±0.03-0.08 range
    float baseWobble = wobbler.wobble(wobblerDtAccum);
    float wobbleAmplifier = 2.0f + ac::humRand(0.0f, 2.0f);  // 2×–4×
    float wobbleOffset = baseWobble * wobbleAmplifier;
    // Clamp to requested range
    if      (wobbleOffset >  0.08f) wobbleOffset =  0.08f;
    else if (wobbleOffset < -0.08f) wobbleOffset = -0.08f;
    if (wobbleOffset > -0.03f && wobbleOffset < 0.03f) {
        wobbleOffset = (wobbleOffset < 0) ? -0.03f : 0.03f;  // ensure at least ±0.03
    }
    wobblerDtAccum = 0;

    float baseReach    = m_floatSettings["reach_distance"];
    float rawReachDist = baseReach + wobbleOffset;
    if (rawReachDist <= 3.0f) {
        setReach(env, 3.0, 4.5);
        return;
    }

    // ── Human Reaction Curve ─────────────────────────────────────
    // Instead of jumping from 3.0 to 3.5 instantly (bot-fingerprint),
    // the effective reach ramps through 4 human-like phases:
    //
    // Phase 0 (0-5  frames): REACTION  — 10-25% of extra reach
    // Phase 1 (5-15 frames): ACCEL     — S-curve ramp to 70-90%
    // Phase 2 (15-50 frames): CRUISING — 80-95% with micro-jitter
    // Phase 3 (50+ frames): FINE-TUNE — 90-105% with overshoot
    //
    // Overshoot: every 30-60 frames, briefly reach +5-15% further,
    // then correct back over 8-15 frames (human overcorrection).
    // Inertia: 15-25% of last frame's direction carries over.
    m_reachFrames++;
    float targetLevel = 1.0f;  // target: 100% of baseReach
    float phaseMult;

    if (m_overshootFrames > 0) {
        // Actively overshooting — add extra reach then correct
        m_overshootFrames--;
        if (m_overshootFrames > 5) {
            // First half: overshoot peak
            phaseMult = 1.0f + m_reachOvershootAmt;
        } else if (m_overshootFrames > 1) {
            // Second half: correction toward normal
            float t = (float)(5 - m_overshootFrames) / 4.0f;  // 0→1
            phaseMult = (1.0f + m_reachOvershootAmt) * (1.0f - t) + 1.0f * t;
            phaseMult += ac::humRand(-0.02f, 0.02f);
        } else {
            // Final frame: settle back to normal
            phaseMult = 1.0f;
        }
    } else {
        // ── Reaction curve ───────────────────────────────────────
        m_overshootTimer--;
        if (m_overshootTimer <= 0) {
            // Trigger new overshoot
            m_overshootTimer    = (int)ac::humRand(30, 60);
            m_overshootFrames   = (int)ac::humRand(8, 15);
            m_reachOvershootAmt = ac::humRand(0.05f, 0.15f);  // 5-15% overshoot
        }

        if (m_reachFrames < 6) {
            // REACTION phase: barely extending (10-25%)
            phaseMult = ac::humRand(0.10f, 0.25f);
        } else if (m_reachFrames < 16) {
            // ACCEL phase: smooth ramp-up
            float t = (float)(m_reachFrames - 6) / 10.0f;
            float sCurve = t * t * (3.0f - 2.0f * t);  // smoothstep
            phaseMult = 0.25f + sCurve * 0.55f;  // 0.25→0.80
            phaseMult += ac::humRand(-0.05f, 0.05f);
        } else if (m_reachFrames < 50) {
            // CRUISING phase: mostly stable with micro-jitter
            phaseMult = ac::humRand(0.80f, 0.95f);
            if ((int)ac::humRand(0, 25) == 0) phaseMult *= ac::humRand(0.6f, 0.8f);
        } else {
            // FINE-TUNE phase: precise adjustments
            phaseMult = ac::humRand(0.85f, 1.02f);
        }
    }

    // ── Inertia smoothing: 20% of last delta carries over ────────
    float rawLevel = phaseMult;
    float delta = rawLevel - m_curReachLevel;
    float inertiaWeight = 0.20f;
    float smoothedDelta = delta * (1.0f - inertiaWeight) + m_prevReachDelta * inertiaWeight;
    m_prevReachDelta = smoothedDelta;
    // Smooth approach to target (no instant jumps)
    m_curReachLevel += smoothedDelta * ac::humRand(0.25f, 0.55f);
    m_curReachLevel = std::max(0.5f, std::min(1.15f, m_curReachLevel));

    // Effective reach = vanilla (3.0) + extra reach * humanized level
    float extraReach = rawReachDist - 3.0f;
    float reachDist  = 3.0f + extraReach * m_curReachLevel;

    // ── ⚠️ Detect warnings ──────────────────────────────────────
    if (reachDist > 3.2f)
        addDetectWarning(std::string("Reach ") + std::to_string((int)reachDist) + "> 3.2 blocks",
                         true, "Watchdog checks interaction distances. Above 3.2 blocks is detected on most servers.");
    if (reachDist > 4.2f)
        addDetectWarning(std::string("Reach ") + std::to_string((int)reachDist) + "> 4.2", true,
            "Extreme reach — all major ACs (GrimAC, Vulcan, Matrix) flag distances >4.2.");
    if (m_boolSettings["tp_reach"] && reachDist > 4.5f)
        addDetectWarning("TP-Reach > 4.5", true, "TP spoofs at extreme range leave teleport traces in server logs.");
    // Warn when constant reach is detected (curReachLevel converges to exactly 1.0)
    if (m_reachFrames > 60 && fabsf(m_curReachLevel - 1.0f) < 0.0005f)
        addDetectWarning("Reach converged — no variation", true,
            "Perfectly stable reach is a GrimAC/Vulcan fingerprint. The humanization engine should prevent this.");

    // Keep the attribute modification so the client detects hits at extended range
    setReach(env, reachDist, reachDist + 1.5);

    // ── Blink/TP Reach: Spoof position closer when attacking ────────
    if (!s_netInit) {
        // One-time init of network packet JNI IDs
        s_mcClass = JvmWrapper::findClass(Mappings::Minecraft_Class);
        if (s_mcClass) {
            s_getConnection = env->GetMethodID(s_mcClass, Mappings::MC_getConnection, Mappings::MC_getConnection_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_getConnection = nullptr; }

            s_hitResultField = env->GetFieldID(s_mcClass, Mappings::MC_hitResult, Mappings::MC_hitResult_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_hitResultField = nullptr; }
        }

        s_ccpliClass = JvmWrapper::findClass(Mappings::ClientCommonPacketListenerImpl_Class);
        if (s_ccpliClass) {
            s_connectionField = env->GetFieldID(s_ccpliClass, Mappings::CCPLI_connection, Mappings::CCPLI_connection_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_connectionField = nullptr; }
        }

        s_connectionClass = JvmWrapper::findClass(Mappings::Connection_Class);
        if (s_connectionClass) {
            s_connectionSend = env->GetMethodID(s_connectionClass, Mappings::Connection_send, Mappings::Connection_send_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_connectionSend = nullptr; }
        }

        s_movePacketPosClass = JvmWrapper::findClass(Mappings::ServerboundMovePlayerPacket_Pos_Class);
        if (s_movePacketPosClass) {
            // 1.21.2+: (DDDZZ)V  |  1.21.1: (DDDZ)V
            s_movePacketPosInit = env->GetMethodID(s_movePacketPosClass, "<init>", Mappings::MovePacketPos_Init_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_movePacketPosInit = nullptr; }
            if (!s_movePacketPosInit) {
                s_movePacketPosInit = env->GetMethodID(s_movePacketPosClass, "<init>", Mappings::MovePacketPos_Init_Sig_Legacy);
                if (env->ExceptionCheck()) { env->ExceptionClear(); s_movePacketPosInit = nullptr; }
            }
        }

        s_entityHitResultClass = JvmWrapper::findClass(Mappings::EntityHitResult_Class);
        if (s_entityHitResultClass) {
            s_getTargetEntity = env->GetMethodID(s_entityHitResultClass, Mappings::EntityHitResult_getEntity, Mappings::EntityHitResult_getEntity_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_getTargetEntity = nullptr; }
        }

        s_playerClass = JvmWrapper::findClass(Mappings::Player_Class);
        if (s_playerClass) {
            s_playerAttack = env->GetMethodID(s_playerClass, Mappings::Player_attack, Mappings::Player_attack_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_playerAttack = nullptr; }

            s_playerSwing = env->GetMethodID(s_playerClass, Mappings::Player_swing, Mappings::Player_swing_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_playerSwing = nullptr; }
        }

        s_interactionHandClass = JvmWrapper::findClass(Mappings::InteractionHand_Class);
        if (s_interactionHandClass) {
            s_mainHandField = env->GetStaticFieldID(s_interactionHandClass, Mappings::InteractionHand_MAIN_HAND, Mappings::InteractionHand_MAIN_HAND_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_mainHandField = nullptr; }
        }

        s_netInit = true;
        printf(STR_C("[Draxo] Reach TP: net init complete (conn=%p send=%p movePkt=%p entityHit=%p attack=%p)\n"),
               s_connectionField, s_connectionSend, s_movePacketPosInit, s_getTargetEntity, s_playerAttack);
    }

    // Check if we have all the required IDs
    if (!s_getConnection || !s_connectionField || !s_connectionSend || !s_movePacketPosInit || !s_hitResultField) {
        return; // Can't do TP reach without network access
    }

    // Get the Minecraft instance and check hitResult
    jobject mc = CMinecraft::getInstance();
    if (!mc) return;

    jobject hitResult = env->GetObjectField(mc, s_hitResultField);
    if (!hitResult || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(mc);
        return;
    }

    // Check if hitResult is an EntityHitResult (player is looking at an entity)
    if (!s_entityHitResultClass || !env->IsInstanceOf(hitResult, s_entityHitResultClass)) {
        env->DeleteLocalRef(hitResult);
        env->DeleteLocalRef(mc);
        return;
    }

    // Get the target entity
    jobject targetEntity = env->CallObjectMethod(hitResult, s_getTargetEntity);
    if (!targetEntity || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(hitResult);
        env->DeleteLocalRef(mc);
        return;
    }

    // Get player
    jobject playerObj = CMinecraft::getPlayer();
    if (!playerObj) {
        env->DeleteLocalRef(targetEntity);
        env->DeleteLocalRef(hitResult);
        env->DeleteLocalRef(mc);
        return;
    }

    // Calculate distance between player and target
    CEntity player(playerObj);
    CEntity target(targetEntity);

    double px = player.getX(), py = player.getY(), pz = player.getZ();
    double tx = target.getX(), ty = target.getY(), tz = target.getZ();
    double dx = tx - px, dy = ty - py, dz = tz - pz;
    double dist = std::sqrt(dx * dx + dy * dy + dz * dz);

    // Only do TP reach if the target is beyond vanilla range but within our extended range
    if (dist > 3.0 && dist <= (double)reachDist) {
        // Check if the player is attacking (left click) — we detect this by checking
        // if the attack cooldown just reset (attackStrengthTicker == 0 means ready)
        // We rely on the game's own attack detection via hitResult being set

        bool leftClick = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        static bool wasLeftClick = false;

        if (!leftClick) {
            wasLeftClick = false;
        } else if (leftClick && !wasLeftClick) {
            wasLeftClick = true;

            jobject listener = env->CallObjectMethod(mc, s_getConnection);
            if (listener && !env->ExceptionCheck()) {
                jobject connection = env->GetObjectField(listener, s_connectionField);
                if (connection && !env->ExceptionCheck()) {
                    // Calculate a spoofed position: move along the line from player to target
                    // Stop at 2.9 blocks away from the target (within vanilla reach)
                    // ── MicroOffset: ±0.03 blocks random noise per axis ──────
                    // Prevents Watchdog from seeing identical spoof coords every attack.
                    wd::MicroOffset mo; mo.regenerate();
                    double baseRatio = (dist - 2.9) / dist;
                    double wobbleRatio = wd::MicroOffset::wobble(baseRatio, 0.012);
                    double spoofX = px + dx * wobbleRatio + mo.x;
                    double spoofY = py + mo.y; // Keep Y nearly the same to avoid falling
                    double spoofZ = pz + dz * wobbleRatio + mo.z;

                    float yaw = player.getYaw();
                    float pitch = player.getPitch();

                    // Step 1: Send position packet to spoofed (closer) position
                    sendPositionPacket(env, connection, spoofX, spoofY, spoofZ, yaw, pitch, true);

                    // Step 2: Attack the entity (this sends the attack packet from the spoofed position)
                    if (s_playerAttack) {
                        env->CallVoidMethod(playerObj, s_playerAttack, targetEntity);
                        if (env->ExceptionCheck()) env->ExceptionClear();
                    }

                    // Step 3: Swing arm for visual feedback
                    if (s_playerSwing && s_interactionHandClass && s_mainHandField) {
                        jobject mainHand = env->GetStaticObjectField(s_interactionHandClass, s_mainHandField);
                        if (mainHand) {
                            env->CallVoidMethod(playerObj, s_playerSwing, mainHand);
                            if (env->ExceptionCheck()) env->ExceptionClear();
                            env->DeleteLocalRef(mainHand);
                        }
                    }

                    // Step 4: Send position packet back to real position
                    sendPositionPacket(env, connection, px, py, pz, yaw, pitch, true);

                    env->DeleteLocalRef(connection);
                } else {
                    if (env->ExceptionCheck()) env->ExceptionClear();
                }
                env->DeleteLocalRef(listener);
            } else {
                if (env->ExceptionCheck()) env->ExceptionClear();
            }
        }
    }

    env->DeleteLocalRef(targetEntity);
    env->DeleteLocalRef(playerObj);
    env->DeleteLocalRef(hitResult);
    env->DeleteLocalRef(mc);
}

void Reach::sendPositionPacket(JNIEnv* env, jobject connection, double x, double y, double z, float yaw, float pitch, bool onGround) {
    if (!s_movePacketPosClass || !s_movePacketPosInit || !s_connectionSend) return;

    // constructor takes 3 doubles (x, y, z) and 2 booleans (onGround, horizontalCollision)
    jobject packet = env->NewObject(s_movePacketPosClass, s_movePacketPosInit,
                                     x, y, z, (jboolean)onGround, (jboolean)false);
    if (packet && !env->ExceptionCheck()) {
        env->CallVoidMethod(connection, s_connectionSend, packet);
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(packet);
    } else {
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
}

void Reach::setReach(JNIEnv* env, double entityReach, double blockReach) {
    static bool init = false;
    if (!init) {
        s_attributesClass = JvmWrapper::findClass(Mappings::Attributes_Class);
        if (s_attributesClass) {
            s_entityInteractionRange = env->GetStaticFieldID(s_attributesClass, Mappings::Attributes_ENTITY_INTERACTION_RANGE, Mappings::Attributes_ENTITY_INTERACTION_RANGE_Sig);
            s_blockInteractionRange = env->GetStaticFieldID(s_attributesClass, Mappings::Attributes_BLOCK_INTERACTION_RANGE, Mappings::Attributes_BLOCK_INTERACTION_RANGE_Sig);
        }

        s_livingEntityClass = JvmWrapper::findClass(Mappings::LivingEntity_Class);
        if (s_livingEntityClass) {
            s_getAttribute = env->GetMethodID(s_livingEntityClass, Mappings::LivingEntity_getAttribute, Mappings::LivingEntity_getAttribute_Sig);
        }

        s_attributeInstanceClass = JvmWrapper::findClass(Mappings::AttributeInstance_Class);
        if (s_attributeInstanceClass) {
            s_setBaseValue = env->GetMethodID(s_attributeInstanceClass, Mappings::AttributeInstance_setBaseValue, Mappings::AttributeInstance_setBaseValue_Sig);
        }
        init = true;
    }

    if (!s_attributesClass || !s_livingEntityClass || !s_attributeInstanceClass) return;
    if (!s_entityInteractionRange || !s_blockInteractionRange || !s_getAttribute || !s_setBaseValue) return;

    jobject playerObj = CMinecraft::getPlayer();
    if (!playerObj) return;

    // Set Entity Reach
    jobject entityHolder = env->GetStaticObjectField(s_attributesClass, s_entityInteractionRange);
    if (entityHolder) {
        jobject attrInst = env->CallObjectMethod(playerObj, s_getAttribute, entityHolder);
        if (attrInst) {
            env->CallVoidMethod(attrInst, s_setBaseValue, entityReach);
            env->DeleteLocalRef(attrInst);
        }
        env->DeleteLocalRef(entityHolder);
        JvmWrapper::checkException();
    }

    // Set Block Reach
    jobject blockHolder = env->GetStaticObjectField(s_attributesClass, s_blockInteractionRange);
    if (blockHolder) {
        jobject attrInst = env->CallObjectMethod(playerObj, s_getAttribute, blockHolder);
        if (attrInst) {
            env->CallVoidMethod(attrInst, s_setBaseValue, blockReach);
            env->DeleteLocalRef(attrInst);
        }
        env->DeleteLocalRef(blockHolder);
        JvmWrapper::checkException();
    }
}
