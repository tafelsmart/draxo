#pragma once
#include "modules/module.h"
#include <random>
#include <chrono>

class Nuker : public Module {
public:
    Nuker();
    void onEnable() override;
    void onUpdate(JNIEnv* env) override;

private:
    std::chrono::steady_clock::time_point m_lastTick;
    std::mt19937 m_rng{std::random_device{}()};

    // ── Block-Break State (Survival mode) ─────────────────────────────
    // startDestroyBlock is called once; continueDestroyBlock must be called
    // every tick until the block breaks. We track which block is active.
    int   m_breakX = 0, m_breakY = 0, m_breakZ = 0;
    bool  m_breaking = false;
    int   m_breakTicks = 0;
    int   m_breakCurrent = 0;  // block index in the current layer scan
    bool  m_startSent = false; // true after startDestroyBlock for this block

    // Cached JNI (gesetzt in erstem onUpdate)
    static inline bool   s_jni = false;
    static inline jfieldID  s_gmField = nullptr;
    static inline jclass    s_gmCls   = nullptr;
    static inline jmethodID s_start   = nullptr;
    static inline jmethodID s_continue = nullptr;
    static inline jmethodID s_stop    = nullptr;
    static inline jclass    s_bpCls   = nullptr;
    static inline jmethodID s_bpInit  = nullptr;
    static inline jclass    s_dirCls  = nullptr;
    static inline jfieldID  s_dirDown = nullptr;
    // MutableBlockPos für schnellen Loop (kein NewObject pro Block)
    static inline jclass    s_mbpCls  = nullptr;
    static inline jmethodID s_mbpInit = nullptr;
    static inline jmethodID s_mbpSet  = nullptr;
    // isAir-Check (BlockStateBase.isAir)
    static inline jclass    s_levelCls  = nullptr;
    static inline jmethodID s_getBlockState = nullptr;
    static inline jclass    s_bsCls = nullptr;
    static inline jmethodID s_isAir = nullptr;
};
