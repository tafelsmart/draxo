#pragma once
#include "pch.h"
#include "modules/module.h"

class AutoFish : public Module {
public:
    AutoFish();
    void onEnable() override;
    void onUpdate(JNIEnv* env) override;

private:
    // ── Cached JNI ──────────────────────────────────────────────────
    static inline bool s_jni = false;

    // FishingHook class detection
    static inline jclass   s_fishHookCls = nullptr;

    // Entity iteration (reuse CWorld pattern)
    static inline jclass   s_lvlCls     = nullptr;
    static inline jmethodID s_entities  = nullptr;
    static inline jclass   s_iterCls    = nullptr;
    static inline jmethodID s_iterNext  = nullptr;
    static inline jmethodID s_iterHas   = nullptr;

    // Inventory + item check
    static inline jclass    s_playerCls  = nullptr;
    static inline jfieldID  s_invFld     = nullptr;
    static inline jclass    s_invCls     = nullptr;
    static inline jmethodID s_getItem    = nullptr;
    static inline jmethodID s_getSlot    = nullptr;
    static inline jmethodID s_setSlot    = nullptr;
    static inline jclass    s_stackCls   = nullptr;
    static inline jmethodID s_stackEmpty = nullptr;
    static inline jmethodID s_stackGetItem = nullptr;
    static inline jclass    s_itemCls    = nullptr;
    static inline jmethodID s_itemDescId = nullptr;

    // Entity.getDeltaMovement (for bite detection)
    static inline jclass    s_entCls     = nullptr;
    static inline jmethodID s_getDelta  = nullptr;

    // ── State machine ───────────────────────────────────────────────
    enum Phase { IDLE, CASTING, WAITING, REELING, COOLDOWN };
    Phase m_phase = IDLE;
    int   m_tick  = 0;
    int   m_cooldownTicks = 0;
    bool  m_hasRod = false;

    // ── Helpers ─────────────────────────────────────────────────────
    bool initJNI(JNIEnv* env);
    bool hasFishingRod(JNIEnv* env, jobject inv);
    jobject findBobber(JNIEnv* env, jobject world, int playerId);
    bool isBiting(JNIEnv* env, jobject bobber);
    void doCast();
    void doReel();
};
