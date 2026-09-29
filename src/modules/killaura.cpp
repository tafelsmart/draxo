#include "pch.h"
#include "core/strcrypt.h"
#include "modules/killaura.h"
#include "sdk/minecraft.h"
#include "sdk/entity.h"
#include "sdk/world.h"
#include "core/watchdog_bypass.h"
#include "core/ac_bypass.h"
#include "core/ac_humanizer.h"
#include <cmath>
#include <algorithm>

KillAura::KillAura() : Module("KillAura", ModuleCategory::COMBAT, 0, "Automatically attacks nearby enemies — Legit is human-like, Rage is fast, both undetected") {
    defineGroup("Mode");
    defineMode("mode", "Mode", 0, {"Legit", "Rage"});
    defineMode("pvp_mode", "PvP Mode", 1, {"1.8 PvP", "1.21 PvP"});
    defineGroupEnd();

    defineGroup("Targeting");
    defineFloat("range",     "Range",       4.2f, 1.0f, 6.0f);
    defineFloat("fov",       "FOV",        90.0f, 30.0f, 360.0f);
    defineBool("players",    "Players",    true);
    defineBool("mobs",       "Mobs",       false);
    defineBool("focus_only", "Only when focused", true);
    defineGroupEnd();

    defineGroup("Aim");
    defineFloat("aim_speed", "Aim Speed",  30.0f, 1.0f, 100.0f);
    defineBool("silent_aim", "Silent Aim", true);
    defineGroupEnd();

    defineGroup("Timing");
    defineFloat("cps",       "CPS (Rage)", 18.0f, 6.0f, 30.0f);
    defineInt("cooldown",    "Cooldown Ticks (Legit)", 18, 12, 20);
    defineFloat("reaction",  "Reaction Time (ms)", 120.0f, 0.0f, 600.0f, "%.0f");
    defineGroupEnd();

    defineGroup("Undetect");
    defineFloat("aim_jitter","Aim Jitter (°)", 2.5f, 0.0f, 10.0f, "%.1f");
    defineFloat("miss_chance","Miss Chance %", 6.0f, 0.0f, 40.0f, "%.0f");
    defineFloat("cps_jitter","CPS Jitter %", 25.0f, 0.0f, 60.0f, "%.0f");
    defineFloat("cps_decay", "CPS Decay /s", 0.5f, 0.0f, 5.0f, "%.1f");
    defineBool("rage_jitter", "Rage Rotation Jitter", true);
    defineGroupEnd();

    defineGroup("Watchdog Bypass");
    defineBool("wd_sine_jitter", "Sine-Wave Jitter", true);
    defineBool("wd_scheduler", "Packet Scheduler", true);
    defineGroupEnd();

    defineGroup("Multi-AC Bypass");
    defineBool("ac_bodypart",  "Body Part Random", true);
    defineBool("ac_accelrot",  "Accel Rotation", true);
    defineBool("ac_ctxmiss",   "Contextual Miss", true);
    defineBool("ac_spread",    "Tick Spreading", true);
    defineBool("ac_grimac",    "GrimAC Safe", true);
    defineBool("ac_fovfalloff","FOV Falloff", true);
    defineGroupEnd();

    addSearchTag("aura");
    addSearchTag("attack");
    addSearchTag("combat");
}

void KillAura::onEnable()  {
    m_hasTarget = false; m_lastTargetId = -1; m_nextAttackAt = 0;
    m_pktScheduler.reset(); m_sineJitter.reset();
    m_bodyPart.reset(); m_accelRot.reset(0,0); m_tickSpread.reset(); m_justSwitched = false;
    m_perlinJitter.reset(m_floatSettings.count("aim_jitter") ? m_floatSettings["aim_jitter"] : 2.5f);
    m_cpsHuman.reset();
    m_reachWobbler.reset();
    m_globalScheduler.reset();
    m_globalScheduler.setWindow(8, 1500);
    m_aimFrames = 0; m_overshootTimer = 0; m_overshootFrames = 0;
    m_overshootYaw = 0; m_overshootPitch = 0;
    m_aimInertiaYaw = 0; m_aimInertiaPitch = 0;
    m_curStepSpeed = 0;
}
void KillAura::onDisable() {
    m_hasTarget = false; m_lastTargetId = -1; m_nextAttackAt = 0;
    m_pktScheduler.reset(); m_sineJitter.reset();
    m_bodyPart.reset(); m_accelRot.reset(0,0); m_tickSpread.reset(); m_justSwitched = false;
    m_globalScheduler.reset(); m_reachWobbler.reset();
    m_aimFrames = 0; m_overshootTimer = 0; m_overshootFrames = 0;
    m_curStepSpeed = 0;
}

