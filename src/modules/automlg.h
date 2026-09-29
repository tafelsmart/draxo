#pragma once
#include "pch.h"
#include "modules/module.h"

class AutoMLG : public Module {
public:
    AutoMLG();
    void onEnable() override;
    void onUpdate(JNIEnv* env) override;

private:
    // ── Cached JNI IDs ──────────────────────────────────────────────
    static inline bool s_jni = false;

    // Player + inventory
    static inline jclass    s_playerCls  = nullptr;
    static inline jfieldID  s_invFld     = nullptr;
    static inline jclass    s_invCls     = nullptr;
    static inline jmethodID s_getItem    = nullptr;
    static inline jmethodID s_getSlot    = nullptr;
    static inline jmethodID s_setSlot    = nullptr;

    // ItemStack + Item
    static inline jclass    s_stackCls   = nullptr;
    static inline jmethodID s_stackEmpty = nullptr;
    static inline jmethodID s_stackGetItem = nullptr;
    static inline jclass    s_itemCls    = nullptr;
    static inline jmethodID s_itemDescId = nullptr;

    // Place via GameMode.useItemOn
    static inline jclass    s_mcCls      = nullptr;
    static inline jfieldID  s_gmFld      = nullptr;
    static inline jclass    s_gmCls      = nullptr;
    static inline jmethodID s_useItemOn  = nullptr;

    // BlockHitResult construction
    static inline jclass    s_bhrCls     = nullptr;
    static inline jmethodID s_bhrInit    = nullptr;
    static inline jclass    s_vec3Cls    = nullptr;
    static inline jmethodID s_vec3Init   = nullptr;
    static inline jclass    s_bpCls      = nullptr;
    static inline jmethodID s_bpInit     = nullptr;
    static inline jclass    s_dirCls     = nullptr;
    static inline jfieldID  s_dirUp      = nullptr;
    static inline jclass    s_handCls    = nullptr;
    static inline jfieldID  s_mainHand   = nullptr;

    // Block check for ground detection
    static inline jclass    s_lvlCls     = nullptr;
    static inline jmethodID s_getState   = nullptr;
    static inline jclass    s_bsCls      = nullptr;
    static inline jmethodID s_bs_isAir   = nullptr;
    static inline jclass    s_mbpCls     = nullptr;
    static inline jmethodID s_mbpInit    = nullptr;

    // ── State ───────────────────────────────────────────────────────
    int  m_mlgSlot    = -1;
    int  m_prevSlot   = -1;
    bool m_mlgActive  = false;
    int  m_tickWait   = 0;

    // ── Helpers ─────────────────────────────────────────────────────
    bool initJNI(JNIEnv* env);
    int  findMLGItem(JNIEnv* env, jobject inv);
    bool placeWater(JNIEnv* env, jobject player, jobject mc, int bx, int by, int bz);
    bool isBlockSolid(JNIEnv* env, jobject world, int bx, int by, int bz);
};
