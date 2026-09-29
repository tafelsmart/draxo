#include "pch.h"
#include "core/strcrypt.h"
#include "modules/nuker.h"
#include "sdk/minecraft.h"
#include "sdk/entity.h"
#include "core/jvm_wrapper.h"
#include <cmath>

Nuker::Nuker() : Module("Nuker", ModuleCategory::WORLD, 0, "Breaks all blocks around you — Creative or Survival mode") {
    setTickInterval(1);  // every tick for smooth continueDestroyBlock
    defineMode("mode", "Mode", 0, {"Creative", "Survival"});
    defineFloat("radius",  "Radius",   4.0f, 1.0f, 8.0f, "%.0f");
    defineFloat("delay_min","Delay Min (ms)", 30.0f, 0.0f, 300.0f, "%.0f");
    defineFloat("delay_max","Delay Max (ms)", 80.0f, 10.0f, 400.0f, "%.0f");
    defineBool("through_walls", "Through Walls", true);
}

void Nuker::onEnable() {
    m_breaking = false;
    m_startSent = false;
    m_breakTicks = 0;
    m_breakCurrent = 0;
}

void Nuker::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;

    // ── Detect warnings ──────────────────────────────────────────────
    int   r      = (int)m_floatSettings["radius"];
    int   dMin   = (int)m_floatSettings["delay_min"];
    int   dMax   = (int)m_floatSettings["delay_max"];
    bool  surv   = (m_intSettings["mode"] == 1);

    if (surv && r > 5)
        addDetectWarning("Survival radius > 5", true,
            "Large survival-mode nuker radius creates suspicious block-break patterns. Reduce to 3-4.");
    if (surv && dMin < 20)
        addDetectWarning("Delay Min < 20ms", true,
            "Sub-20ms block break delay is inhuman. Vulcan/Spartan detect instant multi-break.");
    if (dMin < 5)
        addDetectWarning("Delay Min < 5ms", true,
            "Near-instant breaking. All anticheats detect this. Increase to 30-50ms minimum.");
    if (r > 7)
        addDetectWarning("Radius > 7", true,
            "Massive nuker radius — server logs will show impossible reach. Reduce to 4-6.");

    // ── Rate limiting ──────────────────────────────────────────────────
    auto now = std::chrono::steady_clock::now();
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    if (ms - std::chrono::duration_cast<std::chrono::milliseconds>(m_lastTick.time_since_epoch()).count() < 25) return;
    m_lastTick = now;

    // ── JNI-Init (einmalig) ───────────────────────────────────────────
    if (!s_jni) {
        env->PushLocalFrame(128);
        jobject mc = CMinecraft::getInstance();
        if (!mc) { env->PopLocalFrame(nullptr); return; }

        jclass mcC = env->GetObjectClass(mc);
        s_gmField = env->GetFieldID(mcC, Mappings::MC_gameMode, Mappings::MC_gameMode_Sig);
        if (env->ExceptionCheck()) { env->ExceptionClear(); s_gmField = nullptr; }

        s_gmCls = JvmWrapper::findClass(Mappings::GameMode_Class);
        if (s_gmCls) {
            s_start = env->GetMethodID(s_gmCls, Mappings::GameMode_startDestroyBlock,
                                       Mappings::GameMode_startDestroyBlock_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_start = nullptr; }
            s_continue = env->GetMethodID(s_gmCls, Mappings::GameMode_continueDestroyBlock,
                                          Mappings::GameMode_continueDestroyBlock_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_continue = nullptr; }
            s_stop = env->GetMethodID(s_gmCls, Mappings::GameMode_stopDestroyBlock,
                                      Mappings::GameMode_stopDestroyBlock_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_stop = nullptr; }
        }

        s_bpCls = JvmWrapper::findClass(Mappings::BlockPos_Class);
        if (s_bpCls) {
            s_bpInit = env->GetMethodID(s_bpCls, Mappings::BlockPos_Init, Mappings::BlockPos_Init_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_bpInit = nullptr; }
        }

        s_dirCls = JvmWrapper::findClass(Mappings::Direction_Class);
        if (s_dirCls) {
            s_dirDown = env->GetStaticFieldID(s_dirCls, Mappings::Direction_DOWN, Mappings::Direction_DOWN_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_dirDown = nullptr; }
        }

    s_mbpCls = JvmWrapper::findClass(Mappings::MutableBlockPos_Class);
    if (s_mbpCls) {
        s_mbpInit = env->GetMethodID(s_mbpCls, Mappings::MutableBlockPos_Init, Mappings::MutableBlockPos_Init_Sig);
        if (env->ExceptionCheck()) { env->ExceptionClear(); s_mbpInit = nullptr; }
        s_mbpSet  = env->GetMethodID(s_mbpCls, Mappings::MutableBlockPos_set, Mappings::MutableBlockPos_set_Sig);
        if (env->ExceptionCheck()) { env->ExceptionClear(); s_mbpSet = nullptr; }
    }

    // ── isAir-Check: Level.getBlockState(BlockPos).isAir() ──────────
    // Für Survival-Mode: Luftblöcke überspringen (startDestroyBlock
    // würde sonst sinnlos eine Animation starten). In 1.21.x ist
    // BlockStateBase.isAir() der korrekte JNI-Call.
    s_levelCls = JvmWrapper::findClass(Mappings::Level_Class);
    if (s_levelCls) {
        s_getBlockState = env->GetMethodID(s_levelCls, Mappings::Level_getBlockState, Mappings::Level_getBlockState_Sig);
        if (env->ExceptionCheck()) { env->ExceptionClear(); s_getBlockState = nullptr; }
    }
    s_bsCls = JvmWrapper::findClass(Mappings::BlockState_Class);
    if (s_bsCls) {
        s_isAir = env->GetMethodID(s_bsCls, Mappings::BlockStateBase_isAir, Mappings::BlockStateBase_isAir_Sig);
        if (env->ExceptionCheck()) { env->ExceptionClear(); s_isAir = nullptr; }
    }

        s_jni = true;
        printf(STR_C("[Draxo] Nuker JNI: gm=%p start=%p cont=%p stop=%p bp=%p/%p/mbp=%p dir=%p\n"),
               (void*)s_gmField, (void*)s_start, (void*)s_continue, (void*)s_stop,
               (void*)s_bpCls, (void*)s_bpInit, (void*)s_mbpCls, (void*)s_dirDown);
        env->PopLocalFrame(nullptr);
    }

    if (!s_jni || !s_gmField || !s_start || !s_bpCls || !s_bpInit || !s_dirDown) return;

    // ── Get player + game mode ─────────────────────────────────────────
    jobject mc = CMinecraft::getInstance();
    if (!mc) return;
    jobject gm = env->GetObjectField(mc, s_gmField);
    if (env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef(mc); return; }
    if (!gm) { env->DeleteLocalRef(mc); return; }

    jobject p = CMinecraft::getPlayer();
    if (!p) { env->DeleteLocalRef(gm); env->DeleteLocalRef(mc); return; }
    CEntity pl(p);
    int px = (int)std::floor(pl.getX());
    int py = (int)std::floor(pl.getY());
    int pz = (int)std::floor(pl.getZ());
    env->DeleteLocalRef(p);

    jobject dir = env->GetStaticObjectField(s_dirCls, s_dirDown);
    if (env->ExceptionCheck()) { env->ExceptionClear(); dir = nullptr; }

    bool thruWalls = m_boolSettings["through_walls"];
    if (dMax < dMin) dMax = dMin + 10;
    if (r < 1) r = 1; if (r > 8) r = 8;

    // MutableBlockPos — einmalig anlegen und im Loop per setPos wiederverwenden
    jobject mPos = nullptr;
    if (s_mbpCls && s_mbpInit && s_mbpSet) {
        mPos = env->NewObject(s_mbpCls, s_mbpInit, 0, 0, 0);
        if (env->ExceptionCheck()) { env->ExceptionClear(); mPos = nullptr; }
    }

    env->PushLocalFrame(512);  // Enough for isAir (world+state) per iteration

    // Fetch world ONCE for isAir — doing getWorld() inside the triple loop
    // creates ~2000 JNI calls per tick (one per block in 8-radius mode).
    jobject worldObj = surv ? CMinecraft::getWorld() : nullptr;

    int broken = 0;
    for (int dx = -r; dx <= r; dx++) {
        for (int dy = -r; dy <= r; dy++) {
            for (int dz = -r; dz <= r; dz++) {
                if (dx*dx + dy*dy + dz*dz > r*r) continue;

                // Random delay check (nur im Survival-Mode relevant)
                if (surv && dMax > 0) {
                    int wait = dMin + (int)(m_rng() % (unsigned)(dMax - dMin + 1));
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - m_lastTick).count();
                    if (elapsed < wait) { broken++; continue; }
                }

                // BlockPos setzen
                jobject pos;
                if (mPos) {
                    env->CallObjectMethod(mPos, s_mbpSet, px + dx, py + dy, pz + dz);
                    if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
                    pos = mPos;
                } else {
                    pos = env->NewObject(s_bpCls, s_bpInit, px + dx, py + dy, pz + dz);
                    if (!pos || env->ExceptionCheck()) { if (env->ExceptionCheck()) env->ExceptionClear(); continue; }
                }

                // ── isAir-Check: Luftblöcke überspringen (Survival-Mode) ──
                if (surv && worldObj && s_getBlockState && s_isAir) {
                    jobject state = env->CallObjectMethod(worldObj, s_getBlockState, pos);
                    if (!env->ExceptionCheck() && state) {
                        jboolean air = env->CallBooleanMethod(state, s_isAir);
                        env->DeleteLocalRef(state);
                        if (!env->ExceptionCheck() && air) {
                            if (!mPos) env->DeleteLocalRef(pos);
                            continue;  // Skip air blocks
                        }
                    } else {
                        if (env->ExceptionCheck()) env->ExceptionClear();
                    }
                }

                // ── Survival: State-Tracking über mehrere Ticks ─────────
                // startDestroyBlock startet den Abbau; continueDestroyBlock
                // muss JEDEN Tick aufgerufen werden, bis der Block bricht.
                // Der Server braucht ~10-15 Ticks pro Block bei Vanilla-Tools.
                if (surv) {
                    int bx = px + dx, by = py + dy, bz = pz + dz;
                    if (m_breaking && m_breakX == bx && m_breakY == by && m_breakZ == bz) {
                        // Continue current block
                        if (s_continue) {
                            env->CallVoidMethod(gm, s_continue, pos, dir);
                            if (env->ExceptionCheck()) env->ExceptionClear();
                            m_breakTicks++;
                            // Check if the block is now air (broken)
                            bool isNowAir = false;
                            if (worldObj && s_getBlockState && s_isAir && m_breakTicks > 4) {
                                jobject st = env->CallObjectMethod(worldObj, s_getBlockState, pos);
                                if (!env->ExceptionCheck() && st) {
                                    isNowAir = env->CallBooleanMethod(st, s_isAir) == JNI_TRUE;
                                    env->DeleteLocalRef(st);
                                } else { if (env->ExceptionCheck()) env->ExceptionClear(); }
                            }
                            if (isNowAir || m_breakTicks > 40) {
                                // Block broken or timeout (2s) — move on
                                m_breaking = false; m_startSent = false; m_breakTicks = 0;
                            }
                        }
                        broken++;
                        if (!mPos) env->DeleteLocalRef(pos);
                        continue;  // don't start a new block while breaking this one
                    }
                    // Start new block
                    jboolean started = env->CallBooleanMethod(gm, s_start, pos, dir);
                    if (env->ExceptionCheck()) { env->ExceptionClear(); if (!mPos) env->DeleteLocalRef(pos); continue; }
                    if (started) {
                        m_breaking = true; m_startSent = true;
                        m_breakX = bx; m_breakY = by; m_breakZ = bz;
                        m_breakTicks = 1;
                        // Call continue immediately to accelerate first tick
                        if (s_continue) {
                            env->CallVoidMethod(gm, s_continue, pos, dir);
                            if (env->ExceptionCheck()) env->ExceptionClear();
                        }
                        broken++;
                    }
                    if (!mPos) env->DeleteLocalRef(pos);
                    if (broken >= 1) break;  // one block per tick in survival
                } else {
                    // Creative: instant break
                    jboolean started = env->CallBooleanMethod(gm, s_start, pos, dir);
                    if (env->ExceptionCheck()) { env->ExceptionClear(); if (!mPos) env->DeleteLocalRef(pos); continue; }
                    if (started) {
                        if (s_stop) {
                            env->CallVoidMethod(gm, s_stop);
                            if (env->ExceptionCheck()) env->ExceptionClear();
                        }
                        broken++;
                    }
                }

                if (!mPos) env->DeleteLocalRef(pos);
                if (broken > 50) break; // Frame-Budget
            }
            if (broken > 50) break;
        }
        if (broken > 50) break;
    }

    env->PopLocalFrame(nullptr);
    if (worldObj) env->DeleteLocalRef(worldObj);
    if (mPos) env->DeleteLocalRef(mPos);
    if (dir) env->DeleteLocalRef(dir);
    env->DeleteLocalRef(gm);
    env->DeleteLocalRef(mc);
}