float KillAura::wrapAngle(float a) {
    while (a > 180.0f) a -= 360.0f;
    while (a < -180.0f) a += 360.0f;
    return a;
}
float KillAura::angleDiff(float a, float b) { return wrapAngle(a - b); }

int KillAura::randInt(int lo, int hi) {
    if (hi <= lo) return lo;
    std::uniform_int_distribution<int> d(lo, hi);
    return d(m_rng);
}
float KillAura::randFloat(float lo, float hi) {
    if (hi <= lo) return lo;
    std::uniform_real_distribution<float> d(lo, hi);
    return d(m_rng);
}

// ── JNI-Cache ─────────────────────────────────────────────────────
void KillAura::initJNI(JNIEnv* env) {
    s_playerClass = JvmWrapper::findClass(Mappings::Player_Class);
    if (s_playerClass) {
        s_playerAttack = env->GetMethodID(s_playerClass, Mappings::Player_attack, Mappings::Player_attack_Sig);
        if (env->ExceptionCheck()) { env->ExceptionClear(); s_playerAttack = nullptr; }

        s_playerSwing = env->GetMethodID(s_playerClass, Mappings::Player_swing, Mappings::Player_swing_Sig);
        if (env->ExceptionCheck()) { env->ExceptionClear(); s_playerSwing = nullptr; }

        s_scaleMethod = env->GetMethodID(s_playerClass, Mappings::Player_getAttackStrengthScale, Mappings::Player_getAttackStrengthScale_Sig);
        if (env->ExceptionCheck()) { env->ExceptionClear(); s_scaleMethod = nullptr; }
    }

    s_handClass = JvmWrapper::findClass(Mappings::InteractionHand_Class);
    if (s_handClass) {
        s_mainHand = env->GetStaticFieldID(s_handClass, Mappings::InteractionHand_MAIN_HAND, Mappings::InteractionHand_MAIN_HAND_Sig);
        if (env->ExceptionCheck()) { env->ExceptionClear(); s_mainHand = nullptr; }
    }

    s_livingClass = JvmWrapper::findClass(Mappings::LivingEntity_Class);
    if (s_livingClass) {
        s_tickerField = env->GetFieldID(s_livingClass, Mappings::LivingEntity_attackStrengthTicker, Mappings::LivingEntity_attackStrengthTicker_Sig);
        if (env->ExceptionCheck()) { env->ExceptionClear(); s_tickerField = nullptr; }
    }

    // ── Echtes Attack-Packet: MultiPlayerGameMode.attack(Player, Entity) ──
    // Seit 1.21.x überschreibt LocalPlayer.attack die Basis-Methode nicht
    // mehr — der Packet-Versand läuft über die gameMode-Methode.
    jclass mcClass = JvmWrapper::findClass(Mappings::Minecraft_Class);
    if (mcClass) {
        s_gameModeField = env->GetFieldID(mcClass, Mappings::MC_gameMode, Mappings::MC_gameMode_Sig);
        if (env->ExceptionCheck()) { env->ExceptionClear(); s_gameModeField = nullptr; }
    }
    s_gameModeClass = JvmWrapper::findClass(Mappings::GameMode_Class);
    if (s_gameModeClass) {
        // Try legacy (1-arg: Entity) FIRST for older versions (1.21.1-)
        // then 2-arg (Player, Entity) as fallback for 1.21.2+.
        s_gameModeAttack = env->GetMethodID(s_gameModeClass, Mappings::GameMode_attack, Mappings::GameMode_attack_Sig_Legacy);
        if (env->ExceptionCheck()) { env->ExceptionClear(); s_gameModeAttack = nullptr; }
        s_gameMode2Args = false;
        if (!s_gameModeAttack) {
            // 1.21.2+: (Player, Entity)
            s_gameModeAttack = env->GetMethodID(s_gameModeClass, Mappings::GameMode_attack, Mappings::GameMode_attack_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_gameModeAttack = nullptr; }
            s_gameMode2Args = (s_gameModeAttack != nullptr);
        }
    }

    s_jniInit = true;
    printf(STR_C("[Draxo] KillAura JNI: attack=%p swing=%p scale=%p ticker=%p gameMode=%p gmAttack=%p (2args=%d)\n"),
           (void*)s_playerAttack, (void*)s_playerSwing, (void*)s_scaleMethod, (void*)s_tickerField,
           (void*)s_gameModeField, (void*)s_gameModeAttack, s_gameMode2Args ? 1 : 0);
}

float KillAura::attackScale(JNIEnv* env, jobject player) {
    if (s_scaleMethod) {
        float s = env->CallFloatMethod(player, s_scaleMethod, 0.0f);
        if (env->ExceptionCheck()) { env->ExceptionClear(); }
        return s;
    }
    if (s_tickerField) {
        int t = env->GetIntField(player, s_tickerField);
        if (env->ExceptionCheck()) { env->ExceptionClear(); }
        return (float)t / 20.0f;
    }
    return 1.0f; // unbekannt -> immer bereit
}

void KillAura::jniAttack(JNIEnv* env, jobject player, jobject target) {
    if (!player || !target) return;

    static int atkCount = 0;
    if (++atkCount <= 5) printf(STR_C("[Draxo] KillAura jniAttack #%d: gmField=%p gmAttack=%p 2args=%d pAtk=%p\n"),
                                atkCount, (void*)s_gameModeField, (void*)s_gameModeAttack,
                                s_gameMode2Args, (void*)s_playerAttack);

    bool attacked = false;

    // 1) Primary: MultiPlayerGameMode.attack (sends ServerboundInteractPacket)
    if (s_gameModeAttack && s_gameModeField) {
        jobject mc = CMinecraft::getInstance();
        if (mc && !env->ExceptionCheck()) {
            jobject gameMode = env->GetObjectField(mc, s_gameModeField);
            if (gameMode && !env->ExceptionCheck()) {
                if (s_gameMode2Args)
                    env->CallVoidMethod(gameMode, s_gameModeAttack, player, target);
                else
                    env->CallVoidMethod(gameMode, s_gameModeAttack, target);
                if (env->ExceptionCheck()) env->ExceptionClear();
                else attacked = true;
                env->DeleteLocalRef(gameMode);
            } else {
                if (env->ExceptionCheck()) env->ExceptionClear();
            }
            env->DeleteLocalRef(mc);
        } else {
            if (env->ExceptionCheck()) env->ExceptionClear();
        }
    }

    // 2) Fallback: Player.attack(Entity) direkt (funktioniert auf älteren Versionen
    //    oder wenn gameMode nicht verfügbar ist)
    if (!attacked && s_playerAttack) {
        env->CallVoidMethod(player, s_playerAttack, target);
        if (env->ExceptionCheck()) env->ExceptionClear();
        else attacked = true;
    }

    // 3) Swing-Animation (visuell)
    swingOnly(env, player);
}

void KillAura::swingOnly(JNIEnv* env, jobject player) {
    if (!player || !s_playerSwing || !s_handClass || !s_mainHand) return;
    jobject hand = env->GetStaticObjectField(s_handClass, s_mainHand);
    if (hand) {
        env->CallVoidMethod(player, s_playerSwing, hand);
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(hand);
    }
}

// ── Update ────────────────────────────────────────────────────────
void KillAura::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;
    if (!s_jniInit) initJNI(env);

    // Own frame: entity list + gameMode + hand refs can exceed 50 local refs
    // per tick. Without this, all refs land in the hook's PushLocalFrame(512)
    // and risk overflow when combined with Reach/ESP/Tracers/HUD.
    env->PushLocalFrame(256);

    jobject playerObj = CMinecraft::getPlayer();
    if (!playerObj) return;
    CEntity player(playerObj);
    double px = player.getX(), py = player.getY() + 1.62, pz = player.getZ();
    float pYaw = player.getYaw(), pPitch = player.getPitch();

    jobject world = CMinecraft::getWorld();
    if (!world) { env->PopLocalFrame(nullptr); return; }

    auto entities = CWorld::getAllEntities(world);
    env->DeleteLocalRef(world);

    float baseRange = m_floatSettings["range"];
    // ── ReachWobbler: ±0.02-0.05 blocks per-tick reach variation ──
    // Watchdog fingerprints "always attacks at exactly X blocks" instantly.
    // This wobbles the effective range by a 2-octave noise pattern
    // with occasional micro-slips (mimics human reach inconsistency).
    float rangeWobble = m_reachWobbler.wobble(1.0f / 60.0f);
    float range = baseRange + rangeWobble;
    float fov   = m_floatSettings["fov"];
    float speed = m_floatSettings["aim_speed"];
    float cps   = m_floatSettings["cps"];
    int   mode  = m_intSettings["mode"];
    // Alte Configs können mode=2 (Combo) gespeichert haben — sicher auf Rage mappen
    if (mode >= 2) mode = (int)Mode::Rage;
    int   pvpMode  = m_intSettings["pvp_mode"];   // 0 = 1.8 (Spam), 1 = 1.21 (Cooldown)
    bool  pvp18    = (pvpMode == 0);
    int   cooldownTicks = m_intSettings["cooldown"];
    float reactionMs   = m_floatSettings["reaction"];
    float jitterDeg    = m_floatSettings["aim_jitter"];
    float missChance   = m_floatSettings["miss_chance"];
    float cpsJitter    = m_floatSettings["cps_jitter"];
    bool targetPlayers = m_boolSettings["players"];
    bool targetMobs    = m_boolSettings["mobs"];
    bool silent        = m_boolSettings["silent_aim"];
    bool focusOnly     = m_boolSettings["focus_only"];
    bool rage          = mode == (int)Mode::Rage;

    // Bestes Ziel finden
    CEntity* best = nullptr;
    float bestDist = range;
    int pid = player.getId();

    for (auto& e : entities) {
        if (e.getId() == pid || !e.isAlive()) continue;
        bool isP = e.isPlayer();
        if (isP && !targetPlayers) continue;
        if (!isP && e.isLiving() && !targetMobs) continue;
        if (!isP && !e.isLiving()) continue;

        double ex = e.getX(), ey = e.getY() + 1.0, ez = e.getZ();
        double dx = ex - px, dy = ey - py, dz = ez - pz;
        double dist = sqrt(dx*dx + dy*dy + dz*dz);
        if (dist > range) continue;

        float yawTo = (float)(atan2(dz, dx) * 180.0 / 3.14159265) - 90.0f;
        float yawDiff = fabs(angleDiff(pYaw, yawTo));
        if (fov < 360.0f && yawDiff > fov / 2.0f) continue;

        if (dist < bestDist) { bestDist = (float)dist; best = &e; }
    }

    auto now = std::chrono::steady_clock::now();
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    if (!best) {
        m_hasTarget = false;
        m_lastTargetId = -1;
        m_nextAttackAt = 0;
        env->DeleteLocalRef(playerObj);
        return;
    }

    // ── Zielwechsel: Reset aller Bypass-Engines ────────────────────
    int tid = best->getId();
    if (tid != m_lastTargetId) {
        m_justSwitched = true;
        m_lastTargetId = tid;
        m_hasTarget = false;
        m_firstRageAttack = 0;
        m_pktScheduler.reset();
        m_sineJitter.reset();
        m_bodyPart.reset();
        m_accelRot.reset(pYaw, pPitch);
        m_tickSpread.reset();
        m_lastTargetSpeed = 0.0;
        m_reachWobbler.reset();
        // Human aim state reset
        m_aimFrames = 0;
        m_overshootTimer = randInt(35, 70);  // 0.6-1.2s until first overshoot
        m_overshootFrames = 0;
        m_overshootYaw = 0; m_overshootPitch = 0;
        m_aimInertiaYaw = 0; m_aimInertiaPitch = 0;
        m_curStepSpeed = 0;
        if (!rage) {
            // Legit: 80-250ms reaction delay before starting to aim
            long long reactDelay = (long long)reactionMs + randInt(20, 130);
            m_nextAttackAt = ms + reactDelay;
        }
    } else {
        m_justSwitched = false;
    }

    // ── ⚠️ Detect warnings ────────────────────────────────────────
    if (rage) {
        if (!m_boolSettings["wd_sine_jitter"])
            addDetectWarning("Sine-Jitter OFF", true, "Static rage aim — all ACs detect this. Enable Sine-Wave Jitter.");
        if (!m_boolSettings["wd_scheduler"])
            addDetectWarning("Packet Scheduler OFF", true, "Rhythmic timing — Watchdog flags this. Enable Packet Scheduler.");
        if (!m_boolSettings["ac_bodypart"])
            addDetectWarning("Body Part OFF", true, "Always aiming at head — GrimAC/Matrix fingerprint.");
        if (!m_boolSettings["ac_grimac"])
            addDetectWarning("GrimAC Safe OFF", true, "Rotation deltas unchecked — GrimAC detects impossible movement.");
        float effCps = m_cpsHuman.getEffectiveCps(cps, true);
        if (effCps < 1.0f) effCps = cps;
        if (effCps > 20.0f) addDetectWarning("Rage CPS > 20", true, "Watchdog flags sustained >20 CPS. Reduce to 16-18.");
        if (cpsJitter < 15.0f) addDetectWarning("Low CPS Jitter", true, "Perfect CPS timing — Matrix detects this. Increase Jitter above 15%.");
    } else {
        // ── Legit mode: ALL humanization is forced ON ────────────
        // The human aim curve + overshoot + inertia make undetectability
        // independent of toggleable features. But if the user overrides
        // critical sliders, warn them.
        if (speed > 60.0f)
            addDetectWarning("Aim Speed too high for Legit", true,
                "Human aim speed cap is ~50. Above 60 looks like snap-aim to GrimAC/Vulcan.");
        if (jitterDeg < 1.5f)
            addDetectWarning("Legit: low jitter", true,
                "Below 1.5 deg jitter — your aim path is too clean. GrimAC detects static lock-on. Raise to 2-4 deg.");
        if (m_floatSettings["cps"] > 14.0f)
            addDetectWarning("Legit CPS > 14", true,
                "Human-like CPS range is 8-12. Above 14 in Legit mode is suspicious to Watchdog.");
        if (cooldownTicks < 16 && m_intSettings["pvp_mode"] == 1)
            addDetectWarning("Cooldown too fast", true,
                "1.21 PvP cooldown below 16 ticks is detectable by Spartan/Vulcan. Use 18-20 for legit.");
        if (!m_boolSettings["ac_bodypart"])
            addDetectWarning("Body Part Random OFF in Legit", true,
                "Always aiming at head height is a known KillAura fingerprint. Enable Body Part Random.");
        if (!m_boolSettings["ac_ctxmiss"])
            addDetectWarning("Contextual Miss OFF in Legit", true,
                "Never missing is a dead giveaway. Enable Contextual Miss for natural error rate.");
        if (!m_boolSettings["ac_spread"])
            addDetectWarning("Tick Spreading OFF in Legit", true,
                "Continuous attacks in short windows trigger Watchdog point accumulation.");
    }

    // ── Compute target speed (for contextual miss + FOV falloff) ───
    double tSpeed = 0.0;
    {
        double lx = best->getX(), lz = best->getZ();
        static double lastX = lx, lastZ = lz;
        double sdx = lx - lastX, sdz = lz - lastZ;
        tSpeed = sqrt(sdx*sdx + sdz*sdz) * 20.0;  // blocks/tick * 20 = m/s approx
        lastX = lx; lastZ = lz;
    }
    m_lastTargetSpeed = tSpeed;

    // ── Body Part Randomization (GrimAC/Matrix bypass) ────────────
    // Never aim at the exact same spot. Head -> chest -> waist -> legs.
    double aimY;
    double aimJitterX = 0.0, aimJitterZ = 0.0;
    if (m_boolSettings["ac_bodypart"]) {
        aimY = best->getY() + (double)m_bodyPart.getYOffset();
        aimJitterX = (double)m_bodyPart.getHorizontalJitter();
        aimJitterZ = (double)m_bodyPart.getHorizontalJitter();
    } else {
        aimY = rage ? best->getY() + 1.0 : best->getY() + 0.85;
    }

    double ex = best->getX() + aimJitterX;
    double ey = aimY;
    double ez = best->getZ() + aimJitterZ;
    double dx = ex - px, dy = ey - py, dz = ez - pz;
    double hd = sqrt(dx*dx + dz*dz);
    float tYaw = (float)(atan2(dz, dx) * 180.0 / 3.14159265) - 90.0f;
    float tPitch = (float)(-atan2(dy, hd) * 180.0 / 3.14159265);

    if (!m_hasTarget) {
        m_curYaw = pYaw; m_curPitch = pPitch;
        m_accelRot.reset(pYaw, pPitch);
        m_hasTarget = true;
    }

    // ── FOV Falloff (Matrix/Spartan bypass) ───────────────────────
    // At the edge of FOV, aim gets less accurate (human peripheral vision).
    float fovPct = 0.0f;
    if (m_boolSettings["ac_fovfalloff"] && fov < 360.0f) {
        float yawToTarget = fabs(angleDiff(pYaw, tYaw));
        fovPct = yawToTarget / (fov / 2.0f);
        if (fovPct > 1.0f) fovPct = 1.0f;
        if (fovPct > 0.7f) {
            // Increase jitter proportionally near FOV edge
            float edgeJitter = (fovPct - 0.7f) * 6.0f;  // up to +1.8 deg at edge
            jitterDeg += edgeJitter;
        }
    }

    // ── Perlin-Jitter / Sine-Jitter (Watchdog/GrimAC bypass) ─────
    float jx = 0, jy = 0;
    if (m_boolSettings["wd_sine_jitter"]) {
        auto nowJit = std::chrono::steady_clock::now();
        float dt = 1.0f / 60.0f;
        long long prevNs = m_lastSineTick.time_since_epoch().count();
        long long nowNs = nowJit.time_since_epoch().count();
        if (prevNs > 0) {
            dt = (float)(nowNs - prevNs) / 1.0e9f;
            if (dt <= 0.0f || dt > 0.5f) dt = 1.0f / 60.0f;
        }
        m_lastSineTick = nowJit;

        if (m_usePerlin && m_boolSettings.count("ac_bodypart") && m_boolSettings["ac_bodypart"]) {
            // Perlin: 3-octave layered sine = natural hand tremor
            auto off = m_perlinJitter.tick(dt, rage ? 1.5f : jitterDeg, rage);
            jx = off.yaw; jy = off.pitch;
        } else {
            // Classic single-octave sine (fallback)
            auto off = m_sineJitter.tick(dt, rage ? 1.5f : jitterDeg, rage);
            jx = off.yaw; jy = off.pitch;
        }
    } else if (rage && m_boolSettings["rage_jitter"]) {
        jx = randFloat(-1.5f, 1.5f); jy = randFloat(-1.0f, 1.0f);
    } else if (!rage) {
        jx = randFloat(-jitterDeg, jitterDeg); jy = randFloat(-jitterDeg, jitterDeg);
    }

    // ── HUMAN AIM CURVE (Legit) vs Accel Rotation (Rage) ──────────
    // Legit: multi-phase reaction curve — NOT constant interpolation.
    // Phase 0 (0-5 frames): minimal movement (reaction delay, 0-15% speed)
    // Phase 1 (5-15 frames): accelerating (15-60% speed, variable)
    // Phase 2 (15-40 frames): cruising (60-85% speed with micro-jitter)
    // Phase 3 (40+ frames): fine-tuning (25-55% speed, overshoot patterns)
    //
    // This replaces simple linear interpolation that ACs fingerprint as:
    // "aimbot moves at exactly X degrees per tick toward center of hitbox"
    float step;
    if (!rage) {
        // ── Legit: human reaction curve ────────────────────────────
        m_aimFrames++;
        float baseSpeed = speed * 0.015f;  // slower than before
        
        // Phase-dependent speed multiplier
        float phaseMult;
        if (m_aimFrames < 6) {
            // REACTION PHASE: barely moving (human processing delay)
            phaseMult = randFloat(0.0f, 0.15f);
        } else if (m_aimFrames < 16) {
            // ACCELERATION PHASE: ramping up with variability
            float t = (float)(m_aimFrames - 6) / 10.0f;  // 0→1 over 10 frames
            // S-curve: slow then faster
            float sCurve = t * t * (3.0f - 2.0f * t);  // smoothstep
            phaseMult = 0.15f + sCurve * 0.50f;  // 0.15→0.65
            phaseMult += randFloat(-0.08f, 0.08f);  // per-frame jitter
        } else if (m_aimFrames < 40) {
            // CRUISING PHASE: mostly stable with micro-fluctuations
            phaseMult = randFloat(0.55f, 0.85f);
            // Occasionally slow down (human re-adjustment)
            if (randInt(0, 20) == 0) phaseMult *= randFloat(0.3f, 0.6f);
        } else {
            // FINE-TUNING PHASE: slower adjustments near target
            phaseMult = randFloat(0.25f, 0.55f);
        }
        
        // ── Overshoot pattern (every 35-70 frames) ────────────────
        // Real humans overshoot then correct back. ACs flag "never overshoots."
        if (m_overshootFrames > 0) {
            // Currently overshooting — apply overshoot offset
            m_overshootFrames--;
            float overshootForce;
            if (m_overshootFrames > 8) {
                // Peak overshoot (frames 15→9): add offset to go past target
                overshootForce = randFloat(0.4f, 0.8f);
                tYaw   += m_overshootYaw   * overshootForce;
                tPitch += m_overshootPitch * overshootForce;
            } else if (m_overshootFrames > 3) {
                // Correction phase: reverse direction
                overshootForce = randFloat(-0.5f, -0.2f);
                tYaw   += m_overshootYaw   * overshootForce;
                tPitch += m_overshootPitch * overshootForce;
            }
            // else: final settling frames, aim naturally
        } else {
            m_overshootTimer--;
            if (m_overshootTimer <= 0) {
                // Trigger new overshoot
                m_overshootTimer = randInt(35, 70);
                m_overshootFrames = randInt(12, 18);
                m_overshootYaw   = randFloat(-4.0f, 4.0f);
                m_overshootPitch = randFloat(-2.5f, 2.5f);
            }
        }
        
        // ── Aim inertia: past direction influences current ───────
        // Humans don't change direction instantly — momentum carries.
        float yawToGo = angleDiff(tYaw + jx, m_curYaw);
        float pitchToGo = (tPitch + jy) - m_curPitch;
        
        // Blend: 85% current direction + 15% inertia from last frame
        float inertiaWeight = 0.15f;
        yawToGo   = yawToGo   * (1.0f - inertiaWeight) + m_aimInertiaYaw   * inertiaWeight;
        pitchToGo = pitchToGo * (1.0f - inertiaWeight) + m_aimInertiaPitch * inertiaWeight;
        m_aimInertiaYaw   = yawToGo;
        m_aimInertiaPitch = pitchToGo;
        
        // Compute step with S-curve smoothing toward current speed
        float targetStep = std::min(baseSpeed * phaseMult, 0.85f);
        // Smoothly transition step speed (no instant jumps)
        m_curStepSpeed += (targetStep - m_curStepSpeed) * randFloat(0.25f, 0.55f);
        step = m_curStepSpeed;
        
        // ── Distance-based speed: closer = slower, more precise ───
        float distFactor = std::min(bestDist / 3.0f, 1.0f);
        step *= 0.6f + distFactor * 0.4f;
        
        float rawYawDelta   = yawToGo   * step;
        float rawPitchDelta = pitchToGo * step;
        
        // GrimAC clamping (always ON in Legit)
        rawYawDelta   = ac::grimac::safeYawDelta(rawYawDelta);
        rawPitchDelta = ac::grimac::safePitchDelta(rawPitchDelta);
        
        m_curYaw   += rawYawDelta;
        m_curPitch += rawPitchDelta;
    } else {
        // ── Rage: fast accel-based rotation ────────────────────────
        float dt = 1.0f / 60.0f;
        auto nowAccel = std::chrono::steady_clock::now();
        long long prevNsAccel = m_lastSineTick.time_since_epoch().count();
        long long nowNsAccel = nowAccel.time_since_epoch().count();
        if (prevNsAccel > 0) {
            dt = (float)(nowNsAccel - prevNsAccel) / 1.0e9f;
            if (dt <= 0.0f || dt > 0.5f) dt = 1.0f / 60.0f;
        }
        float speedMult = m_accelRot.step(dt);
        step = std::min(speed * speedMult * 0.025f, 1.0f);
        
        float rawYawDelta = angleDiff(tYaw + jx, m_curYaw) * step;
        float rawPitchDelta = ((tPitch + jy) - m_curPitch) * step;
        if (m_boolSettings["ac_grimac"]) {
            rawYawDelta   = ac::grimac::safeYawDelta(rawYawDelta);
            rawPitchDelta = ac::grimac::safePitchDelta(rawPitchDelta);
            if (ac::grimac::isSuspicious(rawYawDelta, rawPitchDelta))
                addDetectWarning("GrimAC: Suspicious delta", true, "Rotation delta exceeds human limits. GrimAC may flag this.");
        }
        m_curYaw   += rawYawDelta;
        m_curPitch += rawPitchDelta;
    }

    // Clamp pitch to vanilla bounds [-90, 90] — prevents render crashes
    if (m_curPitch > 90.0f)  m_curPitch = 90.0f;
    if (m_curPitch < -90.0f) m_curPitch = -90.0f;
    // Normalize yaw to [-180, 180] — prevents overflow/OOB in HUD look-direction
    while (m_curYaw > 180.0f)  m_curYaw -= 360.0f;
    while (m_curYaw < -180.0f) m_curYaw += 360.0f;

    if (silent) {
        player.setYaw(m_curYaw);
        player.setPitch(m_curPitch);
    }

    bool focused = true;
    if (focusOnly) {
        HWND hwnd = FindWindowA("GLFW30", nullptr);
        focused = hwnd && GetForegroundWindow() == hwnd;
    }
    bool canAttack = focused || !focusOnly;

    float scale = attackScale(env, playerObj);
    // 1.21 PvP: warten bis der Cooldown fast fertig ist (max. Schaden).
    // 1.8 PvP: Cooldown ignorieren — einfach schnell schlagen (wie 1.8).
    bool ready = pvp18 || scale >= ((float)cooldownTicks / 20.0f) || cooldownTicks <= 0;

    if (!canAttack) { env->PopLocalFrame(nullptr); return; }

    // ── Tick Spreading (Watchdog point-accumulation bypass) ─────────
    // Watchdog: too many suspicious actions in a window = ban.
    // We limit attacks to 4 per 600ms window, spreading them naturally.
    if (m_boolSettings["ac_spread"] && !m_tickSpread.canAttack(4, 600)) {
        env->DeleteLocalRef(playerObj);
        return;  // Skip this tick — let the window cool down
    }

    if (rage) {
        // Rage: GlobalPacketScheduler + CPSHumanizer + contextual miss
        float effCps = m_cpsHuman.getEffectiveCps(cps, true);
        if (effCps < 1.0f) effCps = cps;
        long long baseMs = (long long)(1000.0 / std::max(effCps, 1.0f));

        // GlobalPacketScheduler: skip if too many actions in window
        if (!m_globalScheduler.canAct()) { env->PopLocalFrame(nullptr); return; }

        long long delay;
        if (m_boolSettings["wd_scheduler"]) {
            delay = m_pktScheduler.nextDelay(baseMs, cpsJitter / 100.0f);
        } else {
            delay = baseMs;
            long long rawJitter = (long long)(delay * (cpsJitter / 100.0f));
            delay += randInt(-(int)rawJitter, (int)rawJitter);
            delay += randInt(0, 12);
            if (delay < 24) delay = 24;
        }

        if (ms - m_lastAttack >= delay) {
            // Contextual miss check
            bool miss = m_boolSettings["ac_ctxmiss"] &&
                        m_ctxMiss.shouldMiss((float)tSpeed, bestDist, fovPct, m_justSwitched);
            if (miss) {
                swingOnly(env, playerObj);
            } else {
                jniAttack(env, playerObj, best->getObject());
            }
            m_lastAttack = ms;
            m_globalScheduler.recordAction();
            m_pktScheduler.markAttack();
            m_tickSpread.markAttack();
            if (m_firstRageAttack == 0) m_firstRageAttack = ms;
        }
    } else {
        // ── Legit: full human simulation ────────────────────────
        if (ms < m_nextAttackAt) {
            env->DeleteLocalRef(playerObj);
            return;
        }
        if (!ready) {
            env->DeleteLocalRef(playerObj);
            return;
        }

        // Contextual miss (ALWAYS ON in Legit for undetectability)
        bool miss = m_ctxMiss.shouldMiss((float)tSpeed, bestDist, fovPct, m_justSwitched);
        // Also apply flat missChance as an extra layer if configured
        if (!miss && missChance > 0.0f) {
            miss = (randInt(0, 100) < (int)missChance);
        }

        if (miss) {
            swingOnly(env, playerObj);
        } else {
            jniAttack(env, playerObj, best->getObject());
        }
        m_lastAttack = ms;
        m_tickSpread.markAttack();

        long long nextDelay;
        if (pvp18) {
            // 1.8 PvP: human-like CPS 7-12 with natural variance
            float humanCps = 7.0f + randFloat(0.0f, 5.0f);  // 7-12 CPS
            if (cps < humanCps) humanCps = cps;  // respect user cap if lower
            nextDelay = (long long)(1000.0 / std::max(humanCps, 5.0f));
            nextDelay += randInt(-15, 35);
        } else {
            // 1.21 PvP: wait for cooldown + variable human reaction
            long long cdMs = (long long)(cooldownTicks * 50.0f);
            // Human variance: 0-150ms extra delay
            nextDelay = cdMs + randInt(0, 150);
        }
        // Occasional extra pause (15% chance of +100-300ms)
        if (randInt(0, 100) < 15) nextDelay += randInt(100, 300);
        m_nextAttackAt = ms + nextDelay;
    }

    env->DeleteLocalRef(playerObj);
    env->PopLocalFrame(nullptr);
}
